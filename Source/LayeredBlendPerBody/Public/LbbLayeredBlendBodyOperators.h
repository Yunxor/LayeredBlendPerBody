// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "LbbLayeredBlendBodyTypes.h"
#include "LbbLayeredBlendBodyOperators.generated.h"

struct FAnimInstanceProxy;
struct FBoneContainer;
class USkeleton;
struct FLbbOperatorExecutionContext;

struct FLbbOperatorRequirements
{
	bool bNeedsBasePose = false;
	bool bNeedsOverlayPose = false;
	bool bNeedsSlotEvaluation = false;
	bool bCanAffectCurrentPose = false;
};

enum class ELbbCompiledPoseSourceKind : uint8
{
	Motion,
	BasePose,
	OverlayPose,
	CurrentPose,
	CacheSlot,
};

enum class ELbbCompiledPoseTargetKind : uint8
{
	CurrentPose,
	CacheSlot,
};

struct FLbbCompiledPoseSource
{
	ELbbCompiledPoseSourceKind Kind = ELbbCompiledPoseSourceKind::CurrentPose;
	int32 PoseIndex = INDEX_NONE;
};

struct FLbbCompiledPoseTarget
{
	ELbbCompiledPoseTargetKind Kind = ELbbCompiledPoseTargetKind::CurrentPose;
	int32 PoseIndex = INDEX_NONE;
};

class ILbbOperatorCompileContext
{
public:
	virtual ~ILbbOperatorCompileContext() = default;

	virtual FLbbCompiledPoseSource CompileSource(const FLbbLayeredBodyPartPoseSource& InSource) = 0;
	virtual FLbbCompiledPoseTarget CompileTarget(const FLbbLayeredBodyPartPoseTarget& InTarget) = 0;
	virtual int32 FindOrAddSlotNode(FName SlotName) = 0;
};

class LAYEREDBLENDPERBODY_API FLbbCompiledOperator
{
public:
	virtual ~FLbbCompiledOperator() = default;

	virtual void BuildBoneBlendWeights(const USkeleton* InSkeleton);
	virtual void RebuildBoneBlendWeights(const FBoneContainer& RequiredBones);
	virtual void UpdateBlendWeight(const FAnimInstanceProxy* InstanceProxy);
	virtual void AccumulateRequirements(FLbbOperatorRequirements& Requirements) const = 0;
	virtual void Execute(FLbbOperatorExecutionContext& ExecutionContext) const = 0;

protected:
	static void AccumulateCommonRequirements(
		FLbbOperatorRequirements& Requirements,
		const FLbbCompiledPoseSource* SourceA,
		const FLbbCompiledPoseSource* SourceB,
		const FLbbCompiledPoseTarget* Target);
};

USTRUCT(BlueprintType)
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyPartOperatorBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FLbbLayeredBodyPartPoseTarget Output;

	virtual ~FLbbLayeredBlendBodyPartOperatorBase() = default;

	virtual FString ToString() const;
	virtual TUniquePtr<FLbbCompiledOperator> CreateCompiledOperator(
		ILbbOperatorCompileContext& CompileContext) const;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Copy Pose"))
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyPartOperator_CopyPose : public FLbbLayeredBlendBodyPartOperatorBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FLbbLayeredBodyPartPoseSource SourcePose;

	virtual FString ToString() const override;
	virtual TUniquePtr<FLbbCompiledOperator> CreateCompiledOperator(
		ILbbOperatorCompileContext& CompileContext) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Slot"))
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyPartOperator_ApplySlot : public FLbbLayeredBlendBodyPartOperatorBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FLbbLayeredBodyPartPoseSource SourcePose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FName SlotName = NAME_None;

	virtual FString ToString() const override;
	virtual TUniquePtr<FLbbCompiledOperator> CreateCompiledOperator(
		ILbbOperatorCompileContext& CompileContext) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Apply Additive"))
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyPartOperator_ApplyAdditive : public FLbbLayeredBlendBodyPartOperatorBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FLbbLayeredBodyPartPoseSource BasePose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FLbbLayeredBodyPartPoseSource AdditivePose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	ELbbBoneSpace AdditiveSpace = ELbbBoneSpace::LocalSpace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FLbbBlendWeight Weight;

	virtual FString ToString() const override;
	virtual TUniquePtr<FLbbCompiledOperator> CreateCompiledOperator(
		ILbbOperatorCompileContext& CompileContext) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Make Additive"))
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyPartOperator_MakeAdditive : public FLbbLayeredBlendBodyPartOperatorBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FLbbLayeredBodyPartPoseSource BasePose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FLbbLayeredBodyPartPoseSource AdditivePose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	ELbbBoneSpace AdditiveSpace = ELbbBoneSpace::LocalSpace;

	virtual FString ToString() const override;
	virtual TUniquePtr<FLbbCompiledOperator> CreateCompiledOperator(
		ILbbOperatorCompileContext& CompileContext) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Blend Two Poses"))
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyPartOperator_BlendTwoPoses : public FLbbLayeredBlendBodyPartOperatorBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FLbbLayeredBodyPartPoseSource BasePose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FLbbLayeredBodyPartPoseSource BlendPose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FLbbBlendWeight Weight;

	virtual FString ToString() const override;
	virtual TUniquePtr<FLbbCompiledOperator> CreateCompiledOperator(
		ILbbOperatorCompileContext& CompileContext) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Layered blend per bone"))
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyPartOperator_MaskedBlend : public FLbbLayeredBlendBodyPartOperatorBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FLbbLayeredBodyPartPoseSource BasePose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FLbbLayeredBodyPartPoseSource BlendPose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	ELbbBoneSpace BlendSpace = ELbbBoneSpace::LocalSpace;

	UPROPERTY(EditAnywhere, Category = "Operator")
	FInputBlendPose BoneFilter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FLbbBlendWeight Weight;

	virtual FString ToString() const override;
	virtual TUniquePtr<FLbbCompiledOperator> CreateCompiledOperator(
		ILbbOperatorCompileContext& CompileContext) const override;
};
