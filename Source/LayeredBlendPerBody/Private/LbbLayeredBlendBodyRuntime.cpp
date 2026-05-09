

#include "LbbLayeredBlendBodyRuntime.h"

#include "LbbLayeredBlendBodyDefinition.h"
#include "Animation/AnimInstanceProxy.h"

namespace 
{
	static TMap<FName, int32> BuildCacheSlotIndexMap(const TArray<FName>& CacheSlotNames)
	{
		TMap<FName, int32> CacheSlotIndexMap;
		for (int32 Index = 0; Index < CacheSlotNames.Num(); ++Index)
		{
			CacheSlotIndexMap.Add(CacheSlotNames[Index], Index);
		}

		return CacheSlotIndexMap;
	}
	
	class FOperatorCompileContext final : public ILbbOperatorCompileContext
	{
	public:
		FOperatorCompileContext(
			TArray<FLbbSlotNodeData>& InSlotNodesData,
			TMap<FName, int32>& InCacheSlotIndexMap,
			TArray<FName>& InCacheSlotNames)
			: SlotNodesData(InSlotNodesData)
			, CacheSlotIndexMap(InCacheSlotIndexMap)
			, CacheSlotNames(InCacheSlotNames)
		{
		}

		virtual FLbbCompiledPoseSource CompileSource(const FLbbLayeredBodyPartPoseSource& InSource) override
		{
			FLbbCompiledPoseSource OutSource;

			switch (InSource.Type)
			{
			case ELbbLayeredBodyPartPoseSourceType::Motion:
				OutSource.Kind = ELbbCompiledPoseSourceKind::Motion;
				break;
			case ELbbLayeredBodyPartPoseSourceType::BasePose:
				OutSource.Kind = ELbbCompiledPoseSourceKind::BasePose;
				break;
			case ELbbLayeredBodyPartPoseSourceType::OverlayPose:
				OutSource.Kind = ELbbCompiledPoseSourceKind::OverlayPose;
				break;
			case ELbbLayeredBodyPartPoseSourceType::CurrentPose:
				OutSource.Kind = ELbbCompiledPoseSourceKind::CurrentPose;
				break;
			case ELbbLayeredBodyPartPoseSourceType::CachePose:
				OutSource.Kind = ELbbCompiledPoseSourceKind::CacheSlot;
				OutSource.PoseIndex = FindExistingCacheSlotIndex(InSource.CachePoseName);
				break;
			case ELbbLayeredBodyPartPoseSourceType::InputPose:
				OutSource.Kind = ELbbCompiledPoseSourceKind::InputPose;
				OutSource.PoseName = InSource.InputPoseName;
				break;
			default:
				break;
			}

			return OutSource;
		}

		virtual FLbbCompiledPoseTarget CompileTarget(const FLbbLayeredBodyPartPoseTarget& InTarget) override
		{
			FLbbCompiledPoseTarget OutTarget;

			switch (InTarget.Type)
			{
			case ELbbLayeredBodyPartPoseTargetType::CurrentPose:
				OutTarget.Kind = ELbbCompiledPoseTargetKind::CurrentPose;
				break;
			case ELbbLayeredBodyPartPoseTargetType::CachePose:
				OutTarget.Kind = ELbbCompiledPoseTargetKind::CacheSlot;
				OutTarget.PoseIndex = FindOrAddCacheSlotIndex(InTarget.CachePoseName);
				break;
			default:
				break;
			}

			return OutTarget;
		}

		virtual int32 FindOrAddSlotNode(const FName SlotName) override
		{
			if (SlotName.IsNone())
			{
				return INDEX_NONE;
			}

			for (int32 Index = 0; Index < SlotNodesData.Num(); ++Index)
			{
				if (SlotNodesData[Index].SlotNodeName == SlotName)
				{
					return Index;
				}
			}

			const int32 NewIndex = SlotNodesData.Add(FLbbSlotNodeData(SlotName));
			return NewIndex;
		}

	private:
		int32 FindExistingCacheSlotIndex(const FName CachePoseName) const
		{
			const int32* ExistingIndex = CacheSlotIndexMap.Find(CachePoseName);
			checkf(ExistingIndex != nullptr, TEXT("Cache slot '%s' was not registered before source compilation."), *CachePoseName.ToString());
			return *ExistingIndex;
		}

