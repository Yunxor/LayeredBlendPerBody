// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "LbbLayeredBlendBodyEdGraphNode.h"
#include "LbbLayeredBlendBodyEdGraphNode_MakeAdditive.generated.h"

class FLbbLayeredBlendBodyGraphNodeCompileContext;
struct FLbbCompileMessage;
struct FLbbLayeredBlendBodyGraphNodeDescriptor;
struct FLbbLayeredBlendBodyGraphNodeModel;
struct FLbbLayeredBodyPartPoseTarget;

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbLayeredBlendBodyEdGraphNode_MakeAdditive : public ULbbLayeredBlendBodyEdGraphNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Node")
	ELbbBoneSpace AdditiveSpace = ELbbBoneSpace::LocalSpace;

	virtual const UScriptStruct* GetNodeDataStruct() const override { return FLbbLayeredBlendBodyGraphNodeData_MakeAdditive::StaticStruct(); }
	virtual FLbbLayeredBlendBodyGraphNodeDescriptor BuildGraphNodeDescriptor() const override;

protected:
	virtual void AllocatePosePins() override;
	virtual FText GetNodeTitleText() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual void ImportNodeData(const FInstancedStruct& NodeData) override;
	virtual void ExportNodeData(FInstancedStruct& OutNodeData) const override;

private:
	static bool CompileNode(
		const FLbbLayeredBlendBodyGraphNodeCompileContext& CompileContext,
		const FLbbLayeredBlendBodyGraphNodeModel& NodeModel,
		const FLbbLayeredBodyPartPoseTarget& TargetPose,
		TArray<FInstancedStruct>& OutOperators,
		TArray<FLbbCompileMessage>& OutMessages);
};
