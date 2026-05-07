// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Logging/TokenizedMessage.h"
#include "LbbLayeredBlendBodyEditorModel.h"

class ULbbLayeredBlendBodyDefinition;
struct FLbbLayeredBlendBodyPartGraphModel;

struct FLbbCompileMessage
{
	EMessageSeverity::Type Severity = EMessageSeverity::Info;
	ELbbLayeredBlendBodyGraphKind GraphKind = ELbbLayeredBlendBodyGraphKind::BodyPart;
	int32 BodyPartIndex = INDEX_NONE;
	FGuid NodeGuid;
	FString Message;
};

struct FLbbCompileResult
{
	bool bSuccess = false;
	TArray<FLbbCompileMessage> Messages;
};

class FLbbLayeredBlendBodyGraphCompiler
{
public:
	static void CreateDefaultCacheGraph(FLbbLayeredBlendBodyCacheGraphModel& OutGraphModel);
	static void CreateDefaultBodyPartGraph(FLbbLayeredBlendBodyPartGraphModel& OutGraphModel, const FName& PartName);
	static FLbbCompileResult Compile(ULbbLayeredBlendBodyDefinition& Definition);
};
