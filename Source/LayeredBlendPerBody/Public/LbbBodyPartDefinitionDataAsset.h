// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LbbEditorModel.h"
#include "Engine/DataAsset.h"
#include "StructUtils/InstancedStruct.h"
#include "LbbBodyPartDefinitionDataAsset.generated.h"


USTRUCT(BlueprintType)
struct FLbbPart
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Settings")
	FName PartName = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Recipe", meta = (BaseStruct = "/Script/LayeredBlendPerBody.LbbPartOperatorBase", ExcludeBaseStruct))
	TArray<FInstancedStruct> Operators;


#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = Config, meta=(ExcludeFromHash, DisplayPriority = 0))
	FLinearColor DebugColor = FLinearColor::Green;
#endif
	
};

USTRUCT(BlueprintType)
struct LAYEREDBLENDPERBODY_API FLbbCacheProgram
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Runtime")
	TArray<FName> NamedCacheNames;

	UPROPERTY(EditAnywhere, Category = "Runtime", meta = (BaseStruct = "/Script/LayeredBlendPerBody.LbbPartOperatorBase", ExcludeBaseStruct))
	TArray<FInstancedStruct> Operators;
};

/**
 * 
 */
UCLASS(BlueprintType, Category = "Layered Blend Body", meta = (CollapseCategories))
class LAYEREDBLENDPERBODY_API ULbbBodyPartDefinitionDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Inputs", meta = (TitleProperty = "InputName"))
	TArray<FLbbInputPoseDefinition> InputPoseDefinitions;

	UPROPERTY(VisibleAnywhere, Category ="Runtime")
	FLbbCacheProgram CacheProgram;

	UPROPERTY(VisibleAnywhere, Category ="Runtime", meta = (TitleProperty = "{PartName}"))
	TArray<FLbbPart> BodyParts;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FLbbEditorModel EditorModel;
#endif
};
