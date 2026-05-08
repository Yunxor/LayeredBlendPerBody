// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbLayeredBlendBodyEdGraphNode_MakeAdditive.h"

#include "GraphNodes/LbbLayeredBlendBodyEdGraphNodeUtils.h"
#include "LbbLayeredBlendBodyGraphNodeRegistry.h"

FLbbLayeredBlendBodyGraphNodeDescriptor ULbbLayeredBlendBodyEdGraphNode_MakeAdditive::BuildGraphNodeDescriptor() const
{
	return LbbLayeredBlendBodyEdGraphNodeUtils::MakeNodeDescriptor(
		FLbbLayeredBlendBodyGraphNodeData_MakeAdditive::StaticStruct(),
		ULbbLayeredBlendBodyEdGraphNode_MakeAdditive::StaticClass(),
		TEXT("Make Additive"),
		MakeArrayView(LbbLayeredBlendBodyEdGraphNodeUtils::BaseAdditiveInputPins),
		MakeArrayView(LbbLayeredBlendBodyEdGraphNodeUtils::ResultOutputPins),
		false,
		false,
		false,
		true,
		false,
		ELbbLayeredBlendBodyGraphKindFlags::Cache,
		ELbbLayeredBlendBodyGraphKindFlags::Cache,
		&ULbbLayeredBlendBodyEdGraphNode_MakeAdditive::CompileNode);
}

void ULbbLayeredBlendBodyEdGraphNode_MakeAdditive::AllocatePosePins()
{
	CreatePoseInputPin(LbbLayeredBlendBodyEdGraphNodeUtils::PinBase);
	CreatePoseInputPin(LbbLayeredBlendBodyEdGraphNodeUtils::PinAdditive);
	CreatePoseOutputPin(LbbLayeredBlendBodyEdGraphNodeUtils::PinResult);
}

FText ULbbLayeredBlendBodyEdGraphNode_MakeAdditive::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Make Additive\n%s"),
		*LbbLayeredBlendBodyEdGraphNodeUtils::FormatBoneSpaceSummary(AdditiveSpace)));
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode_MakeAdditive::GetNodeColor() const
{
	return FLinearColor(0.45f, 0.22f, 0.13f);
}

void ULbbLayeredBlendBodyEdGraphNode_MakeAdditive::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbLayeredBlendBodyGraphNodeData_MakeAdditive* Data = NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_MakeAdditive>())
	{
		AdditiveSpace = Data->AdditiveSpace;
	}
}

void ULbbLayeredBlendBodyEdGraphNode_MakeAdditive::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbLayeredBlendBodyGraphNodeData_MakeAdditive>();
	FLbbLayeredBlendBodyGraphNodeData_MakeAdditive& Data = OutNodeData.GetMutable<FLbbLayeredBlendBodyGraphNodeData_MakeAdditive>();
	Data.AdditiveSpace = AdditiveSpace;
}

bool ULbbLayeredBlendBodyEdGraphNode_MakeAdditive::CompileNode(
	const FLbbLayeredBlendBodyGraphNodeCompileContext& CompileContext,
	const FLbbLayeredBlendBodyGraphNodeModel& NodeModel,
	const FLbbLayeredBodyPartPoseTarget& TargetPose,
	TArray<FInstancedStruct>& OutOperators,
	TArray<FLbbCompileMessage>& OutMessages)
{
	FLbbLayeredBodyPartPoseSource BaseSource;
	FLbbLayeredBodyPartPoseSource AdditiveSource;
	if (!LbbLayeredBlendBodyEdGraphNodeUtils::ResolveRequiredInputSource(
			CompileContext,
			NodeModel,
			LbbLayeredBlendBodyEdGraphNodeUtils::PinBase,
			BaseSource,
			OutMessages,
			TEXT("Make Additive failed to resolve one of its inputs."))
		|| !LbbLayeredBlendBodyEdGraphNodeUtils::ResolveRequiredInputSource(
			CompileContext,
			NodeModel,
			LbbLayeredBlendBodyEdGraphNodeUtils::PinAdditive,
			AdditiveSource,
			OutMessages,
			TEXT("Make Additive failed to resolve one of its inputs.")))
	{
		return false;
	}

	const FLbbLayeredBlendBodyGraphNodeData_MakeAdditive* NodeData = NodeModel.NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_MakeAdditive>();
	if (NodeData == nullptr)
	{
		LbbLayeredBlendBodyEdGraphNodeUtils::AddMessage(
			OutMessages,
			EMessageSeverity::Error,
			CompileContext.GetGraphKind(),
			CompileContext.GetBodyPartIndex(),
			NodeModel.NodeGuid,
			TEXT("Make Additive node data is missing."));
		return false;
	}

	FLbbLayeredBlendBodyPartOperator_MakeAdditive Operator;
	Operator.BasePose = BaseSource;
	Operator.AdditivePose = AdditiveSource;
	Operator.AdditiveSpace = NodeData->AdditiveSpace;
	Operator.Output = TargetPose;
	LbbLayeredBlendBodyEdGraphNodeUtils::AddOperatorStruct(OutOperators, Operator);
	return true;
}
