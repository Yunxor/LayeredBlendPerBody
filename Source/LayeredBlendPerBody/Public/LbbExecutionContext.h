#pragma once

#include "CoreMinimal.h"
#include "LbbTypes.h"
#include "Animation/AnimNodeBase.h"
#include "Misc/Optional.h"

struct FLbbCompiledPoseSource;
struct FLbbCompiledPoseTarget;

struct FLbbOperatorExecutionInputs
{
	TArray<TOptional<FPoseContext>, TInlineAllocator<4>>* InputPoses = nullptr;
	const TMap<FName, int32>* InputPoseIndices = nullptr;
	TArray<TOptional<FPoseContext>, TInlineAllocator<4>>* CachedPoses = nullptr;
};

struct FLbbOperatorExecutionContext
{
	const TArray<FLbbSlotNodeData>& SlotNodesData;
	FPoseContext& CurrentPose;
	const FLbbOperatorExecutionInputs& Inputs;

	FLbbOperatorExecutionContext(
		const TArray<FLbbSlotNodeData>& InSlotNodesData,
		FPoseContext& InCurrentPose,
		const FLbbOperatorExecutionInputs& InInputs);

	static const FPoseContext& ResolveSource(const FLbbOperatorExecutionContext& Context, const FLbbCompiledPoseSource& Source);
	static FPoseContext& ResolveTarget(FLbbOperatorExecutionContext& Context, const FLbbCompiledPoseTarget& Target);
	static void AssignTarget(FLbbOperatorExecutionContext& Context, const FLbbCompiledPoseTarget& Target, const FPoseContext& Source);
	static bool CanEvaluateSlot(const FLbbOperatorExecutionContext& Context, int32 SlotNodeIndex);
	static void EvaluateSlot(const FLbbOperatorExecutionContext& Context, int32 SlotNodeIndex, const FPoseContext& SourcePose, FPoseContext& TargetPose);
	static void MakeAdditive(const FLbbOperatorExecutionContext& Context, const FPoseContext& BasePose, const FPoseContext& AdditivePose, FPoseContext& TargetPose, bool bMeshSpace);
	static void AccumulateAdditive(const FLbbOperatorExecutionContext& Context, FPoseContext& TargetPose, const FPoseContext& AdditivePose, bool bMeshSpace, float BlendWeight);
	static void BlendTwoPoses(const FLbbOperatorExecutionContext& Context, const FPoseContext& BasePose, const FPoseContext& BlendPose, FPoseContext& TargetPose, float BlendWeight);
};
