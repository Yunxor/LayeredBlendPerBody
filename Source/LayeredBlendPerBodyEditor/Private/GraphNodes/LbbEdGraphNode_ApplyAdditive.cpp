// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbEdGraphNode_ApplyAdditive.h"

#include "GraphNodes/LbbEdGraphNodeUtils.h"
#include "LbbGraphNodeRegistry.h"

FLbbGraphNodeDescriptor ULbbEdGraphNode_ApplyAdditive::BuildGraphNodeDescriptor() const
{
	return LbbEdGraphNodeUtils::MakeNodeDescriptor(
		FLbbGraphNodeData_ApplyAdditive::StaticStruct(),
		ULbbEdGraphNode_ApplyAdditive::StaticClass(),
		TEXT("Apply Additive"),
		MakeArrayView(LbbEdGraphNodeUtils::BaseAdditiveInputPins),
		MakeArrayView(LbbEdGraphNodeUtils::ResultOutputPins),
		false,
		false,
		false,
		true,
		false,
		ELbbGraphKindFlags::None,
		ELbbGraphKindFlags::All,
		&ULbbEdGraphNode_ApplyAdditive::CompileNode);
}

void ULbbEdGraphNode_ApplyAdditive::AllocatePosePins()
{
	CreatePoseInputPin(LbbEdGraphNodeUtils::PinBase);
	CreatePoseInputPin(LbbEdGraphNodeUtils::PinAdditive);
	CreatePoseOutputPin(LbbEdGraphNodeUtils::PinResult);
}

FText ULbbEdGraphNode_ApplyAdditive::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Apply Additive\n%s | Weight: %s"),
		*LbbEdGraphNodeUtils::FormatBoneSpaceSummary(AdditiveSpace),
		*LbbEdGraphNodeUtils::FormatBlendWeightSummary(Weight)));
}

FLinearColor ULbbEdGraphNode_ApplyAdditive::GetNodeColor() const
{
	return FLinearColor(0.62f, 0.21f, 0.18f);
}

void ULbbEdGraphNode_ApplyAdditive::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbGraphNodeData_ApplyAdditive* Data = NodeData.GetPtr<FLbbGraphNodeData_ApplyAdditive>())
	{
		AdditiveSpace = Data->AdditiveSpace;
		Weight = Data->Weight;
	}
}

void ULbbEdGraphNode_ApplyAdditive::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbGraphNodeData_ApplyAdditive>();
	FLbbGraphNodeData_ApplyAdditive& Data = OutNodeData.GetMutable<FLbbGraphNodeData_ApplyAdditive>();
	Data.AdditiveSpace = AdditiveSpace;
	Data.Weight = Weight;
}

bool ULbbEdGraphNode_ApplyAdditive::CompileNode(
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
			TEXT("Apply Additive failed to resolve one of its inputs."))
		|| !LbbEdGraphNodeUtils::ResolveRequiredInputSource(
			CompileContext,
			NodeModel,
			LbbEdGraphNodeUtils::PinAdditive,
			AdditiveSource,
			OutMessages,
			TEXT("Apply Additive failed to resolve one of its inputs.")))
	{
		return false;
	}

	const FLbbGraphNodeData_ApplyAdditive* NodeData = NodeModel.NodeData.GetPtr<FLbbGraphNodeData_ApplyAdditive>();
	if (NodeData == nullptr)
	{
		LbbEdGraphNodeUtils::AddMessage(
			OutMessages,
			EMessageSeverity::Error,
			CompileContext.GetGraphKind(),
			CompileContext.GetBodyPartIndex(),
			NodeModel.NodeGuid,
			TEXT("Apply Additive node data is missing."));
		return false;
	}

	if (!LbbEdGraphNodeUtils::ValidateCurveWeight(
			NodeData->Weight,
			CompileContext,
			NodeModel,
			TEXT("Apply Additive uses a curve weight but BlendCurveName is None."),
			OutMessages))
	{
		return false;
	}

	FLbbPartOperator_ApplyAdditive Operator;
	Operator.BasePose = BaseSource;
	Operator.AdditivePose = AdditiveSource;
	Operator.AdditiveSpace = NodeData->AdditiveSpace;
	Operator.Weight = NodeData->Weight;
	Operator.Output = TargetPose;
	LbbEdGraphNodeUtils::AddOperatorStruct(OutOperators, Operator);
	return true;
}
