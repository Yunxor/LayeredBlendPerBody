// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbEdGraphNode_MaskedBlend.h"

#include "GraphNodes/LbbEdGraphNodeUtils.h"
#include "LbbGraphNodeRegistry.h"

FLbbGraphNodeDescriptor ULbbEdGraphNode_MaskedBlend::BuildGraphNodeDescriptor() const
{
	return LbbEdGraphNodeUtils::MakeNodeDescriptor(
		FLbbGraphNodeData_MaskedBlend::StaticStruct(),
		ULbbEdGraphNode_MaskedBlend::StaticClass(),
		TEXT("Masked Blend"),
		MakeArrayView(LbbEdGraphNodeUtils::BaseBlendInputPins),
		MakeArrayView(LbbEdGraphNodeUtils::ResultOutputPins),
		false,
		false,
		false,
		true,
		false,
		ELbbGraphKindFlags::None,
		ELbbGraphKindFlags::All,
		&ULbbEdGraphNode_MaskedBlend::CompileNode);
}

void ULbbEdGraphNode_MaskedBlend::AllocatePosePins()
{
	CreatePoseInputPin(LbbEdGraphNodeUtils::PinBase);
	CreatePoseInputPin(LbbEdGraphNodeUtils::PinBlend);
	CreatePoseOutputPin(LbbEdGraphNodeUtils::PinResult);
}

FText ULbbEdGraphNode_MaskedBlend::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Masked Blend\n%s | Branches: %d | Weight: %s"),
		*LbbEdGraphNodeUtils::FormatBoneSpaceSummary(BlendSpace),
		BoneFilter.BranchFilters.Num(),
		*LbbEdGraphNodeUtils::FormatBlendWeightSummary(Weight)));
}

FLinearColor ULbbEdGraphNode_MaskedBlend::GetNodeColor() const
{
	return FLinearColor(0.16f, 0.52f, 0.36f);
}

void ULbbEdGraphNode_MaskedBlend::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbGraphNodeData_MaskedBlend* Data = NodeData.GetPtr<FLbbGraphNodeData_MaskedBlend>())
	{
		BlendSpace = Data->BlendSpace;
		BoneFilter = Data->BoneFilter;
		Weight = Data->Weight;
	}
}

void ULbbEdGraphNode_MaskedBlend::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbGraphNodeData_MaskedBlend>();
	FLbbGraphNodeData_MaskedBlend& Data = OutNodeData.GetMutable<FLbbGraphNodeData_MaskedBlend>();
	Data.BlendSpace = BlendSpace;
	Data.BoneFilter = BoneFilter;
	Data.Weight = Weight;
}

bool ULbbEdGraphNode_MaskedBlend::CompileNode(
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
			TEXT("Masked Blend failed to resolve one of its inputs."))
		|| !LbbEdGraphNodeUtils::ResolveRequiredInputSource(
			CompileContext,
			NodeModel,
			LbbEdGraphNodeUtils::PinBlend,
			BlendSource,
			OutMessages,
			TEXT("Masked Blend failed to resolve one of its inputs.")))
	{
		return false;
	}

	const FLbbGraphNodeData_MaskedBlend* NodeData = NodeModel.NodeData.GetPtr<FLbbGraphNodeData_MaskedBlend>();
	if (NodeData == nullptr)
	{
		LbbEdGraphNodeUtils::AddMessage(
			OutMessages,
			EMessageSeverity::Error,
			CompileContext.GetGraphKind(),
			CompileContext.GetBodyPartIndex(),
			NodeModel.NodeGuid,
			TEXT("Masked Blend node data is missing."));
		return false;
	}

	if (NodeData->BoneFilter.BranchFilters.IsEmpty())
	{
		LbbEdGraphNodeUtils::AddMessage(
			OutMessages,
			EMessageSeverity::Error,
			CompileContext.GetGraphKind(),
			CompileContext.GetBodyPartIndex(),
			NodeModel.NodeGuid,
			TEXT("Masked Blend requires a non-empty BoneFilter."));
		return false;
	}

	if (!LbbEdGraphNodeUtils::ValidateCurveWeight(
			NodeData->Weight,
			CompileContext,
			NodeModel,
			TEXT("Masked Blend uses a curve weight but BlendCurveName is None."),
			OutMessages))
	{
		return false;
	}

	FLbbPartOperator_MaskedBlend Operator;
	Operator.BasePose = BaseSource;
	Operator.BlendPose = BlendSource;
	Operator.BlendSpace = NodeData->BlendSpace;
	Operator.BoneFilter = NodeData->BoneFilter;
	Operator.Weight = NodeData->Weight;
	Operator.Output = TargetPose;
	LbbEdGraphNodeUtils::AddOperatorStruct(OutOperators, Operator);
	return true;
}
