// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbLayeredBlendBodyEdGraphNode_SavePose.h"

#include "GraphNodes/LbbLayeredBlendBodyEdGraphNodeUtils.h"
#include "LbbLayeredBlendBodyGraphNodeRegistry.h"

FLbbLayeredBlendBodyGraphNodeDescriptor ULbbLayeredBlendBodyEdGraphNode_SavePose::BuildGraphNodeDescriptor() const
{
	return LbbLayeredBlendBodyEdGraphNodeUtils::MakeNodeDescriptor(
		FLbbLayeredBlendBodyGraphNodeData_SavePose::StaticStruct(),
		ULbbLayeredBlendBodyEdGraphNode_SavePose::StaticClass(),
		TEXT("Save Pose"),
		MakeArrayView(LbbLayeredBlendBodyEdGraphNodeUtils::InputPins),
		TConstArrayView<FName>(),
		false,
		false,
		true,
		true,
		false,
		ELbbLayeredBlendBodyGraphKindFlags::Cache,
		ELbbLayeredBlendBodyGraphKindFlags::Cache,
		&ULbbLayeredBlendBodyEdGraphNode_SavePose::CompileNode);
}

void ULbbLayeredBlendBodyEdGraphNode_SavePose::AllocatePosePins()
{
	CreatePoseInputPin(LbbLayeredBlendBodyEdGraphNodeUtils::PinInput);
}

FText ULbbLayeredBlendBodyEdGraphNode_SavePose::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Save Pose\nCache: %s"),
		CachePoseName.IsNone() ? TEXT("<None>") : *CachePoseName.ToString()));
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode_SavePose::GetNodeColor() const
{
	return FLinearColor(0.56f, 0.34f, 0.17f);
}

void ULbbLayeredBlendBodyEdGraphNode_SavePose::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbLayeredBlendBodyGraphNodeData_SavePose* Data = NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_SavePose>())
	{
		CachePoseName = Data->CachePoseName;
	}
}

void ULbbLayeredBlendBodyEdGraphNode_SavePose::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbLayeredBlendBodyGraphNodeData_SavePose>();
	FLbbLayeredBlendBodyGraphNodeData_SavePose& Data = OutNodeData.GetMutable<FLbbLayeredBlendBodyGraphNodeData_SavePose>();
	Data.CachePoseName = CachePoseName;
}

bool ULbbLayeredBlendBodyEdGraphNode_SavePose::CompileNode(
	const FLbbLayeredBlendBodyGraphNodeCompileContext& CompileContext,
	const FLbbLayeredBlendBodyGraphNodeModel& NodeModel,
	const FLbbLayeredBodyPartPoseTarget& TargetPose,
	TArray<FInstancedStruct>& OutOperators,
	TArray<FLbbCompileMessage>& OutMessages)
{
	FLbbLayeredBodyPartPoseSource InputPose;
	if (!LbbLayeredBlendBodyEdGraphNodeUtils::ResolveRequiredInputSource(
			CompileContext,
			NodeModel,
			LbbLayeredBlendBodyEdGraphNodeUtils::PinInput,
			InputPose,
			OutMessages,
			TEXT("Save Pose failed to resolve its input.")))
	{
		return false;
	}

	const FLbbLayeredBlendBodyGraphNodeData_SavePose* NodeData = NodeModel.NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_SavePose>();
	if (NodeData == nullptr || NodeData->CachePoseName.IsNone())
	{
		LbbLayeredBlendBodyEdGraphNodeUtils::AddMessage(
			OutMessages,
			EMessageSeverity::Error,
			CompileContext.GetGraphKind(),
			CompileContext.GetBodyPartIndex(),
			NodeModel.NodeGuid,
			TEXT("Save Pose requires a valid CachePoseName."));
		return false;
	}

	FLbbLayeredBlendBodyPartOperator_CopyPose Operator;
	Operator.SourcePose = InputPose;
	Operator.Output = TargetPose;
	LbbLayeredBlendBodyEdGraphNodeUtils::AddOperatorStruct(OutOperators, Operator);
	return true;
}
