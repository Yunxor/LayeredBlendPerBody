// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "LbbGraphNodeTypes.h"

class UEdGraph;
class ULbbEdGraphNode;

void RegisterLbbGraphNode(const FLbbGraphNodeDescriptor& Descriptor);
void ResetLbbGraphNodeRegistry();
const FLbbGraphNodeDescriptor* FindLbbGraphNodeDescriptor(const UScriptStruct* NodeDataStruct);
const TArray<FLbbGraphNodeDescriptor>& GetLbbGraphNodeDescriptors();
ULbbEdGraphNode* CreateLbbNodeForDataStruct(UEdGraph* OuterGraph, const UScriptStruct* NodeDataStruct);
