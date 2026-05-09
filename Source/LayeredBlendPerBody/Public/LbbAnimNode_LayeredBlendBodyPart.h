#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "LbbBodyPartDefinitionDataAsset.h"
#include "LbbRuntime.h"
#include "LbbTypes.h"
#include "Animation/AnimNodeBase.h"
#include "LbbAnimNode_LayeredBlendBodyPart.generated.h"

#if ENABLE_ANIM_DEBUG && ENABLE_VISUAL_LOG
#define ENABLE_LBBANIMATION_DEBUG 1
#else
#define  ENABLE_LBBANIMATION_DEBUG 0
#endif

/**
 * 
 */
USTRUCT(BlueprintInternalUseOnly)
struct LAYEREDBLENDPERBODY_API FLbbAnimNode_LayeredBlendBodyPart : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FPoseLink BasePose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, EditFixedSize, Category=Links, meta=(BlueprintCompilerGeneratedDefaults))
	TArray<FPoseLink> InputPoses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, EditFixedSize, Category="Inputs", meta=(TitleProperty = "PoseAlias"))
	TArray<FLbbInputPoseAlias> InputPoseAliases;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Definition", meta =(PinShownByDefault))
	TObjectPtr<class ULbbBodyPartDefinitionDataAsset> BodyDefinition = nullptr;

public:
	FLbbAnimNode_LayeredBlendBodyPart();
	
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	void SetBodyDefinition(const TObjectPtr<class ULbbBodyPartDefinitionDataAsset> InBodyDefinition);
	const ULbbBodyPartDefinitionDataAsset* GetBodyDefinition() const { return BodyDefinition.Get(); }
	void ReinitRuntimeData();
	int32 AddInputPose();
	void RemoveInputPose(int32 PoseIndex);
	
private:
	void SyncInputPoseAliases();
	void RebuildBoneBlendWeights(const USkeleton* InSkeleton);
	bool AreValidBoneBlendWeights(const USkeleton* InSkeleton) const;
	
	void UpdateCachedBoneData(const FBoneContainer& RequiredBones, const USkeleton* InSkeleton);
	
protected:
	FLbbRuntimeData RuntimeData;


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
struct TStructOpsTypeTraits<FLbbAnimNode_LayeredBlendBodyPart> : public TStructOpsTypeTraitsBase2<FLbbAnimNode_LayeredBlendBodyPart>
{
	enum
	{
		WithCopy = false,
	};
};



