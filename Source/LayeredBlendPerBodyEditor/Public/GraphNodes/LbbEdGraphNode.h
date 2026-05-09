// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "LbbEditorModel.h"
#include "LbbEdGraphNode.generated.h"

struct FLbbGraphNodeDescriptor;

UCLASS(Abstract)
class LAYEREDBLENDPERBODYEDITOR_API ULbbEdGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	static const FName PosePinCategory;

	virtual const UScriptStruct* GetNodeDataStruct() const PURE_VIRTUAL(ULbbEdGraphNode::GetNodeDataStruct, return nullptr;);
	virtual FLbbGraphNodeDescriptor BuildGraphNodeDescriptor() const;
	virtual bool IsFixedNode() const { return false; }

	virtual void AllocateDefaultPins() override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual bool CanUserDeleteNode() const override;
	virtual bool CanDuplicateNode() const override;

	virtual void ImportFromNodeModel(const FLbbGraphNodeModel& NodeModel);
	virtual void ExportToNodeModel(FLbbGraphNodeModel& OutNodeModel) const;

protected:
	virtual void AllocatePosePins() PURE_VIRTUAL(ULbbEdGraphNode::AllocatePosePins, );
	virtual FText GetNodeTitleText() const PURE_VIRTUAL(ULbbEdGraphNode::GetNodeTitleText, return FText::GetEmpty(););
	virtual FLinearColor GetNodeColor() const;
	virtual void ImportNodeData(const FInstancedStruct& NodeData);
	virtual void ExportNodeData(FInstancedStruct& OutNodeData) const;

	UEdGraphPin* CreatePoseInputPin(const FName& PinName) const;
	UEdGraphPin* CreatePoseOutputPin(const FName& PinName) const;

	const class ULbbEdGraph* GetOwningLayeredBlendGraph() const;
	const class ULbbBodyPartDefinitionDataAsset* GetOwningDefinition() const;
};
