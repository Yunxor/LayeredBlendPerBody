#include "LbbAnimNode_LayeredBlendBodyPart.h"
#include "LbbExecutionContext.h"
#include "LbbBodyPartDefinitionDataAsset.h"
#include "Animation/AnimInstanceProxy.h"
#include "Misc/Optional.h"

#if ENABLE_LBBANIMATION_DEBUG
static TAutoConsoleVariable<int32> CVarAnimNodeLayeredBodyPartDrawDebug(TEXT("lbb.AnimNode.LayeredBodyPartBlend.Debug.DrawBone"), -1, TEXT(" <0 : disable; 0: draw all; >0: specific body part."));
static TAutoConsoleVariable<float> CVarAnimNodeLayeredBodyPartDebugDrawSizeScale(TEXT("lbb.AnimNode.LayeredBodyPartBlend.Debug.DrawSizeScale"), 1, TEXT("Scale bone size."));
#endif

//************************************
//   FLbbAnimNode_LayeredBodyPartBlend
//************************************

namespace
{
	static void MovePoseContextData(FPoseContext& Dest, FPoseContext& Src)
	{
		Dest.Pose.MoveBonesFrom(Src.Pose);
		Dest.Curve.MoveFrom(Src.Curve);
		Dest.CustomAttributes.MoveFrom(Src.CustomAttributes);
	}

	static void ExecuteOperatorProgram(
		FLbbOperatorProgramRuntimeData& ProgramRuntimeData,
		const FLbbOperatorExecutionInputs& Inputs,
		FPoseContext& CurrentPose)
	{
		if (!ProgramRuntimeData.HasOperators())
		{
			return;
		}

		FLbbOperatorExecutionContext ExecutionContext(
			ProgramRuntimeData.SlotNodesData,
			CurrentPose,
			Inputs);
		for (const TUniquePtr<FLbbCompiledOperator>& Operator : ProgramRuntimeData.Operators)
		{
			Operator->Execute(ExecutionContext);
		}
	}

	static bool ShouldProcessInputPose(
		const FLbbRuntimeData& RuntimeData,
		const TArray<FLbbInputPoseAlias>& InputPoseAliases,
		const int32 PoseIndex,
		TSet<FName>* SeenAliases = nullptr)
	{
		if (!InputPoseAliases.IsValidIndex(PoseIndex))
		{
			return false;
		}

		const FName PoseAlias = InputPoseAliases[PoseIndex].PoseAlias;
		if (PoseAlias.IsNone() || !RuntimeData.UsesInputPose(PoseAlias))
		{
			return false;
		}

		if (SeenAliases != nullptr)
		{
			if (SeenAliases->Contains(PoseAlias))
			{
				return false;
			}

			SeenAliases->Add(PoseAlias);
		}

		return true;
	}
}


FLbbAnimNode_LayeredBlendBodyPart::FLbbAnimNode_LayeredBlendBodyPart()
{
}


void FLbbAnimNode_LayeredBlendBodyPart::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
	FAnimNode_Base::Initialize_AnyThread(Context);

	BasePose.Initialize(Context);
	for (FPoseLink& InputPose : InputPoses)
	{
		InputPose.Initialize(Context);
	}

	if (!RuntimeData.IsInitialized())
	{
		ReinitRuntimeData();
	}
}

void FLbbAnimNode_LayeredBlendBodyPart::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread)
	FAnimNode_Base::CacheBones_AnyThread(Context);

	BasePose.CacheBones(Context);
	for (FPoseLink& InputPose : InputPoses)
	{
		InputPose.CacheBones(Context);
	}

	UpdateCachedBoneData(Context.AnimInstanceProxy->GetRequiredBones(), Context.AnimInstanceProxy->GetSkeleton());
}

void FLbbAnimNode_LayeredBlendBodyPart::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread)

	if (IsLODEnabled(Context.AnimInstanceProxy))
	{
		GetEvaluateGraphExposedInputs().Execute(Context);

		if (BodyDefinition && !RuntimeData.IsInitialized())
		{
			ReinitRuntimeData();
		}

		if (BodyDefinition)
		{
			UpdateCachedBoneData(Context.AnimInstanceProxy->GetRequiredBones(), Context.AnimInstanceProxy->GetSkeleton());
			RuntimeData.UpdateBlendWeights(Context);

			if (RuntimeData.IsNeedsSlotEvaluation())
			{
				RuntimeData.UpdateSlotNodeWeights(Context);
			}
		}

		if (RuntimeData.HasInputPoseDependencies())
		{
			TSet<FName> UpdatedInputAliases;
			for (int32 PoseIndex = 0; PoseIndex < InputPoses.Num(); ++PoseIndex)
			{
				if (!ShouldProcessInputPose(RuntimeData, InputPoseAliases, PoseIndex, &UpdatedInputAliases))
				{
					continue;
				}

				InputPoses[PoseIndex].Update(Context.FractionalWeight(1.f));
			}
		}
	}

	BasePose.Update(Context.FractionalWeightAndRootMotion(1.f, 1.f));
}

