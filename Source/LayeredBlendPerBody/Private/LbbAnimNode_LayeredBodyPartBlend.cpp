#include "LbbAnimNode_LayeredBodyPartBlend.h"
#include "LbbLayeredBlendBodyExecutionContext.h"
#include "LbbLayeredBlendBodyDefinition.h"
#include "Animation/AnimInstanceProxy.h"
#include "Misc/Optional.h"

#if ENABLE_LBBANIMATION_DEBUG
static TAutoConsoleVariable<int32> CVarAnimNodeLayeredBodyPartDrawDebug(TEXT("lbb.AnimNode.LayeredBodyPartBlend.Debug.DrawBone"), -1, TEXT(" <0 : disable; 0: draw all; >0: specific body part."));
static TAutoConsoleVariable<float> CVarAnimNodeLayeredBodyPartDebugDrawSizeScale(TEXT("lbb.AnimNode.LayeredBodyPartBlend.Debug.DrawSizeScale"), 1, TEXT("Scale bone size."));
#endif

namespace LbbLayeredBlendBodyPart
{
	class FOperatorCompileContext final : public IOperatorCompileContext
	{
	public:
		explicit FOperatorCompileContext(FBodyPartRuntimeData& InRuntimeData)
			: RuntimeData(InRuntimeData)
		{
		}

		virtual FCompiledPoseSource CompileSource(const FLbbLayeredBodyPartPoseSource& InSource) override
		{
			FCompiledPoseSource OutSource;
			
			OutSource.Type = InSource.Type;
			if (InSource.Type == ELbbLayeredBodyPartPoseSourceType::TemporaryPose)
			{
				OutSource.TemporaryPoseIndex = FindOrAddTemporaryPose(InSource.TemporaryPoseName);
			}
			
			return OutSource;
		}

		virtual FCompiledPoseTarget CompileTarget(const FLbbLayeredBodyPartPoseTarget& InTarget) override
		{
			FCompiledPoseTarget OutTarget;
			
			OutTarget.Type = InTarget.Type;
			if (InTarget.Type == ELbbLayeredBodyPartPoseTargetType::TemporaryPose)
			{
				OutTarget.TemporaryPoseIndex = FindOrAddTemporaryPose(InTarget.TemporaryPoseName);
			}

			return OutTarget;
		}

		virtual int32 FindOrAddSlotNode(const FName SlotName) override
		{
			if (SlotName.IsNone())
			{
				return INDEX_NONE;
			}

			for (int32 Index = 0; Index < RuntimeData.SlotNodesData.Num(); ++Index)
			{
				if (RuntimeData.SlotNodesData[Index].SlotNodeName == SlotName)
				{
					return Index;
				}
			}

			const int32 NewIndex = RuntimeData.SlotNodesData.Add(FSlotNodeData(SlotName));
			return NewIndex;
		}

		int32 FindOrAddTemporaryPose(const FName TempPoseName)
		{
			if (TempPoseName.IsNone())
			{
				return INDEX_NONE;
			}

			if (const int32* ExistingIndex = TemporaryPoseIndexMap.Find(TempPoseName))
			{
				return *ExistingIndex;
			}

			const int32 NewIndex = RuntimeData.NumTemporaryPoses++;
			TemporaryPoseIndexMap.Add(TempPoseName, NewIndex);
			return NewIndex;
		}

	private:
		FBodyPartRuntimeData& RuntimeData;
		TMap<FName, int32> TemporaryPoseIndexMap;
	};
}

namespace LbbLayeredBlendBodyPart
{

	FBodyPartRuntimeData::FBodyPartRuntimeData()
	{
	}

	void FBodyPartRuntimeData::Reset()
	{
		Operators.Reset();
		NumTemporaryPoses = 0;
		SlotNodesData.Reset();

		bNeedsBasePose = false;
		bNeedsOverlayPose = false;
		bNeedsMeshSpaceAdditive = false;
		bNeedsLocalSpaceAdditive = false;
		bNeedsSlotEvaluation = false;
		bCanAffectOutput = false;
	}

