#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimTypes.h"
#include "BoneContainer.h"
#include "Animation/InputScaleBias.h"
#include "LbbTypes.generated.h"

struct FLbbSlotNodeData
{
	FName SlotNodeName = NAME_None;
	float SlotNodeWeight = 0.f;
	float SourceWeight = 0.f;
	float TotalNodeWeight = 0.f;

	FLbbSlotNodeData() = default;
	explicit FLbbSlotNodeData(const FName& InSlotNodeName)
		: SlotNodeName(InSlotNodeName)
	{
	}

	bool HasSlotNodeBlending() const
	{
		return !SlotNodeName.IsNone() && SlotNodeWeight > ZERO_ANIMWEIGHT_THRESH;
	}
};

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

USTRUCT(BlueprintType)
struct LAYEREDBLENDPERBODY_API FLbbInputPoseDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FName InputName = NAME_None;
};


USTRUCT(BlueprintType)
struct LAYEREDBLENDPERBODY_API FLbbInputPoseAlias
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FName PoseAlias = NAME_None;
};


namespace Lbb
{
	inline const FName& GetBasePoseInputName()
	{
		static const FName BasePoseInputName(TEXT("BasePose"));
		return BasePoseInputName;
	}

	inline bool IsBuiltInInputName(const FName InputName)
	{
		return InputName == GetBasePoseInputName();
	}
}

UENUM(BlueprintType)
enum class ELbbLayeredBodyPartPoseSourceType : uint8
{
	Motion UMETA(Hidden),
	BasePose,
	OverlayPose UMETA(Hidden),
	CurrentPose UMETA(Hidden),
	CachePose,
	InputPose,
};

UENUM(BlueprintType)
enum class ELbbLayeredBodyPartPoseTargetType : uint8
{
	CurrentPose,
	CachePose,
};

USTRUCT(BlueprintType)
struct FLbbLayeredBodyPartPoseSource
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
	ELbbLayeredBodyPartPoseSourceType Type = ELbbLayeredBodyPartPoseSourceType::CurrentPose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (EditCondition = "Type == ELbbLayeredBodyPartPoseSourceType::CachePose", EditConditionHides))
	FName CachePoseName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (EditCondition = "Type == ELbbLayeredBodyPartPoseSourceType::InputPose", EditConditionHides))
	FName InputPoseName = NAME_None;
};

USTRUCT(BlueprintType)
struct FLbbLayeredBodyPartPoseTarget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
	ELbbLayeredBodyPartPoseTargetType Type = ELbbLayeredBodyPartPoseTargetType::CurrentPose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (EditCondition = "Type == ELbbLayeredBodyPartPoseTargetType::CachePose", EditConditionHides))
	FName CachePoseName = NAME_None;
};
