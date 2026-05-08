// Fill out your copyright notice in the Description page of Project Settings.

#include "LbbLayeredBlendBodyGraphNodeRegistry.h"

#include "Algo/Find.h"
#include "GraphNodes/LbbLayeredBlendBodyEdGraphNode.h"

namespace
{
	TArray<FLbbLayeredBlendBodyGraphNodeDescriptor>& GetMutableLbbLayeredBlendBodyGraphNodeDescriptors()
	{
		static TArray<FLbbLayeredBlendBodyGraphNodeDescriptor> Descriptors;
		return Descriptors;
	}
}

void RegisterLbbLayeredBlendBodyGraphNode(const FLbbLayeredBlendBodyGraphNodeDescriptor& Descriptor)
{
	if (Descriptor.NodeDataStruct == nullptr)
	{
		return;
	}

	TArray<FLbbLayeredBlendBodyGraphNodeDescriptor>& Descriptors = GetMutableLbbLayeredBlendBodyGraphNodeDescriptors();
	if (FLbbLayeredBlendBodyGraphNodeDescriptor* ExistingDescriptor = Algo::FindByPredicate(
			Descriptors,
			[NodeDataStruct = Descriptor.NodeDataStruct](const FLbbLayeredBlendBodyGraphNodeDescriptor& Existing)
			{
				return Existing.NodeDataStruct == NodeDataStruct;
			}))
	{
		*ExistingDescriptor = Descriptor;
		return;
	}

	Descriptors.Add(Descriptor);
}

void ResetLbbLayeredBlendBodyGraphNodeRegistry()
{
	GetMutableLbbLayeredBlendBodyGraphNodeDescriptors().Reset();
}

const FLbbLayeredBlendBodyGraphNodeDescriptor* FindLbbLayeredBlendBodyGraphNodeDescriptor(const UScriptStruct* NodeDataStruct)
{
	if (NodeDataStruct == nullptr)
	{
		return nullptr;
	}

	return Algo::FindByPredicate(
		GetMutableLbbLayeredBlendBodyGraphNodeDescriptors(),
		[NodeDataStruct](const FLbbLayeredBlendBodyGraphNodeDescriptor& Descriptor)
		{
			return Descriptor.NodeDataStruct == NodeDataStruct;
		});
}

const TArray<FLbbLayeredBlendBodyGraphNodeDescriptor>& GetLbbLayeredBlendBodyGraphNodeDescriptors()
{
	return GetMutableLbbLayeredBlendBodyGraphNodeDescriptors();
}

ULbbLayeredBlendBodyEdGraphNode* CreateLbbLayeredBlendBodyNodeForDataStruct(UEdGraph* OuterGraph, const UScriptStruct* NodeDataStruct)
{
	const FLbbLayeredBlendBodyGraphNodeDescriptor* Descriptor = FindLbbLayeredBlendBodyGraphNodeDescriptor(NodeDataStruct);
	if (Descriptor == nullptr || Descriptor->NodeClass == nullptr || OuterGraph == nullptr)
	{
		return nullptr;
	}

	return NewObject<ULbbLayeredBlendBodyEdGraphNode>(OuterGraph, Descriptor->NodeClass, NAME_None, RF_Transactional);
}
