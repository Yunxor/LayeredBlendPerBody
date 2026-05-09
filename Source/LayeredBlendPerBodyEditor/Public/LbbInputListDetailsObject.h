// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LbbTypes.h"
#include "UObject/Object.h"
#include "LbbInputListDetailsObject.generated.h"

UCLASS(Transient)
class LAYEREDBLENDPERBODYEDITOR_API ULbbInputListDetailsObject : public UObject
{
	GENERATED_BODY()

public:
	ULbbInputListDetailsObject()
	{
		BuiltInInputNames.Add(Lbb::GetBasePoseInputName());
	}

	UPROPERTY(VisibleAnywhere, Category = "Inputs", meta = (DisplayName = "Built-in Inputs"))
	TArray<FName> BuiltInInputNames;

	UPROPERTY(EditAnywhere, Category = "Inputs", meta = (TitleProperty = "InputName", DisplayName = "Custom Inputs"))
	TArray<FLbbInputPoseDefinition> CustomInputDefinitions;
};
