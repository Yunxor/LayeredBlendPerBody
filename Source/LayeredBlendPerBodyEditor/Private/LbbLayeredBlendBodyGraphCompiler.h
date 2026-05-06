// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Logging/TokenizedMessage.h"

class ULbbLayeredBlendBodyDefinition;
struct FLbbLayeredBlendBodyPartGraphModel;

namespace LbbLayeredBlendBodyPartEditor
{
	struct FCompileMessage
	{
		EMessageSeverity::Type Severity = EMessageSeverity::Info;
		int32 BodyPartIndex = INDEX_NONE;
		FGuid NodeGuid;
		FString Message;
	};

	struct FCompileResult
	{
		bool bSuccess = false;
		TArray<FCompileMessage> Messages;
	};

	class FLbbLayeredBlendBodyGraphCompiler
	{
	public:
		static void CreateDefaultBodyPartGraph(FLbbLayeredBlendBodyPartGraphModel& OutGraphModel, const FName& PartName);
		static FCompileResult Compile(ULbbLayeredBlendBodyDefinition& Definition);
	};
}
