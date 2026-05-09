// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "LbbEditorModel.h"
#include "LbbEdGraph.generated.h"

class ULbbBodyPartDefinitionDataAsset;

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbEdGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TObjectPtr<ULbbBodyPartDefinitionDataAsset> EditingDefinition = nullptr;

	UPROPERTY(Transient)
	ELbbGraphKind GraphKind = ELbbGraphKind::BodyPart;

	UPROPERTY(Transient)
	int32 BodyPartIndex = INDEX_NONE;
};
