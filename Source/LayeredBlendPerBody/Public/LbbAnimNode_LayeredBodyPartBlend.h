#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "LbbLayeredBlendBodyDefinition.h"
#include "LbbLayeredBlendBodyRuntime.h"
#include "LbbLayeredBlendBodyTypes.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimNodeReference.h"
#include "LbbAnimNode_LayeredBodyPartBlend.generated.h"

#if ENABLE_ANIM_DEBUG && ENABLE_VISUAL_LOG
#define ENABLE_LBBANIMATION_DEBUG 1
#else
#define  ENABLE_LBBANIMATION_DEBUG 0
#endif

USTRUCT(BlueprintType)
struct FLbbAnimNodeRef_LayeredBodyPartBlend : public FAnimNodeReference
{
	GENERATED_BODY()

	typedef struct FLbbAnimNode_LayeredBodyPartBlend FInternalNodeType;
};

USTRUCT(BlueprintType)
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyInputPoseAlias
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FName PoseAlias = NAME_None;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, EditFixedSize, Category=Links, meta=(BlueprintCompilerGeneratedDefaults))
	TArray<FPoseLink> InputPoses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, EditFixedSize, Category="Inputs", meta=(TitleProperty = "PoseAlias"))
	TArray<FLbbLayeredBlendBodyInputPoseAlias> InputPoseAliases;
	
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
	const ULbbLayeredBlendBodyDefinition* GetBodyDefinition() const { return BodyDefinition.Get(); }
	void ReinitRuntimeData();
	int32 AddInputPose();
	void RemoveInputPose(int32 PoseIndex);
	
private:
	void SyncInputPoseAliases();
	void RebuildBoneBlendWeights(const USkeleton* InSkeleton);
	bool AreValidBoneBlendWeights(const USkeleton* InSkeleton) const;
	
	void UpdateCachedBoneData(const FBoneContainer& RequiredBones, const USkeleton* InSkeleton);
	
protected:
	FLbbLayeredBlendBodyRuntimeData RuntimeData;


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



