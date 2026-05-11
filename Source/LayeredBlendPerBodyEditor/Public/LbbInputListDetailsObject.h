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
	UPROPERTY(EditAnywhere, Category = "Inputs", meta = (TitleProperty = "InputName"))
	TArray<FLbbInputPoseDefinition> InputDefinitions;

	void NormalizeInputDefinitions(bool bNotifyIfBasePoseWasRestored = false);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

private:
	bool bIsNormalizingInputDefinitions = false;
};
