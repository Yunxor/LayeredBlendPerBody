// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbEdGraphNode_Result.h"

#include "GraphNodes/LbbEdGraphNodeUtils.h"
#include "LbbGraphNodeRegistry.h"

FLbbGraphNodeDescriptor ULbbEdGraphNode_Result::BuildGraphNodeDescriptor() const
{
	return LbbEdGraphNodeUtils::MakeNodeDescriptor(
		FLbbGraphNodeData_Result::StaticStruct(),
		ULbbEdGraphNode_Result::StaticClass(),
		TEXT("Result"),
		MakeArrayView(LbbEdGraphNodeUtils::PoseInputPins),
		TConstArrayView<FName>(),
		false,
		true,
		false,
		false,
		true,
		ELbbGraphKindFlags::BodyPart,
		ELbbGraphKindFlags::BodyPart,
		nullptr);
}

void ULbbEdGraphNode_Result::AllocatePosePins()
{
	CreatePoseInputPin(LbbEdGraphNodeUtils::PinPose);
}

FText ULbbEdGraphNode_Result::GetNodeTitleText() const
{
	return NSLOCTEXT("LbbEdGraphNode_Result", "ResultTitle", "Result");
}

FLinearColor ULbbEdGraphNode_Result::GetNodeColor() const
{
	return FLinearColor(0.18f, 0.34f, 0.14f);
}
