// Fill out your copyright notice in the Description page of Project Settings.

#include "LbbAssetDefinition_BodyPartDefDataAsset.h"

#include "LbbBodyPartDefinitionDataAsset.h"
#include "LbbDefinitionEditorToolkit.h"

#define LOCTEXT_NAMESPACE "LbbAssetDefinition_LayeredBlendBodyDefinition"

FText ULbbAssetDefinition_BodyPartDefDataAsset::GetAssetDisplayName() const
{
	return LOCTEXT("LayeredBlendBodyDefinitionDisplayName", "Layered Blend Body Part Definition Asset");
}

FLinearColor ULbbAssetDefinition_BodyPartDefDataAsset::GetAssetColor() const
{
	return FLinearColor(0.17f, 0.68f, 0.30f);
}

TSoftClassPtr<UObject> ULbbAssetDefinition_BodyPartDefDataAsset::GetAssetClass() const
{
	return ULbbBodyPartDefinitionDataAsset::StaticClass();
}

TConstArrayView<FAssetCategoryPath> ULbbAssetDefinition_BodyPartDefDataAsset::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { EAssetCategoryPaths::Animation };
	return Categories;
}

EAssetCommandResult ULbbAssetDefinition_BodyPartDefDataAsset::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	const EToolkitMode::Type Mode = OpenArgs.GetToolkitMode();

	for (ULbbBodyPartDefinitionDataAsset* Definition : OpenArgs.LoadObjects<ULbbBodyPartDefinitionDataAsset>())
	{
		FLbbDefinitionEditorToolkit::CreateEditor(Mode, OpenArgs.ToolkitHost, Definition);
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