		int32 FindOrAddCacheSlotIndex(const FName CachePoseName)
		{
			if (CachePoseName.IsNone())
			{
				return INDEX_NONE;
			}

			if (const int32* ExistingIndex = CacheSlotIndexMap.Find(CachePoseName))
			{
				return *ExistingIndex;
			}

			const int32 NewIndex = CacheSlotNames.Add(CachePoseName);
			CacheSlotIndexMap.Add(CachePoseName, NewIndex);
			return NewIndex;
		}

		TArray<FLbbSlotNodeData>& SlotNodesData;
		TMap<FName, int32>& CacheSlotIndexMap;
		TArray<FName>& CacheSlotNames;
	};
	
}

//************************************
//   FLbbOperatorProgramRuntimeData
//************************************

FLbbOperatorProgramRuntimeData::FLbbOperatorProgramRuntimeData()
{
}

void FLbbOperatorProgramRuntimeData::Reset()
{
	Operators.Reset();
	SlotNodesData.Reset();
	UsedInputPoseNames.Reset();
	bNeedsBasePose = false;
	bNeedsOverlayPose = false;
	bNeedsSlotEvaluation = false;
	bCanAffectCurrentPose = false;
}


void FLbbOperatorProgramRuntimeData::BuildBodyBoneBlendWeights(const USkeleton* InSkeleton)
{
	for (TUniquePtr<FLbbCompiledOperator>& Operator : Operators)
	{
		Operator->BuildBoneBlendWeights(InSkeleton);
	}
}

void FLbbOperatorProgramRuntimeData::RebuildDesiredBoneWeights(const FBoneContainer& RequiredBones)
{
	for (TUniquePtr<FLbbCompiledOperator>& Operator : Operators)
	{
		Operator->RebuildBoneBlendWeights(RequiredBones);
	}
}

void FLbbOperatorProgramRuntimeData::UpdateBlendWeights(const FAnimationUpdateContext& Context)
{
	for (TUniquePtr<FLbbCompiledOperator>& Operator : Operators)
	{
		Operator->UpdateBlendWeight(Context.AnimInstanceProxy);
	}
}

