// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Animation/AnimNodeReference.h"
#include "LbbBlueprintFunctionLibrary.generated.h"

USTRUCT(BlueprintType)
struct FLbbAnimNodeRef_LayeredBlendBodyPart : public FAnimNodeReference
{
	GENERATED_BODY()

	typedef struct FLbbAnimNode_LayeredBlendBodyPart FInternalNodeType;
};


/**
 * 蓝图函数库
 */
UCLASS()
class LAYEREDBLENDPERBODY_API ULbbBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
    UFUNCTION(BlueprintCallable, Category = "LbbAnimation|LayeredBodyPartBlend", meta = (NotBlueprintThreadSafe, ExpandEnumAsExecs = "Result", DisplayName = "Convert to Layered Blend Body Part Node"))
    static FLbbAnimNodeRef_LayeredBlendBodyPart ConvertToLayeredBodyPartBlendNode(const FAnimNodeReference& Node, EAnimNodeReferenceConversionResult& Result);

    UFUNCTION(BlueprintPure, Category = "LbbAnimation|LayeredBodyPartBlend", meta = (BlueprintThreadSafe, DisplayName = "Convert to Layered Blend Body Part Node (Pure)"))
    static void ConvertToLayeredBodyPartBlendNodePure(const FAnimNodeReference& Node, FLbbAnimNodeRef_LayeredBlendBodyPart& LayeredBodyPartBlendNode, bool& Result)
    {
    	EAnimNodeReferenceConversionResult ConversionResult;
    	LayeredBodyPartBlendNode = ConvertToLayeredBodyPartBlendNode(Node, ConversionResult);
    	Result = (ConversionResult == EAnimNodeReferenceConversionResult::Succeeded);
    };
    
    UFUNCTION(BlueprintCallable, Category = "LbbAnimation|LayeredBodyPartBlend", meta = (NotBlueprintThreadSafe))
    static void SetBodyPartDefinition(const FLbbAnimNodeRef_LayeredBlendBodyPart& Node, class ULbbBodyPartDefinitionDataAsset* InBodyDefinition);
};
