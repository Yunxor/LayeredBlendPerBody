// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LbbLayeredBlendBodyOperators.h"
#include "Animation/AnimNodeBase.h"

struct FInstancedStruct;
struct FLbbLayeredBlendBodyCacheProgram;
struct FLbbLayeredBlendBodyPart;

struct FLbbOperatorProgramRuntimeData
{
	TArray<TUniquePtr<FLbbCompiledOperator>> Operators;
	TArray<FLbbSlotNodeData> SlotNodesData;

	FLbbOperatorProgramRuntimeData();
	FLbbOperatorProgramRuntimeData(const FLbbOperatorProgramRuntimeData&) = delete;
	FLbbOperatorProgramRuntimeData& operator=(const FLbbOperatorProgramRuntimeData&) = delete;
	FLbbOperatorProgramRuntimeData(FLbbOperatorProgramRuntimeData&&) = default;
	FLbbOperatorProgramRuntimeData& operator=(FLbbOperatorProgramRuntimeData&&) = default;

	void Reset();
	void BuildBodyBoneBlendWeights(const USkeleton* InSkeleton);
	void RebuildDesiredBoneWeights(const FBoneContainer& RequiredBones);
	void UpdateBlendWeights(const FAnimationUpdateContext& Context);
	void UpdateSlotNodeWeights(const FAnimationUpdateContext& Context);
	
	void AddCompiledOperatorToProgram(TUniquePtr<FLbbCompiledOperator>&& Operator);
	void CompileOperatorProgram(
		const TArray<FInstancedStruct>& OperatorDefinitions,
		TMap<FName, int32>& CacheSlotIndexMap,
		TArray<FName>& CacheSlotNames);
	
	bool HasOperators() const { return !Operators.IsEmpty(); }
	bool CanAffectCurrentPose() const { return bCanAffectCurrentPose; }
	bool IsNeedsBasePose() const { return bNeedsBasePose; }
	bool IsNeedsOverlayPose() const { return bNeedsOverlayPose; }
	bool IsNeedsSlotEvaluation() const { return bNeedsSlotEvaluation; }

	bool bNeedsBasePose = false;
	bool bNeedsOverlayPose = false;
	bool bNeedsSlotEvaluation = false;
	bool bCanAffectCurrentPose = false;
};


struct FLbbLayeredBlendBodyRuntimeData
{
	FLbbOperatorProgramRuntimeData CacheProgram;
	TArray<FLbbOperatorProgramRuntimeData> BodyParts;
	TArray<FName> CacheSlotNames;

	FLbbLayeredBlendBodyRuntimeData();
	FLbbLayeredBlendBodyRuntimeData(const FLbbLayeredBlendBodyRuntimeData&) = delete;
	FLbbLayeredBlendBodyRuntimeData& operator=(const FLbbLayeredBlendBodyRuntimeData&) = delete;
	FLbbLayeredBlendBodyRuntimeData(FLbbLayeredBlendBodyRuntimeData&&) = default;
	FLbbLayeredBlendBodyRuntimeData& operator=(FLbbLayeredBlendBodyRuntimeData&&) = default;

	void InitFromDefinition(
		const FLbbLayeredBlendBodyCacheProgram& CacheProgramDefinition,
		const TArray<FLbbLayeredBlendBodyPart>& BodyPartDefinitions);

	bool IsInitialized() const { return bIsInitialized; }

	void Reset();

	void BuildBodyBoneBlendWeights(const USkeleton* InSkeleton);
	void RebuildDesiredBoneWeights(const FBoneContainer& RequiredBones);
	void UpdateBlendWeights(const FAnimationUpdateContext& Context);
	void UpdateSlotNodeWeights(const FAnimationUpdateContext& Context);

	int32 GetBodyPartCount() const { return BodyParts.Num(); }
	bool HasOutputAffectingOperators() const { return bHasOutputAffectingOperators; }
	bool IsNeedsBasePoseEvaluation() const { return bNeedsBasePose; }
	bool IsNeedsOverlayPoseEvaluation() const { return bNeedsOverlayPose; }
	bool IsNeedsSlotEvaluation() const { return bNeedsSlotEvaluation; }
	int32 GetCacheSlotCount() const { return CacheSlotNames.Num(); }

private:
	bool bIsInitialized = false;
	bool bNeedsBasePose = false;
	bool bNeedsOverlayPose = false;
	bool bNeedsSlotEvaluation = false;
	bool bHasOutputAffectingOperators = false;
};
