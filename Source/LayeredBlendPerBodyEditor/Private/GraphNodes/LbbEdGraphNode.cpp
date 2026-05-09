// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbEdGraphNode.h"

#include "LbbBodyPartDefinitionDataAsset.h"
#include "LbbEdGraph.h"
#include "GraphNodes/LbbEdGraphNodeUtils.h"
#include "LbbGraphSchema.h"

const FName ULbbEdGraphNode::PosePinCategory(TEXT("Pose"));

namespace LbbEdGraphNodeUtils
{
	const FName PinPose(TEXT("Pose"));
	const FName PinBasePose(TEXT("BasePose"));
	const FName PinInput(TEXT("Input"));
	const FName PinBase(TEXT("Base"));
	const FName PinBlend(TEXT("Blend"));
	const FName PinAdditive(TEXT("Additive"));
	const FName PinResult(TEXT("Result"));

	const FName PoseInputPins[] = {PinPose};
	const FName PoseOutputPins[] = {PinPose};
	const FName InputPins[] = {PinInput};
	const FName BaseBlendInputPins[] = {PinBase, PinBlend};
	const FName BaseAdditiveInputPins[] = {PinBase, PinAdditive};
	const FName ResultOutputPins[] = {PinResult};

	FString FormatBlendWeightSummary(const FLbbBlendWeight& Weight)
	{
		if (Weight.bUseBlendCurve)
		{
			const FString CurveName = Weight.BlendCurveName.IsNone()
				? TEXT("<None>")
				: Weight.BlendCurveName.ToString();
			return FString::Printf(TEXT("Curve(%s)"), *CurveName);
		}

		return FString::Printf(TEXT("%.2f"), Weight.BlendWeight);
	}

	FString FormatBoneSpaceSummary(const ELbbBoneSpace BoneSpace)
	{
		return BoneSpace == ELbbBoneSpace::MeshSpace
			? TEXT("Mesh Space")
			: TEXT("Local Space");
	}

	FString FormatPoseSourceSummary(const FLbbLayeredBodyPartPoseSource& SourcePose)
	{
		switch (SourcePose.Type)
		{
		case ELbbLayeredBodyPartPoseSourceType::Motion:
			return TEXT("Motion");
		case ELbbLayeredBodyPartPoseSourceType::BasePose:
			return TEXT("Base Pose");
		case ELbbLayeredBodyPartPoseSourceType::OverlayPose:
			return TEXT("Overlay Pose");
		case ELbbLayeredBodyPartPoseSourceType::CurrentPose:
			return TEXT("Current Pose");
		case ELbbLayeredBodyPartPoseSourceType::CachePose:
			return FString::Printf(
				TEXT("Cache(%s)"),
				SourcePose.CachePoseName.IsNone() ? TEXT("<None>") : *SourcePose.CachePoseName.ToString());
		case ELbbLayeredBodyPartPoseSourceType::InputPose:
			return FString::Printf(
				TEXT("Input(%s)"),
				SourcePose.InputPoseName.IsNone() ? TEXT("<None>") : *SourcePose.InputPoseName.ToString());
		default:
			return TEXT("Unknown");
		}
	}

	FString FormatInputNameSummary(const FName InputName)
	{
		return InputName.IsNone() ? TEXT("<None>") : InputName.ToString();
	}

	FLbbGraphNodeDescriptor MakeNodeDescriptor(
		const UScriptStruct* NodeDataStruct,
		UClass* NodeClass,
		const TCHAR* DisplayName,
		const TConstArrayView<FName> InputPinNames,
		const TConstArrayView<FName> OutputPinNames,
		const bool bIsInputNode,
		const bool bIsResultNode,
		const bool bIsSavePoseNode,
		const bool bShowInNodeMenu,
		const bool bIsSingleton,
		const ELbbGraphKindFlags SingletonGraphKinds,
		const ELbbGraphKindFlags AllowedGraphKinds,
		const FLbbCompileGraphNodeFunction CompileNode)
	{
		FLbbGraphNodeDescriptor Descriptor;
		Descriptor.NodeDataStruct = NodeDataStruct;
		Descriptor.NodeClass = NodeClass;
		Descriptor.DisplayName = DisplayName;
		Descriptor.InputPins = InputPinNames.Num() > 0 ? InputPinNames.GetData() : nullptr;
		Descriptor.NumInputPins = InputPinNames.Num();
		Descriptor.OutputPins = OutputPinNames.Num() > 0 ? OutputPinNames.GetData() : nullptr;
		Descriptor.NumOutputPins = OutputPinNames.Num();
		Descriptor.bIsInputNode = bIsInputNode;
		Descriptor.bIsResultNode = bIsResultNode;
		Descriptor.bIsSavePoseNode = bIsSavePoseNode;
		Descriptor.bShowInNodeMenu = bShowInNodeMenu;
		Descriptor.bIsSingleton = bIsSingleton;
		Descriptor.SingletonGraphKinds = SingletonGraphKinds;
		Descriptor.AllowedGraphKinds = AllowedGraphKinds;
		Descriptor.CompileNode = CompileNode;
		return Descriptor;
	}

