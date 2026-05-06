// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "LbbLayeredBlendBodyTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "LbbLayeredBlendBodyEditorModel.generated.h"

UENUM()
enum class ELbbLayeredBlendBodyGraphNodeType : uint8
{
	CurrentPose,
	Motion,
	BasePose,
	OverlayPose,
	Result,
	ApplySlot,
	Blend,
	MaskedBlend,
	ApplyMotionDelta,
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyGraphNodeDataBase
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
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyGraphNodeData_ApplyMotionDelta : public FLbbLayeredBlendBodyGraphNodeDataBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Node")
	ELbbBoneSpace AdditiveSpace = ELbbBoneSpace::LocalSpace;

	UPROPERTY(EditAnywhere, Category = "Node")
	FLbbBlendWeight Weight;
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyGraphNodeModel
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid NodeGuid;

	UPROPERTY()
	ELbbLayeredBlendBodyGraphNodeType NodeType = ELbbLayeredBlendBodyGraphNodeType::CurrentPose;

	UPROPERTY()
	FVector2D NodePosition = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/LayeredBlendPerBody.LbbLayeredBlendBodyGraphNodeDataBase", ExcludeBaseStruct))
	FInstancedStruct NodeData;
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyGraphLinkModel
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid FromNodeGuid;

	UPROPERTY()
	FName FromPinName = NAME_None;

	UPROPERTY()
	FGuid ToNodeGuid;

	UPROPERTY()
	FName ToPinName = NAME_None;
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyPartGraphModel
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid GraphGuid;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FName PartName = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FLinearColor DebugColor = FLinearColor::Green;

	UPROPERTY()
	TArray<FLbbLayeredBlendBodyGraphNodeModel> Nodes;

	UPROPERTY()
	TArray<FLbbLayeredBlendBodyGraphLinkModel> Links;

	UPROPERTY()
	FVector2D SavedViewLocation = FVector2D::ZeroVector;

	UPROPERTY()
	float SavedZoomAmount = 1.f;
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyEditorModel
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	TArray<FLbbLayeredBlendBodyPartGraphModel> BodyPartGraphs;
};
