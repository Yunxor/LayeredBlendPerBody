
#include "LbbLayeredBlendBodyExecutionContext.h"
#include "LbbLayeredBlendBodyOperators.h"
#include "Animation/AnimInstanceProxy.h"

namespace
{
	inline static void ConvertToAdditivePose(FPoseContext& InOutAdditive, const FPoseContext& BasePose, const bool bIsMeshSpace)
	{
		if (bIsMeshSpace)
		{
			FCompactPose BasePoseMeshSpace = BasePose.Pose;
			FAnimationRuntime::ConvertPoseToMeshRotation(BasePoseMeshSpace);
			FAnimationRuntime::ConvertPoseToMeshRotation(InOutAdditive.Pose);
			FAnimationRuntime::ConvertPoseToAdditive(InOutAdditive.Pose, BasePoseMeshSpace);
			return;
		}

		FAnimationRuntime::ConvertPoseToAdditive(InOutAdditive.Pose, BasePose.Pose);
	}

	inline static void AccumulateAdditivePose(
		FPoseContext& InOut,
		const FPoseContext& AdditivePose,
		const bool bIsMeshSpace,
		const float Weight)
	{
		FAnimationPoseData OutputPoseData(InOut);
		FCompactPose& MutableAdditivePose = const_cast<FCompactPose&>(AdditivePose.Pose);
		FBlendedCurve& MutableAdditiveCurve = const_cast<FBlendedCurve&>(AdditivePose.Curve);
		UE::Anim::FStackAttributeContainer& MutableAdditiveAttributes = const_cast<UE::Anim::FStackAttributeContainer&>(AdditivePose.CustomAttributes);
		const FAnimationPoseData AdditivePoseData(MutableAdditivePose, MutableAdditiveCurve, MutableAdditiveAttributes);

		FAnimationRuntime::AccumulateAdditivePose(
			OutputPoseData,
			AdditivePoseData,
			Weight,
			(bIsMeshSpace ? AAT_RotationOffsetMeshSpace : AAT_LocalSpaceBase));
	}
}

FLbbOperatorExecutionContext::FLbbOperatorExecutionContext(
	const TArray<FLbbSlotNodeData>& InSlotNodesData,
	FPoseContext& InCurrentPose,
	const FLbbOperatorExecutionInputs& InInputs)
	: SlotNodesData(InSlotNodesData)
	, CurrentPose(InCurrentPose)
	, Inputs(InInputs)
{
}

const FPoseContext& FLbbOperatorExecutionContext::ResolveSource(const FLbbOperatorExecutionContext& Context, const FLbbCompiledPoseSource& Source)
{
	switch (Source.Kind)
	{
	case ELbbCompiledPoseSourceKind::Motion:
	case ELbbCompiledPoseSourceKind::BasePose:
		return Context.Inputs.RootPose;
	case ELbbCompiledPoseSourceKind::OverlayPose:
		if (Context.Inputs.OverlayPose != nullptr)
		{
			return *Context.Inputs.OverlayPose;
		}
		ensureMsgf(false, TEXT("OverlayPose source was requested but no overlay pose was provided."));
		return Context.Inputs.RootPose;
	case ELbbCompiledPoseSourceKind::CurrentPose:
		return Context.CurrentPose;
	case ELbbCompiledPoseSourceKind::InputPose:
		{
			check(Context.Inputs.InputPoses != nullptr);
			check(Context.Inputs.InputPoseIndices != nullptr);
			const int32* InputPoseIndex = Context.Inputs.InputPoseIndices->Find(Source.PoseName);
			if (InputPoseIndex == nullptr || !Context.Inputs.InputPoses->IsValidIndex(*InputPoseIndex) || !(*Context.Inputs.InputPoses)[*InputPoseIndex].IsSet())
			{
				ensureMsgf(false, TEXT("Input pose '%s' was not found or not evaluated."), *Source.PoseName.ToString());
				return Context.Inputs.RootPose;
			}

			return (*Context.Inputs.InputPoses)[*InputPoseIndex].GetValue();
		}
	case ELbbCompiledPoseSourceKind::CacheSlot:
		check(Context.Inputs.CacheSlots != nullptr);
		check(Context.Inputs.CacheSlots->IsValidIndex(Source.PoseIndex));
		check((*Context.Inputs.CacheSlots)[Source.PoseIndex].IsSet());
		return (*Context.Inputs.CacheSlots)[Source.PoseIndex].GetValue();
	default:
		return Context.CurrentPose;
	}
}

