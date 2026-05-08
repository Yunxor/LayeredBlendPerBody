// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "LbbLayeredBlendBodyEdGraphNode.h"
#include "LbbLayeredBlendBodyEdGraphNode_Input.generated.h"

class FLbbLayeredBlendBodyGraphNodeCompileContext;
class FProperty;
struct FLbbCompileMessage;
struct FLbbLayeredBlendBodyGraphNodeDescriptor;
struct FLbbLayeredBlendBodyGraphNodeModel;
struct FPropertyChangedEvent;
struct FLbbLayeredBodyPartPoseTarget;

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
	virtual FLbbLayeredBlendBodyGraphNodeDescriptor BuildGraphNodeDescriptor() const override;

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
