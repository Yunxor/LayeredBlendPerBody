// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "LbbEdGraphNode.h"
#include "LbbEdGraphNode_Input.generated.h"

class FLbbGraphNodeCompileContext;
class FProperty;
struct FLbbCompileMessage;
struct FLbbGraphNodeDescriptor;
struct FLbbGraphNodeModel;
struct FPropertyChangedEvent;
struct FLbbLayeredBodyPartPoseTarget;

UCLASS()
class LAYEREDBLENDPERBODYEDITOR_API ULbbEdGraphNode_Input : public ULbbEdGraphNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Node")
	ELbbLayeredBodyPartPoseSourceType SourceType = ELbbLayeredBodyPartPoseSourceType::InputPose;

	UPROPERTY(EditAnywhere, Category = "Node", meta = (EditCondition = "SourceType == ELbbLayeredBodyPartPoseSourceType::InputPose", EditConditionHides, GetOptions = "GetAvailableInputPoseOptions"))
	FName InputPoseName = TEXT("BasePose");

	UPROPERTY(EditAnywhere, Category = "Node", meta = (EditCondition = "SourceType == ELbbLayeredBodyPartPoseSourceType::CachePose", EditConditionHides, GetOptions = "GetAvailableCachePoseOptions"))
	FName CachePoseName = NAME_None;

	UFUNCTION()
	TArray<FString> GetAvailableInputPoseOptions() const;

	UFUNCTION()
	TArray<FString> GetAvailableCachePoseOptions() const;

	TArray<ELbbLayeredBodyPartPoseSourceType> GetAvailableSourceTypes() const;

	virtual const UScriptStruct* GetNodeDataStruct() const override { return FLbbGraphNodeData_Input::StaticStruct(); }
	virtual FLbbGraphNodeDescriptor BuildGraphNodeDescriptor() const override;

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
	bool IsAvailableInputPose(FName InInputPoseName) const;
	bool IsAvailableNamedCache(FName InCachePoseName) const;
};
