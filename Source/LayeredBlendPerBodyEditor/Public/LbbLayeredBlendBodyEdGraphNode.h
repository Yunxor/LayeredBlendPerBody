// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "LbbLayeredBlendBodyEditorModel.h"
#include "LbbLayeredBlendBodyDefinition.h"
#include "LbbLayeredBlendBodyEdGraphNode.generated.h"

UCLASS(Abstract)
class LAYEREDBLENDPERBODYEDITOR_API ULbbLayeredBlendBodyEdGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	static const FName PosePinCategory;

	virtual ELbbLayeredBlendBodyGraphNodeType GetGraphNodeType() const PURE_VIRTUAL(ULbbLayeredBlendBodyEdGraphNode::GetGraphNodeType, return ELbbLayeredBlendBodyGraphNodeType::CurrentPose;);
	virtual bool IsFixedNode() const { return false; }

	virtual void AllocateDefaultPins() override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual bool CanUserDeleteNode() const override;
	virtual bool CanDuplicateNode() const override;

	virtual void ImportFromNodeModel(const FLbbLayeredBlendBodyGraphNodeModel& NodeModel);
	virtual void ExportToNodeModel(FLbbLayeredBlendBodyGraphNodeModel& OutNodeModel) const;

protected:
	virtual void AllocatePosePins() PURE_VIRTUAL(ULbbLayeredBlendBodyEdGraphNode::AllocatePosePins, );
	virtual FText GetNodeTitleText() const PURE_VIRTUAL(ULbbLayeredBlendBodyEdGraphNode::GetNodeTitleText, return FText::GetEmpty(););
	virtual FLinearColor GetNodeColor() const;
	virtual void ImportNodeData(const FInstancedStruct& NodeData);
	virtual void ExportNodeData(FInstancedStruct& OutNodeData) const;

	UEdGraphPin* CreatePoseInputPin(const FName& PinName) const;
	UEdGraphPin* CreatePoseOutputPin(const FName& PinName) const;
};

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbLayeredBlendBodyEdGraphNode_FixedPose : public ULbbLayeredBlendBodyEdGraphNode
{
	GENERATED_BODY()

public:
	UPROPERTY()
	ELbbLayeredBlendBodyGraphNodeType FixedNodeType = ELbbLayeredBlendBodyGraphNodeType::CurrentPose;

	virtual ELbbLayeredBlendBodyGraphNodeType GetGraphNodeType() const override { return FixedNodeType; }
	virtual bool IsFixedNode() const override { return true; }

protected:
	virtual void AllocatePosePins() override;
	virtual FText GetNodeTitleText() const override;
	virtual FLinearColor GetNodeColor() const override;
};

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbLayeredBlendBodyEdGraphNode_Result : public ULbbLayeredBlendBodyEdGraphNode
{
	GENERATED_BODY()

public:
	virtual ELbbLayeredBlendBodyGraphNodeType GetGraphNodeType() const override { return ELbbLayeredBlendBodyGraphNodeType::Result; }
	virtual bool IsFixedNode() const override { return true; }

protected:
	virtual void AllocatePosePins() override;
	virtual FText GetNodeTitleText() const override;
	virtual FLinearColor GetNodeColor() const override;
};

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbLayeredBlendBodyEdGraphNode_ApplySlot : public ULbbLayeredBlendBodyEdGraphNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Node")
	FName SlotName = NAME_None;

	virtual ELbbLayeredBlendBodyGraphNodeType GetGraphNodeType() const override { return ELbbLayeredBlendBodyGraphNodeType::ApplySlot; }

protected:
	virtual void AllocatePosePins() override;
	virtual FText GetNodeTitleText() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual void ImportNodeData(const FInstancedStruct& NodeData) override;
	virtual void ExportNodeData(FInstancedStruct& OutNodeData) const override;
};

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbLayeredBlendBodyEdGraphNode_Blend : public ULbbLayeredBlendBodyEdGraphNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Node")
	FLbbBlendWeight Weight;

	virtual ELbbLayeredBlendBodyGraphNodeType GetGraphNodeType() const override { return ELbbLayeredBlendBodyGraphNodeType::Blend; }

protected:
	virtual void AllocatePosePins() override;
	virtual FText GetNodeTitleText() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual void ImportNodeData(const FInstancedStruct& NodeData) override;
	virtual void ExportNodeData(FInstancedStruct& OutNodeData) const override;
};

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbLayeredBlendBodyEdGraphNode_MaskedBlend : public ULbbLayeredBlendBodyEdGraphNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Node")
	ELbbBoneSpace BlendSpace = ELbbBoneSpace::LocalSpace;

	UPROPERTY(EditAnywhere, Category = "Node")
	FInputBlendPose BoneFilter;

	UPROPERTY(EditAnywhere, Category = "Node")
	FLbbBlendWeight Weight;

	virtual ELbbLayeredBlendBodyGraphNodeType GetGraphNodeType() const override { return ELbbLayeredBlendBodyGraphNodeType::MaskedBlend; }

protected:
	virtual void AllocatePosePins() override;
	virtual FText GetNodeTitleText() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual void ImportNodeData(const FInstancedStruct& NodeData) override;
	virtual void ExportNodeData(FInstancedStruct& OutNodeData) const override;
};

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbLayeredBlendBodyEdGraphNode_ApplyMotionDelta : public ULbbLayeredBlendBodyEdGraphNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Node")
	ELbbBoneSpace AdditiveSpace = ELbbBoneSpace::LocalSpace;

	UPROPERTY(EditAnywhere, Category = "Node")
	FLbbBlendWeight Weight;

	virtual ELbbLayeredBlendBodyGraphNodeType GetGraphNodeType() const override { return ELbbLayeredBlendBodyGraphNodeType::ApplyMotionDelta; }

protected:
	virtual void AllocatePosePins() override;
	virtual FText GetNodeTitleText() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual void ImportNodeData(const FInstancedStruct& NodeData) override;
	virtual void ExportNodeData(FInstancedStruct& OutNodeData) const override;
};
