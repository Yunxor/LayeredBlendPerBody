#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "LbbLayeredBlendBodyDefinition.h"
#include "LbbLayeredBlendBodyTypes.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimNodeReference.h"
#include "LbbAnimNode_LayeredBodyPartBlend.generated.h"

#if ENABLE_ANIM_DEBUG && ENABLE_VISUAL_LOG
#define ENABLE_LBBANIMATION_DEBUG 1
#else
#define  ENABLE_LBBANIMATION_DEBUG 0
#endif

namespace LbbLayeredBlendBodyPart
{
	class FOperatorCompileContext;

	struct FBodyPartRuntimeData
	{
		TArray<TUniquePtr<FCompiledOperator>> Operators;
		int32 NumTemporaryPoses = 0;
		TArray<FSlotNodeData> SlotNodesData;

		FBodyPartRuntimeData();
		FBodyPartRuntimeData(const FBodyPartRuntimeData&) = delete;
		FBodyPartRuntimeData& operator=(const FBodyPartRuntimeData&) = delete;
		FBodyPartRuntimeData(FBodyPartRuntimeData&&) = default;
		FBodyPartRuntimeData& operator=(FBodyPartRuntimeData&&) = default;

		void Reset();
		void InitFromDefinition(const FLbbLayeredBlendBodyPart& BodyPartDefinition);
		void BuildBodyBoneBlendWeights(const USkeleton* InSkeleton);
		void RebuildDesiredBoneWeights(const FBoneContainer& RequiredBones);
		void UpdateBlendWeights(const FAnimationUpdateContext& Context);
		void UpdateSlotNodeWeights(const FAnimationUpdateContext& Context);

		bool HasOperators() const { return !Operators.IsEmpty(); }
		bool CanAffectOutput() const { return bCanAffectOutput; }
		bool NeedsBasePose() const { return bNeedsBasePose; }
		bool NeedsOverlayPose() const { return bNeedsOverlayPose; }
		bool NeedsMeshSpaceAdditive() const { return bNeedsMeshSpaceAdditive; }
		bool NeedsLocalSpaceAdditive() const { return bNeedsLocalSpaceAdditive; }
		bool NeedsSlotEvaluation() const { return bNeedsSlotEvaluation; }

	private:
		friend class FOperatorCompileContext;

		bool bNeedsBasePose = false;
		bool bNeedsOverlayPose = false;
		bool bNeedsMeshSpaceAdditive = false;
		bool bNeedsLocalSpaceAdditive = false;
		bool bNeedsSlotEvaluation = false;
		bool bCanAffectOutput = false;
	};

	struct FRuntimeData
	{
		TArray<FBodyPartRuntimeData> BodyParts; // Index: Body Part.

		FRuntimeData();
		FRuntimeData(const FRuntimeData&) = delete;
		FRuntimeData& operator=(const FRuntimeData&) = delete;
		FRuntimeData(FRuntimeData&&) = default;
		FRuntimeData& operator=(FRuntimeData&&) = default;

		void InitFromDefinition(const TArray<FLbbLayeredBlendBodyPart>& BodyPartDefinitions);

		bool IsInitialized() const {return bIsInitialized; }

		void Reset();

		void BuildBodyBoneBlendWeights(const USkeleton* InSkeleton);
		void RebuildDesiredBoneWeights(const FBoneContainer& RequiredBones);
		void UpdateBlendWeights(const FAnimationUpdateContext& Context);
		void UpdateSlotNodeWeights(const FAnimationUpdateContext& Context);

		int32 GetBodyPartCount() const { return BodyParts.Num(); }
		bool HasOutputAffectingOperators() const { return bHasOutputAffectingOperators; }
		bool NeedsBasePoseEvaluation() const { return bNeedsBasePose; }
		bool NeedsOverlayPoseEvaluation() const { return bNeedsOverlayPose; }
		bool NeedsMeshSpaceAdditive() const { return bNeedsMeshSpaceAdditive; }
		bool NeedsLocalSpaceAdditive() const { return bNeedsLocalSpaceAdditive; }
		bool NeedsSlotEvaluation() const { return bNeedsSlotEvaluation; }

