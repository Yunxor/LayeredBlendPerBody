// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LbbBodyPartDetailsObject.generated.h"

UCLASS(Transient)
class LAYEREDBLENDPERBODYEDITOR_API ULbbBodyPartDetailsObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Body Part")
	FName PartName = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Body Part")
	FLinearColor DebugColor = FLinearColor::Green;
};
