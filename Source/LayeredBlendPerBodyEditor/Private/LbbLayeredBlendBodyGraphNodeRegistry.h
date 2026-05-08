// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "LbbLayeredBlendBodyGraphNodeTypes.h"

class UEdGraph;
class ULbbLayeredBlendBodyEdGraphNode;

void RegisterLbbLayeredBlendBodyGraphNode(const FLbbLayeredBlendBodyGraphNodeDescriptor& Descriptor);
void ResetLbbLayeredBlendBodyGraphNodeRegistry();
const FLbbLayeredBlendBodyGraphNodeDescriptor* FindLbbLayeredBlendBodyGraphNodeDescriptor(const UScriptStruct* NodeDataStruct);
const TArray<FLbbLayeredBlendBodyGraphNodeDescriptor>& GetLbbLayeredBlendBodyGraphNodeDescriptors();
ULbbLayeredBlendBodyEdGraphNode* CreateLbbLayeredBlendBodyNodeForDataStruct(UEdGraph* OuterGraph, const UScriptStruct* NodeDataStruct);