void FLbbAnimNode_LayeredBlendBodyPart::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread)
	ANIM_MT_SCOPE_CYCLE_COUNTER_VERBOSE(LayeredBlendBody, !IsInGameThread());

	FScopedExpectsAdditiveOverride ScopedExpectsAdditiveOverride(Output, false);

	FPoseContext BaseEvalContext(Output);
	BasePose.Evaluate(BaseEvalContext);

	if (RuntimeData.GetBodyPartCount() < 1 || !RuntimeData.HasOutputAffectingOperators())
	{
		MovePoseContextData(Output, BaseEvalContext);
		return;
	}

	TArray<TOptional<FPoseContext>, TInlineAllocator<4>> InputPoseContexts;
	TMap<FName, int32> InputPoseIndexMap;
	InputPoseContexts.Add(BaseEvalContext);
	InputPoseIndexMap.Add(Lbb::GetBasePoseInputName(), 0);
	if (RuntimeData.HasInputPoseDependencies())
	{
		TSet<FName> EvaluatedInputAliases;
		for (int32 PoseIndex = 0; PoseIndex < InputPoses.Num(); ++PoseIndex)
		{
			if (!ShouldProcessInputPose(RuntimeData, InputPoseAliases, PoseIndex, &EvaluatedInputAliases))
			{
				continue;
			}

			const int32 RuntimeInputPoseIndex = InputPoseContexts.Emplace(Output);
			InputPoses[PoseIndex].Evaluate(InputPoseContexts[RuntimeInputPoseIndex].GetValue());
			InputPoseIndexMap.Add(InputPoseAliases[PoseIndex].PoseAlias, RuntimeInputPoseIndex);
		}
	}

	TArray<TOptional<FPoseContext>, TInlineAllocator<4>> CachedPoses;
	CachedPoses.SetNum(RuntimeData.GetCachedPoseCount());

	Output = BaseEvalContext;

	const FLbbOperatorExecutionInputs ExecutionInputs{
		&InputPoseContexts,
		&InputPoseIndexMap,
		&CachedPoses};

	FPoseContext CacheProgramCurrentPose(Output);
	CacheProgramCurrentPose = BaseEvalContext;
	ExecuteOperatorProgram(RuntimeData.CacheProgram, ExecutionInputs, CacheProgramCurrentPose);

	for (FLbbOperatorProgramRuntimeData& BodyPart : RuntimeData.BodyParts)
	{
		ExecuteOperatorProgram(BodyPart, ExecutionInputs, Output);
	}

	Output.Pose.NormalizeRotations();

	check(!Output.ContainsNaN());
	check(Output.IsNormalized());
}

void FLbbAnimNode_LayeredBlendBodyPart::SetBodyDefinition(const TObjectPtr<class ULbbBodyPartDefinitionDataAsset> InBodyDefinition)
{
	BodyDefinition = InBodyDefinition;
	ReinitRuntimeData();
}

int32 FLbbAnimNode_LayeredBlendBodyPart::AddInputPose()
{
	InputPoses.AddDefaulted();
	InputPoseAliases.AddDefaulted();
	return InputPoses.Num();
}

void FLbbAnimNode_LayeredBlendBodyPart::RemoveInputPose(const int32 PoseIndex)
{
	if (!InputPoses.IsValidIndex(PoseIndex))
	{
		return;
	}

	InputPoses.RemoveAt(PoseIndex);
	if (InputPoseAliases.IsValidIndex(PoseIndex))
	{
		InputPoseAliases.RemoveAt(PoseIndex);
	}

	SyncInputPoseAliases();
}

void FLbbAnimNode_LayeredBlendBodyPart::ReinitRuntimeData()
{
	SyncInputPoseAliases();
	RuntimeData.Reset();
	if (BodyDefinition)
	{
		RuntimeData.InitFromDefinition(BodyDefinition->CacheProgram, BodyDefinition->BodyParts);
	}

	SkeletonGuid.Invalidate();
	VirtualBoneGuid.Invalidate();
	RequiredBonesSerialNumber = -1;
}

void FLbbAnimNode_LayeredBlendBodyPart::SyncInputPoseAliases()
{
	InputPoseAliases.SetNum(InputPoses.Num());
}


void FLbbAnimNode_LayeredBlendBodyPart::RebuildBoneBlendWeights(const USkeleton* InSkeleton)
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

bool FLbbAnimNode_LayeredBlendBodyPart::AreValidBoneBlendWeights(const USkeleton* InSkeleton) const
{
	return (InSkeleton && InSkeleton->GetGuid() == SkeletonGuid && InSkeleton->GetVirtualBoneGuid() == VirtualBoneGuid);
}

void FLbbAnimNode_LayeredBlendBodyPart::UpdateCachedBoneData(const FBoneContainer& RequiredBones, const USkeleton* InSkeleton)
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
void FLbbAnimNode_LayeredBlendBodyPart::DrawPoses(const TArray<FCompactPose>& DrawPoses, FAnimInstanceProxy* Proxy)
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
