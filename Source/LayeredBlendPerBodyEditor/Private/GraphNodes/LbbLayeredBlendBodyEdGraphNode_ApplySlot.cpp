// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbLayeredBlendBodyEdGraphNode_ApplySlot.h"

#include "GraphNodes/LbbLayeredBlendBodyEdGraphNodeUtils.h"
#include "LbbLayeredBlendBodyGraphNodeRegistry.h"

FLbbLayeredBlendBodyGraphNodeDescriptor ULbbLayeredBlendBodyEdGraphNode_ApplySlot::BuildGraphNodeDescriptor() const
{
	return LbbLayeredBlendBodyEdGraphNodeUtils::MakeNodeDescriptor(
		FLbbLayeredBlendBodyGraphNodeData_ApplySlot::StaticStruct(),
		ULbbLayeredBlendBodyEdGraphNode_ApplySlot::StaticClass(),
		TEXT("Apply Slot"),
		MakeArrayView(LbbLayeredBlendBodyEdGraphNodeUtils::InputPins),
		MakeArrayView(LbbLayeredBlendBodyEdGraphNodeUtils::ResultOutputPins),
		false,
		false,
		false,
		true,
		false,
		ELbbLayeredBlendBodyGraphKindFlags::None,
		ELbbLayeredBlendBodyGraphKindFlags::BodyPart,
		&ULbbLayeredBlendBodyEdGraphNode_ApplySlot::CompileNode);
}

void ULbbLayeredBlendBodyEdGraphNode_ApplySlot::AllocatePosePins()
{
	CreatePoseInputPin(LbbLayeredBlendBodyEdGraphNodeUtils::PinInput);
	CreatePoseOutputPin(LbbLayeredBlendBodyEdGraphNodeUtils::PinResult);
}

FText ULbbLayeredBlendBodyEdGraphNode_ApplySlot::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Apply Slot\nSlot: %s"),
		SlotName.IsNone() ? TEXT("<None>") : *SlotName.ToString()));
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode_ApplySlot::GetNodeColor() const
{
	return FLinearColor(0.73f, 0.46f, 0.17f);
}

void ULbbLayeredBlendBodyEdGraphNode_ApplySlot::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbLayeredBlendBodyGraphNodeData_ApplySlot* Data = NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_ApplySlot>())
	{
		SlotName = Data->SlotName;
	}
}

void ULbbLayeredBlendBodyEdGraphNode_ApplySlot::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbLayeredBlendBodyGraphNodeData_ApplySlot>();
	FLbbLayeredBlendBodyGraphNodeData_ApplySlot& Data = OutNodeData.GetMutable<FLbbLayeredBlendBodyGraphNodeData_ApplySlot>();
	Data.SlotName = SlotName;
}

bool ULbbLayeredBlendBodyEdGraphNode_ApplySlot::CompileNode(
	const FLbbLayeredBlendBodyGraphNodeCompileContext& CompileContext,
	const FLbbLayeredBlendBodyGraphNodeModel& NodeModel,
	const FLbbLayeredBodyPartPoseTarget& TargetPose,
	TArray<FInstancedStruct>& OutOperators,
	TArray<FLbbCompileMessage>& OutMessages)
{
	FLbbLayeredBodyPartPoseSource SourcePose;
	if (!LbbLayeredBlendBodyEdGraphNodeUtils::ResolveRequiredInputSource(
			CompileContext,
			NodeModel,
			LbbLayeredBlendBodyEdGraphNodeUtils::PinInput,
			SourcePose,
			OutMessages,
			TEXT("Apply Slot failed to resolve its input.")))
	{
		return false;
	}

	const FLbbLayeredBlendBodyGraphNodeData_ApplySlot* NodeData = NodeModel.NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_ApplySlot>();
	if (NodeData == nullptr || NodeData->SlotName.IsNone())
	{
		LbbLayeredBlendBodyEdGraphNodeUtils::AddMessage(
			OutMessages,
			EMessageSeverity::Error,
			CompileContext.GetGraphKind(),
			CompileContext.GetBodyPartIndex(),
			NodeModel.NodeGuid,
			TEXT("Apply Slot requires a valid SlotName."));
		return false;
	}

	FLbbLayeredBlendBodyPartOperator_ApplySlot Operator;
	Operator.SourcePose = SourcePose;
	Operator.SlotName = NodeData->SlotName;
	Operator.Output = TargetPose;
	LbbLayeredBlendBodyEdGraphNodeUtils::AddOperatorStruct(OutOperators, Operator);
	return true;
}
