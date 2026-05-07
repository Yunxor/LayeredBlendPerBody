// Fill out your copyright notice in the Description page of Project Settings.

#include "LbbLayeredBlendBodyEdGraphNode.h"

#include "LbbLayeredBlendBodyDefinition.h"
#include "LbbLayeredBlendBodyEdGraph.h"
#include "LbbLayeredBlendBodyGraphSchema.h"

#define LOCTEXT_NAMESPACE "LbbLayeredBlendBodyEdGraphNode"

const FName ULbbLayeredBlendBodyEdGraphNode::PosePinCategory(TEXT("Pose"));

namespace
{
	static FString FormatBlendWeightSummary(const FLbbBlendWeight& Weight)
	{
		if (Weight.bUseBlendCurve)
		{
			const FString CurveName = Weight.BlendCurveName.IsNone()
				? TEXT("<None>")
				: Weight.BlendCurveName.ToString();
			return FString::Printf(TEXT("Curve(%s)"), *CurveName);
		}

		return FString::Printf(TEXT("%.2f"), Weight.BlendWeight);
	}

	static FString FormatBoneSpaceSummary(const ELbbBoneSpace BoneSpace)
	{
		return BoneSpace == ELbbBoneSpace::MeshSpace
			? TEXT("Mesh Space")
			: TEXT("Local Space");
	}

	static FString FormatPoseSourceSummary(const FLbbLayeredBodyPartPoseSource& SourcePose)
	{
		switch (SourcePose.Type)
		{
		case ELbbLayeredBodyPartPoseSourceType::Motion:
			return TEXT("Motion");
		case ELbbLayeredBodyPartPoseSourceType::BasePose:
			return TEXT("Base Pose");
		case ELbbLayeredBodyPartPoseSourceType::OverlayPose:
			return TEXT("Overlay Pose");
		case ELbbLayeredBodyPartPoseSourceType::CurrentPose:
			return TEXT("Current Pose");
		case ELbbLayeredBodyPartPoseSourceType::CachePose:
			return FString::Printf(
				TEXT("Cache(%s)"),
				SourcePose.CachePoseName.IsNone() ? TEXT("<None>") : *SourcePose.CachePoseName.ToString());
		default:
			return TEXT("Unknown");
		}
	}
}

const ULbbLayeredBlendBodyEdGraph* ULbbLayeredBlendBodyEdGraphNode::GetOwningLayeredBlendGraph() const
{
	return Cast<ULbbLayeredBlendBodyEdGraph>(GetGraph());
}

const ULbbLayeredBlendBodyDefinition* ULbbLayeredBlendBodyEdGraphNode::GetOwningDefinition() const
{
	const ULbbLayeredBlendBodyEdGraph* OwningGraph = GetOwningLayeredBlendGraph();
	return OwningGraph != nullptr ? OwningGraph->EditingDefinition.Get() : nullptr;
}

void ULbbLayeredBlendBodyEdGraphNode::AllocateDefaultPins()
{
	AllocatePosePins();
}

void ULbbLayeredBlendBodyEdGraphNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	if (FromPin == nullptr)
	{
		return;
	}

	const UEdGraphSchema* Schema = GetSchema();
	if (Schema == nullptr)
	{
		return;
	}

	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin == nullptr || Pin->Direction == FromPin->Direction)
		{
			continue;
		}

		if (Schema->TryCreateConnection(FromPin, Pin))
		{
			NodeConnectionListChanged();
			return;
		}
	}
}

FText ULbbLayeredBlendBodyEdGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetNodeTitleText();
}

