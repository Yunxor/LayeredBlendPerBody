// Fill out your copyright notice in the Description page of Project Settings.

#include "LbbLayeredBlendBodyEdGraphNode.h"

#include "LbbLayeredBlendBodyGraphSchema.h"

#define LOCTEXT_NAMESPACE "LbbLayeredBlendBodyEdGraphNode"

const FName ULbbLayeredBlendBodyEdGraphNode::PosePinCategory(TEXT("Pose"));

namespace
{
	static FString FormatBlendWeightSummary(const FLbbBlendWeight& Weight)
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

	static FString FormatBoneSpaceSummary(const ELbbBoneSpace BoneSpace)
	{
		return BoneSpace == ELbbBoneSpace::MeshSpace
			? TEXT("Mesh Space")
			: TEXT("Local Space");
	}
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
	OutNodeModel.NodeType = GetGraphNodeType();
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

void ULbbLayeredBlendBodyEdGraphNode_FixedPose::AllocatePosePins()
{
	CreatePoseOutputPin(TEXT("Pose"));
}

FText ULbbLayeredBlendBodyEdGraphNode_FixedPose::GetNodeTitleText() const
{
	switch (FixedNodeType)
	{
	case ELbbLayeredBlendBodyGraphNodeType::CurrentPose:
		return LOCTEXT("CurrentPoseTitle", "Current Pose");
	case ELbbLayeredBlendBodyGraphNodeType::Motion:
		return LOCTEXT("MotionTitle", "Motion");
	case ELbbLayeredBlendBodyGraphNodeType::BasePose:
		return LOCTEXT("BasePoseTitle", "Base Pose");
	case ELbbLayeredBlendBodyGraphNodeType::OverlayPose:
		return LOCTEXT("OverlayPoseTitle", "Overlay Pose");
	default:
		return LOCTEXT("UnknownFixedTitle", "Fixed Pose");
	}
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode_FixedPose::GetNodeColor() const
{
	return FLinearColor(0.08f, 0.24f, 0.38f);
}

void ULbbLayeredBlendBodyEdGraphNode_Result::AllocatePosePins()
{
	CreatePoseInputPin(TEXT("Pose"));
}

FText ULbbLayeredBlendBodyEdGraphNode_Result::GetNodeTitleText() const
{
	return LOCTEXT("ResultTitle", "Result");
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode_Result::GetNodeColor() const
{
	return FLinearColor(0.18f, 0.34f, 0.14f);
}

void ULbbLayeredBlendBodyEdGraphNode_ApplySlot::AllocatePosePins()
{
	CreatePoseInputPin(TEXT("Input"));
	CreatePoseOutputPin(TEXT("Result"));
}

FText ULbbLayeredBlendBodyEdGraphNode_ApplySlot::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Apply Slot\nSlot: %s"),
		SlotName.IsNone() ? TEXT("<None>") : *SlotName.ToString()));
}

void ULbbLayeredBlendBodyEdGraphNode_ApplySlot::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbLayeredBlendBodyGraphNodeData_ApplySlot* Data = NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_ApplySlot>())
	{
		SlotName = Data->SlotName;
	}
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode_ApplySlot::GetNodeColor() const
{
	return FLinearColor(0.73f, 0.46f, 0.17f);
}

void ULbbLayeredBlendBodyEdGraphNode_ApplySlot::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbLayeredBlendBodyGraphNodeData_ApplySlot>();
	FLbbLayeredBlendBodyGraphNodeData_ApplySlot& Data = OutNodeData.GetMutable<FLbbLayeredBlendBodyGraphNodeData_ApplySlot>();
	Data.SlotName = SlotName;
}

void ULbbLayeredBlendBodyEdGraphNode_Blend::AllocatePosePins()
{
	CreatePoseInputPin(TEXT("Base"));
	CreatePoseInputPin(TEXT("Blend"));
	CreatePoseOutputPin(TEXT("Result"));
}

