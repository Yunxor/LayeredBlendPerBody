#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimTypes.h"
#include "BoneContainer.h"
#include "Animation/InputScaleBias.h"
#include "LbbLayeredBlendBodyTypes.generated.h"

namespace LbbLayeredBlendBodyPart
{
	struct FSlotNodeData
	{
		FName SlotNodeName = NAME_None;
		float SlotNodeWeight = 0.f;
		float SourceWeight = 0.f;
		float TotalNodeWeight = 0.f;

		FSlotNodeData() = default;
		explicit FSlotNodeData(const FName& InSlotNodeName)
			: SlotNodeName(InSlotNodeName)
		{
		}

		bool HasSlotNodeBlending() const
		{
			return !SlotNodeName.IsNone() && SlotNodeWeight > ZERO_ANIMWEIGHT_THRESH;
		}
	};
}

/// ==============================
///				Enum
/// ==============================

UENUM(BlueprintType)
enum class ELbbBoneSpace : uint8
{
	LocalSpace UMETA(DisplayName = "Local Space"),
	MeshSpace UMETA(DisplayName = "Mesh Space"),
};

/// ==============================
///				Struct
/// ==============================

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbBoneReferenceArray
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (TitleProperty = "BoneName"))
	TArray<FBoneReference> Bones;
};

struct LAYEREDBLENDPERBODY_API FLbbBoneIndexWeight
{
	FCompactPoseBoneIndex BoneIndex;
	float Weight;

	FLbbBoneIndexWeight()
			: BoneIndex(INDEX_NONE)
			, Weight(0.f)
	{
	}

	FLbbBoneIndexWeight(const FCompactPoseBoneIndex& InBoneIndex)
		: BoneIndex(InBoneIndex)
		, Weight(0.f)
	{
	}

	// Use example: MyTArray.Sort();
	FORCEINLINE bool operator<(const FLbbBoneIndexWeight& Other) const
	{
		return BoneIndex < Other.BoneIndex;
	}
};


USTRUCT(BlueprintType)
struct FLbbBlendWeight
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "!bUseBlendCurve"))
	float BlendWeight;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUseBlendCurve;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bUseBlendCurve"))
	FName BlendCurveName;
	// Bias : Scale
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bUseBlendCurve"))
	FInputScaleBias ScaleBias;
	
	FLbbBlendWeight(): BlendWeight(1.f), bUseBlendCurve(false), BlendCurveName(NAME_None){ }

	FLbbBlendWeight(bool bInUseBlendCurve, float InBlendWeight, FName InBlendCurveName)
		: BlendWeight(InBlendWeight)
		, bUseBlendCurve(bInUseBlendCurve)
		, BlendCurveName(InBlendCurveName)
	{}

	FLbbBlendWeight(const FLbbBlendWeight& Other)
	{
		bUseBlendCurve = Other.bUseBlendCurve;
		BlendWeight = Other.BlendWeight;
		BlendCurveName = Other.BlendCurveName;
		ScaleBias = Other.ScaleBias;
	}

	float GetBlendWeight() const { return BlendWeight; }
	void Reset() { BlendWeight = 0.f; }
};


UENUM(BlueprintType)
enum class ELbbLayeredBodyPartPoseSourceType : uint8
{
	Motion,
	BasePose,
	OverlayPose,
	OutputPose,
	TemporaryPose,
};

UENUM(BlueprintType)
enum class ELbbLayeredBodyPartPoseTargetType : uint8
{
	OutputPose,
	TemporaryPose,
};

USTRUCT(BlueprintType)
struct FLbbLayeredBodyPartPoseSource
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
	ELbbLayeredBodyPartPoseSourceType Type = ELbbLayeredBodyPartPoseSourceType::OutputPose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (EditCondition = "Type == ELbbLayeredBodyPartPoseSourceType::TemporaryPose", EditConditionHides))
	FName TemporaryPoseName = NAME_None;
};

USTRUCT(BlueprintType)
struct FLbbLayeredBodyPartPoseTarget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
	ELbbLayeredBodyPartPoseTargetType Type = ELbbLayeredBodyPartPoseTargetType::OutputPose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (EditCondition = "Type == ELbbLayeredBodyPartPoseTargetType::TemporaryPose", EditConditionHides))
	FName TemporaryPoseName = NAME_None;
};