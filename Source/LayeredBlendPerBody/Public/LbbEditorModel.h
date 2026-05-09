// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LbbGraphNodeData.h"
#include "StructUtils/InstancedStruct.h"
#include "LbbEditorModel.generated.h"

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbGraphNodeModel
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid NodeGuid;

	UPROPERTY()
	FVector2D NodePosition = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/LayeredBlendPerBody.LbbGraphNodeDataBase", ExcludeBaseStruct))
	FInstancedStruct NodeData;
	
	
	inline bool operator==(const FLbbGraphNodeModel& Other) const
	{
		return NodeGuid == Other.NodeGuid
			&& NodePosition.Equals(Other.NodePosition, KINDA_SMALL_NUMBER)
			&& NodeData == Other.NodeData;
	}
	
	inline bool operator!=(const FLbbGraphNodeModel& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbGraphLinkModel
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
	
	
	inline bool operator==(const FLbbGraphLinkModel& Other) const
	{
		return FromNodeGuid == Other.FromNodeGuid
			&& FromPinName == Other.FromPinName
			&& ToNodeGuid == Other.ToNodeGuid
			&& ToPinName == Other.ToPinName;
	}
	
	inline bool operator!=(const FLbbGraphLinkModel& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbGraphModelBase
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid GraphGuid;

	UPROPERTY()
	TArray<FLbbGraphNodeModel> Nodes;

	UPROPERTY()
	TArray<FLbbGraphLinkModel> Links;

	UPROPERTY()
	FVector2D SavedViewLocation = FVector2D::ZeroVector;

	UPROPERTY()
	float SavedZoomAmount = 1.f;
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbCacheGraphModel : public FLbbGraphModelBase
{
	GENERATED_BODY()
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbPartGraphModel : public FLbbGraphModelBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Settings")
	FName PartName = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FLinearColor DebugColor = FLinearColor::Green;
};

USTRUCT()
struct LAYEREDBLENDPERBODY_API FLbbEditorModel
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Version = 3;

	UPROPERTY()
	FLbbCacheGraphModel CacheGraph;

	UPROPERTY()
	TArray<FLbbPartGraphModel> BodyPartGraphs;
};
