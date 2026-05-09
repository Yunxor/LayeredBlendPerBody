// Fill out your copyright notice in the Description page of Project Settings.


#include "LbbBlueprintFunctionLibrary.h"
#include "LbbAnimNode_LayeredBlendBodyPart.h"


FLbbAnimNodeRef_LayeredBlendBodyPart ULbbBlueprintFunctionLibrary::ConvertToLayeredBodyPartBlendNode(const FAnimNodeReference& Node, EAnimNodeReferenceConversionResult& Result)
{
	return FAnimNodeReference::ConvertToType<FLbbAnimNodeRef_LayeredBlendBodyPart>(Node, Result);
}

void ULbbBlueprintFunctionLibrary::SetBodyPartDefinition(const FLbbAnimNodeRef_LayeredBlendBodyPart& Node, class ULbbBodyPartDefinitionDataAsset* InBodyDefinition)
{
	if (FLbbAnimNode_LayeredBlendBodyPart* AnimNodePtr = Node.GetAnimNodePtr<FLbbAnimNode_LayeredBlendBodyPart>())
	{
		AnimNodePtr->SetBodyDefinition(InBodyDefinition);
	}
	else
	{
		UE_LOG(LogAnimation, Warning, TEXT("ULbbStatics::SetBodyPartDefinition called on an invalid context or with an invalid type"));
	}
}