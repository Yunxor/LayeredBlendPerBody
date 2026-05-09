// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbEdGraphNode_SavePose.h"

#include "GraphNodes/LbbEdGraphNodeUtils.h"
#include "LbbGraphNodeRegistry.h"

FLbbGraphNodeDescriptor ULbbEdGraphNode_SavePose::BuildGraphNodeDescriptor() const
{
	return LbbEdGraphNodeUtils::MakeNodeDescriptor(
		FLbbGraphNodeData_SavePose::StaticStruct(),
		ULbbEdGraphNode_SavePose::StaticClass(),
		TEXT("Save Pose"),
		MakeArrayView(LbbEdGraphNodeUtils::InputPins),
		TConstArrayView<FName>(),
		false,
		false,
		true,
		true,
		false,
		ELbbGraphKindFlags::Cache,
		ELbbGraphKindFlags::Cache,
		&ULbbEdGraphNode_SavePose::CompileNode);
}

void ULbbEdGraphNode_SavePose::AllocatePosePins()
{
	CreatePoseInputPin(LbbEdGraphNodeUtils::PinInput);
}

FText ULbbEdGraphNode_SavePose::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Save Pose\nCache: %s"),
		CachePoseName.IsNone() ? TEXT("<None>") : *CachePoseName.ToString()));
}

FLinearColor ULbbEdGraphNode_SavePose::GetNodeColor() const
{
	return FLinearColor(0.56f, 0.34f, 0.17f);
}

void ULbbEdGraphNode_SavePose::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbGraphNodeData_SavePose* Data = NodeData.GetPtr<FLbbGraphNodeData_SavePose>())
	{
		CachePoseName = Data->CachePoseName;
	}
}

void ULbbEdGraphNode_SavePose::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbGraphNodeData_SavePose>();
	FLbbGraphNodeData_SavePose& Data = OutNodeData.GetMutable<FLbbGraphNodeData_SavePose>();
	Data.CachePoseName = CachePoseName;
}

bool ULbbEdGraphNode_SavePose::CompileNode(
	const FLbbGraphNodeCompileContext& CompileContext,
	const FLbbGraphNodeModel& NodeModel,
	const FLbbLayeredBodyPartPoseTarget& TargetPose,
	TArray<FInstancedStruct>& OutOperators,
	TArray<FLbbCompileMessage>& OutMessages)
{
	FLbbLayeredBodyPartPoseSource InputPose;
	if (!LbbEdGraphNodeUtils::ResolveRequiredInputSource(
			CompileContext,
			NodeModel,
			LbbEdGraphNodeUtils::PinInput,
			InputPose,
			OutMessages,
			TEXT("Save Pose failed to resolve its input.")))
	{
		return false;
	}

	const FLbbGraphNodeData_SavePose* NodeData = NodeModel.NodeData.GetPtr<FLbbGraphNodeData_SavePose>();
	if (NodeData == nullptr || NodeData->CachePoseName.IsNone())
	{
		LbbEdGraphNodeUtils::AddMessage(
			OutMessages,
			EMessageSeverity::Error,
			CompileContext.GetGraphKind(),
			CompileContext.GetBodyPartIndex(),
			NodeModel.NodeGuid,
			TEXT("Save Pose requires a valid CachePoseName."));
		return false;
	}

	FLbbPartOperator_CopyPose Operator;
	Operator.SourcePose = InputPose;
	Operator.Output = TargetPose;
	LbbEdGraphNodeUtils::AddOperatorStruct(OutOperators, Operator);
	return true;
}
