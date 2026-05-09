// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimGraph/LbbAnimGraphNode_LayeredBlendBodyPart.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "LayeredBlendBodyPart"

ULbbAnimGraphNode_LayeredBlendBodyPart::ULbbAnimGraphNode_LayeredBlendBodyPart(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
}

FText ULbbAnimGraphNode_LayeredBlendBodyPart::GetTooltipText() const
{
	return LOCTEXT("LbbAnimGraphNode_LayeredBlendBodyPart_Tooltip", "Layered blend per body part");
}

FText ULbbAnimGraphNode_LayeredBlendBodyPart::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("LbbAnimGraphNode_LayeredBlendBodyPart_Title", "Layered blend per body part");
}

FText ULbbAnimGraphNode_LayeredBlendBodyPart::GetMenuCategory() const
{
	return LOCTEXT("NodeCategory", "Lbb Animation");
}

FLinearColor ULbbAnimGraphNode_LayeredBlendBodyPart::GetNodeTitleColor() const
{
	return FLinearColor(0.2f, 0.8f, 0.2f);
}

void ULbbAnimGraphNode_LayeredBlendBodyPart::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const
{
	Super::CustomizePinData(Pin, SourcePropertyName, ArrayIndex);

	if (ArrayIndex == INDEX_NONE || SourcePropertyName != GET_MEMBER_NAME_CHECKED(FLbbAnimNode_LayeredBlendBodyPart, InputPoses))
	{
		return;
	}

	const FName PoseAlias = Node.InputPoseAliases.IsValidIndex(ArrayIndex)
		? Node.InputPoseAliases[ArrayIndex].PoseAlias
		: NAME_None;

	Pin->PinFriendlyName = PoseAlias.IsNone()
		? FText::FromString(FString::Printf(TEXT("Input Pose %d"), ArrayIndex + 1))
		: FText::FromName(PoseAlias);
}

void ULbbAnimGraphNode_LayeredBlendBodyPart::PreloadRequiredAssets()
{
	// if (Node.BodyDefinition)
	// {
	// 	PreloadObject(Node.BodyDefinition);
	// }
	
	Super::PreloadRequiredAssets();
}

void ULbbAnimGraphNode_LayeredBlendBodyPart::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None);

	if (PropertyName == TEXT("BodyDefinition"))
	{
		Node.ReinitRuntimeData();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FLbbInputPoseAlias, PoseAlias)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FLbbAnimNode_LayeredBlendBodyPart, InputPoseAliases))
	{
		ReconstructNode();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ULbbAnimGraphNode_LayeredBlendBodyPart::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);

	if (Node.InputPoses.Num() != Node.InputPoseAliases.Num())
	{
		MessageLog.Error(TEXT("@@ InputPoses and InputPoseAliases length mismatch."), this);
	}

	TSet<FName> SeenAliases;
	for (int32 PoseIndex = 0; PoseIndex < Node.InputPoseAliases.Num(); ++PoseIndex)
	{
		const FName PoseAlias = Node.InputPoseAliases[PoseIndex].PoseAlias;
		if (PoseAlias.IsNone())
		{
			continue;
		}

		if (Lbb::IsBuiltInInputName(PoseAlias))
		{
			MessageLog.Error(TEXT("@@ uses reserved Input Pose alias 'BasePose'."), this);
			continue;
		}

		if (SeenAliases.Contains(PoseAlias))
		{
			MessageLog.Error(*FString::Printf(TEXT("@@ has duplicated Input Pose alias '%s'."), *PoseAlias.ToString()), this);
			continue;
		}

		SeenAliases.Add(PoseAlias);
	}

	if (const ULbbBodyPartDefinitionDataAsset* BodyDefinition = Node.GetBodyDefinition())
	{
		for (const FLbbInputPoseDefinition& InputDefinition : BodyDefinition->InputPoseDefinitions)
		{
			if (!InputDefinition.InputName.IsNone() && !SeenAliases.Contains(InputDefinition.InputName))
			{
				MessageLog.Warning(
					*FString::Printf(TEXT("@@ does not provide an Input Pose alias for definition input '%s'."), *InputDefinition.InputName.ToString()),
					this);
			}
		}
	}
}

