// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimGraph/LbbAnimGraphNode_LayeredBodyPartBlend.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "LbbAnimation"

ULbbAnimGraphNode_LayeredBodyPartBlend::ULbbAnimGraphNode_LayeredBodyPartBlend(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
}

FText ULbbAnimGraphNode_LayeredBodyPartBlend::GetTooltipText() const
{
	return LOCTEXT("LbbAnimGraphNode_LayeredBodyPartBlend_Tooltip", "Layered blend per body part");
}

FText ULbbAnimGraphNode_LayeredBodyPartBlend::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("LbbAnimGraphNode_LayeredBodyPartBlend_Title", "Layered blend per body part");
}

FText ULbbAnimGraphNode_LayeredBodyPartBlend::GetMenuCategory() const
{
	return LOCTEXT("NodeCategory", "Lbb Animation");
}

FLinearColor ULbbAnimGraphNode_LayeredBodyPartBlend::GetNodeTitleColor() const
{
	return FLinearColor(0.2f, 0.8f, 0.2f);
}

void ULbbAnimGraphNode_LayeredBodyPartBlend::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const
{
	Super::CustomizePinData(Pin, SourcePropertyName, ArrayIndex);

	if (ArrayIndex == INDEX_NONE || SourcePropertyName != GET_MEMBER_NAME_CHECKED(FLbbAnimNode_LayeredBodyPartBlend, InputPoses))
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

void ULbbAnimGraphNode_LayeredBodyPartBlend::PreloadRequiredAssets()
{
	// if (Node.BodyDefinition)
	// {
	// 	PreloadObject(Node.BodyDefinition);
	// }
	
	Super::PreloadRequiredAssets();
}

void ULbbAnimGraphNode_LayeredBodyPartBlend::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None);

	if (PropertyName == TEXT("BodyDefinition"))
	{
		Node.ReinitRuntimeData();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FLbbLayeredBlendBodyInputPoseAlias, PoseAlias)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FLbbAnimNode_LayeredBodyPartBlend, InputPoseAliases))
	{
		ReconstructNode();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ULbbAnimGraphNode_LayeredBodyPartBlend::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
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

		if (LbbLayeredBlendBody::IsBuiltInInputName(PoseAlias))
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

	if (const ULbbLayeredBlendBodyDefinition* BodyDefinition = Node.GetBodyDefinition())
	{
		for (const FLbbLayeredBlendBodyInputDefinition& InputDefinition : BodyDefinition->InputDefinitions)
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

void ULbbAnimGraphNode_LayeredBodyPartBlend::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	if (Context->bIsDebugging)
	{
		return;
	}

	ULbbAnimGraphNode_LayeredBodyPartBlend* MutableThis = const_cast<ULbbAnimGraphNode_LayeredBodyPartBlend*>(this);
	FToolMenuSection& Section = Menu->AddSection("LbbLayeredBodyPartBlend", LOCTEXT("LayeredBodyPartBlendSection", "Layered Blend Body"));
	if (Context->Pin != nullptr)
	{
		FProperty* AssociatedProperty = nullptr;
		int32 ArrayIndex = INDEX_NONE;
		MutableThis->GetPinAssociatedProperty(MutableThis->GetFNodeType(), Context->Pin, AssociatedProperty, ArrayIndex);
		if (Context->Pin->Direction == EGPD_Input
			&& ArrayIndex != INDEX_NONE
			&& AssociatedProperty != nullptr
			&& AssociatedProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FLbbAnimNode_LayeredBodyPartBlend, InputPoses))
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

void ULbbAnimGraphNode_LayeredBodyPartBlend::AddInputPosePin()
{
	FScopedTransaction Transaction(LOCTEXT("AddInputPosePin", "Add Input Pose Pin"));
	Modify();

	Node.AddInputPose();
	ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
}

void ULbbAnimGraphNode_LayeredBodyPartBlend::RemoveInputPosePin(UEdGraphPin* Pin)
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

void ULbbAnimGraphNode_LayeredBodyPartBlend::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	Super::ReallocatePinsDuringReconstruction(OldPins);

	if (RemovedPinArrayIndex != INDEX_NONE)
	{
		RemovePinsFromOldPins(OldPins, RemovedPinArrayIndex);
		RemovedPinArrayIndex = INDEX_NONE;
	}
}

void ULbbAnimGraphNode_LayeredBodyPartBlend::RemovePinsFromOldPins(TArray<UEdGraphPin*>& OldPins, const int32 RemovedArrayIndex)
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
