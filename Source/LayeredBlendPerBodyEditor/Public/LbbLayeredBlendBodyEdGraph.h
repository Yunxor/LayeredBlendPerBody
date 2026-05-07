// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "LbbLayeredBlendBodyEditorModel.h"
#include "LbbLayeredBlendBodyEdGraph.generated.h"

class ULbbLayeredBlendBodyDefinition;

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbLayeredBlendBodyEdGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TObjectPtr<ULbbLayeredBlendBodyDefinition> EditingDefinition = nullptr;

	UPROPERTY(Transient)
	ELbbLayeredBlendBodyGraphKind GraphKind = ELbbLayeredBlendBodyGraphKind::BodyPart;

	UPROPERTY(Transient)
	int32 BodyPartIndex = INDEX_NONE;
};