	void AddMessage(
		TArray<FLbbCompileMessage>& OutMessages,
		const EMessageSeverity::Type Severity,
		const ELbbGraphKind GraphKind,
		const int32 BodyPartIndex,
		const FGuid& NodeGuid,
		const FString& Message)
	{
		FLbbCompileMessage& NewMessage = OutMessages.AddDefaulted_GetRef();
		NewMessage.Severity = Severity;
		NewMessage.GraphKind = GraphKind;
		NewMessage.BodyPartIndex = BodyPartIndex;
		NewMessage.NodeGuid = NodeGuid;
		NewMessage.Message = Message;
	}

	bool ResolveRequiredInputSource(
		const FLbbGraphNodeCompileContext& CompileContext,
		const FLbbGraphNodeModel& NodeModel,
		const FName PinName,
		FLbbLayeredBodyPartPoseSource& OutSourcePose,
		TArray<FLbbCompileMessage>& OutMessages,
		const TCHAR* FailureMessage)
	{
		const FLbbGraphLinkModel* InputLink = CompileContext.FindInputLink(NodeModel.NodeGuid, PinName);
		if (InputLink == nullptr || !CompileContext.ResolveSource(InputLink->FromNodeGuid, OutSourcePose))
		{
			AddMessage(
				OutMessages,
				EMessageSeverity::Error,
				CompileContext.GetGraphKind(),
				CompileContext.GetBodyPartIndex(),
				NodeModel.NodeGuid,
				FailureMessage);
			return false;
		}

		return true;
	}

	bool ValidateCurveWeight(
		const FLbbBlendWeight& Weight,
		const FLbbGraphNodeCompileContext& CompileContext,
		const FLbbGraphNodeModel& NodeModel,
		const TCHAR* FailureMessage,
		TArray<FLbbCompileMessage>& OutMessages)
	{
		if (Weight.bUseBlendCurve && Weight.BlendCurveName.IsNone())
		{
			AddMessage(
				OutMessages,
				EMessageSeverity::Error,
				CompileContext.GetGraphKind(),
				CompileContext.GetBodyPartIndex(),
				NodeModel.NodeGuid,
				FailureMessage);
			return false;
		}

		return true;
	}
}

const ULbbEdGraph* ULbbEdGraphNode::GetOwningLayeredBlendGraph() const
{
	return Cast<ULbbEdGraph>(GetGraph());
}

FLbbGraphNodeDescriptor ULbbEdGraphNode::BuildGraphNodeDescriptor() const
{
	return FLbbGraphNodeDescriptor();
}

const ULbbBodyPartDefinitionDataAsset* ULbbEdGraphNode::GetOwningDefinition() const
{
	const ULbbEdGraph* OwningGraph = GetOwningLayeredBlendGraph();
	return OwningGraph != nullptr ? OwningGraph->EditingDefinition.Get() : nullptr;
}

void ULbbEdGraphNode::AllocateDefaultPins()
{
	AllocatePosePins();
}

void ULbbEdGraphNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	if (FromPin == nullptr)
	{
		return;
	}

	const UEdGraphSchema* Schema = GetSchema();
	if (Schema == nullptr)
	{
		return;
	}

	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin == nullptr || Pin->Direction == FromPin->Direction)
		{
			continue;
		}

		if (Schema->TryCreateConnection(FromPin, Pin))
		{
			NodeConnectionListChanged();
			return;
		}
	}
}

FText ULbbEdGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetNodeTitleText();
}

FText ULbbEdGraphNode::GetTooltipText() const
{
	return GetNodeTitleText();
}

FLinearColor ULbbEdGraphNode::GetNodeTitleColor() const
{
	return GetNodeColor();
}

bool ULbbEdGraphNode::CanUserDeleteNode() const
{
	return !IsFixedNode();
}

bool ULbbEdGraphNode::CanDuplicateNode() const
{
	return !IsFixedNode();
}

void ULbbEdGraphNode::ImportFromNodeModel(const FLbbGraphNodeModel& NodeModel)
{
	NodeGuid = NodeModel.NodeGuid;
	NodePosX = FMath::RoundToInt(NodeModel.NodePosition.X);
	NodePosY = FMath::RoundToInt(NodeModel.NodePosition.Y);
	ImportNodeData(NodeModel.NodeData);
}

void ULbbEdGraphNode::ExportToNodeModel(FLbbGraphNodeModel& OutNodeModel) const
{
	OutNodeModel.NodeGuid = NodeGuid;
	OutNodeModel.NodePosition = FVector2D(NodePosX, NodePosY);
	ExportNodeData(OutNodeModel.NodeData);
}

FLinearColor ULbbEdGraphNode::GetNodeColor() const
{
	return FLinearColor(0.18f, 0.18f, 0.18f);
}

void ULbbEdGraphNode::ImportNodeData(const FInstancedStruct& NodeData)
{
}

void ULbbEdGraphNode::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	if (const UScriptStruct* NodeDataStruct = GetNodeDataStruct())
	{
		OutNodeData.InitializeAs(NodeDataStruct);
		return;
	}

	OutNodeData.Reset();
}

UEdGraphPin* ULbbEdGraphNode::CreatePoseInputPin(const FName& PinName) const
{
	return const_cast<ULbbEdGraphNode*>(this)->CreatePin(EGPD_Input, ULbbGraphSchema::PC_Pose, PinName);
}

UEdGraphPin* ULbbEdGraphNode::CreatePoseOutputPin(const FName& PinName) const
{
	return const_cast<ULbbEdGraphNode*>(this)->CreatePin(EGPD_Output, ULbbGraphSchema::PC_Pose, PinName);
}
