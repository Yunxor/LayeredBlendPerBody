// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbLayeredBlendBodyEdGraphNode_Result.h"

#include "GraphNodes/LbbLayeredBlendBodyEdGraphNodeUtils.h"
#include "LbbLayeredBlendBodyGraphNodeRegistry.h"

FLbbLayeredBlendBodyGraphNodeDescriptor ULbbLayeredBlendBodyEdGraphNode_Result::BuildGraphNodeDescriptor() const
{
	return LbbLayeredBlendBodyEdGraphNodeUtils::MakeNodeDescriptor(
		FLbbLayeredBlendBodyGraphNodeData_Result::StaticStruct(),
		ULbbLayeredBlendBodyEdGraphNode_Result::StaticClass(),
		TEXT("Result"),
		MakeArrayView(LbbLayeredBlendBodyEdGraphNodeUtils::PoseInputPins),
		TConstArrayView<FName>(),
		false,
		true,
		false,
		false,
		true,
		ELbbLayeredBlendBodyGraphKindFlags::BodyPart,
		ELbbLayeredBlendBodyGraphKindFlags::BodyPart,
		nullptr);
}

void ULbbLayeredBlendBodyEdGraphNode_Result::AllocatePosePins()
{
	CreatePoseInputPin(LbbLayeredBlendBodyEdGraphNodeUtils::PinPose);
}

FText ULbbLayeredBlendBodyEdGraphNode_Result::GetNodeTitleText() const
{
	return NSLOCTEXT("LbbLayeredBlendBodyEdGraphNode_Result", "ResultTitle", "Result");
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode_Result::GetNodeColor() const
{
	return FLinearColor(0.18f, 0.34f, 0.14f);
}
