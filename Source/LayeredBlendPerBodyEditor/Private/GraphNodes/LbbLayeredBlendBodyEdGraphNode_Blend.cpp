// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbLayeredBlendBodyEdGraphNode_Blend.h"

#include "GraphNodes/LbbLayeredBlendBodyEdGraphNodeUtils.h"
#include "LbbLayeredBlendBodyGraphNodeRegistry.h"

FLbbLayeredBlendBodyGraphNodeDescriptor ULbbLayeredBlendBodyEdGraphNode_Blend::BuildGraphNodeDescriptor() const
{
	return LbbLayeredBlendBodyEdGraphNodeUtils::MakeNodeDescriptor(
		FLbbLayeredBlendBodyGraphNodeData_Blend::StaticStruct(),
		ULbbLayeredBlendBodyEdGraphNode_Blend::StaticClass(),
		TEXT("Blend"),
		MakeArrayView(LbbLayeredBlendBodyEdGraphNodeUtils::BaseBlendInputPins),
		MakeArrayView(LbbLayeredBlendBodyEdGraphNodeUtils::ResultOutputPins),
		false,
		false,
		false,
		true,
		false,
		ELbbLayeredBlendBodyGraphKindFlags::None,
		ELbbLayeredBlendBodyGraphKindFlags::All,
		&ULbbLayeredBlendBodyEdGraphNode_Blend::CompileNode);
}

void ULbbLayeredBlendBodyEdGraphNode_Blend::AllocatePosePins()
{
	CreatePoseInputPin(LbbLayeredBlendBodyEdGraphNodeUtils::PinBase);
	CreatePoseInputPin(LbbLayeredBlendBodyEdGraphNodeUtils::PinBlend);
	CreatePoseOutputPin(LbbLayeredBlendBodyEdGraphNodeUtils::PinResult);
}

FText ULbbLayeredBlendBodyEdGraphNode_Blend::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Blend\nWeight: %s"),
		*LbbLayeredBlendBodyEdGraphNodeUtils::FormatBlendWeightSummary(Weight)));
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode_Blend::GetNodeColor() const
{
	return FLinearColor(0.14f, 0.45f, 0.71f);
}

void ULbbLayeredBlendBodyEdGraphNode_Blend::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbLayeredBlendBodyGraphNodeData_Blend* Data = NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_Blend>())
	{
		Weight = Data->Weight;
	}
}

void ULbbLayeredBlendBodyEdGraphNode_Blend::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbLayeredBlendBodyGraphNodeData_Blend>();
	FLbbLayeredBlendBodyGraphNodeData_Blend& Data = OutNodeData.GetMutable<FLbbLayeredBlendBodyGraphNodeData_Blend>();
	Data.Weight = Weight;
}

bool ULbbLayeredBlendBodyEdGraphNode_Blend::CompileNode(
	const FLbbLayeredBlendBodyGraphNodeCompileContext& CompileContext,
	const FLbbLayeredBlendBodyGraphNodeModel& NodeModel,
	const FLbbLayeredBodyPartPoseTarget& TargetPose,
	TArray<FInstancedStruct>& OutOperators,
	TArray<FLbbCompileMessage>& OutMessages)
{
	FLbbLayeredBodyPartPoseSource BaseSource;
	FLbbLayeredBodyPartPoseSource BlendSource;
	if (!LbbLayeredBlendBodyEdGraphNodeUtils::ResolveRequiredInputSource(
			CompileContext,
			NodeModel,
			LbbLayeredBlendBodyEdGraphNodeUtils::PinBase,
			BaseSource,
			OutMessages,
			TEXT("Blend failed to resolve one of its inputs."))
		|| !LbbLayeredBlendBodyEdGraphNodeUtils::ResolveRequiredInputSource(
			CompileContext,
			NodeModel,
			LbbLayeredBlendBodyEdGraphNodeUtils::PinBlend,
			BlendSource,
			OutMessages,
			TEXT("Blend failed to resolve one of its inputs.")))
	{
		return false;
	}

	const FLbbLayeredBlendBodyGraphNodeData_Blend* NodeData = NodeModel.NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_Blend>();
	if (NodeData == nullptr)
	{
		LbbLayeredBlendBodyEdGraphNodeUtils::AddMessage(
			OutMessages,
			EMessageSeverity::Error,
			CompileContext.GetGraphKind(),
			CompileContext.GetBodyPartIndex(),
			NodeModel.NodeGuid,
			TEXT("Blend node data is missing."));
		return false;
	}

	if (!LbbLayeredBlendBodyEdGraphNodeUtils::ValidateCurveWeight(
			NodeData->Weight,
			CompileContext,
			NodeModel,
			TEXT("Blend uses a curve weight but BlendCurveName is None."),
			OutMessages))
	{
		return false;
	}

	FLbbLayeredBlendBodyPartOperator_BlendTwoPoses Operator;
	Operator.BasePose = BaseSource;
	Operator.BlendPose = BlendSource;
	Operator.Weight = NodeData->Weight;
	Operator.Output = TargetPose;
	LbbLayeredBlendBodyEdGraphNodeUtils::AddOperatorStruct(OutOperators, Operator);
	return true;
}
