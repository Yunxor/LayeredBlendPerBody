// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "LbbLayeredBlendBodyEdGraphNode.h"
#include "LbbLayeredBlendBodyEdGraphNode_Result.generated.h"

struct FLbbLayeredBlendBodyGraphNodeDescriptor;

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbLayeredBlendBodyEdGraphNode_Result : public ULbbLayeredBlendBodyEdGraphNode
{
	GENERATED_BODY()

public:
	virtual const UScriptStruct* GetNodeDataStruct() const override { return FLbbLayeredBlendBodyGraphNodeData_Result::StaticStruct(); }
	virtual FLbbLayeredBlendBodyGraphNodeDescriptor BuildGraphNodeDescriptor() const override;
	virtual bool IsFixedNode() const override { return true; }

protected:
	virtual void AllocatePosePins() override;
	virtual FText GetNodeTitleText() const override;
	virtual FLinearColor GetNodeColor() const override;

};
