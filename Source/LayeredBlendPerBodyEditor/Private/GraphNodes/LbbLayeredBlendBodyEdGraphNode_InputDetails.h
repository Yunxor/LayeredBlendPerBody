// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "IDetailCustomization.h"
#include "LbbLayeredBlendBodyTypes.h"

class IPropertyHandle;
class ULbbLayeredBlendBodyEdGraphNode_Input;
template <typename OptionType>
class SComboBox;

class FLbbLayeredBlendBodyEdGraphNodeInputDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	using FSourceTypeOption = TSharedPtr<ELbbLayeredBodyPartPoseSourceType>;

	void RefreshSourceTypeOptions();
	FSourceTypeOption FindSelectedSourceTypeOption() const;
	FText GetSelectedSourceTypeText() const;
	void HandleSourceTypeChanged(FSourceTypeOption NewSelection, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateSourceTypeOptionWidget(FSourceTypeOption InOption) const;

	TWeakObjectPtr<ULbbLayeredBlendBodyEdGraphNode_Input> InputNode;
	TSharedPtr<IPropertyHandle> SourceTypeHandle;
	TSharedPtr<SComboBox<FSourceTypeOption>> SourceTypeComboBox;
	TArray<FSourceTypeOption> SourceTypeOptions;
};