FText ULbbLayeredBlendBodyEdGraphNode_Blend::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Blend\nWeight: %s"),
		*FormatBlendWeightSummary(Weight)));
}

void ULbbLayeredBlendBodyEdGraphNode_Blend::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbLayeredBlendBodyGraphNodeData_Blend* Data = NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_Blend>())
	{
		Weight = Data->Weight;
	}
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode_Blend::GetNodeColor() const
{
	return FLinearColor(0.14f, 0.45f, 0.71f);
}

void ULbbLayeredBlendBodyEdGraphNode_Blend::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbLayeredBlendBodyGraphNodeData_Blend>();
	FLbbLayeredBlendBodyGraphNodeData_Blend& Data = OutNodeData.GetMutable<FLbbLayeredBlendBodyGraphNodeData_Blend>();
	Data.Weight = Weight;
}

void ULbbLayeredBlendBodyEdGraphNode_MaskedBlend::AllocatePosePins()
{
	CreatePoseInputPin(TEXT("Base"));
	CreatePoseInputPin(TEXT("Blend"));
	CreatePoseOutputPin(TEXT("Result"));
}

FText ULbbLayeredBlendBodyEdGraphNode_MaskedBlend::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Masked Blend\n%s | Branches: %d | Weight: %s"),
		*FormatBoneSpaceSummary(BlendSpace),
		BoneFilter.BranchFilters.Num(),
		*FormatBlendWeightSummary(Weight)));
}

void ULbbLayeredBlendBodyEdGraphNode_MaskedBlend::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbLayeredBlendBodyGraphNodeData_MaskedBlend* Data = NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_MaskedBlend>())
	{
		BlendSpace = Data->BlendSpace;
		BoneFilter = Data->BoneFilter;
		Weight = Data->Weight;
	}
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode_MaskedBlend::GetNodeColor() const
{
	return FLinearColor(0.16f, 0.52f, 0.36f);
}

void ULbbLayeredBlendBodyEdGraphNode_MaskedBlend::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbLayeredBlendBodyGraphNodeData_MaskedBlend>();
	FLbbLayeredBlendBodyGraphNodeData_MaskedBlend& Data = OutNodeData.GetMutable<FLbbLayeredBlendBodyGraphNodeData_MaskedBlend>();
	Data.BlendSpace = BlendSpace;
	Data.BoneFilter = BoneFilter;
	Data.Weight = Weight;
}

void ULbbLayeredBlendBodyEdGraphNode_ApplyMotionDelta::AllocatePosePins()
{
	CreatePoseInputPin(TEXT("Base"));
	CreatePoseOutputPin(TEXT("Result"));
}

FText ULbbLayeredBlendBodyEdGraphNode_ApplyMotionDelta::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Apply Motion Delta\n%s | Weight: %s"),
		*FormatBoneSpaceSummary(AdditiveSpace),
		*FormatBlendWeightSummary(Weight)));
}

void ULbbLayeredBlendBodyEdGraphNode_ApplyMotionDelta::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbLayeredBlendBodyGraphNodeData_ApplyMotionDelta* Data = NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_ApplyMotionDelta>())
	{
		AdditiveSpace = Data->AdditiveSpace;
		Weight = Data->Weight;
	}
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode_ApplyMotionDelta::GetNodeColor() const
{
	return FLinearColor(0.62f, 0.21f, 0.18f);
}

void ULbbLayeredBlendBodyEdGraphNode_ApplyMotionDelta::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbLayeredBlendBodyGraphNodeData_ApplyMotionDelta>();
	FLbbLayeredBlendBodyGraphNodeData_ApplyMotionDelta& Data = OutNodeData.GetMutable<FLbbLayeredBlendBodyGraphNodeData_ApplyMotionDelta>();
	Data.AdditiveSpace = AdditiveSpace;
	Data.Weight = Weight;
}

#undef LOCTEXT_NAMESPACE
