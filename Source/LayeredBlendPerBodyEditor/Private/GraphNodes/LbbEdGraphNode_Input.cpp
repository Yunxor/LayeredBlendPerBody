// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphNodes/LbbEdGraphNode_Input.h"

#include "LbbBodyPartDefinitionDataAsset.h"
#include "LbbEdGraph.h"
#include "GraphNodes/LbbEdGraphNodeUtils.h"
#include "LbbGraphNodeRegistry.h"

FLbbGraphNodeDescriptor ULbbEdGraphNode_Input::BuildGraphNodeDescriptor() const
{
	return LbbEdGraphNodeUtils::MakeNodeDescriptor(
		FLbbGraphNodeData_Input::StaticStruct(),
		ULbbEdGraphNode_Input::StaticClass(),
		TEXT("Input"),
		TConstArrayView<FName>(),
		MakeArrayView(LbbEdGraphNodeUtils::PoseOutputPins),
		true,
		false,
		false,
		true,
		false,
		ELbbGraphKindFlags::None,
		ELbbGraphKindFlags::All,
		nullptr);
}

void ULbbEdGraphNode_Input::AllocatePosePins()
{
	CreatePoseOutputPin(LbbEdGraphNodeUtils::PinPose);
}

FLbbLayeredBodyPartPoseSource ULbbEdGraphNode_Input::GetSourcePose() const
{
	FLbbLayeredBodyPartPoseSource SourcePose;
	SourcePose.Type = SourceType;
	SourcePose.CachePoseName = CachePoseName;
	SourcePose.InputPoseName = InputPoseName;
	return SourcePose;
}

void ULbbEdGraphNode_Input::SetSourcePose(const FLbbLayeredBodyPartPoseSource& InSourcePose)
{
	SourceType = InSourcePose.Type;
	CachePoseName = InSourcePose.CachePoseName;
	InputPoseName = InSourcePose.InputPoseName;
	SanitizeSourceSelection();
}

bool ULbbEdGraphNode_Input::IsSourceTypeAllowedInCurrentGraph(const ELbbLayeredBodyPartPoseSourceType InSourceType) const
{
	const ULbbEdGraph* OwningGraph = GetOwningLayeredBlendGraph();
	if (OwningGraph == nullptr)
	{
		return true;
	}

	if (OwningGraph->GraphKind == ELbbGraphKind::Cache)
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

TArray<ELbbLayeredBodyPartPoseSourceType> ULbbEdGraphNode_Input::GetAvailableSourceTypes() const
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

bool ULbbEdGraphNode_Input::IsAvailableInputPose(FName InInputPoseName) const
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

bool ULbbEdGraphNode_Input::IsAvailableNamedCache(const FName InCachePoseName) const
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

void ULbbEdGraphNode_Input::SanitizeSourceSelection()
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

TArray<FString> ULbbEdGraphNode_Input::GetAvailableInputPoseOptions() const
{
	TArray<FString> Result;

	const ULbbBodyPartDefinitionDataAsset* Definition = GetOwningDefinition();
	if (Definition == nullptr)
	{
		return Result;
	}

	TSet<FName> UniqueNames;
	for (const FLbbInputPoseDefinition& InputDefinition : Definition->InputPoseDefinitions)
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

TArray<FString> ULbbEdGraphNode_Input::GetAvailableCachePoseOptions() const
{
	TArray<FString> Result;

	const ULbbEdGraph* OwningGraph = GetOwningLayeredBlendGraph();
	const ULbbBodyPartDefinitionDataAsset* Definition = GetOwningDefinition();
	if (OwningGraph == nullptr || Definition == nullptr || OwningGraph->GraphKind != ELbbGraphKind::BodyPart)
	{
		return Result;
	}

	TSet<FName> UniqueNames;
	for (const FLbbGraphNodeModel& NodeModel : Definition->EditorModel.CacheGraph.Nodes)
	{
		const FLbbGraphNodeData_SavePose* SavePoseData = NodeModel.NodeData.GetPtr<FLbbGraphNodeData_SavePose>();
		if (SavePoseData == nullptr || SavePoseData->CachePoseName.IsNone() || UniqueNames.Contains(SavePoseData->CachePoseName))
		{
			continue;
		}

		UniqueNames.Add(SavePoseData->CachePoseName);
		Result.Add(SavePoseData->CachePoseName.ToString());
	}

	return Result;
}

FText ULbbEdGraphNode_Input::GetNodeTitleText() const
{
	return FText::FromString(FString::Printf(
		TEXT("Input\n%s"),
		*LbbEdGraphNodeUtils::FormatPoseSourceSummary(GetSourcePose())));
}

FLinearColor ULbbEdGraphNode_Input::GetNodeColor() const
{
	return FLinearColor(0.08f, 0.24f, 0.38f);
}

void ULbbEdGraphNode_Input::ImportNodeData(const FInstancedStruct& NodeData)
{
	if (const FLbbGraphNodeData_Input* Data = NodeData.GetPtr<FLbbGraphNodeData_Input>())
	{
		SetSourcePose(Data->SourcePose);
	}
}

void ULbbEdGraphNode_Input::ExportNodeData(FInstancedStruct& OutNodeData) const
{
	OutNodeData.InitializeAs<FLbbGraphNodeData_Input>();
	FLbbGraphNodeData_Input& Data = OutNodeData.GetMutable<FLbbGraphNodeData_Input>();
	Data.SourcePose = GetSourcePose();
}

#if WITH_EDITOR
bool ULbbEdGraphNode_Input::CanEditChange(const FProperty* InProperty) const
{
	if (InProperty != nullptr)
	{
		if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULbbEdGraphNode_Input, InputPoseName))
		{
			return SourceType == ELbbLayeredBodyPartPoseSourceType::InputPose;
		}

		if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ULbbEdGraphNode_Input, CachePoseName))
		{
			const ULbbEdGraph* OwningGraph = GetOwningLayeredBlendGraph();
			return SourceType == ELbbLayeredBodyPartPoseSourceType::CachePose
				&& OwningGraph != nullptr
				&& OwningGraph->GraphKind == ELbbGraphKind::BodyPart;
		}
	}

	return Super::CanEditChange(InProperty);
}

void ULbbEdGraphNode_Input::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	SanitizeSourceSelection();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