	private:
		bool bIsInitialized = false;
		bool bNeedsBasePose = false;
		bool bNeedsOverlayPose = false;
		bool bNeedsMeshSpaceAdditive = false;
		bool bNeedsLocalSpaceAdditive = false;
		bool bNeedsSlotEvaluation = false;
		bool bHasOutputAffectingOperators = false;
	};
}


USTRUCT(BlueprintType)
struct FLbbAnimNodeRef_LayeredBodyPartBlend : public FAnimNodeReference
{
	GENERATED_BODY()

	typedef struct FLbbAnimNode_LayeredBodyPartBlend FInternalNodeType;
};

UCLASS()
class LAYEREDBLENDPERBODY_API ULbbLayeredBodyPartBlendAnimNodeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "LbbAnimation|LayeredBodyPartBlend", meta = (NotBlueprintThreadSafe, ExpandEnumAsExecs = "Result", DisplayName = "Convert to Layered Blend Body Part Node"))
	static FLbbAnimNodeRef_LayeredBodyPartBlend ConvertToLayeredBodyPartBlendNode(const FAnimNodeReference& Node, EAnimNodeReferenceConversionResult& Result);

	UFUNCTION(BlueprintPure, Category = "LbbAnimation|LayeredBodyPartBlend", meta = (BlueprintThreadSafe, DisplayName = "Convert to Layered Blend Body Part Node (Pure)"))
	static void ConvertToLayeredBodyPartBlendNodePure(const FAnimNodeReference& Node, FLbbAnimNodeRef_LayeredBodyPartBlend& LayeredBodyPartBlendNode, bool& Result)
	{
		EAnimNodeReferenceConversionResult ConversionResult;
		LayeredBodyPartBlendNode = ConvertToLayeredBodyPartBlendNode(Node, ConversionResult);
		Result = (ConversionResult == EAnimNodeReferenceConversionResult::Succeeded);
	};
	
	UFUNCTION(BlueprintCallable, Category = "LbbAnimation|LayeredBodyPartBlend", meta = (NotBlueprintThreadSafe))
	static void SetBodyPartDefinition(const FLbbAnimNodeRef_LayeredBodyPartBlend& Node, class ULbbLayeredBlendBodyDefinition* InBodyDefinition);
};

/**
 * 
 */
USTRUCT(BlueprintInternalUseOnly)
struct LAYEREDBLENDPERBODY_API FLbbAnimNode_LayeredBodyPartBlend : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FPoseLink BasePose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FPoseLink OverlayPose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FPoseLink Motion;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Definition", meta =(PinShownByDefault))
	TObjectPtr<class ULbbLayeredBlendBodyDefinition> BodyDefinition = nullptr;

public:
	FLbbAnimNode_LayeredBodyPartBlend();
	
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	void SetBodyDefinition(const TObjectPtr<class ULbbLayeredBlendBodyDefinition> InBodyDefinition);
	void ReinitRuntimeData();
	
private:
	void RebuildBoneBlendWeights(const USkeleton* InSkeleton);
	bool AreValidBoneBlendWeights(const USkeleton* InSkeleton) const;
	
	void UpdateCachedBoneData(const FBoneContainer& RequiredBones, const USkeleton* InSkeleton);
	
protected:
	LbbLayeredBlendBodyPart::FRuntimeData RuntimeData;


	// Guids for skeleton used to determine whether the PerBoneBlendWeights need rebuilding
	UPROPERTY()
	FGuid SkeletonGuid;

	// Guid for virtual bones used to determine whether the PerBoneBlendWeights need rebuilding
	UPROPERTY()
	FGuid VirtualBoneGuid;

	// Serial number of the required bones container
	uint16 RequiredBonesSerialNumber = -1;

#if ENABLE_LBBANIMATION_DEBUG
	void DrawPoses(const TArray<FCompactPose>& DrawPoses, FAnimInstanceProxy* Proxy);
#endif
};

template<>
struct TStructOpsTypeTraits<FLbbAnimNode_LayeredBodyPartBlend> : public TStructOpsTypeTraitsBase2<FLbbAnimNode_LayeredBodyPartBlend>
{
	enum
	{
		WithCopy = false,
	};
};



