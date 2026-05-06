// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"
#include "LbbAssetDefinition_LayeredBlendBodyDefinition.generated.h"

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbAssetDefinition_LayeredBlendBodyDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

protected:
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
