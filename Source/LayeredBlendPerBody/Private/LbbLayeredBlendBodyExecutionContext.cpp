
#include "LbbLayeredBlendBodyExecutionContext.h"
#include "LbbLayeredBlendBodyDefinition.h"
#include "Animation/AnimInstanceProxy.h"

namespace LbbLayeredBlendBodyPart
{
	FOperatorExecutionContext::FOperatorExecutionContext(
		const TArray<FSlotNodeData>& InSlotNodesData,
		FPoseContext& InOutputPose,
		const FBodyPartExecutionInputs& InInputs,
		const int32 NumTemporaryPoses)
		: SlotNodesData(InSlotNodesData)
		, OutputPose(InOutputPose)
		, Inputs(InInputs)
	{
		TemporaryPoses.SetNum(NumTemporaryPoses);
	}

	const FPoseContext& FOperatorExecutionContext::ResolveSource(const FOperatorExecutionContext& Context, const FCompiledPoseSource& Source)
	{
		switch (Source.Type)
		{
		case ELbbLayeredBodyPartPoseSourceType::Motion:
			return Context.Inputs.MotionPose;
		case ELbbLayeredBodyPartPoseSourceType::BasePose:
			check(Context.Inputs.BasePose != nullptr);
			return *Context.Inputs.BasePose;
		case ELbbLayeredBodyPartPoseSourceType::OverlayPose:
			check(Context.Inputs.OverlayPose != nullptr);
			return *Context.Inputs.OverlayPose;
		case ELbbLayeredBodyPartPoseSourceType::OutputPose:
			return Context.OutputPose;
		case ELbbLayeredBodyPartPoseSourceType::TemporaryPose:
			check(Context.TemporaryPoses.IsValidIndex(Source.TemporaryPoseIndex));
			check(Context.TemporaryPoses[Source.TemporaryPoseIndex].IsSet());
			return Context.TemporaryPoses[Source.TemporaryPoseIndex].GetValue();
		default:
			return Context.OutputPose;
		}
	}

	FPoseContext& FOperatorExecutionContext::ResolveTarget(FOperatorExecutionContext& Context, const FCompiledPoseTarget& Target)
	{
		if (Target.Type == ELbbLayeredBodyPartPoseTargetType::OutputPose)
		{
			return Context.OutputPose;
		}

		check(Context.TemporaryPoses.IsValidIndex(Target.TemporaryPoseIndex));
		if (!Context.TemporaryPoses[Target.TemporaryPoseIndex].IsSet())
		{
			Context.TemporaryPoses[Target.TemporaryPoseIndex].Emplace(Context.OutputPose);
		}

		return Context.TemporaryPoses[Target.TemporaryPoseIndex].GetValue();
	}

	void FOperatorExecutionContext::AssignTarget(FOperatorExecutionContext& Context, const FCompiledPoseTarget& Target, const FPoseContext& Source)
	{
		FPoseContext& TargetPose = ResolveTarget(Context, Target);
		TargetPose = Source;
	}

	bool FOperatorExecutionContext::CanEvaluateSlot(const FOperatorExecutionContext& Context, const int32 SlotNodeIndex)
	{
		if (!Context.SlotNodesData.IsValidIndex(SlotNodeIndex))
		{
			return false;
		}

		const FSlotNodeData& SlotNodeData = Context.SlotNodesData[SlotNodeIndex];
		return SlotNodeData.HasSlotNodeBlending();
	}

	void FOperatorExecutionContext::EvaluateSlot(const FOperatorExecutionContext& Context, const int32 SlotNodeIndex, const FPoseContext& SourcePose, FPoseContext& TargetPose)
	{
		check(Context.SlotNodesData.IsValidIndex(SlotNodeIndex));

		const FSlotNodeData& SlotNodeData = Context.SlotNodesData[SlotNodeIndex];

		FPoseContext SourcePoseCopy(Context.OutputPose);
		SourcePoseCopy = SourcePose;

		const FAnimationPoseData SourcePoseData(SourcePoseCopy);
		FAnimationPoseData OutputPoseData(TargetPose);
		Context.OutputPose.AnimInstanceProxy->SlotEvaluatePose(
			SlotNodeData.SlotNodeName,
			SourcePoseData,
			SlotNodeData.SourceWeight,
			OutputPoseData,
			SlotNodeData.SlotNodeWeight,
			SlotNodeData.TotalNodeWeight);
	}

	inline static void AccumulateAdditivePose(FPoseContext& InOut, const FCompactPose& AdditivePose, const bool bIsMeshSpace, const float Weight)
	{
		FAnimationPoseData OutputPoseData(InOut);
		const FAnimationPoseData AdditivePoseData = {const_cast<FCompactPose&>(AdditivePose), const_cast<FBlendedCurve&>(InOut.Curve), InOut.CustomAttributes};

		FAnimationRuntime::AccumulateAdditivePose(OutputPoseData, AdditivePoseData, Weight, (bIsMeshSpace ? AAT_RotationOffsetMeshSpace : AAT_LocalSpaceBase));
	}
	
	void FOperatorExecutionContext::AccumulateAdditive(const FOperatorExecutionContext& Context, FPoseContext& TargetPose, const bool bMeshSpace, const float BlendWeight)
	{
		if (FAnimWeight::IsRelevant(BlendWeight))
		{
			AccumulateAdditivePose(TargetPose, ResolveAdditivePose(Context, bMeshSpace), bMeshSpace, BlendWeight);
		}
	}

	void FOperatorExecutionContext::BlendTwoPoses(const FOperatorExecutionContext& Context, const FPoseContext& BasePose, const FPoseContext& BlendPose, FPoseContext& TargetPose, const float BlendWeight)
	{
		FPoseContext BasePoseCopy(Context.OutputPose);
		BasePoseCopy = BasePose;
		FPoseContext BlendPoseCopy(Context.OutputPose);
		BlendPoseCopy = BlendPose;

		const FAnimationPoseData BasePoseData(BasePoseCopy);
		const FAnimationPoseData BlendPoseData(BlendPoseCopy);
		FAnimationPoseData OutputPoseData(TargetPose);
		FAnimationRuntime::BlendTwoPosesTogether(BasePoseData, BlendPoseData, BlendWeight, OutputPoseData);
	}

	const FCompactPose& FOperatorExecutionContext::ResolveAdditivePose(const FOperatorExecutionContext& Context, const bool bMeshSpace)
	{
		const FCompactPose* AdditivePose = bMeshSpace ? Context.Inputs.AdditiveDeltaMS : Context.Inputs.AdditiveDeltaLS;
		check(AdditivePose != nullptr);
		return *AdditivePose;
	}
}