FText ULbbLayeredBlendBodyEdGraphNode::GetTooltipText() const
{
	return GetNodeTitleText();
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode::GetNodeTitleColor() const
{
	return GetNodeColor();
}

bool ULbbLayeredBlendBodyEdGraphNode::CanUserDeleteNode() const
{
	return !IsFixedNode();
}

bool ULbbLayeredBlendBodyEdGraphNode::CanDuplicateNode() const
{
	return !IsFixedNode();
}

void ULbbLayeredBlendBodyEdGraphNode::ImportFromNodeModel(const FLbbLayeredBlendBodyGraphNodeModel& NodeModel)
{
	NodeGuid = NodeModel.NodeGuid;
	NodePosX = FMath::RoundToInt(NodeModel.NodePosition.X);
	NodePosY = FMath::RoundToInt(NodeModel.NodePosition.Y);
	ImportNodeData(NodeModel.NodeData);
}

void ULbbLayeredBlendBodyEdGraphNode::ExportToNodeModel(FLbbLayeredBlendBodyGraphNodeModel& OutNodeModel) const
{
	OutNodeModel.NodeGuid = NodeGuid;
	OutNodeModel.NodePosition = FVector2D(NodePosX, NodePosY);
	ExportNodeData(OutNodeModel.NodeData);
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode::GetNodeColor() const
{
	return FLinearColor(0.18f, 0.18f, 0.18f);
}

void ULbbLayeredBlendBodyEdGraphNode::ImportNodeData(const FInstancedStruct& NodeData)
{
}

void ULbbLayeredBlendBodyEdGraphNode::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	if (const UScriptStruct* NodeDataStruct = GetNodeDataStruct())
	{
		OutNodeData.InitializeAs(NodeDataStruct);
		return;
	}

	OutNodeData.Reset();
}

UEdGraphPin* ULbbLayeredBlendBodyEdGraphNode::CreatePoseInputPin(const FName& PinName) const
{
	return const_cast<ULbbLayeredBlendBodyEdGraphNode*>(this)->CreatePin(EGPD_Input, ULbbLayeredBlendBodyGraphSchema::PC_Pose, PinName);
}

UEdGraphPin* ULbbLayeredBlendBodyEdGraphNode::CreatePoseOutputPin(const FName& PinName) const
{
	return const_cast<ULbbLayeredBlendBodyEdGraphNode*>(this)->CreatePin(EGPD_Output, ULbbLayeredBlendBodyGraphSchema::PC_Pose, PinName);
}

void ULbbLayeredBlendBodyEdGraphNode_Input::AllocatePosePins()
{
	CreatePoseOutputPin(TEXT("Pose"));
}

FLbbLayeredBodyPartPoseSource ULbbLayeredBlendBodyEdGraphNode_Input::GetSourcePose() const
{
	FLbbLayeredBodyPartPoseSource SourcePose;
	SourcePose.Type = SourceType;
	SourcePose.CachePoseName = CachePoseName;
	return SourcePose;
}

void ULbbLayeredBlendBodyEdGraphNode_Input::SetSourcePose(const FLbbLayeredBodyPartPoseSource& InSourcePose)
{
	SourceType = InSourcePose.Type;
	CachePoseName = InSourcePose.CachePoseName;
	SanitizeSourceSelection();
}

bool ULbbLayeredBlendBodyEdGraphNode_Input::IsSourceTypeAllowedInCurrentGraph(const ELbbLayeredBodyPartPoseSourceType InSourceType) const
{
	const ULbbLayeredBlendBodyEdGraph* OwningGraph = GetOwningLayeredBlendGraph();
	if (OwningGraph == nullptr)
	{
		return true;
	}

	if (OwningGraph->GraphKind == ELbbLayeredBlendBodyGraphKind::Cache)
	{
		return InSourceType == ELbbLayeredBodyPartPoseSourceType::Motion
			|| InSourceType == ELbbLayeredBodyPartPoseSourceType::BasePose
			|| InSourceType == ELbbLayeredBodyPartPoseSourceType::OverlayPose;
	}

	return true;
}

bool ULbbLayeredBlendBodyEdGraphNode_Input::IsAvailableNamedCache(const FName InCachePoseName) const
{
	if (InCachePoseName.IsNone())
	{
		return false;
	}

	for (const FString& Option : GetAvailableCachePoseOptions())
	{
		if (Option == InCachePoseName.ToString())
		{
			return true;
		}
	}

	return false;
}

void ULbbLayeredBlendBodyEdGraphNode_Input::SanitizeSourceSelection()
{
	if (!IsSourceTypeAllowedInCurrentGraph(SourceType))
	{
		SourceType = ELbbLayeredBodyPartPoseSourceType::Motion;
	}

	if (SourceType != ELbbLayeredBodyPartPoseSourceType::CachePose)
	{
		CachePoseName = NAME_None;
		return;
	}

	if (!IsAvailableNamedCache(CachePoseName))
	{
		const TArray<FString> AvailableOptions = GetAvailableCachePoseOptions();
		CachePoseName = AvailableOptions.IsEmpty() ? NAME_None : FName(*AvailableOptions[0]);
	}
}

TArray<FString> ULbbLayeredBlendBodyEdGraphNode_Input::GetAvailableCachePoseOptions() const
{
	TArray<FString> Result;

	const ULbbLayeredBlendBodyEdGraph* OwningGraph = GetOwningLayeredBlendGraph();
	const ULbbLayeredBlendBodyDefinition* Definition = GetOwningDefinition();
	if (OwningGraph == nullptr || Definition == nullptr || OwningGraph->GraphKind != ELbbLayeredBlendBodyGraphKind::BodyPart)
	{
		return Result;
	}

	TSet<FName> UniqueNames;
	for (const FLbbLayeredBlendBodyGraphNodeModel& NodeModel : Definition->EditorModel.CacheGraph.Nodes)
	{
		const FLbbLayeredBlendBodyGraphNodeData_SavePose* SavePoseData = NodeModel.NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_SavePose>();
		if (SavePoseData == nullptr || SavePoseData->CachePoseName.IsNone() || UniqueNames.Contains(SavePoseData->CachePoseName))
		{
			continue;
		}

		UniqueNames.Add(SavePoseData->CachePoseName);
		Result.Add(SavePoseData->CachePoseName.ToString());
	}

	return Result;
}

FText ULbbLayeredBlendBodyEdGraphNode_Input::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Input\n%s"),
		*FormatPoseSourceSummary(GetSourcePose())));
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode_Input::GetNodeColor() const
{
	return FLinearColor(0.08f, 0.24f, 0.38f);
}

