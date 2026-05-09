// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbEdGraphNode_MakeAdditive.h"

#include "GraphNodes/LbbEdGraphNodeUtils.h"
#include "LbbGraphNodeRegistry.h"

FLbbGraphNodeDescriptor ULbbEdGraphNode_MakeAdditive::BuildGraphNodeDescriptor() const
{
	return LbbEdGraphNodeUtils::MakeNodeDescriptor(
		FLbbGraphNodeData_MakeAdditive::StaticStruct(),
		ULbbEdGraphNode_MakeAdditive::StaticClass(),
		TEXT("Make Additive"),
		MakeArrayView(LbbEdGraphNodeUtils::BaseAdditiveInputPins),
		MakeArrayView(LbbEdGraphNodeUtils::ResultOutputPins),
		false,
		false,
		false,
		true,
		false,
		ELbbGraphKindFlags::Cache,
		ELbbGraphKindFlags::Cache,
		&ULbbEdGraphNode_MakeAdditive::CompileNode);
}

void ULbbEdGraphNode_MakeAdditive::AllocatePosePins()
{
	CreatePoseInputPin(LbbEdGraphNodeUtils::PinBase);
	CreatePoseInputPin(LbbEdGraphNodeUtils::PinAdditive);
	CreatePoseOutputPin(LbbEdGraphNodeUtils::PinResult);
}

FText ULbbEdGraphNode_MakeAdditive::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Make Additive\n%s"),
		*LbbEdGraphNodeUtils::FormatBoneSpaceSummary(AdditiveSpace)));
}

FLinearColor ULbbEdGraphNode_MakeAdditive::GetNodeColor() const
{
	return FLinearColor(0.45f, 0.22f, 0.13f);
}

void ULbbEdGraphNode_MakeAdditive::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbGraphNodeData_MakeAdditive* Data = NodeData.GetPtr<FLbbGraphNodeData_MakeAdditive>())
	{
		AdditiveSpace = Data->AdditiveSpace;
	}
}

void ULbbEdGraphNode_MakeAdditive::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbGraphNodeData_MakeAdditive>();
	FLbbGraphNodeData_MakeAdditive& Data = OutNodeData.GetMutable<FLbbGraphNodeData_MakeAdditive>();
	Data.AdditiveSpace = AdditiveSpace;
}

bool ULbbEdGraphNode_MakeAdditive::CompileNode(
	const FLbbGraphNodeCompileContext& CompileContext,
	const FLbbGraphNodeModel& NodeModel,
	const FLbbLayeredBodyPartPoseTarget& TargetPose,
	TArray<FInstancedStruct>& OutOperators,
	TArray<FLbbCompileMessage>& OutMessages)
{
	FLbbLayeredBodyPartPoseSource BaseSource;
	FLbbLayeredBodyPartPoseSource AdditiveSource;
	if (!LbbEdGraphNodeUtils::ResolveRequiredInputSource(
			CompileContext,
			NodeModel,
			LbbEdGraphNodeUtils::PinBase,
			BaseSource,
			OutMessages,
			TEXT("Make Additive failed to resolve one of its inputs."))
		|| !LbbEdGraphNodeUtils::ResolveRequiredInputSource(
			CompileContext,
			NodeModel,
			LbbEdGraphNodeUtils::PinAdditive,
			AdditiveSource,
			OutMessages,
			TEXT("Make Additive failed to resolve one of its inputs.")))
	{
		return false;
	}

	const FLbbGraphNodeData_MakeAdditive* NodeData = NodeModel.NodeData.GetPtr<FLbbGraphNodeData_MakeAdditive>();
	if (NodeData == nullptr)
	{
		LbbEdGraphNodeUtils::AddMessage(
			OutMessages,
			EMessageSeverity::Error,
			CompileContext.GetGraphKind(),
			CompileContext.GetBodyPartIndex(),
			NodeModel.NodeGuid,
			TEXT("Make Additive node data is missing."));
		return false;
	}

	FLbbPartOperator_MakeAdditive Operator;
	Operator.BasePose = BaseSource;
	Operator.AdditivePose = AdditiveSource;
	Operator.AdditiveSpace = NodeData->AdditiveSpace;
	Operator.Output = TargetPose;
	LbbEdGraphNodeUtils::AddOperatorStruct(OutOperators, Operator);
	return true;
}
