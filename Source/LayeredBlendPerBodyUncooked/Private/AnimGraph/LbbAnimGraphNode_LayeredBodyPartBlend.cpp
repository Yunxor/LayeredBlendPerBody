// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimGraph/LbbAnimGraphNode_LayeredBodyPartBlend.h"

#define LOCTEXT_NAMESPACE "LbbAnimation"

ULbbAnimGraphNode_LayeredBodyPartBlend::ULbbAnimGraphNode_LayeredBodyPartBlend(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
}

FText ULbbAnimGraphNode_LayeredBodyPartBlend::GetTooltipText() const
{
	return LOCTEXT("LbbAnimGraphNode_LayeredBodyPartBlend_Tooltip", "Layered blend per body part");
}

FText ULbbAnimGraphNode_LayeredBodyPartBlend::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("LbbAnimGraphNode_LayeredBodyPartBlend_Title", "Layered blend per body part");
}

FText ULbbAnimGraphNode_LayeredBodyPartBlend::GetMenuCategory() const
{
	return LOCTEXT("NodeCategory", "Lbb Animation");
}

FLinearColor ULbbAnimGraphNode_LayeredBodyPartBlend::GetNodeTitleColor() const
{
	return FLinearColor(0.2f, 0.8f, 0.2f);
}

void ULbbAnimGraphNode_LayeredBodyPartBlend::PreloadRequiredAssets()
{
	// if (Node.BodyDefinition)
	// {
	// 	PreloadObject(Node.BodyDefinition);
	// }
	
	Super::PreloadRequiredAssets();
}

void ULbbAnimGraphNode_LayeredBodyPartBlend::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	// const FName PropertyName = (PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None);
	//
	// if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLbbAnimNode_LayeredBodyPartBlend, BodyDefinition))
	// {
	// 	Node.ReinitRuntimeData();
	// }
	
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ULbbAnimGraphNode_LayeredBodyPartBlend::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);


	// // ensure to cache the per-bone blend weights
	// Node.CheckBodyBoneWeightValidation(ForSkeleton);
}

#undef LOCTEXT_NAMESPACE
