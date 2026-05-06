// Fill out your copyright notice in the Description page of Project Settings.

#include "LbbAssetDefinition_LayeredBlendBodyDefinition.h"

#include "LbbLayeredBlendBodyDefinition.h"
#include "LbbLayeredBlendBodyDefinitionEditorToolkit.h"

#define LOCTEXT_NAMESPACE "LbbAssetDefinition_LayeredBlendBodyDefinition"

FText ULbbAssetDefinition_LayeredBlendBodyDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("LayeredBlendBodyDefinitionDisplayName", "Layered Blend Body Definition");
}

FLinearColor ULbbAssetDefinition_LayeredBlendBodyDefinition::GetAssetColor() const
{
	return FLinearColor(0.17f, 0.68f, 0.30f);
}

TSoftClassPtr<UObject> ULbbAssetDefinition_LayeredBlendBodyDefinition::GetAssetClass() const
{
	return ULbbLayeredBlendBodyDefinition::StaticClass();
}

TConstArrayView<FAssetCategoryPath> ULbbAssetDefinition_LayeredBlendBodyDefinition::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { EAssetCategoryPaths::Animation };
	return Categories;
}

EAssetCommandResult ULbbAssetDefinition_LayeredBlendBodyDefinition::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	const EToolkitMode::Type Mode = OpenArgs.GetToolkitMode();

	for (ULbbLayeredBlendBodyDefinition* Definition : OpenArgs.LoadObjects<ULbbLayeredBlendBodyDefinition>())
	{
		FLbbLayeredBlendBodyDefinitionEditorToolkit::CreateEditor(Mode, OpenArgs.ToolkitHost, Definition);
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