FPoseContext& FLbbOperatorExecutionContext::ResolveTarget(FLbbOperatorExecutionContext& Context, const FLbbCompiledPoseTarget& Target)
{
	if (Target.Kind == ELbbCompiledPoseTargetKind::CurrentPose)
	{
		return Context.CurrentPose;
	}

	check(Target.Kind == ELbbCompiledPoseTargetKind::CacheSlot);
	check(Context.Inputs.CacheSlots != nullptr);
	check(Context.Inputs.CacheSlots->IsValidIndex(Target.PoseIndex));
	if (!(*Context.Inputs.CacheSlots)[Target.PoseIndex].IsSet())
	{
		(*Context.Inputs.CacheSlots)[Target.PoseIndex].Emplace(Context.CurrentPose);
	}

	return (*Context.Inputs.CacheSlots)[Target.PoseIndex].GetValue();
}

void FLbbOperatorExecutionContext::AssignTarget(FLbbOperatorExecutionContext& Context, const FLbbCompiledPoseTarget& Target, const FPoseContext& Source)
{
	FPoseContext& TargetPose = ResolveTarget(Context, Target);
	TargetPose = Source;
}

bool FLbbOperatorExecutionContext::CanEvaluateSlot(const FLbbOperatorExecutionContext& Context, const int32 SlotNodeIndex)
{
	if (!Context.SlotNodesData.IsValidIndex(SlotNodeIndex))
	{
		return false;
	}

	const FLbbSlotNodeData& SlotNodeData = Context.SlotNodesData[SlotNodeIndex];
	return SlotNodeData.HasSlotNodeBlending();
}

void FLbbOperatorExecutionContext::EvaluateSlot(const FLbbOperatorExecutionContext& Context, const int32 SlotNodeIndex, const FPoseContext& SourcePose, FPoseContext& TargetPose)
{
	check(Context.SlotNodesData.IsValidIndex(SlotNodeIndex));

	const FLbbSlotNodeData& SlotNodeData = Context.SlotNodesData[SlotNodeIndex];

	FPoseContext SourcePoseCopy(Context.CurrentPose);
	SourcePoseCopy = SourcePose;

	const FAnimationPoseData SourcePoseData(SourcePoseCopy);
	FAnimationPoseData OutputPoseData(TargetPose);
	Context.CurrentPose.AnimInstanceProxy->SlotEvaluatePose(
		SlotNodeData.SlotNodeName,
		SourcePoseData,
		SlotNodeData.SourceWeight,
		OutputPoseData,
		SlotNodeData.SlotNodeWeight,
		SlotNodeData.TotalNodeWeight);
}

void FLbbOperatorExecutionContext::MakeAdditive(
	const FLbbOperatorExecutionContext& Context,
	const FPoseContext& BasePose,
	const FPoseContext& AdditivePose,
	FPoseContext& TargetPose,
	const bool bMeshSpace)
{
	FPoseContext BasePoseCopy(Context.CurrentPose);
	BasePoseCopy = BasePose;
	TargetPose = AdditivePose;
	ConvertToAdditivePose(TargetPose, BasePoseCopy, bMeshSpace);
}

void FLbbOperatorExecutionContext::AccumulateAdditive(
	const FLbbOperatorExecutionContext& Context,
	FPoseContext& TargetPose,
	const FPoseContext& AdditivePose,
	const bool bMeshSpace,
	const float BlendWeight)
{
	if (FAnimWeight::IsRelevant(BlendWeight))
	{
		AccumulateAdditivePose(TargetPose, AdditivePose, bMeshSpace, BlendWeight);
	}
}

void FLbbOperatorExecutionContext::BlendTwoPoses(const FLbbOperatorExecutionContext& Context, const FPoseContext& BasePose, const FPoseContext& BlendPose, FPoseContext& TargetPose, const float BlendWeight)
{
	FPoseContext BasePoseCopy(Context.CurrentPose);
	BasePoseCopy = BasePose;
	FPoseContext BlendPoseCopy(Context.CurrentPose);
	BlendPoseCopy = BlendPose;

	const FAnimationPoseData BasePoseData(BasePoseCopy);
	const FAnimationPoseData BlendPoseData(BlendPoseCopy);
	FAnimationPoseData OutputPoseData(TargetPose);
	FAnimationRuntime::BlendTwoPosesTogether(BasePoseData, BlendPoseData, BlendWeight, OutputPoseData);
}
