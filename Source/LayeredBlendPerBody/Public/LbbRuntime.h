// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LbbOperators.h"
#include "Animation/AnimNodeBase.h"

struct FInstancedStruct;
struct FLbbCacheProgram;
struct FLbbPart;

struct FLbbOperatorProgramRuntimeData
{
	TArray<TUniquePtr<FLbbCompiledOperator>> Operators;
	TArray<FLbbSlotNodeData> SlotNodesData;
	TSet<FName> UsedInputPoseNames;

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
		TMap<FName, int32>& CachedPoseIndexMap,
		TArray<FName>& CachedPoseNames);
	
	bool HasOperators() const { return !Operators.IsEmpty(); }
	bool CanAffectCurrentPose() const { return bCanAffectCurrentPose; }
	bool IsNeedsSlotEvaluation() const { return bNeedsSlotEvaluation; }
	bool UsesInputPose(FName PoseName) const { return UsedInputPoseNames.Contains(PoseName); }

	bool bNeedsSlotEvaluation = false;
	bool bCanAffectCurrentPose = false;
};


struct FLbbRuntimeData
{
	FLbbOperatorProgramRuntimeData CacheProgram;
	TArray<FLbbOperatorProgramRuntimeData> BodyParts;
	TArray<FName> CachedPoseNames;
	TSet<FName> UsedInputPoseNames;

	FLbbRuntimeData();
	FLbbRuntimeData(const FLbbRuntimeData&) = delete;
	FLbbRuntimeData& operator=(const FLbbRuntimeData&) = delete;
	FLbbRuntimeData(FLbbRuntimeData&&) = default;
	FLbbRuntimeData& operator=(FLbbRuntimeData&&) = default;

	void InitFromDefinition(
		const FLbbCacheProgram& CacheProgramDefinition,
		const TArray<FLbbPart>& BodyPartDefinitions);

	bool IsInitialized() const { return bIsInitialized; }

	void Reset();

	void BuildBodyBoneBlendWeights(const USkeleton* InSkeleton);
	void RebuildDesiredBoneWeights(const FBoneContainer& RequiredBones);
	void UpdateBlendWeights(const FAnimationUpdateContext& Context);
	void UpdateSlotNodeWeights(const FAnimationUpdateContext& Context);

	int32 GetBodyPartCount() const { return BodyParts.Num(); }
	bool HasOutputAffectingOperators() const { return bHasOutputAffectingOperators; }
	bool IsNeedsSlotEvaluation() const { return bNeedsSlotEvaluation; }
	int32 GetCachedPoseCount() const { return CachedPoseNames.Num(); }
	bool HasInputPoseDependencies() const { return !UsedInputPoseNames.IsEmpty(); }
	bool UsesInputPose(FName PoseName) const { return UsedInputPoseNames.Contains(PoseName); }

private:
	bool bIsInitialized = false;
	bool bNeedsSlotEvaluation = false;
	bool bHasOutputAffectingOperators = false;
};