void ULbbAnimGraphNode_LayeredBlendBodyPart::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	if (Context->bIsDebugging)
	{
		return;
	}

	ULbbAnimGraphNode_LayeredBlendBodyPart* MutableThis = const_cast<ULbbAnimGraphNode_LayeredBlendBodyPart*>(this);
	FToolMenuSection& Section = Menu->AddSection("LbbLayeredBlendBodyPart", LOCTEXT("LayeredBlendBodyPartSection", "Layered Blend Body"));
	if (Context->Pin != nullptr)
	{
		FProperty* AssociatedProperty = nullptr;
		int32 ArrayIndex = INDEX_NONE;
		MutableThis->GetPinAssociatedProperty(MutableThis->GetFNodeType(), Context->Pin, AssociatedProperty, ArrayIndex);
		if (Context->Pin->Direction == EGPD_Input
			&& ArrayIndex != INDEX_NONE
			&& AssociatedProperty != nullptr
			&& AssociatedProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FLbbAnimNode_LayeredBlendBodyPart, InputPoses))
		{
			Section.AddMenuEntry(
				"RemoveInputPosePin",
				LOCTEXT("RemoveInputPosePinLabel", "Remove Input Pose"),
				LOCTEXT("RemoveInputPosePinTooltip", "Remove this input pose pin."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([MutableThis, Pin = const_cast<UEdGraphPin*>(Context->Pin)]()
				{
					MutableThis->RemoveInputPosePin(Pin);
				})));
		}
	}
	else
	{
		Section.AddMenuEntry(
			"AddInputPosePin",
			LOCTEXT("AddInputPosePinLabel", "Add Input Pose"),
			LOCTEXT("AddInputPosePinTooltip", "Add a new named input pose pin."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([MutableThis]()
			{
				MutableThis->AddInputPosePin();
			})));
	}
}

void ULbbAnimGraphNode_LayeredBlendBodyPart::AddInputPosePin()
{
	FScopedTransaction Transaction(LOCTEXT("AddInputPosePin", "Add Input Pose Pin"));
	Modify();

	Node.AddInputPose();
	ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
}

void ULbbAnimGraphNode_LayeredBlendBodyPart::RemoveInputPosePin(UEdGraphPin* Pin)
{
	FScopedTransaction Transaction(LOCTEXT("RemoveInputPosePin", "Remove Input Pose Pin"));
	Modify();

	FProperty* AssociatedProperty = nullptr;
	int32 ArrayIndex = INDEX_NONE;
	GetPinAssociatedProperty(GetFNodeType(), Pin, AssociatedProperty, ArrayIndex);
	if (ArrayIndex == INDEX_NONE)
	{
		return;
	}

	RemovedPinArrayIndex = ArrayIndex;
	Node.RemoveInputPose(ArrayIndex);
	ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
}

void ULbbAnimGraphNode_LayeredBlendBodyPart::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	Super::ReallocatePinsDuringReconstruction(OldPins);

	if (RemovedPinArrayIndex != INDEX_NONE)
	{
		RemovePinsFromOldPins(OldPins, RemovedPinArrayIndex);
		RemovedPinArrayIndex = INDEX_NONE;
	}
}

void ULbbAnimGraphNode_LayeredBlendBodyPart::RemovePinsFromOldPins(TArray<UEdGraphPin*>& OldPins, const int32 RemovedArrayIndex)
{
	TArray<FName> NewPinNames;
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin != nullptr)
		{
			NewPinNames.Add(Pin->PinName);
		}
	}

	TArray<FString> RemovedPropertyNames;
	for (UEdGraphPin* OldPin : OldPins)
	{
		if (OldPin == nullptr || NewPinNames.Contains(OldPin->PinName))
		{
			continue;
		}

		const FString OldPinNameStr = OldPin->PinName.ToString();
		const int32 UnderscoreIndex = OldPinNameStr.Find(TEXT("_"), ESearchCase::CaseSensitive);
		if (UnderscoreIndex != INDEX_NONE)
		{
			RemovedPropertyNames.Add(OldPinNameStr.Left(UnderscoreIndex));
		}
	}

	for (int32 PinIndex = 0; PinIndex < OldPins.Num(); ++PinIndex)
	{
		UEdGraphPin* OldPin = OldPins[PinIndex];
		if (OldPin == nullptr)
		{
			continue;
		}

		const FString OldPinNameStr = OldPin->PinName.ToString();
		const int32 UnderscoreIndex = OldPinNameStr.Find(TEXT("_"), ESearchCase::CaseSensitive);
		if (UnderscoreIndex == INDEX_NONE)
		{
			continue;
		}

		const FString PropertyName = OldPinNameStr.Left(UnderscoreIndex);
		if (!RemovedPropertyNames.Contains(PropertyName))
		{
			continue;
		}

		const int32 ArrayIndex = FCString::Atoi(*(OldPinNameStr.Mid(UnderscoreIndex + 1)));
		if (ArrayIndex == RemovedArrayIndex)
		{
			OldPin->MarkAsGarbage();
			OldPins.RemoveAt(PinIndex);
			--PinIndex;
		}
		else if (ArrayIndex > RemovedArrayIndex)
		{
			OldPin->PinName = *FString::Printf(TEXT("%s_%d"), *PropertyName, ArrayIndex - 1);
		}
	}
}

#undef LOCTEXT_NAMESPACE
