// Fill out your copyright notice in the Description page of Project Settings.

#include "LbbLayeredBlendBodyGraphSchema.h"

#include "LbbLayeredBlendBodyEdGraph.h"
#include "GraphNodes/LbbLayeredBlendBodyEdGraphNode.h"
#include "LbbLayeredBlendBodyGraphNodeRegistry.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "LbbLayeredBlendBodyGraphSchema"

const FName ULbbLayeredBlendBodyGraphSchema::PC_Pose(TEXT("Pose"));

namespace
{
	static bool CanReachNodeByFollowingOutputs(
		const UEdGraphNode* const StartNode,
		const UEdGraphNode* const TargetNode)
	{
		if (StartNode == nullptr || TargetNode == nullptr)
		{
			return false;
		}

		TArray<const UEdGraphNode*, TInlineAllocator<16>> NodesToVisit;
		TSet<const UEdGraphNode*> QueuedNodes;
		NodesToVisit.Add(StartNode);
		QueuedNodes.Add(StartNode);

		while (!NodesToVisit.IsEmpty())
		{
			const UEdGraphNode* const CurrentNode = NodesToVisit.Pop(EAllowShrinking::No);
			if (CurrentNode == TargetNode)
			{
				return true;
			}

			for (const UEdGraphPin* Pin : CurrentNode->Pins)
			{
				if (Pin == nullptr || Pin->Direction != EGPD_Output)
				{
					continue;
				}

				for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (LinkedPin == nullptr)
					{
						continue;
					}

					if (const UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode())
					{
						if (!QueuedNodes.Contains(LinkedNode))
						{
							QueuedNodes.Add(LinkedNode);
							NodesToVisit.Add(LinkedNode);
						}
					}
				}
			}
		}

		return false;
	}

	static bool WouldCreateCycle(const UEdGraphPin* OutputPin, const UEdGraphPin* InputPin)
	{
		const UEdGraphNode* const SourceNode = OutputPin != nullptr ? OutputPin->GetOwningNode() : nullptr;
		const UEdGraphNode* const TargetNode = InputPin != nullptr ? InputPin->GetOwningNode() : nullptr;
		if (SourceNode == nullptr || TargetNode == nullptr)
		{
			return false;
		}

		return CanReachNodeByFollowingOutputs(TargetNode, SourceNode);
	}

	struct FGraphSchemaAction_NewNode : public FEdGraphSchemaAction
	{
	public:
		FGraphSchemaAction_NewNode(const FText& InNodeCategory, const FText& InMenuDesc, UClass* InNodeClass)
			: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, FText::GetEmpty(), 0)
			, NodeClass(InNodeClass)
		{
		}

		virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2f& Location, bool bSelectNewNode = true) override
		{
			if (ParentGraph == nullptr || NodeClass == nullptr)
			{
				return nullptr;
			}

			const FScopedTransaction Transaction(LOCTEXT("AddGraphNode", "Add Layered Blend Body Graph Node"));
			ParentGraph->Modify();

			ULbbLayeredBlendBodyEdGraphNode* NewNode = NewObject<ULbbLayeredBlendBodyEdGraphNode>(ParentGraph, NodeClass, NAME_None, RF_Transactional);
			ParentGraph->AddNode(NewNode, true, bSelectNewNode);
			NewNode->CreateNewGuid();
			NewNode->NodePosX = FMath::RoundToInt(Location.X);
			NewNode->NodePosY = FMath::RoundToInt(Location.Y);
			NewNode->SnapToGrid(16);
			NewNode->PostPlacedNewNode();
			NewNode->AllocateDefaultPins();
			NewNode->AutowireNewNode(FromPin);

			return NewNode;
		}

	private:
		UClass* NodeClass = nullptr;
	};
}

void ULbbLayeredBlendBodyGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	const ULbbLayeredBlendBodyEdGraph* Graph = Cast<ULbbLayeredBlendBodyEdGraph>(ContextMenuBuilder.CurrentGraph);
	const ELbbLayeredBlendBodyGraphKind GraphKind = Graph != nullptr
		? Graph->GraphKind
		: ELbbLayeredBlendBodyGraphKind::BodyPart;

	for (const FLbbLayeredBlendBodyGraphNodeDescriptor& Descriptor : GetLbbLayeredBlendBodyGraphNodeDescriptors())
	{
		if (!Descriptor.bShowInNodeMenu || Descriptor.NodeClass == nullptr || !Descriptor.IsAllowedInGraph(GraphKind))
		{
			continue;
		}

		ContextMenuBuilder.AddAction(MakeShared<FGraphSchemaAction_NewNode>(
			LOCTEXT("NodeCategory", "Nodes"),
			FText::FromString(Descriptor.DisplayName),
			Descriptor.NodeClass));
	}
}

const FPinConnectionResponse ULbbLayeredBlendBodyGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	if (A == nullptr || B == nullptr)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("InvalidPin", "Invalid pin."));
	}

	if (A == B)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("SamePin", "Cannot connect a pin to itself."));
	}

	if (A->GetOwningNode() == B->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("SameNode", "Cannot connect pins on the same node."));
	}

	if (A->Direction == B->Direction)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("SameDirection", "Pins must have opposite directions."));
	}

	if (A->PinType.PinCategory != PC_Pose || B->PinType.PinCategory != PC_Pose)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("CategoryMismatch", "Only pose pins can be connected."));
	}

	const UEdGraphPin* InputPin = (A->Direction == EGPD_Input) ? A : B;
	const UEdGraphPin* OutputPin = (A->Direction == EGPD_Output) ? A : B;
	if (WouldCreateCycle(OutputPin, InputPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("CycleConnection", "This connection would create a cycle."));
	}

	if (InputPin->LinkedTo.Num() > 0)
	{
		return (InputPin == A)
			? FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, LOCTEXT("ReplaceInputA", "Replace the existing input connection."))
			: FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_B, LOCTEXT("ReplaceInputB", "Replace the existing input connection."));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, LOCTEXT("ConnectPose", "Connect pose pins."));
}

bool ULbbLayeredBlendBodyGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
	return UEdGraphSchema::TryCreateConnection(A, B);
}

FLinearColor ULbbLayeredBlendBodyGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return (PinType.PinCategory == PC_Pose)
		? FLinearColor(0.94f, 0.72f, 0.22f)
		: FLinearColor::White;
}

void ULbbLayeredBlendBodyGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	Super::BreakNodeLinks(TargetNode);
}

void ULbbLayeredBlendBodyGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	Super::BreakPinLinks(TargetPin, bSendsNodeNotifcation);
}

#undef LOCTEXT_NAMESPACE