	void FBodyPartRuntimeData::InitFromDefinition(const FLbbLayeredBlendBodyPart& BodyPartDefinition)
	{
		Reset();
		FOperatorCompileContext CompileContext(*this);

		auto AddOperator = [this](TUniquePtr<FCompiledOperator>&& Operator)
		{
			if (!Operator.IsValid())
			{
				return;
			}

			FOperatorRequirements Requirements;
			Operator->AccumulateRequirements(Requirements);
			bNeedsBasePose |= Requirements.bNeedsBasePose;
			bNeedsOverlayPose |= Requirements.bNeedsOverlayPose;
			bNeedsMeshSpaceAdditive |= Requirements.bNeedsMeshSpaceAdditive;
			bNeedsLocalSpaceAdditive |= Requirements.bNeedsLocalSpaceAdditive;
			bNeedsSlotEvaluation |= Requirements.bNeedsSlotEvaluation;
			bCanAffectOutput |= Requirements.bCanAffectOutput;

			Operators.Add(MoveTemp(Operator));
		};

		for (const FInstancedStruct& OperatorData : BodyPartDefinition.Operators)
		{
			const FLbbLayeredBlendBodyPartOperatorBase* OperatorDefinition = OperatorData.GetPtr<FLbbLayeredBlendBodyPartOperatorBase>();
			if (OperatorDefinition == nullptr || !OperatorDefinition->bEnabled)
			{
				continue;
			}

			AddOperator(OperatorDefinition->CreateCompiledOperator(CompileContext));
		}
	}

	void FBodyPartRuntimeData::BuildBodyBoneBlendWeights(const USkeleton* InSkeleton)
	{
		for (TUniquePtr<FCompiledOperator>& Operator : Operators)
		{
			Operator->BuildBoneBlendWeights(InSkeleton);
		}
	}

	void FBodyPartRuntimeData::RebuildDesiredBoneWeights(const FBoneContainer& RequiredBones)
	{
		for (TUniquePtr<FCompiledOperator>& Operator : Operators)
		{
			Operator->RebuildBoneBlendWeights(RequiredBones);
		}
	}

	void FBodyPartRuntimeData::UpdateBlendWeights(const FAnimationUpdateContext& Context)
	{
		for (TUniquePtr<FCompiledOperator>& Operator : Operators)
		{
			Operator->UpdateBlendWeight(Context.AnimInstanceProxy);
		}
	}

	void FBodyPartRuntimeData::UpdateSlotNodeWeights(const FAnimationUpdateContext& Context)
	{
		for (FSlotNodeData& NodeData : SlotNodesData)
		{
			if (NodeData.SlotNodeName.IsNone())
			{
				NodeData.SlotNodeWeight = 0.f;
				NodeData.SourceWeight = 0.f;
				NodeData.TotalNodeWeight = 0.f;
				continue;
			}

			Context.AnimInstanceProxy->GetSlotWeight(NodeData.SlotNodeName, NodeData.SlotNodeWeight, NodeData.SourceWeight, NodeData.TotalNodeWeight);
			Context.AnimInstanceProxy->UpdateSlotNodeWeight(NodeData.SlotNodeName, NodeData.SlotNodeWeight, Context.GetFinalBlendWeight());
		}
	}

	FRuntimeData::FRuntimeData()
	{
	}

	void FRuntimeData::InitFromDefinition(const TArray<FLbbLayeredBlendBodyPart>& BodyPartDefinitions)
	{
		Reset();

		BodyParts.Reset(BodyPartDefinitions.Num());
		for (const FLbbLayeredBlendBodyPart& BodyPartDefinition : BodyPartDefinitions)
		{
			FBodyPartRuntimeData& RuntimeBodyPart = BodyParts.AddDefaulted_GetRef();
			RuntimeBodyPart.InitFromDefinition(BodyPartDefinition);

			bNeedsBasePose |= RuntimeBodyPart.NeedsBasePose();
			bNeedsOverlayPose |= RuntimeBodyPart.NeedsOverlayPose();
			bNeedsMeshSpaceAdditive |= RuntimeBodyPart.NeedsMeshSpaceAdditive();
			bNeedsLocalSpaceAdditive |= RuntimeBodyPart.NeedsLocalSpaceAdditive();
			bNeedsSlotEvaluation |= RuntimeBodyPart.NeedsSlotEvaluation();
			bHasOutputAffectingOperators |= RuntimeBodyPart.CanAffectOutput();
		}

		bIsInitialized = true;
	}

	void FRuntimeData::Reset()
	{
		BodyParts.Reset();
		bIsInitialized = false;
		bNeedsBasePose = false;
		bNeedsOverlayPose = false;
		bNeedsMeshSpaceAdditive = false;
		bNeedsLocalSpaceAdditive = false;
		bNeedsSlotEvaluation = false;
		bHasOutputAffectingOperators = false;
	}

