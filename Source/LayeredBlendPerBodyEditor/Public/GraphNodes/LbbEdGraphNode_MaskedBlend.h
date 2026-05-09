// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "LbbEdGraphNode.h"
#include "LbbEdGraphNode_MaskedBlend.generated.h"

class FLbbGraphNodeCompileContext;
struct FLbbCompileMessage;
struct FLbbGraphNodeDescriptor;
struct FLbbGraphNodeModel;
struct FLbbLayeredBodyPartPoseTarget;

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbEdGraphNode_MaskedBlend : public ULbbEdGraphNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Node")
	ELbbBoneSpace BlendSpace = ELbbBoneSpace::LocalSpace;

	UPROPERTY(EditAnywhere, Category = "Node")
	FInputBlendPose BoneFilter;

	UPROPERTY(EditAnywhere, Category = "Node")
	FLbbBlendWeight Weight;

	virtual const UScriptStruct* GetNodeDataStruct() const override { return FLbbGraphNodeData_MaskedBlend::StaticStruct(); }
	virtual FLbbGraphNodeDescriptor BuildGraphNodeDescriptor() const override;

protected:
	virtual void AllocatePosePins() override;
	virtual FText GetNodeTitleText() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual void ImportNodeData(const FInstancedStruct& NodeData) override;
	virtual void ExportNodeData(FInstancedStruct& OutNodeData) const override;

private:
	static bool CompileNode(
		const FLbbGraphNodeCompileContext& CompileContext,
		const FLbbGraphNodeModel& NodeModel,
		const FLbbLayeredBodyPartPoseTarget& TargetPose,
		TArray<FInstancedStruct>& OutOperators,
		TArray<FLbbCompileMessage>& OutMessages);
};
