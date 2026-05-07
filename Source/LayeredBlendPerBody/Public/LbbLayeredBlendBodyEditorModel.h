// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LbbLayeredBlendBodyGraphNodeData.h"
#include "StructUtils/InstancedStruct.h"
#include "LbbLayeredBlendBodyEditorModel.generated.h"

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyGraphNodeModel
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid NodeGuid;

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
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyGraphModelBase
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid GraphGuid;

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
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyCacheGraphModel : public FLbbLayeredBlendBodyGraphModelBase
{
	GENERATED_BODY()
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyPartGraphModel : public FLbbLayeredBlendBodyGraphModelBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Settings")
	FName PartName = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FLinearColor DebugColor = FLinearColor::Green;
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyEditorModel
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Version = 3;

	UPROPERTY()
	FLbbLayeredBlendBodyCacheGraphModel CacheGraph;

	UPROPERTY()
	TArray<FLbbLayeredBlendBodyPartGraphModel> BodyPartGraphs;
};