	void FRuntimeData::BuildBodyBoneBlendWeights(const USkeleton* InSkeleton)
	{
		for (FBodyPartRuntimeData& BodyPart : BodyParts)
		{
			BodyPart.BuildBodyBoneBlendWeights(InSkeleton);
		}
	}

	void FRuntimeData::RebuildDesiredBoneWeights(const FBoneContainer& RequiredBones)
	{
		for (FBodyPartRuntimeData& BodyPart : BodyParts)
		{
			BodyPart.RebuildDesiredBoneWeights(RequiredBones);
		}
	}

	void FRuntimeData::UpdateBlendWeights(const FAnimationUpdateContext& Context)
	{
		for (FBodyPartRuntimeData& BodyPart : BodyParts)
		{
			BodyPart.UpdateBlendWeights(Context);
		}
	}

	void FRuntimeData::UpdateSlotNodeWeights(const FAnimationUpdateContext& Context)
	{
		for (FBodyPartRuntimeData& BodyPart : BodyParts)
		{
			BodyPart.UpdateSlotNodeWeights(Context);
		}
	}
}

//***********************************************
//   ULbbLayeredBodyPartBlendAnimNodeLibrary
//***********************************************

FLbbAnimNodeRef_LayeredBodyPartBlend ULbbLayeredBodyPartBlendAnimNodeLibrary::ConvertToLayeredBodyPartBlendNode(const FAnimNodeReference& Node, EAnimNodeReferenceConversionResult& Result)
{
	return FAnimNodeReference::ConvertToType<FLbbAnimNodeRef_LayeredBodyPartBlend>(Node, Result);
}

void ULbbLayeredBodyPartBlendAnimNodeLibrary::SetBodyPartDefinition(const FLbbAnimNodeRef_LayeredBodyPartBlend& Node, class ULbbLayeredBlendBodyDefinition* InBodyDefinition)
{
	if (FLbbAnimNode_LayeredBodyPartBlend* AnimNodePtr = Node.GetAnimNodePtr<FLbbAnimNode_LayeredBodyPartBlend>())
	{
		AnimNodePtr->SetBodyDefinition(InBodyDefinition);
	}
	else
	{
		UE_LOG(LogAnimation, Warning, TEXT("ULbbLayeredBodyPartBlendAnimNodeLibrary::SetBodyPartDefinition called on an invalid context or with an invalid type"));
	}
}

//************************************
//   FLbbAnimNode_LayeredBodyPartBlend
//************************************

namespace
{
	static FCompactPose GetAdditivePose(const FCompactPose& Base, FCompactPose Additive, bool bIsMeshSpace)
	{
		if (bIsMeshSpace)
		{
			FCompactPose BaseMS = Base;
			FAnimationRuntime::ConvertPoseToMeshRotation(BaseMS);
			FAnimationRuntime::ConvertPoseToMeshRotation(Additive);
			FAnimationRuntime::ConvertPoseToAdditive(Additive, BaseMS);
		}
		else
		{
			FAnimationRuntime::ConvertPoseToAdditive(Additive, Base);
		}
		return Additive;
	}
	
	static void MovePoseContextData(FPoseContext& Dest, FPoseContext& Src)
	{
		Dest.Pose.MoveBonesFrom(Src.Pose);
		Dest.Curve.MoveFrom(Src.Curve);
		Dest.CustomAttributes.MoveFrom(Src.CustomAttributes);
	}
	
	static void ExecuteBodyPartOperators(
		LbbLayeredBlendBodyPart::FBodyPartRuntimeData& BodyPartRuntimeData,
		const LbbLayeredBlendBodyPart::FBodyPartExecutionInputs& Inputs,
		FPoseContext& Output)
	{
		if (!BodyPartRuntimeData.HasOperators())
		{
			return;
		}

		LbbLayeredBlendBodyPart::FOperatorExecutionContext ExecutionContext(
			BodyPartRuntimeData.SlotNodesData,
			Output,
			Inputs,
			BodyPartRuntimeData.NumTemporaryPoses);
		for (const TUniquePtr<LbbLayeredBlendBodyPart::FCompiledOperator>& Operator : BodyPartRuntimeData.Operators)
		{
			Operator->Execute(ExecutionContext);
		}
	}
}


FLbbAnimNode_LayeredBodyPartBlend::FLbbAnimNode_LayeredBodyPartBlend()
{
}


void FLbbAnimNode_LayeredBodyPartBlend::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
	FAnimNode_Base::Initialize_AnyThread(Context);

	Motion.Initialize(Context);
	OverlayPose.Initialize(Context);
	BasePose.Initialize(Context);

	if (!RuntimeData.IsInitialized())
	{
		ReinitRuntimeData();
	}
}

