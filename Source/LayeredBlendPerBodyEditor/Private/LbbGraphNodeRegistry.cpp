// Fill out your copyright notice in the Description page of Project Settings.

#include "LbbGraphNodeRegistry.h"

#include "Algo/Find.h"
#include "GraphNodes/LbbEdGraphNode.h"

namespace
{
	TArray<FLbbGraphNodeDescriptor>& GetMutableLbbGraphNodeDescriptors()
	{
		static TArray<FLbbGraphNodeDescriptor> Descriptors;
		return Descriptors;
	}
}

void RegisterLbbGraphNode(const FLbbGraphNodeDescriptor& Descriptor)
{
	if (Descriptor.NodeDataStruct == nullptr)
	{
		return;
	}

	TArray<FLbbGraphNodeDescriptor>& Descriptors = GetMutableLbbGraphNodeDescriptors();
	if (FLbbGraphNodeDescriptor* ExistingDescriptor = Algo::FindByPredicate(
			Descriptors,
			[NodeDataStruct = Descriptor.NodeDataStruct](const FLbbGraphNodeDescriptor& Existing)
			{
				return Existing.NodeDataStruct == NodeDataStruct;
			}))
	{
		*ExistingDescriptor = Descriptor;
		return;
	}

	Descriptors.Add(Descriptor);
}

void ResetLbbGraphNodeRegistry()
{
	GetMutableLbbGraphNodeDescriptors().Reset();
}

const FLbbGraphNodeDescriptor* FindLbbGraphNodeDescriptor(const UScriptStruct* NodeDataStruct)
{
	if (NodeDataStruct == nullptr)
	{
		return nullptr;
	}

	return Algo::FindByPredicate(
		GetMutableLbbGraphNodeDescriptors(),
		[NodeDataStruct](const FLbbGraphNodeDescriptor& Descriptor)
		{
			return Descriptor.NodeDataStruct == NodeDataStruct;
		});
}

const TArray<FLbbGraphNodeDescriptor>& GetLbbGraphNodeDescriptors()
{
	return GetMutableLbbGraphNodeDescriptors();
}

ULbbEdGraphNode* CreateLbbNodeForDataStruct(UEdGraph* OuterGraph, const UScriptStruct* NodeDataStruct)
{
	const FLbbGraphNodeDescriptor* Descriptor = FindLbbGraphNodeDescriptor(NodeDataStruct);
	if (Descriptor == nullptr || Descriptor->NodeClass == nullptr || OuterGraph == nullptr)
	{
		return nullptr;
	}

	return NewObject<ULbbEdGraphNode>(OuterGraph, Descriptor->NodeClass, NAME_None, RF_Transactional);
}
