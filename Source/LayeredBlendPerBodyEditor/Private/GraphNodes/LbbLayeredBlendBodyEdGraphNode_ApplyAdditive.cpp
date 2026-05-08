// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbLayeredBlendBodyEdGraphNode_ApplyAdditive.h"

#include "GraphNodes/LbbLayeredBlendBodyEdGraphNodeUtils.h"
#include "LbbLayeredBlendBodyGraphNodeRegistry.h"

FLbbLayeredBlendBodyGraphNodeDescriptor ULbbLayeredBlendBodyEdGraphNode_ApplyAdditive::BuildGraphNodeDescriptor() const
{
	return LbbLayeredBlendBodyEdGraphNodeUtils::MakeNodeDescriptor(
		FLbbLayeredBlendBodyGraphNodeData_ApplyAdditive::StaticStruct(),
		ULbbLayeredBlendBodyEdGraphNode_ApplyAdditive::StaticClass(),
		TEXT("Apply Additive"),
		MakeArrayView(LbbLayeredBlendBodyEdGraphNodeUtils::BaseAdditiveInputPins),
		MakeArrayView(LbbLayeredBlendBodyEdGraphNodeUtils::ResultOutputPins),
		false,
		false,
		false,
		true,
		false,
		ELbbLayeredBlendBodyGraphKindFlags::None,
		ELbbLayeredBlendBodyGraphKindFlags::All,
		&ULbbLayeredBlendBodyEdGraphNode_ApplyAdditive::CompileNode);
}

void ULbbLayeredBlendBodyEdGraphNode_ApplyAdditive::AllocatePosePins()
{
	CreatePoseInputPin(LbbLayeredBlendBodyEdGraphNodeUtils::PinBase);
	CreatePoseInputPin(LbbLayeredBlendBodyEdGraphNodeUtils::PinAdditive);
	CreatePoseOutputPin(LbbLayeredBlendBodyEdGraphNodeUtils::PinResult);
}

FText ULbbLayeredBlendBodyEdGraphNode_ApplyAdditive::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Apply Additive\n%s | Weight: %s"),
		*LbbLayeredBlendBodyEdGraphNodeUtils::FormatBoneSpaceSummary(AdditiveSpace),
		*LbbLayeredBlendBodyEdGraphNodeUtils::FormatBlendWeightSummary(Weight)));
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode_ApplyAdditive::GetNodeColor() const
{
	return FLinearColor(0.62f, 0.21f, 0.18f);
}

void ULbbLayeredBlendBodyEdGraphNode_ApplyAdditive::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbLayeredBlendBodyGraphNodeData_ApplyAdditive* Data = NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_ApplyAdditive>())
	{
		AdditiveSpace = Data->AdditiveSpace;
		Weight = Data->Weight;
	}
}

void ULbbLayeredBlendBodyEdGraphNode_ApplyAdditive::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbLayeredBlendBodyGraphNodeData_ApplyAdditive>();
	FLbbLayeredBlendBodyGraphNodeData_ApplyAdditive& Data = OutNodeData.GetMutable<FLbbLayeredBlendBodyGraphNodeData_ApplyAdditive>();
	Data.AdditiveSpace = AdditiveSpace;
	Data.Weight = Weight;
}

bool ULbbLayeredBlendBodyEdGraphNode_ApplyAdditive::CompileNode(
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
			TEXT("Apply Additive failed to resolve one of its inputs."))
		|| !LbbLayeredBlendBodyEdGraphNodeUtils::ResolveRequiredInputSource(
			CompileContext,
			NodeModel,
			LbbLayeredBlendBodyEdGraphNodeUtils::PinAdditive,
			AdditiveSource,
			OutMessages,
			TEXT("Apply Additive failed to resolve one of its inputs.")))
	{
		return false;
	}

	const FLbbLayeredBlendBodyGraphNodeData_ApplyAdditive* NodeData = NodeModel.NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_ApplyAdditive>();
	if (NodeData == nullptr)
	{
		LbbLayeredBlendBodyEdGraphNodeUtils::AddMessage(
			OutMessages,
			EMessageSeverity::Error,
			CompileContext.GetGraphKind(),
			CompileContext.GetBodyPartIndex(),
			NodeModel.NodeGuid,
			TEXT("Apply Additive node data is missing."));
		return false;
	}

	if (!LbbLayeredBlendBodyEdGraphNodeUtils::ValidateCurveWeight(
			NodeData->Weight,
			CompileContext,
			NodeModel,
			TEXT("Apply Additive uses a curve weight but BlendCurveName is None."),
			OutMessages))
	{
		return false;
	}

	FLbbLayeredBlendBodyPartOperator_ApplyAdditive Operator;
	Operator.BasePose = BaseSource;
	Operator.AdditivePose = AdditiveSource;
	Operator.AdditiveSpace = NodeData->AdditiveSpace;
	Operator.Weight = NodeData->Weight;
	Operator.Output = TargetPose;
	LbbLayeredBlendBodyEdGraphNodeUtils::AddOperatorStruct(OutOperators, Operator);
	return true;
}
