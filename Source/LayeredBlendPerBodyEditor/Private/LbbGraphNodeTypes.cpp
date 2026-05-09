// Fill out your copyright notice in the Description page of Project Settings.

#include "LbbGraphNodeTypes.h"

namespace
{
	ELbbGraphKindFlags ToGraphKindFlag(const ELbbGraphKind GraphKind)
	{
		return GraphKind == ELbbGraphKind::Cache
			? ELbbGraphKindFlags::Cache
			: ELbbGraphKindFlags::BodyPart;
	}
}

FLbbGraphNodeCompileContext::FLbbGraphNodeCompileContext(
	const ELbbGraphKind InGraphKind,
	const int32 InBodyPartIndex,
	FFindInputLinkCallback InFindInputLinkCallback,
	FResolveSourceCallback InResolveSourceCallback)
	: GraphKind(InGraphKind)
	, BodyPartIndex(InBodyPartIndex)
	, FindInputLinkCallback(MoveTemp(InFindInputLinkCallback))
	, ResolveSourceCallback(MoveTemp(InResolveSourceCallback))
{
}

const FLbbGraphLinkModel* FLbbGraphNodeCompileContext::FindInputLink(
	const FGuid& NodeGuid,
	const FName PinName) const
{
	return FindInputLinkCallback ? FindInputLinkCallback(NodeGuid, PinName) : nullptr;
}

bool FLbbGraphNodeCompileContext::ResolveSource(
	const FGuid& SourceNodeGuid,
	FLbbLayeredBodyPartPoseSource& OutSourcePose) const
{
	return ResolveSourceCallback ? ResolveSourceCallback(SourceNodeGuid, OutSourcePose) : false;
}

bool FLbbGraphNodeDescriptor::IsAllowedInGraph(const ELbbGraphKind GraphKind) const
{
	return EnumHasAnyFlags(AllowedGraphKinds, ToGraphKindFlag(GraphKind));
}

bool FLbbGraphNodeDescriptor::IsSingletonInGraph(const ELbbGraphKind GraphKind) const
{
	return bIsSingleton && EnumHasAnyFlags(SingletonGraphKinds, ToGraphKindFlag(GraphKind));
}
