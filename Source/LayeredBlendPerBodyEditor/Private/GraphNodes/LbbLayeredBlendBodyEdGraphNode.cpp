// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbLayeredBlendBodyEdGraphNode.h"

#include "LbbLayeredBlendBodyDefinition.h"
#include "LbbLayeredBlendBodyEdGraph.h"
#include "GraphNodes/LbbLayeredBlendBodyEdGraphNodeUtils.h"
#include "LbbLayeredBlendBodyGraphSchema.h"

const FName ULbbLayeredBlendBodyEdGraphNode::PosePinCategory(TEXT("Pose"));

namespace LbbLayeredBlendBodyEdGraphNodeUtils
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

	FLbbLayeredBlendBodyGraphNodeDescriptor MakeNodeDescriptor(
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
		const ELbbLayeredBlendBodyGraphKindFlags SingletonGraphKinds,
		const ELbbLayeredBlendBodyGraphKindFlags AllowedGraphKinds,
		const FLbbCompileGraphNodeFunction CompileNode)
	{
		FLbbLayeredBlendBodyGraphNodeDescriptor Descriptor;
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
		const ELbbLayeredBlendBodyGraphKind GraphKind,
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
		const FLbbLayeredBlendBodyGraphNodeCompileContext& CompileContext,
		const FLbbLayeredBlendBodyGraphNodeModel& NodeModel,
		const FName PinName,
		FLbbLayeredBodyPartPoseSource& OutSourcePose,
		TArray<FLbbCompileMessage>& OutMessages,
		const TCHAR* FailureMessage)
	{
		const FLbbLayeredBlendBodyGraphLinkModel* InputLink = CompileContext.FindInputLink(NodeModel.NodeGuid, PinName);
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
		const FLbbLayeredBlendBodyGraphNodeCompileContext& CompileContext,
		const FLbbLayeredBlendBodyGraphNodeModel& NodeModel,
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

const ULbbLayeredBlendBodyEdGraph* ULbbLayeredBlendBodyEdGraphNode::GetOwningLayeredBlendGraph() const
{
	return Cast<ULbbLayeredBlendBodyEdGraph>(GetGraph());
}

FLbbLayeredBlendBodyGraphNodeDescriptor ULbbLayeredBlendBodyEdGraphNode::BuildGraphNodeDescriptor() const
{
	return FLbbLayeredBlendBodyGraphNodeDescriptor();
}

const ULbbLayeredBlendBodyDefinition* ULbbLayeredBlendBodyEdGraphNode::GetOwningDefinition() const
{
	const ULbbLayeredBlendBodyEdGraph* OwningGraph = GetOwningLayeredBlendGraph();
	return OwningGraph != nullptr ? OwningGraph->EditingDefinition.Get() : nullptr;
}

void ULbbLayeredBlendBodyEdGraphNode::AllocateDefaultPins()
{
	AllocatePosePins();
}

void ULbbLayeredBlendBodyEdGraphNode::AutowireNewNode(UEdGraphPin* FromPin)
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

FText ULbbLayeredBlendBodyEdGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetNodeTitleText();
}

FText ULbbLayeredBlendBodyEdGraphNode::GetTooltipText() const
{
	return GetNodeTitleText();
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode::GetNodeTitleColor() const
{
	return GetNodeColor();
}

bool ULbbLayeredBlendBodyEdGraphNode::CanUserDeleteNode() const
{
	return !IsFixedNode();
}

bool ULbbLayeredBlendBodyEdGraphNode::CanDuplicateNode() const
{
	return !IsFixedNode();
}

void ULbbLayeredBlendBodyEdGraphNode::ImportFromNodeModel(const FLbbLayeredBlendBodyGraphNodeModel& NodeModel)
{
	NodeGuid = NodeModel.NodeGuid;
	NodePosX = FMath::RoundToInt(NodeModel.NodePosition.X);
	NodePosY = FMath::RoundToInt(NodeModel.NodePosition.Y);
	ImportNodeData(NodeModel.NodeData);
}

void ULbbLayeredBlendBodyEdGraphNode::ExportToNodeModel(FLbbLayeredBlendBodyGraphNodeModel& OutNodeModel) const
{
	OutNodeModel.NodeGuid = NodeGuid;
	OutNodeModel.NodePosition = FVector2D(NodePosX, NodePosY);
	ExportNodeData(OutNodeModel.NodeData);
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode::GetNodeColor() const
{
	return FLinearColor(0.18f, 0.18f, 0.18f);
}

void ULbbLayeredBlendBodyEdGraphNode::ImportNodeData(const FInstancedStruct& NodeData)
{
}

void ULbbLayeredBlendBodyEdGraphNode::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	if (const UScriptStruct* NodeDataStruct = GetNodeDataStruct())
	{
		OutNodeData.InitializeAs(NodeDataStruct);
		return;
	}

	OutNodeData.Reset();
}

UEdGraphPin* ULbbLayeredBlendBodyEdGraphNode::CreatePoseInputPin(const FName& PinName) const
{
	return const_cast<ULbbLayeredBlendBodyEdGraphNode*>(this)->CreatePin(EGPD_Input, ULbbLayeredBlendBodyGraphSchema::PC_Pose, PinName);
}

UEdGraphPin* ULbbLayeredBlendBodyEdGraphNode::CreatePoseOutputPin(const FName& PinName) const
{
	return const_cast<ULbbLayeredBlendBodyEdGraphNode*>(this)->CreatePin(EGPD_Output, ULbbLayeredBlendBodyGraphSchema::PC_Pose, PinName);
}
