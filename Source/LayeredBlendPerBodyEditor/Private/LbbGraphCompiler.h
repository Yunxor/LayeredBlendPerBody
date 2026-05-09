// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Logging/TokenizedMessage.h"
#include "LbbBodyPartDefinitionDataAsset.h"

struct FLbbCompileMessage
{
	EMessageSeverity::Type Severity = EMessageSeverity::Info;
	ELbbGraphKind GraphKind = ELbbGraphKind::BodyPart;
	int32 BodyPartIndex = INDEX_NONE;
	FGuid NodeGuid;
	FString Message;
};

struct FLbbCompiledDefinitionData
{
	FLbbCacheProgram CacheProgram;
	TArray<FLbbPart> BodyParts;
};

struct FLbbCompileResult
{
	bool bSuccess = false;
	TArray<FLbbCompileMessage> Messages;
	FLbbCompiledDefinitionData CompiledDefinition;
};

class FLbbGraphCompiler
{
public:
	static void CreateDefaultCacheGraph(FLbbCacheGraphModel& OutGraphModel);
	static void CreateDefaultBodyPartGraph(FLbbPartGraphModel& OutGraphModel, const FName& PartName);
	static FLbbCompileResult Compile(const ULbbBodyPartDefinitionDataAsset& Definition);
	static void ApplyCompiledDefinition(const FLbbCompiledDefinitionData& CompiledDefinition, ULbbBodyPartDefinitionDataAsset& Definition);
};
