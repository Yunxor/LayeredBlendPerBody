// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "LbbEdGraphNode.h"
#include "LbbEdGraphNode_Result.generated.h"

struct FLbbGraphNodeDescriptor;

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbEdGraphNode_Result : public ULbbEdGraphNode
{
	GENERATED_BODY()

public:
	virtual const UScriptStruct* GetNodeDataStruct() const override { return FLbbGraphNodeData_Result::StaticStruct(); }
	virtual FLbbGraphNodeDescriptor BuildGraphNodeDescriptor() const override;
	virtual bool IsFixedNode() const override { return true; }

protected:
	virtual void AllocatePosePins() override;
	virtual FText GetNodeTitleText() const override;
	virtual FLinearColor GetNodeColor() const override;

};