void ULbbLayeredBlendBodyEdGraphNode_Input::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbLayeredBlendBodyGraphNodeData_Input* Data = NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_Input>())
	{
		SetSourcePose(Data->SourcePose);
	}
}

void ULbbLayeredBlendBodyEdGraphNode_Input::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbLayeredBlendBodyGraphNodeData_Input>();
	FLbbLayeredBlendBodyGraphNodeData_Input& Data = OutNodeData.GetMutable<FLbbLayeredBlendBodyGraphNodeData_Input>();
	Data.SourcePose = GetSourcePose();
}

#if WITH_EDITOR
bool ULbbLayeredBlendBodyEdGraphNode_Input::CanEditChange(const FProperty* InProperty) const
{
	if (InProperty != nullptr && InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULbbLayeredBlendBodyEdGraphNode_Input, CachePoseName))
	{
		const ULbbLayeredBlendBodyEdGraph* OwningGraph = GetOwningLayeredBlendGraph();
		return SourceType == ELbbLayeredBodyPartPoseSourceType::CachePose
			&& OwningGraph != nullptr
			&& OwningGraph->GraphKind == ELbbLayeredBlendBodyGraphKind::BodyPart;
	}

	return Super::CanEditChange(InProperty);
}

void ULbbLayeredBlendBodyEdGraphNode_Input::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	SanitizeSourceSelection();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void ULbbLayeredBlendBodyEdGraphNode_Result::AllocatePosePins()
{
	CreatePoseInputPin(TEXT("Pose"));
}

