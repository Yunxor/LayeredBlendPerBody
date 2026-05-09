// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "LbbTypes.h"
#include "LbbGraphNodeData.generated.h"

UENUM()
enum class ELbbGraphKind : uint8
{
	Cache,
	BodyPart,
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbGraphNodeDataBase
{
	GENERATED_BODY()
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbGraphNodeData_Input : public FLbbGraphNodeDataBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Node")
	FLbbLayeredBodyPartPoseSource SourcePose;
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbGraphNodeData_Result : public FLbbGraphNodeDataBase
{
	GENERATED_BODY()
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbGraphNodeData_ApplySlot : public FLbbGraphNodeDataBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Node")
	FName SlotName = NAME_None;
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbGraphNodeData_Blend : public FLbbGraphNodeDataBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Node")
	FLbbBlendWeight Weight;
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbGraphNodeData_MaskedBlend : public FLbbGraphNodeDataBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Node")
	ELbbBoneSpace BlendSpace = ELbbBoneSpace::LocalSpace;

	UPROPERTY(EditAnywhere, Category = "Node")
	FInputBlendPose BoneFilter;

	UPROPERTY(EditAnywhere, Category = "Node")
	FLbbBlendWeight Weight;
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbGraphNodeData_SavePose : public FLbbGraphNodeDataBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Node")
	FName CachePoseName = NAME_None;
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbGraphNodeData_MakeAdditive : public FLbbGraphNodeDataBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Node")
	ELbbBoneSpace AdditiveSpace = ELbbBoneSpace::LocalSpace;
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbGraphNodeData_ApplyAdditive : public FLbbGraphNodeDataBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Node")
	ELbbBoneSpace AdditiveSpace = ELbbBoneSpace::LocalSpace;

	UPROPERTY(EditAnywhere, Category = "Node")
	FLbbBlendWeight Weight;
};
