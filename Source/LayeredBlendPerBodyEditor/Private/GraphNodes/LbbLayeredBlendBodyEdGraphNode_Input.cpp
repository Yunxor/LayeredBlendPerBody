// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbLayeredBlendBodyEdGraphNode_Input.h"

#include "LbbLayeredBlendBodyDefinition.h"
#include "LbbLayeredBlendBodyEdGraph.h"
#include "GraphNodes/LbbLayeredBlendBodyEdGraphNodeUtils.h"
#include "LbbLayeredBlendBodyGraphNodeRegistry.h"

FLbbLayeredBlendBodyGraphNodeDescriptor ULbbLayeredBlendBodyEdGraphNode_Input::BuildGraphNodeDescriptor() const
{
	return LbbLayeredBlendBodyEdGraphNodeUtils::MakeNodeDescriptor(
		FLbbLayeredBlendBodyGraphNodeData_Input::StaticStruct(),
		ULbbLayeredBlendBodyEdGraphNode_Input::StaticClass(),
		TEXT("Input"),
		TConstArrayView<FName>(),
		MakeArrayView(LbbLayeredBlendBodyEdGraphNodeUtils::PoseOutputPins),
		true,
		false,
		false,
		true,
		false,
		ELbbLayeredBlendBodyGraphKindFlags::None,
		ELbbLayeredBlendBodyGraphKindFlags::All,
		nullptr);
}

void ULbbLayeredBlendBodyEdGraphNode_Input::AllocatePosePins()
{
	CreatePoseOutputPin(LbbLayeredBlendBodyEdGraphNodeUtils::PinPose);
}

FLbbLayeredBodyPartPoseSource ULbbLayeredBlendBodyEdGraphNode_Input::GetSourcePose() const
{
	FLbbLayeredBodyPartPoseSource SourcePose;
	SourcePose.Type = SourceType;
	SourcePose.CachePoseName = CachePoseName;
	SourcePose.InputPoseName = InputPoseName;
	return SourcePose;
}

void ULbbLayeredBlendBodyEdGraphNode_Input::SetSourcePose(const FLbbLayeredBodyPartPoseSource& InSourcePose)
{
	SourceType = InSourcePose.Type;
	CachePoseName = InSourcePose.CachePoseName;
	InputPoseName = InSourcePose.InputPoseName;
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
		return InSourceType == ELbbLayeredBodyPartPoseSourceType::BasePose
			|| InSourceType == ELbbLayeredBodyPartPoseSourceType::InputPose
			|| InSourceType == ELbbLayeredBodyPartPoseSourceType::CurrentPose;
	}

	return InSourceType == ELbbLayeredBodyPartPoseSourceType::BasePose
		|| InSourceType == ELbbLayeredBodyPartPoseSourceType::InputPose
		|| InSourceType == ELbbLayeredBodyPartPoseSourceType::CachePose
		|| InSourceType == ELbbLayeredBodyPartPoseSourceType::CurrentPose;
}

TArray<ELbbLayeredBodyPartPoseSourceType> ULbbLayeredBlendBodyEdGraphNode_Input::GetAvailableSourceTypes() const
{
	TArray<ELbbLayeredBodyPartPoseSourceType> Result;
	static const ELbbLayeredBodyPartPoseSourceType SourceTypeOptions[] = {
		ELbbLayeredBodyPartPoseSourceType::BasePose,
		ELbbLayeredBodyPartPoseSourceType::CachePose,
		ELbbLayeredBodyPartPoseSourceType::InputPose,
	};

	for (const ELbbLayeredBodyPartPoseSourceType SourceTypeOption : SourceTypeOptions)
	{
		if (IsSourceTypeAllowedInCurrentGraph(SourceTypeOption))
		{
			Result.Add(SourceTypeOption);
		}
	}

	return Result;
}

bool ULbbLayeredBlendBodyEdGraphNode_Input::IsAvailableInputPose(FName InInputPoseName) const
{
	if (InInputPoseName.IsNone())
	{
		return false;
	}

	for (const FString& Option : GetAvailableInputPoseOptions())
	{
		if (Option == InInputPoseName.ToString())
		{
			return true;
		}
	}

	return false;
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
		SourceType = ELbbLayeredBodyPartPoseSourceType::BasePose;
	}

	if (SourceType != ELbbLayeredBodyPartPoseSourceType::InputPose)
	{
		InputPoseName = NAME_None;
	}
	else if (!IsAvailableInputPose(InputPoseName))
	{
		const TArray<FString> AvailableOptions = GetAvailableInputPoseOptions();
		InputPoseName = AvailableOptions.IsEmpty() ? NAME_None : FName(*AvailableOptions[0]);
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

TArray<FString> ULbbLayeredBlendBodyEdGraphNode_Input::GetAvailableInputPoseOptions() const
{
	TArray<FString> Result;

	const ULbbLayeredBlendBodyDefinition* Definition = GetOwningDefinition();
	if (Definition == nullptr)
	{
		return Result;
	}

	TSet<FName> UniqueNames;
	for (const FLbbLayeredBlendBodyInputDefinition& InputDefinition : Definition->InputDefinitions)
	{
		if (InputDefinition.InputName.IsNone() || UniqueNames.Contains(InputDefinition.InputName))
		{
			continue;
		}

		UniqueNames.Add(InputDefinition.InputName);
		Result.Add(InputDefinition.InputName.ToString());
	}

	return Result;
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
		*LbbLayeredBlendBodyEdGraphNodeUtils::FormatPoseSourceSummary(GetSourcePose())));
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
	if (InProperty != nullptr)
	{
		if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULbbLayeredBlendBodyEdGraphNode_Input, InputPoseName))
		{
			return SourceType == ELbbLayeredBodyPartPoseSourceType::InputPose;
		}

		if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULbbLayeredBlendBodyEdGraphNode_Input, CachePoseName))
		{
			const ULbbLayeredBlendBodyEdGraph* OwningGraph = GetOwningLayeredBlendGraph();
			return SourceType == ELbbLayeredBodyPartPoseSourceType::CachePose
				&& OwningGraph != nullptr
				&& OwningGraph->GraphKind == ELbbLayeredBlendBodyGraphKind::BodyPart;
		}
	}

	return Super::CanEditChange(InProperty);
}

void ULbbLayeredBlendBodyEdGraphNode_Input::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	SanitizeSourceSelection();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
