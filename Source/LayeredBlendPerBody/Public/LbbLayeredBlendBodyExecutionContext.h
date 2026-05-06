#pragma once

#include "CoreMinimal.h"
#include "LbbLayeredBlendBodyTypes.h"
#include "Animation/AnimNodeBase.h"
#include "Misc/Optional.h"

namespace LbbLayeredBlendBodyPart
{
	struct FCompiledPoseSource;
	struct FCompiledPoseTarget;

	struct FBodyPartExecutionInputs
	{
		const FPoseContext& MotionPose;
		const FPoseContext* BasePose = nullptr;
		const FPoseContext* OverlayPose = nullptr;
		const FCompactPose* AdditiveDeltaLS = nullptr;
		const FCompactPose* AdditiveDeltaMS = nullptr;
	};

	struct FOperatorExecutionContext
	{
		const TArray<FSlotNodeData>& SlotNodesData;
		FPoseContext& OutputPose;
		const FBodyPartExecutionInputs& Inputs;
		TArray<TOptional<FPoseContext>, TInlineAllocator<4>> TemporaryPoses;

		FOperatorExecutionContext(
			const TArray<FSlotNodeData>& InSlotNodesData,
			FPoseContext& InOutputPose,
			const FBodyPartExecutionInputs& InInputs,
			int32 NumTemporaryPoses);

		static const FPoseContext& ResolveSource(const FOperatorExecutionContext& Context, const FCompiledPoseSource& Source);
		static FPoseContext& ResolveTarget(FOperatorExecutionContext& Context, const FCompiledPoseTarget& Target);
		static void AssignTarget(FOperatorExecutionContext& Context, const FCompiledPoseTarget& Target, const FPoseContext& Source);
		static bool CanEvaluateSlot(const FOperatorExecutionContext& Context, int32 SlotNodeIndex);
		static void EvaluateSlot(const FOperatorExecutionContext& Context, int32 SlotNodeIndex, const FPoseContext& SourcePose, FPoseContext& TargetPose);
		static void AccumulateAdditive(const FOperatorExecutionContext& Context, FPoseContext& TargetPose, bool bMeshSpace, float BlendWeight);
		static void BlendTwoPoses(const FOperatorExecutionContext& Context, const FPoseContext& BasePose, const FPoseContext& BlendPose, FPoseContext& TargetPose, float BlendWeight);
		static const FCompactPose& ResolveAdditivePose(const FOperatorExecutionContext& Context, bool bMeshSpace);
	};
}