void FLbbOperatorProgramRuntimeData::UpdateSlotNodeWeights(const FAnimationUpdateContext& Context)
{
	for (FLbbSlotNodeData& NodeData : SlotNodesData)
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

void FLbbOperatorProgramRuntimeData::AddCompiledOperatorToProgram(TUniquePtr<FLbbCompiledOperator>&& Operator)
{
	if (!Operator.IsValid())
	{
		return;
	}

	FLbbOperatorRequirements Requirements;
	Operator->AccumulateRequirements(Requirements);
	bNeedsBasePose |= Requirements.bNeedsBasePose;
	bNeedsOverlayPose |= Requirements.bNeedsOverlayPose;
	bNeedsSlotEvaluation |= Requirements.bNeedsSlotEvaluation;
	bCanAffectCurrentPose |= Requirements.bCanAffectCurrentPose;
	for (const FName InputPoseName : Requirements.UsedInputPoseNames)
	{
		UsedInputPoseNames.Add(InputPoseName);
	}
	Operators.Add(MoveTemp(Operator));
}

void FLbbOperatorProgramRuntimeData::CompileOperatorProgram(
	const TArray<FInstancedStruct>& OperatorDefinitions,
	TMap<FName, int32>& CacheSlotIndexMap,
	TArray<FName>& CacheSlotNames)
{
	Reset();
	FOperatorCompileContext CompileContext(
		SlotNodesData,
		CacheSlotIndexMap,
		CacheSlotNames);

	for (const FInstancedStruct& OperatorData : OperatorDefinitions)
	{
		const FLbbLayeredBlendBodyPartOperatorBase* OperatorDefinition = OperatorData.GetPtr<FLbbLayeredBlendBodyPartOperatorBase>();
		if (OperatorDefinition == nullptr || !OperatorDefinition->bEnabled)
		{
			continue;
		}

		AddCompiledOperatorToProgram(OperatorDefinition->CreateCompiledOperator(CompileContext));
	}
}

//************************************
//   FLbbLayeredBlendBodyRuntimeData
//************************************

FLbbLayeredBlendBodyRuntimeData::FLbbLayeredBlendBodyRuntimeData()
{
}

void FLbbLayeredBlendBodyRuntimeData::InitFromDefinition(
	const FLbbLayeredBlendBodyCacheProgram& CacheProgramDefinition,
	const TArray<FLbbLayeredBlendBodyPart>& BodyPartDefinitions)
{
	Reset();
	CacheSlotNames = CacheProgramDefinition.NamedCacheNames;
	TMap<FName, int32> CacheSlotIndexMap = BuildCacheSlotIndexMap(CacheSlotNames);

	CacheProgram.CompileOperatorProgram(CacheProgramDefinition.Operators, CacheSlotIndexMap, CacheSlotNames);
	bNeedsBasePose |= CacheProgram.IsNeedsBasePose();
	bNeedsOverlayPose |= CacheProgram.IsNeedsOverlayPose();
	bNeedsSlotEvaluation |= CacheProgram.IsNeedsSlotEvaluation();
	for (const FName InputPoseName : CacheProgram.UsedInputPoseNames)
	{
		UsedInputPoseNames.Add(InputPoseName);
	}

	BodyParts.Reset(BodyPartDefinitions.Num());
	for (const FLbbLayeredBlendBodyPart& BodyPartDefinition : BodyPartDefinitions)
	{
		FLbbOperatorProgramRuntimeData& RuntimeBodyPart = BodyParts.AddDefaulted_GetRef();
		RuntimeBodyPart.CompileOperatorProgram(BodyPartDefinition.Operators, CacheSlotIndexMap, CacheSlotNames);

		bNeedsBasePose |= RuntimeBodyPart.IsNeedsBasePose();
		bNeedsOverlayPose |= RuntimeBodyPart.IsNeedsOverlayPose();
		bNeedsSlotEvaluation |= RuntimeBodyPart.IsNeedsSlotEvaluation();
		bHasOutputAffectingOperators |= RuntimeBodyPart.CanAffectCurrentPose();
		for (const FName InputPoseName : RuntimeBodyPart.UsedInputPoseNames)
		{
			UsedInputPoseNames.Add(InputPoseName);
		}
	}

	bIsInitialized = true;
}

void FLbbLayeredBlendBodyRuntimeData::Reset()
{
	CacheProgram.Reset();
	BodyParts.Reset();
	CacheSlotNames.Reset();
	UsedInputPoseNames.Reset();
	bIsInitialized = false;
	bNeedsBasePose = false;
	bNeedsOverlayPose = false;
	bNeedsSlotEvaluation = false;
	bHasOutputAffectingOperators = false;
}

void FLbbLayeredBlendBodyRuntimeData::BuildBodyBoneBlendWeights(const USkeleton* InSkeleton)
{
	CacheProgram.BuildBodyBoneBlendWeights(InSkeleton);
	for (FLbbOperatorProgramRuntimeData& BodyPart : BodyParts)
	{
		BodyPart.BuildBodyBoneBlendWeights(InSkeleton);
	}
}

void FLbbLayeredBlendBodyRuntimeData::RebuildDesiredBoneWeights(const FBoneContainer& RequiredBones)
{
	CacheProgram.RebuildDesiredBoneWeights(RequiredBones);
	for (FLbbOperatorProgramRuntimeData& BodyPart : BodyParts)
	{
		BodyPart.RebuildDesiredBoneWeights(RequiredBones);
	}
}

void FLbbLayeredBlendBodyRuntimeData::UpdateBlendWeights(const FAnimationUpdateContext& Context)
{
	CacheProgram.UpdateBlendWeights(Context);
	for (FLbbOperatorProgramRuntimeData& BodyPart : BodyParts)
	{
		BodyPart.UpdateBlendWeights(Context);
	}
}

void FLbbLayeredBlendBodyRuntimeData::UpdateSlotNodeWeights(const FAnimationUpdateContext& Context)
{
	for (FLbbOperatorProgramRuntimeData& BodyPart : BodyParts)
	{
		BodyPart.UpdateSlotNodeWeights(Context);
	}
}
