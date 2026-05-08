#include "LayeredBlendPerBodyEditor.h"

#include "GraphNodes/LbbLayeredBlendBodyEdGraphNode.h"
#include "LbbLayeredBlendBodyGraphNodeRegistry.h"
#include "UObject/UObjectHash.h"

#define LOCTEXT_NAMESPACE "FLayeredBlendPerBodyEditorModule"

namespace
{
	void RegisterAllLbbLayeredBlendBodyGraphNodes()
	{
		TArray<UClass*> NodeClasses;
		GetDerivedClasses(ULbbLayeredBlendBodyEdGraphNode::StaticClass(), NodeClasses, true);

		NodeClasses.Sort([](const UClass& Left, const UClass& Right)
		{
			const FString LeftPath = Left.GetPathName();
			const FString RightPath = Right.GetPathName();
			return LeftPath < RightPath;
		});

		for (UClass* NodeClass : NodeClasses)
		{
			if (NodeClass == nullptr || !NodeClass->HasAnyClassFlags(CLASS_Native))
			{
				continue;
			}

			if (NodeClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				continue;
			}

			const ULbbLayeredBlendBodyEdGraphNode* NodeCDO = NodeClass->GetDefaultObject<ULbbLayeredBlendBodyEdGraphNode>();
			if (NodeCDO == nullptr)
			{
				continue;
			}

			const FLbbLayeredBlendBodyGraphNodeDescriptor Descriptor = NodeCDO->BuildGraphNodeDescriptor();
			if (Descriptor.NodeDataStruct == nullptr || Descriptor.NodeClass == nullptr)
			{
				continue;
			}

			RegisterLbbLayeredBlendBodyGraphNode(Descriptor);
		}
	}
}

void FLayeredBlendPerBodyEditorModule::StartupModule()
{
	ResetLbbLayeredBlendBodyGraphNodeRegistry();
	RegisterAllLbbLayeredBlendBodyGraphNodes();
}

void FLayeredBlendPerBodyEditorModule::ShutdownModule()
{
	ResetLbbLayeredBlendBodyGraphNodeRegistry();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLayeredBlendPerBodyEditorModule, LayeredBlendPerBodyEditor)
