// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "LbbLayeredBlendBodyEdGraph.generated.h"

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbLayeredBlendBodyEdGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	int32 BodyPartIndex = INDEX_NONE;
};
