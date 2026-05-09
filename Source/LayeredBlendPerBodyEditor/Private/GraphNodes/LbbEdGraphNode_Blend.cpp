// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbEdGraphNode_Blend.h"

#include "GraphNodes/LbbEdGraphNodeUtils.h"
#include "LbbGraphNodeRegistry.h"

FLbbGraphNodeDescriptor ULbbEdGraphNode_Blend::BuildGraphNodeDescriptor() const
{
	return LbbEdGraphNodeUtils::MakeNodeDescriptor(
		FLbbGraphNodeData_Blend::StaticStruct(),
		ULbbEdGraphNode_Blend::StaticClass(),
		TEXT("Blend"),
		MakeArrayView(LbbEdGraphNodeUtils::BaseBlendInputPins),
		MakeArrayView(LbbEdGraphNodeUtils::ResultOutputPins),
		false,
		false,
		false,
		true,
		false,
		ELbbGraphKindFlags::None,
		ELbbGraphKindFlags::All,
		&ULbbEdGraphNode_Blend::CompileNode);
}

void ULbbEdGraphNode_Blend::AllocatePosePins()
{
	CreatePoseInputPin(LbbEdGraphNodeUtils::PinBase);
	CreatePoseInputPin(LbbEdGraphNodeUtils::PinBlend);
	CreatePoseOutputPin(LbbEdGraphNodeUtils::PinResult);
}

FText ULbbEdGraphNode_Blend::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Blend\nWeight: %s"),
		*LbbEdGraphNodeUtils::FormatBlendWeightSummary(Weight)));
}

FLinearColor ULbbEdGraphNode_Blend::GetNodeColor() const
{
	return FLinearColor(0.14f, 0.45f, 0.71f);
}

void ULbbEdGraphNode_Blend::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbGraphNodeData_Blend* Data = NodeData.GetPtr<FLbbGraphNodeData_Blend>())
	{
		Weight = Data->Weight;
	}
}

void ULbbEdGraphNode_Blend::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbGraphNodeData_Blend>();
	FLbbGraphNodeData_Blend& Data = OutNodeData.GetMutable<FLbbGraphNodeData_Blend>();
	Data.Weight = Weight;
}

bool ULbbEdGraphNode_Blend::CompileNode(
	const FLbbGraphNodeCompileContext& CompileContext,
	const FLbbGraphNodeModel& NodeModel,
	const FLbbLayeredBodyPartPoseTarget& TargetPose,
	TArray<FInstancedStruct>& OutOperators,
	TArray<FLbbCompileMessage>& OutMessages)
{
	FLbbLayeredBodyPartPoseSource BaseSource;
	FLbbLayeredBodyPartPoseSource BlendSource;
	if (!LbbEdGraphNodeUtils::ResolveRequiredInputSource(
			CompileContext,
			NodeModel,
			LbbEdGraphNodeUtils::PinBase,
			BaseSource,
			OutMessages,
			TEXT("Blend failed to resolve one of its inputs."))
		|| !LbbEdGraphNodeUtils::ResolveRequiredInputSource(
			CompileContext,
			NodeModel,
			LbbEdGraphNodeUtils::PinBlend,
			BlendSource,
			OutMessages,
			TEXT("Blend failed to resolve one of its inputs.")))
	{
		return false;
	}

	const FLbbGraphNodeData_Blend* NodeData = NodeModel.NodeData.GetPtr<FLbbGraphNodeData_Blend>();
	if (NodeData == nullptr)
	{
		LbbEdGraphNodeUtils::AddMessage(
			OutMessages,
			EMessageSeverity::Error,
			CompileContext.GetGraphKind(),
			CompileContext.GetBodyPartIndex(),
			NodeModel.NodeGuid,
			TEXT("Blend node data is missing."));
		return false;
	}

	if (!LbbEdGraphNodeUtils::ValidateCurveWeight(
			NodeData->Weight,
			CompileContext,
			NodeModel,
			TEXT("Blend uses a curve weight but BlendCurveName is None."),
			OutMessages))
	{
		return false;
	}

	FLbbPartOperator_BlendTwoPoses Operator;
	Operator.BasePose = BaseSource;
	Operator.BlendPose = BlendSource;
	Operator.Weight = NodeData->Weight;
	Operator.Output = TargetPose;
	LbbEdGraphNodeUtils::AddOperatorStruct(OutOperators, Operator);
	return true;
}
