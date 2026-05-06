// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LbbLayeredBlendBodyEditorModel.h"
#include "Engine/DataAsset.h"
#include "LbbLayeredBlendBodyTypes.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "StructUtils/InstancedStruct.h"
#include "LbbLayeredBlendBodyDefinition.generated.h"

namespace LbbLayeredBlendBodyPart
{
	struct FOperatorRequirements
	{
		bool bNeedsBasePose = false;
		bool bNeedsOverlayPose = false;
		bool bNeedsMeshSpaceAdditive = false;
		bool bNeedsLocalSpaceAdditive = false;
		bool bNeedsSlotEvaluation = false;
		bool bCanAffectOutput = false;
	};
	
	struct FCompiledPoseSource
	{
		ELbbLayeredBodyPartPoseSourceType Type = ELbbLayeredBodyPartPoseSourceType::OutputPose;
		int32 TemporaryPoseIndex = INDEX_NONE;
	};
	
	struct FCompiledPoseTarget
	{
		ELbbLayeredBodyPartPoseTargetType Type = ELbbLayeredBodyPartPoseTargetType::OutputPose;
		int32 TemporaryPoseIndex = INDEX_NONE;
	};

	struct FOperatorExecutionContext;

	class LAYEREDBLENDPERBODY_API FCompiledOperator
	{
	public:
		virtual ~FCompiledOperator() = default;

		virtual void BuildBoneBlendWeights(const USkeleton* InSkeleton);
		virtual void RebuildBoneBlendWeights(const FBoneContainer& RequiredBones);
		virtual void UpdateBlendWeight(const FAnimInstanceProxy* InstanceProxy);
		virtual void AccumulateRequirements(FOperatorRequirements& Requirements) const = 0;
		virtual void Execute(FOperatorExecutionContext& ExecutionContext) const = 0;

	protected:
		static void AccumulateCommonRequirements(
			FOperatorRequirements& Requirements,
			const FCompiledPoseSource* SourceA,
			const FCompiledPoseSource* SourceB,
			const FCompiledPoseTarget* Target);
	};

	class IOperatorCompileContext
	{
	public:
		virtual ~IOperatorCompileContext() = default;

		virtual FCompiledPoseSource CompileSource(const FLbbLayeredBodyPartPoseSource& InSource) = 0;
		virtual FCompiledPoseTarget CompileTarget(const FLbbLayeredBodyPartPoseTarget& InTarget) = 0;
		virtual int32 FindOrAddSlotNode(FName SlotName) = 0;
	};

}


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
	virtual TUniquePtr<LbbLayeredBlendBodyPart::FCompiledOperator> CreateCompiledOperator(
		LbbLayeredBlendBodyPart::IOperatorCompileContext& CompileContext) const;
};


USTRUCT(BlueprintType, meta = (DisplayName = "Copy Pose"))
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyPartOperator_CopyPose : public FLbbLayeredBlendBodyPartOperatorBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FLbbLayeredBodyPartPoseSource SourcePose;

	virtual FString ToString() const override;
	virtual TUniquePtr<LbbLayeredBlendBodyPart::FCompiledOperator> CreateCompiledOperator(
		LbbLayeredBlendBodyPart::IOperatorCompileContext& CompileContext) const override;
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
	virtual TUniquePtr<LbbLayeredBlendBodyPart::FCompiledOperator> CreateCompiledOperator(
		LbbLayeredBlendBodyPart::IOperatorCompileContext& CompileContext) const override;
};


USTRUCT(BlueprintType, meta = (DisplayName = "Apply Additive"))
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyPartOperator_ApplyAdditive : public FLbbLayeredBlendBodyPartOperatorBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FLbbLayeredBodyPartPoseSource BasePose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	ELbbBoneSpace AdditiveSpace = ELbbBoneSpace::LocalSpace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FLbbBlendWeight Weight;

	virtual FString ToString() const override;
	virtual TUniquePtr<LbbLayeredBlendBodyPart::FCompiledOperator> CreateCompiledOperator(
		LbbLayeredBlendBodyPart::IOperatorCompileContext& CompileContext) const override;
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
	virtual TUniquePtr<LbbLayeredBlendBodyPart::FCompiledOperator> CreateCompiledOperator(
		LbbLayeredBlendBodyPart::IOperatorCompileContext& CompileContext) const override;
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

	// The function for calculate blend weight of the bones is the same as for the 'AnimNode_LayeredBlendPerBone'.
	UPROPERTY(EditAnywhere, Category = "Operator")
	FInputBlendPose BoneFilter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator")
	FLbbBlendWeight Weight;

	virtual FString ToString() const override;
	virtual TUniquePtr<LbbLayeredBlendBodyPart::FCompiledOperator> CreateCompiledOperator(
		LbbLayeredBlendBodyPart::IOperatorCompileContext& CompileContext) const override;
};


USTRUCT(BlueprintType)
struct FLbbLayeredBlendBodyPart
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Settings")
	FName PartName = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Recipe", meta = (BaseStruct = "/Script/LayeredBlendPerBody.LbbLayeredBlendBodyPartOperatorBase", ExcludeBaseStruct))
	TArray<FInstancedStruct> Operators;


#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = Config, meta=(ExcludeFromHash, DisplayPriority = 0))
	FLinearColor DebugColor = FLinearColor::Green;
#endif
	
};

/**
 * 
 */
UCLASS(BlueprintType, Category = "LbbAnimation|Layered Blend Body", meta = (DisplayName = "Lbb Layered Blend Body Definition", CollapseCategories))
class LAYEREDBLENDPERBODY_API ULbbLayeredBlendBodyDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category ="Runtime", meta = (TitleProperty = "{PartName}"))
	TArray<FLbbLayeredBlendBodyPart> BodyParts;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FLbbLayeredBlendBodyEditorModel EditorModel;
#endif
};
