// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbEdGraphNode_ApplySlot.h"

#include "GraphNodes/LbbEdGraphNodeUtils.h"
#include "LbbGraphNodeRegistry.h"

FLbbGraphNodeDescriptor ULbbEdGraphNode_ApplySlot::BuildGraphNodeDescriptor() const
{
	return LbbEdGraphNodeUtils::MakeNodeDescriptor(
		FLbbGraphNodeData_ApplySlot::StaticStruct(),
		ULbbEdGraphNode_ApplySlot::StaticClass(),
		TEXT("Apply Slot"),
		MakeArrayView(LbbEdGraphNodeUtils::InputPins),
		MakeArrayView(LbbEdGraphNodeUtils::ResultOutputPins),
		false,
		false,
		false,
		true,
		false,
		ELbbGraphKindFlags::None,
		ELbbGraphKindFlags::BodyPart,
		&ULbbEdGraphNode_ApplySlot::CompileNode);
}

void ULbbEdGraphNode_ApplySlot::AllocatePosePins()
{
	CreatePoseInputPin(LbbEdGraphNodeUtils::PinInput);
	CreatePoseOutputPin(LbbEdGraphNodeUtils::PinResult);
}

FText ULbbEdGraphNode_ApplySlot::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Apply Slot\nSlot: %s"),
		SlotName.IsNone() ? TEXT("<None>") : *SlotName.ToString()));
}

FLinearColor ULbbEdGraphNode_ApplySlot::GetNodeColor() const
{
	return FLinearColor(0.73f, 0.46f, 0.17f);
}

void ULbbEdGraphNode_ApplySlot::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbGraphNodeData_ApplySlot* Data = NodeData.GetPtr<FLbbGraphNodeData_ApplySlot>())
	{
		SlotName = Data->SlotName;
	}
}

void ULbbEdGraphNode_ApplySlot::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbGraphNodeData_ApplySlot>();
	FLbbGraphNodeData_ApplySlot& Data = OutNodeData.GetMutable<FLbbGraphNodeData_ApplySlot>();
	Data.SlotName = SlotName;
}

bool ULbbEdGraphNode_ApplySlot::CompileNode(
	const FLbbGraphNodeCompileContext& CompileContext,
	const FLbbGraphNodeModel& NodeModel,
	const FLbbLayeredBodyPartPoseTarget& TargetPose,
	TArray<FInstancedStruct>& OutOperators,
	TArray<FLbbCompileMessage>& OutMessages)
{
	FLbbLayeredBodyPartPoseSource SourcePose;
	if (!LbbEdGraphNodeUtils::ResolveRequiredInputSource(
			CompileContext,
			NodeModel,
			LbbEdGraphNodeUtils::PinInput,
			SourcePose,
			OutMessages,
			TEXT("Apply Slot failed to resolve its input.")))
	{
		return false;
	}

	const FLbbGraphNodeData_ApplySlot* NodeData = NodeModel.NodeData.GetPtr<FLbbGraphNodeData_ApplySlot>();
	if (NodeData == nullptr || NodeData->SlotName.IsNone())
	{
		LbbEdGraphNodeUtils::AddMessage(
			OutMessages,
			EMessageSeverity::Error,
			CompileContext.GetGraphKind(),
			CompileContext.GetBodyPartIndex(),
			NodeModel.NodeGuid,
			TEXT("Apply Slot requires a valid SlotName."));
		return false;
	}

	FLbbPartOperator_ApplySlot Operator;
	Operator.SourcePose = SourcePose;
	Operator.SlotName = NodeData->SlotName;
	Operator.Output = TargetPose;
	LbbEdGraphNodeUtils::AddOperatorStruct(OutOperators, Operator);
	return true;
}