void FLbbAnimNode_LayeredBodyPartBlend::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread)
	FAnimNode_Base::CacheBones_AnyThread(Context);

	Motion.CacheBones(Context);
	OverlayPose.CacheBones(Context);
	BasePose.CacheBones(Context);

	UpdateCachedBoneData(Context.AnimInstanceProxy->GetRequiredBones(), Context.AnimInstanceProxy->GetSkeleton());
}

void FLbbAnimNode_LayeredBodyPartBlend::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread)

	if (IsLODEnabled(Context.AnimInstanceProxy) && BodyDefinition)
	{
		GetEvaluateGraphExposedInputs().Execute(Context);

		if (!RuntimeData.IsInitialized())
		{
			ReinitRuntimeData();
		}

		UpdateCachedBoneData(Context.AnimInstanceProxy->GetRequiredBones(), Context.AnimInstanceProxy->GetSkeleton());
		RuntimeData.UpdateBlendWeights(Context);

		if (RuntimeData.NeedsSlotEvaluation())
		{
			RuntimeData.UpdateSlotNodeWeights(Context);
		}

		if (RuntimeData.NeedsOverlayPoseEvaluation())
		{
			OverlayPose.Update(Context.FractionalWeight(1.f));
		}

		if (RuntimeData.NeedsBasePoseEvaluation() || RuntimeData.NeedsLocalSpaceAdditive() || RuntimeData.NeedsMeshSpaceAdditive())
		{
			BasePose.Update(Context.FractionalWeight(1.f));
		}
	}

	Motion.Update(Context.FractionalWeightAndRootMotion(1.f, 1.f));
}

void FLbbAnimNode_LayeredBodyPartBlend::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread)
	ANIM_MT_SCOPE_CYCLE_COUNTER_VERBOSE(LayeredBlendBody, !IsInGameThread());

	FScopedExpectsAdditiveOverride ScopedExpectsAdditiveOverride(Output, false);

	FPoseContext MotionEvalContext(Output);
	Motion.Evaluate(MotionEvalContext);

	if (RuntimeData.GetBodyPartCount() < 1 || !RuntimeData.HasOutputAffectingOperators())
	{
		MovePoseContextData(Output, MotionEvalContext);
		return;
	}

	TOptional<FPoseContext> BaseEvalContext;
	if (RuntimeData.NeedsBasePoseEvaluation() || RuntimeData.NeedsLocalSpaceAdditive() || RuntimeData.NeedsMeshSpaceAdditive())
	{
		BaseEvalContext.Emplace(Output);
		BasePose.Evaluate(BaseEvalContext.GetValue());
	}

	TOptional<FPoseContext> OverlayEvalContext;
	if (RuntimeData.NeedsOverlayPoseEvaluation())
	{
		OverlayEvalContext.Emplace(Output);
		OverlayPose.Evaluate(OverlayEvalContext.GetValue());
	}

	FCompactPose AdditiveDeltaLS;
	FCompactPose AdditiveDeltaMS;
	if (RuntimeData.NeedsLocalSpaceAdditive())
	{
		check(BaseEvalContext.IsSet());
		AdditiveDeltaLS = GetAdditivePose(BaseEvalContext->Pose, MotionEvalContext.Pose, false);
	}
	if (RuntimeData.NeedsMeshSpaceAdditive())
	{
		check(BaseEvalContext.IsSet());
		AdditiveDeltaMS = GetAdditivePose(BaseEvalContext->Pose, MotionEvalContext.Pose, true);
	}

	Output = MotionEvalContext;

	const LbbLayeredBlendBodyPart::FBodyPartExecutionInputs ExecutionInputs{
		MotionEvalContext,
		BaseEvalContext.IsSet() ? &BaseEvalContext.GetValue() : nullptr,
		OverlayEvalContext.IsSet() ? &OverlayEvalContext.GetValue() : nullptr,
		RuntimeData.NeedsLocalSpaceAdditive() ? &AdditiveDeltaLS : nullptr,
		RuntimeData.NeedsMeshSpaceAdditive() ? &AdditiveDeltaMS : nullptr};

	for (LbbLayeredBlendBodyPart::FBodyPartRuntimeData& BodyPart : RuntimeData.BodyParts)
	{
		ExecuteBodyPartOperators(BodyPart, ExecutionInputs, Output);
	}

	Output.Pose.NormalizeRotations();

	check(!Output.ContainsNaN());
	check(Output.IsNormalized());
}

