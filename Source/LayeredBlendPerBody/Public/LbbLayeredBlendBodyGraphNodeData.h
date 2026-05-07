// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "LbbLayeredBlendBodyTypes.h"
#include "LbbLayeredBlendBodyGraphNodeData.generated.h"

UENUM()
enum class ELbbLayeredBlendBodyGraphKind : uint8
{
	Cache,
	BodyPart,
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyGraphNodeDataBase
{
	GENERATED_BODY()
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyGraphNodeData_Input : public FLbbLayeredBlendBodyGraphNodeDataBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Node")
	FLbbLayeredBodyPartPoseSource SourcePose;
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyGraphNodeData_Result : public FLbbLayeredBlendBodyGraphNodeDataBase
{
	GENERATED_BODY()
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyGraphNodeData_ApplySlot : public FLbbLayeredBlendBodyGraphNodeDataBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Node")
	FName SlotName = NAME_None;
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyGraphNodeData_Blend : public FLbbLayeredBlendBodyGraphNodeDataBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Node")
	FLbbBlendWeight Weight;
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyGraphNodeData_MaskedBlend : public FLbbLayeredBlendBodyGraphNodeDataBase
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
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyGraphNodeData_SavePose : public FLbbLayeredBlendBodyGraphNodeDataBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Node")
	FName CachePoseName = NAME_None;
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyGraphNodeData_MakeAdditive : public FLbbLayeredBlendBodyGraphNodeDataBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Node")
	ELbbBoneSpace AdditiveSpace = ELbbBoneSpace::LocalSpace;
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyGraphNodeData_ApplyAdditive : public FLbbLayeredBlendBodyGraphNodeDataBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Node")
	ELbbBoneSpace AdditiveSpace = ELbbBoneSpace::LocalSpace;

	UPROPERTY(EditAnywhere, Category = "Node")
	FLbbBlendWeight Weight;
};
