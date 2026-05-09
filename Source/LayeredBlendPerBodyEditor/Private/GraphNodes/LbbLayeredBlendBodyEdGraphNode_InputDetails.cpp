// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbLayeredBlendBodyEdGraphNode_InputDetails.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "GraphNodes/LbbLayeredBlendBodyEdGraphNode_Input.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LbbLayeredBlendBodyEdGraphNodeInputDetails"

TSharedRef<IDetailCustomization> FLbbLayeredBlendBodyEdGraphNodeInputDetails::MakeInstance()
{
	return MakeShared<FLbbLayeredBlendBodyEdGraphNodeInputDetails>();
}

void FLbbLayeredBlendBodyEdGraphNodeInputDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	for (const TWeakObjectPtr<UObject>& Object : ObjectsBeingCustomized)
	{
		if (ULbbLayeredBlendBodyEdGraphNode_Input* TypedNode = Cast<ULbbLayeredBlendBodyEdGraphNode_Input>(Object.Get()))
		{
			InputNode = TypedNode;
			break;
		}
	}

	SourceTypeHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(ULbbLayeredBlendBodyEdGraphNode_Input, SourceType),
		ULbbLayeredBlendBodyEdGraphNode_Input::StaticClass());
	if (!SourceTypeHandle.IsValid() || !SourceTypeHandle->IsValidHandle())
	{
		return;
	}

	RefreshSourceTypeOptions();
	DetailBuilder.HideProperty(SourceTypeHandle);

	IDetailCategoryBuilder& NodeCategory = DetailBuilder.EditCategory(TEXT("Node"));
	NodeCategory.AddCustomRow(LOCTEXT("SourceTypeSearchText", "Source Type"))
	.NameContent()
	[
		SourceTypeHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(180.f)
	.MaxDesiredWidth(0.f)
	[
		SAssignNew(SourceTypeComboBox, SComboBox<FSourceTypeOption>)
		.OptionsSource(&SourceTypeOptions)
		.InitiallySelectedItem(FindSelectedSourceTypeOption())
		.OnGenerateWidget(this, &FLbbLayeredBlendBodyEdGraphNodeInputDetails::GenerateSourceTypeOptionWidget)
		.OnSelectionChanged(this, &FLbbLayeredBlendBodyEdGraphNodeInputDetails::HandleSourceTypeChanged)
		[
			SNew(STextBlock)
			.Text(this, &FLbbLayeredBlendBodyEdGraphNodeInputDetails::GetSelectedSourceTypeText)
		]
	];
}

void FLbbLayeredBlendBodyEdGraphNodeInputDetails::RefreshSourceTypeOptions()
{
	SourceTypeOptions.Reset();

	if (!InputNode.IsValid())
	{
		return;
	}

	for (const ELbbLayeredBodyPartPoseSourceType SourceType : InputNode->GetAvailableSourceTypes())
	{
		SourceTypeOptions.Add(MakeShared<ELbbLayeredBodyPartPoseSourceType>(SourceType));
	}

	if (SourceTypeComboBox.IsValid())
	{
		SourceTypeComboBox->RefreshOptions();
	}
}

FLbbLayeredBlendBodyEdGraphNodeInputDetails::FSourceTypeOption FLbbLayeredBlendBodyEdGraphNodeInputDetails::FindSelectedSourceTypeOption() const
{
	if (!SourceTypeHandle.IsValid())
	{
		return nullptr;
	}

	uint8 SourceTypeValue = static_cast<uint8>(ELbbLayeredBodyPartPoseSourceType::BasePose);
	if (SourceTypeHandle->GetValue(SourceTypeValue) != FPropertyAccess::Success)
	{
		return nullptr;
	}

	for (const FSourceTypeOption& Option : SourceTypeOptions)
	{
		if (Option.IsValid() && *Option == static_cast<ELbbLayeredBodyPartPoseSourceType>(SourceTypeValue))
		{
			return Option;
		}
	}

	return nullptr;
}

FText FLbbLayeredBlendBodyEdGraphNodeInputDetails::GetSelectedSourceTypeText() const
{
	if (!SourceTypeHandle.IsValid())
	{
		return FText::GetEmpty();
	}

	uint8 SourceTypeValue = static_cast<uint8>(ELbbLayeredBodyPartPoseSourceType::BasePose);
	if (SourceTypeHandle->GetValue(SourceTypeValue) != FPropertyAccess::Success)
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}

	if (const UEnum* SourceTypeEnum = StaticEnum<ELbbLayeredBodyPartPoseSourceType>())
	{
		return SourceTypeEnum->GetDisplayNameTextByValue(SourceTypeValue);
	}

	return FText::AsNumber(SourceTypeValue);
}

void FLbbLayeredBlendBodyEdGraphNodeInputDetails::HandleSourceTypeChanged(
	FSourceTypeOption NewSelection,
	ESelectInfo::Type SelectInfo)
{
	if (!SourceTypeHandle.IsValid() || !NewSelection.IsValid())
	{
		return;
	}

	SourceTypeHandle->SetValue(static_cast<uint8>(*NewSelection));
}

TSharedRef<SWidget> FLbbLayeredBlendBodyEdGraphNodeInputDetails::GenerateSourceTypeOptionWidget(FSourceTypeOption InOption) const
{
	const UEnum* SourceTypeEnum = StaticEnum<ELbbLayeredBodyPartPoseSourceType>();
	const FText OptionText = (InOption.IsValid() && SourceTypeEnum != nullptr)
		? SourceTypeEnum->GetDisplayNameTextByValue(static_cast<int64>(*InOption))
		: FText::GetEmpty();

	return SNew(STextBlock)
		.Text(OptionText);
}

#undef LOCTEXT_NAMESPACE