void FLbbAnimNode_LayeredBodyPartBlend::SetBodyDefinition(const TObjectPtr<class ULbbLayeredBlendBodyDefinition> InBodyDefinition)
{
	BodyDefinition = InBodyDefinition;
	ReinitRuntimeData();
}

void FLbbAnimNode_LayeredBodyPartBlend::ReinitRuntimeData()
{
	RuntimeData.Reset();
	if (BodyDefinition)
	{
		RuntimeData.InitFromDefinition(BodyDefinition->BodyParts);
	}

	SkeletonGuid.Invalidate();
	VirtualBoneGuid.Invalidate();
	RequiredBonesSerialNumber = -1;
}


void FLbbAnimNode_LayeredBodyPartBlend::RebuildBoneBlendWeights(const USkeleton* InSkeleton)
{
	if (!BodyDefinition)
	{
		RuntimeData.Reset();
		return;
	}

	if (InSkeleton)
	{
		RuntimeData.BuildBodyBoneBlendWeights(InSkeleton);

		SkeletonGuid = InSkeleton->GetGuid();
		VirtualBoneGuid = InSkeleton->GetVirtualBoneGuid();
	}
}

bool FLbbAnimNode_LayeredBodyPartBlend::AreValidBoneBlendWeights(const USkeleton* InSkeleton) const
{
	return (InSkeleton && InSkeleton->GetGuid() == SkeletonGuid && InSkeleton->GetVirtualBoneGuid() == VirtualBoneGuid);
}

void FLbbAnimNode_LayeredBodyPartBlend::UpdateCachedBoneData(const FBoneContainer& RequiredBones, const USkeleton* InSkeleton)
{
	if (RequiredBonesSerialNumber == RequiredBones.GetSerialNumber())
	{
		return;
	}

	if (!AreValidBoneBlendWeights(InSkeleton))
	{
		RebuildBoneBlendWeights(InSkeleton);
	}

	RuntimeData.RebuildDesiredBoneWeights(RequiredBones);

	RequiredBonesSerialNumber = RequiredBones.GetSerialNumber();
}

#if ENABLE_LBBANIMATION_DEBUG
void FLbbAnimNode_LayeredBodyPartBlend::DrawPoses(const TArray<FCompactPose>& DrawPoses, FAnimInstanceProxy* Proxy)
{
	if (!Proxy || DrawPoses.Num() < 1)
	{
		return;
	}
	const int32 DebugDrawIndex = CVarAnimNodeLayeredBodyPartDrawDebug.GetValueOnAnyThread();
	if (DebugDrawIndex < 0)
	{
		return;
	}

	const float DrawSizeScale = FMath::Max(0.0001f, CVarAnimNodeLayeredBodyPartDebugDrawSizeScale.GetValueOnAnyThread());

	const FTransform& ComponentTransform = Proxy->GetComponentTransform();

	auto DrawPose = [&](const int32& PartIndex)
	{
		const FColor& DebugColor = BodyDefinition ? BodyDefinition->BodyParts[PartIndex].DebugColor.ToFColorSRGB() : FColor::White;
		FCSPose<FCompactPose> CSPose;
		CSPose.InitPose(DrawPoses[PartIndex]);

		for (const FCompactPoseBoneIndex& BoneIndex : CSPose.GetPose().ForEachBoneIndex())
		{
			const FVector BoneLoc = ComponentTransform.TransformPosition(CSPose.GetComponentSpaceTransform(BoneIndex).GetLocation());
			Proxy->AnimDrawDebugPoint(BoneLoc, DrawSizeScale, DebugColor, false);

			if (!BoneIndex.IsRootBone())
			{
				const FCompactPoseBoneIndex& ParentBoneIndex = CSPose.GetPose().GetParentBoneIndex(BoneIndex);
				const FVector ParentBoneLoc = ComponentTransform.TransformPosition(CSPose.GetComponentSpaceTransform(ParentBoneIndex).GetLocation());
				Proxy->AnimDrawDebugLine(ParentBoneLoc, BoneLoc, DebugColor, false, -1, DrawSizeScale);
			}
		}
	};

	if (DebugDrawIndex > 0)
	{
		DrawPose(DebugDrawIndex);
	}
	else
	{
		for (int32 PartIndex = 0; PartIndex < DrawPoses.Num(); PartIndex++)
		{
			DrawPose(PartIndex);
		}
	}
}
#endif
