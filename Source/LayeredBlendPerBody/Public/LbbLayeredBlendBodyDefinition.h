// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LbbLayeredBlendBodyEditorModel.h"
#include "Engine/DataAsset.h"
#include "StructUtils/InstancedStruct.h"
#include "LbbLayeredBlendBodyDefinition.generated.h"


USTRUCT(BlueprintType)
struct FLbbLayeredBlendBodyPart
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Settings")
	FName PartName = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Recipe", meta = (BaseStruct = "/Script/LayeredBlendPerBody.LbbLayeredBlendBodyPartOperatorBase", ExcludeBaseStruct))
	TArray<FInstancedStruct> Operators;


#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = Config, meta=(ExcludeFromHash, DisplayPriority = 0))
	FLinearColor DebugColor = FLinearColor::Green;
#endif
	
};

USTRUCT(BlueprintType)
struct LAYEREDBLENDPERBODY_API FLbbLayeredBlendBodyCacheProgram
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Runtime")
	TArray<FName> NamedCacheNames;

	UPROPERTY(EditAnywhere, Category = "Runtime", meta = (BaseStruct = "/Script/LayeredBlendPerBody.LbbLayeredBlendBodyPartOperatorBase", ExcludeBaseStruct))
	TArray<FInstancedStruct> Operators;
};

/**
 * 
 */
UCLASS(BlueprintType, Category = "LbbAnimation|Layered Blend Body", meta = (DisplayName = "Lbb Layered Blend Body Definition", CollapseCategories))
class LAYEREDBLENDPERBODY_API ULbbLayeredBlendBodyDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Inputs", meta = (TitleProperty = "InputName"))
	TArray<FLbbLayeredBlendBodyInputDefinition> InputDefinitions;

	UPROPERTY(VisibleAnywhere, Category ="Runtime")
	FLbbLayeredBlendBodyCacheProgram CacheProgram;

	UPROPERTY(VisibleAnywhere, Category ="Runtime", meta = (TitleProperty = "{PartName}"))
	TArray<FLbbLayeredBlendBodyPart> BodyParts;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FLbbLayeredBlendBodyEditorModel EditorModel;
#endif
};
