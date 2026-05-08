// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LbbLayeredBlendBodyGraphCompiler.h"
#include "LbbLayeredBlendBodyGraphNodeTypes.h"
#include "LbbLayeredBlendBodyOperators.h"

namespace LbbLayeredBlendBodyEdGraphNodeUtils
{
	extern const FName PinPose;
	extern const FName PinInput;
	extern const FName PinBase;
	extern const FName PinBlend;
	extern const FName PinAdditive;
	extern const FName PinResult;

	extern const FName PoseInputPins[1];
	extern const FName PoseOutputPins[1];
	extern const FName InputPins[1];
	extern const FName BaseBlendInputPins[2];
	extern const FName BaseAdditiveInputPins[2];
	extern const FName ResultOutputPins[1];

	FString FormatBlendWeightSummary(const FLbbBlendWeight& Weight);
	FString FormatBoneSpaceSummary(ELbbBoneSpace BoneSpace);
	FString FormatPoseSourceSummary(const FLbbLayeredBodyPartPoseSource& SourcePose);

	FLbbLayeredBlendBodyGraphNodeDescriptor MakeNodeDescriptor(
		const UScriptStruct* NodeDataStruct,
		UClass* NodeClass,
		const TCHAR* DisplayName,
		TConstArrayView<FName> InputPinNames,
		TConstArrayView<FName> OutputPinNames,
		bool bIsInputNode,
		bool bIsResultNode,
		bool bIsSavePoseNode,
		bool bShowInNodeMenu,
		bool bIsSingleton,
		ELbbLayeredBlendBodyGraphKindFlags SingletonGraphKinds,
		ELbbLayeredBlendBodyGraphKindFlags AllowedGraphKinds,
		FLbbCompileGraphNodeFunction CompileNode);

	void AddMessage(
		TArray<FLbbCompileMessage>& OutMessages,
		EMessageSeverity::Type Severity,
		ELbbLayeredBlendBodyGraphKind GraphKind,
		int32 BodyPartIndex,
		const FGuid& NodeGuid,
		const FString& Message);

	template <typename OperatorType>
	void AddOperatorStruct(TArray<FInstancedStruct>& Operators, const OperatorType& Operator)
	{
		FInstancedStruct& OperatorData = Operators.AddDefaulted_GetRef();
		OperatorData.InitializeAs<OperatorType>();
		OperatorData.GetMutable<OperatorType>() = Operator;
	}

	bool ResolveRequiredInputSource(
		const FLbbLayeredBlendBodyGraphNodeCompileContext& CompileContext,
		const FLbbLayeredBlendBodyGraphNodeModel& NodeModel,
		FName PinName,
		FLbbLayeredBodyPartPoseSource& OutSourcePose,
		TArray<FLbbCompileMessage>& OutMessages,
		const TCHAR* FailureMessage);

	bool ValidateCurveWeight(
		const FLbbBlendWeight& Weight,
		const FLbbLayeredBlendBodyGraphNodeCompileContext& CompileContext,
		const FLbbLayeredBlendBodyGraphNodeModel& NodeModel,
		const TCHAR* FailureMessage,
		TArray<FLbbCompileMessage>& OutMessages);
}
