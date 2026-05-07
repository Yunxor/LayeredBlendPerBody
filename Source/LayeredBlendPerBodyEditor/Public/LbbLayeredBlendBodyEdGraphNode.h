// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "LbbLayeredBlendBodyEditorModel.h"
#include "LbbLayeredBlendBodyEdGraphNode.generated.h"

class FProperty;
struct FPropertyChangedEvent;

UCLASS(Abstract)
class LAYEREDBLENDPERBODYEDITOR_API ULbbLayeredBlendBodyEdGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	static const FName PosePinCategory;

	virtual const UScriptStruct* GetNodeDataStruct() const PURE_VIRTUAL(ULbbLayeredBlendBodyEdGraphNode::GetNodeDataStruct, return nullptr;);
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

	const class ULbbLayeredBlendBodyEdGraph* GetOwningLayeredBlendGraph() const;
	const class ULbbLayeredBlendBodyDefinition* GetOwningDefinition() const;
};

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbLayeredBlendBodyEdGraphNode_Input : public ULbbLayeredBlendBodyEdGraphNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Node")
	ELbbLayeredBodyPartPoseSourceType SourceType = ELbbLayeredBodyPartPoseSourceType::CurrentPose;

	UPROPERTY(EditAnywhere, Category = "Node", meta = (EditCondition = "SourceType == ELbbLayeredBodyPartPoseSourceType::CachePose", EditConditionHides, GetOptions = "GetAvailableCachePoseOptions"))
	FName CachePoseName = NAME_None;

	UFUNCTION()
	TArray<FString> GetAvailableCachePoseOptions() const;

	virtual const UScriptStruct* GetNodeDataStruct() const override { return FLbbLayeredBlendBodyGraphNodeData_Input::StaticStruct(); }

protected:
	virtual void AllocatePosePins() override;
	virtual FText GetNodeTitleText() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual void ImportNodeData(const FInstancedStruct& NodeData) override;
	virtual void ExportNodeData(FInstancedStruct& OutNodeData) const override;
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	FLbbLayeredBodyPartPoseSource GetSourcePose() const;
	void SetSourcePose(const FLbbLayeredBodyPartPoseSource& InSourcePose);
	void SanitizeSourceSelection();
	bool IsSourceTypeAllowedInCurrentGraph(ELbbLayeredBodyPartPoseSourceType InSourceType) const;
	bool IsAvailableNamedCache(FName InCachePoseName) const;
};

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbLayeredBlendBodyEdGraphNode_Result : public ULbbLayeredBlendBodyEdGraphNode
{
	GENERATED_BODY()

public:
	virtual const UScriptStruct* GetNodeDataStruct() const override { return FLbbLayeredBlendBodyGraphNodeData_Result::StaticStruct(); }
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

	virtual const UScriptStruct* GetNodeDataStruct() const override { return FLbbLayeredBlendBodyGraphNodeData_ApplySlot::StaticStruct(); }

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

	virtual const UScriptStruct* GetNodeDataStruct() const override { return FLbbLayeredBlendBodyGraphNodeData_Blend::StaticStruct(); }

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

	virtual const UScriptStruct* GetNodeDataStruct() const override { return FLbbLayeredBlendBodyGraphNodeData_MaskedBlend::StaticStruct(); }

protected:
	virtual void AllocatePosePins() override;
	virtual FText GetNodeTitleText() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual void ImportNodeData(const FInstancedStruct& NodeData) override;
	virtual void ExportNodeData(FInstancedStruct& OutNodeData) const override;
};

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbLayeredBlendBodyEdGraphNode_SavePose : public ULbbLayeredBlendBodyEdGraphNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Node")
	FName CachePoseName = NAME_None;

	virtual const UScriptStruct* GetNodeDataStruct() const override { return FLbbLayeredBlendBodyGraphNodeData_SavePose::StaticStruct(); }

protected:
	virtual void AllocatePosePins() override;
	virtual FText GetNodeTitleText() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual void ImportNodeData(const FInstancedStruct& NodeData) override;
	virtual void ExportNodeData(FInstancedStruct& OutNodeData) const override;
};

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbLayeredBlendBodyEdGraphNode_MakeAdditive : public ULbbLayeredBlendBodyEdGraphNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Node")
	ELbbBoneSpace AdditiveSpace = ELbbBoneSpace::LocalSpace;

	virtual const UScriptStruct* GetNodeDataStruct() const override { return FLbbLayeredBlendBodyGraphNodeData_MakeAdditive::StaticStruct(); }

protected:
	virtual void AllocatePosePins() override;
	virtual FText GetNodeTitleText() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual void ImportNodeData(const FInstancedStruct& NodeData) override;
	virtual void ExportNodeData(FInstancedStruct& OutNodeData) const override;
};

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbLayeredBlendBodyEdGraphNode_ApplyAdditive : public ULbbLayeredBlendBodyEdGraphNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Node")
	ELbbBoneSpace AdditiveSpace = ELbbBoneSpace::LocalSpace;

	UPROPERTY(EditAnywhere, Category = "Node")
	FLbbBlendWeight Weight;

	virtual const UScriptStruct* GetNodeDataStruct() const override { return FLbbLayeredBlendBodyGraphNodeData_ApplyAdditive::StaticStruct(); }

protected:
	virtual void AllocatePosePins() override;
	virtual FText GetNodeTitleText() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual void ImportNodeData(const FInstancedStruct& NodeData) override;
	virtual void ExportNodeData(FInstancedStruct& OutNodeData) const override;
};
