// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "LbbBodyPartDefinitionDataAsset.h"
#include "LbbEditorModel.h"
#include "Templates/Function.h"

struct FLbbCompileMessage;

enum class ELbbGraphKindFlags : uint8
{
	None = 0,
	Cache = 1 << 0,
	BodyPart = 1 << 1,
	All = Cache | BodyPart,
};

ENUM_CLASS_FLAGS(ELbbGraphKindFlags)

class FLbbGraphNodeCompileContext
{
public:
	using FFindInputLinkCallback = TFunction<const FLbbGraphLinkModel*(const FGuid&, FName)>;
	using FResolveSourceCallback = TFunction<bool(const FGuid&, FLbbLayeredBodyPartPoseSource&)>;

	FLbbGraphNodeCompileContext(
		ELbbGraphKind InGraphKind,
		int32 InBodyPartIndex,
		FFindInputLinkCallback InFindInputLinkCallback,
		FResolveSourceCallback InResolveSourceCallback);

	ELbbGraphKind GetGraphKind() const { return GraphKind; }
	int32 GetBodyPartIndex() const { return BodyPartIndex; }

	const FLbbGraphLinkModel* FindInputLink(const FGuid& NodeGuid, FName PinName) const;
	bool ResolveSource(const FGuid& SourceNodeGuid, FLbbLayeredBodyPartPoseSource& OutSourcePose) const;

private:
	ELbbGraphKind GraphKind = ELbbGraphKind::BodyPart;
	int32 BodyPartIndex = INDEX_NONE;
	FFindInputLinkCallback FindInputLinkCallback;
	FResolveSourceCallback ResolveSourceCallback;
};

using FLbbCompileGraphNodeFunction = bool (*)(
	const FLbbGraphNodeCompileContext& CompileContext,
	const FLbbGraphNodeModel& NodeModel,
	const FLbbLayeredBodyPartPoseTarget& TargetPose,
	TArray<FInstancedStruct>& OutOperators,
	TArray<FLbbCompileMessage>& OutMessages);

struct FLbbGraphNodeDescriptor
{
	const UScriptStruct* NodeDataStruct = nullptr;
	UClass* NodeClass = nullptr;
	const TCHAR* DisplayName = TEXT("Node");
	const FName* InputPins = nullptr;
	int32 NumInputPins = 0;
	const FName* OutputPins = nullptr;
	int32 NumOutputPins = 0;
	bool bIsInputNode = false;
	bool bIsResultNode = false;
	bool bIsSavePoseNode = false;
	bool bShowInNodeMenu = true;
	bool bIsSingleton = false;
	ELbbGraphKindFlags SingletonGraphKinds = ELbbGraphKindFlags::None;
	ELbbGraphKindFlags AllowedGraphKinds = ELbbGraphKindFlags::All;
	FLbbCompileGraphNodeFunction CompileNode = nullptr;

	TConstArrayView<FName> GetInputPins() const { return MakeArrayView(InputPins, NumInputPins); }
	TConstArrayView<FName> GetOutputPins() const { return MakeArrayView(OutputPins, NumOutputPins); }
	bool IsAuthorNode() const { return CompileNode != nullptr; }
	bool ProducesPose() const { return NumOutputPins > 0; }
	bool IsAllowedInGraph(ELbbGraphKind GraphKind) const;
	bool IsSingletonInGraph(ELbbGraphKind GraphKind) const;
};