FText ULbbLayeredBlendBodyEdGraphNode_Result::GetNodeTitleText() const
{
	return LOCTEXT("ResultTitle", "Result");
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode_Result::GetNodeColor() const
{
	return FLinearColor(0.18f, 0.34f, 0.14f);
}

void ULbbLayeredBlendBodyEdGraphNode_ApplySlot::AllocatePosePins()
{
	CreatePoseInputPin(TEXT("Input"));
	CreatePoseOutputPin(TEXT("Result"));
}

FText ULbbLayeredBlendBodyEdGraphNode_ApplySlot::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Apply Slot\nSlot: %s"),
		SlotName.IsNone() ? TEXT("<None>") : *SlotName.ToString()));
}

FLinearColor ULbbLayeredBlendBodyEdGraphNode_ApplySlot::GetNodeColor() const
{
	return FLinearColor(0.73f, 0.46f, 0.17f);
}

void ULbbLayeredBlendBodyEdGraphNode_ApplySlot::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbLayeredBlendBodyGraphNodeData_ApplySlot* Data = NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_ApplySlot>())
	{
		SlotName = Data->SlotName;
	}
}

void ULbbLayeredBlendBodyEdGraphNode_ApplySlot::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbLayeredBlendBodyGraphNodeData_ApplySlot>();
	FLbbLayeredBlendBodyGraphNodeData_ApplySlot& Data = OutNodeData.GetMutable<FLbbLayeredBlendBodyGraphNodeData_ApplySlot>();
	Data.SlotName = SlotName;
}

void ULbbLayeredBlendBodyEdGraphNode_Blend::AllocatePosePins()
{
	CreatePoseInputPin(TEXT("Base"));
	CreatePoseInputPin(TEXT("Blend"));
	CreatePoseOutputPin(TEXT("Result"));
}

FText ULbbLayeredBlendBodyEdGraphNode_Blend::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Blend\nWeight: %s"),
		*FormatBlendWeightSummary(Weight)));
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

void ULbbLayeredBlendBodyEdGraphNode_MaskedBlend::AllocatePosePins()
{
	CreatePoseInputPin(TEXT("Base"));
	CreatePoseInputPin(TEXT("Blend"));
	CreatePoseOutputPin(TEXT("Result"));
}

FText ULbbLayeredBlendBodyEdGraphNode_MaskedBlend::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Masked Blend\n%s | Branches: %d | Weight: %s"),
		*FormatBoneSpaceSummary(BlendSpace),
		BoneFilter.BranchFilters.Num(),
		*FormatBlendWeightSummary(Weight)));
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

void ULbbLayeredBlendBodyEdGraphNode_SavePose::AllocatePosePins()
{
	CreatePoseInputPin(TEXT("Input"));
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

void ULbbLayeredBlendBodyEdGraphNode_MakeAdditive::AllocatePosePins()
{
	CreatePoseInputPin(TEXT("Base"));
	CreatePoseInputPin(TEXT("Additive"));
	CreatePoseOutputPin(TEXT("Result"));
}

FText ULbbLayeredBlendBodyEdGraphNode_MakeAdditive::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Make Additive\n%s"),
		*FormatBoneSpaceSummary(AdditiveSpace)));
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

void ULbbLayeredBlendBodyEdGraphNode_ApplyAdditive::AllocatePosePins()
{
	CreatePoseInputPin(TEXT("Base"));
	CreatePoseInputPin(TEXT("Additive"));
	CreatePoseOutputPin(TEXT("Result"));
}

FText ULbbLayeredBlendBodyEdGraphNode_ApplyAdditive::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Apply Additive\n%s | Weight: %s"),
		*FormatBoneSpaceSummary(AdditiveSpace),
		*FormatBlendWeightSummary(Weight)));
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

#undef LOCTEXT_NAMESPACE
