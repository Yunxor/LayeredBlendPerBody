// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LbbLayeredBlendBodyTypes.h"
#include "UObject/Object.h"
#include "LbbLayeredBlendBodyInputListDetailsObject.generated.h"

UCLASS(Transient)
class LAYEREDBLENDPERBODYEDITOR_API ULbbLayeredBlendBodyInputListDetailsObject : public UObject
{
	GENERATED_BODY()

public:
	ULbbLayeredBlendBodyInputListDetailsObject()
	{
		BuiltInInputNames.Add(LbbLayeredBlendBody::GetBasePoseInputName());
	}

	UPROPERTY(VisibleAnywhere, Category = "Inputs", meta = (DisplayName = "Built-in Inputs"))
	TArray<FName> BuiltInInputNames;

	UPROPERTY(EditAnywhere, Category = "Inputs", meta = (TitleProperty = "InputName", DisplayName = "Custom Inputs"))
	TArray<FLbbLayeredBlendBodyInputDefinition> CustomInputDefinitions;
};
