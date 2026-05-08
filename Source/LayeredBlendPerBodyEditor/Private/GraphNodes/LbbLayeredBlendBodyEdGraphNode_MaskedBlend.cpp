// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbLayeredBlendBodyEdGraphNode_MaskedBlend.h"

#include "GraphNodes/LbbLayeredBlendBodyEdGraphNodeUtils.h"
#include "LbbLayeredBlendBodyGraphNodeRegistry.h"

FLbbLayeredBlendBodyGraphNodeDescriptor ULbbLayeredBlendBodyEdGraphNode_MaskedBlend::BuildGraphNodeDescriptor() const
{
	return LbbLayeredBlendBodyEdGraphNodeUtils::MakeNodeDescriptor(
		FLbbLayeredBlendBodyGraphNodeData_MaskedBlend::StaticStruct(),
		ULbbLayeredBlendBodyEdGraphNode_MaskedBlend::StaticClass(),
		TEXT("Masked Blend"),
		MakeArrayView(LbbLayeredBlendBodyEdGraphNodeUtils::BaseBlendInputPins),
		MakeArrayView(LbbLayeredBlendBodyEdGraphNodeUtils::ResultOutputPins),
		false,
		false,
		false,
		true,
		false,
		ELbbLayeredBlendBodyGraphKindFlags::None,
		ELbbLayeredBlendBodyGraphKindFlags::All,
		&ULbbLayeredBlendBodyEdGraphNode_MaskedBlend::CompileNode);
}

void ULbbLayeredBlendBodyEdGraphNode_MaskedBlend::AllocatePosePins()
{
	CreatePoseInputPin(LbbLayeredBlendBodyEdGraphNodeUtils::PinBase);
	CreatePoseInputPin(LbbLayeredBlendBodyEdGraphNodeUtils::PinBlend);
	CreatePoseOutputPin(LbbLayeredBlendBodyEdGraphNodeUtils::PinResult);
}

FText ULbbLayeredBlendBodyEdGraphNode_MaskedBlend::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Masked Blend\n%s | Branches: %d | Weight: %s"),
		*LbbLayeredBlendBodyEdGraphNodeUtils::FormatBoneSpaceSummary(BlendSpace),
		BoneFilter.BranchFilters.Num(),
		*LbbLayeredBlendBodyEdGraphNodeUtils::FormatBlendWeightSummary(Weight)));
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode_MaskedBlend::GetNodeColor() const
{
	return FLinearColor(0.16f, 0.52f, 0.36f);
}

void ULbbLayeredBlendBodyEdGraphNode_MaskedBlend::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbLayeredBlendBodyGraphNodeData_MaskedBlend* Data = NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_MaskedBlend>())
	{
		BlendSpace = Data->BlendSpace;
		BoneFilter = Data->BoneFilter;
		Weight = Data->Weight;
	}
}

void ULbbLayeredBlendBodyEdGraphNode_MaskedBlend::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbLayeredBlendBodyGraphNodeData_MaskedBlend>();
	FLbbLayeredBlendBodyGraphNodeData_MaskedBlend& Data = OutNodeData.GetMutable<FLbbLayeredBlendBodyGraphNodeData_MaskedBlend>();
	Data.BlendSpace = BlendSpace;
	Data.BoneFilter = BoneFilter;
	Data.Weight = Weight;
}

bool ULbbLayeredBlendBodyEdGraphNode_MaskedBlend::CompileNode(
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
			TEXT("Masked Blend failed to resolve one of its inputs."))
		|| !LbbLayeredBlendBodyEdGraphNodeUtils::ResolveRequiredInputSource(
			CompileContext,
			NodeModel,
			LbbLayeredBlendBodyEdGraphNodeUtils::PinBlend,
			BlendSource,
			OutMessages,
			TEXT("Masked Blend failed to resolve one of its inputs.")))
	{
		return false;
	}

	const FLbbLayeredBlendBodyGraphNodeData_MaskedBlend* NodeData = NodeModel.NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_MaskedBlend>();
	if (NodeData == nullptr)
	{
		LbbLayeredBlendBodyEdGraphNodeUtils::AddMessage(
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
		LbbLayeredBlendBodyEdGraphNodeUtils::AddMessage(
			OutMessages,
			EMessageSeverity::Error,
			CompileContext.GetGraphKind(),
			CompileContext.GetBodyPartIndex(),
			NodeModel.NodeGuid,
			TEXT("Masked Blend requires a non-empty BoneFilter."));
		return false;
	}

	if (!LbbLayeredBlendBodyEdGraphNodeUtils::ValidateCurveWeight(
			NodeData->Weight,
			CompileContext,
			NodeModel,
			TEXT("Masked Blend uses a curve weight but BlendCurveName is None."),
			OutMessages))
	{
		return false;
	}

	FLbbLayeredBlendBodyPartOperator_MaskedBlend Operator;
	Operator.BasePose = BaseSource;
	Operator.BlendPose = BlendSource;
	Operator.BlendSpace = NodeData->BlendSpace;
	Operator.BoneFilter = NodeData->BoneFilter;
	Operator.Weight = NodeData->Weight;
	Operator.Output = TargetPose;
	LbbLayeredBlendBodyEdGraphNodeUtils::AddOperatorStruct(OutOperators, Operator);
	return true;
}
