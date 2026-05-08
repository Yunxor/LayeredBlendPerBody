// Fill out your copyright notice in the Description page of Project Settings.

#include "LbbLayeredBlendBodyGraphNodeTypes.h"

namespace
{
	ELbbLayeredBlendBodyGraphKindFlags ToGraphKindFlag(const ELbbLayeredBlendBodyGraphKind GraphKind)
	{
		return GraphKind == ELbbLayeredBlendBodyGraphKind::Cache
			? ELbbLayeredBlendBodyGraphKindFlags::Cache
			: ELbbLayeredBlendBodyGraphKindFlags::BodyPart;
	}
}

FLbbLayeredBlendBodyGraphNodeCompileContext::FLbbLayeredBlendBodyGraphNodeCompileContext(
	const ELbbLayeredBlendBodyGraphKind InGraphKind,
	const int32 InBodyPartIndex,
	FFindInputLinkCallback InFindInputLinkCallback,
	FResolveSourceCallback InResolveSourceCallback)
	: GraphKind(InGraphKind)
	, BodyPartIndex(InBodyPartIndex)
	, FindInputLinkCallback(MoveTemp(InFindInputLinkCallback))
	, ResolveSourceCallback(MoveTemp(InResolveSourceCallback))
{
}

const FLbbLayeredBlendBodyGraphLinkModel* FLbbLayeredBlendBodyGraphNodeCompileContext::FindInputLink(
	const FGuid& NodeGuid,
	const FName PinName) const
{
	return FindInputLinkCallback ? FindInputLinkCallback(NodeGuid, PinName) : nullptr;
}

bool FLbbLayeredBlendBodyGraphNodeCompileContext::ResolveSource(
	const FGuid& SourceNodeGuid,
	FLbbLayeredBodyPartPoseSource& OutSourcePose) const
{
	return ResolveSourceCallback ? ResolveSourceCallback(SourceNodeGuid, OutSourcePose) : false;
}

bool FLbbLayeredBlendBodyGraphNodeDescriptor::IsAllowedInGraph(const ELbbLayeredBlendBodyGraphKind GraphKind) const
{
	return EnumHasAnyFlags(AllowedGraphKinds, ToGraphKindFlag(GraphKind));
}

bool FLbbLayeredBlendBodyGraphNodeDescriptor::IsSingletonInGraph(const ELbbLayeredBlendBodyGraphKind GraphKind) const
{
	return bIsSingleton && EnumHasAnyFlags(SingletonGraphKinds, ToGraphKindFlag(GraphKind));
}
