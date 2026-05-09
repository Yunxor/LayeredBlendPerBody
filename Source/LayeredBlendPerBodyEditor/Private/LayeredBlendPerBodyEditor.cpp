#include "LayeredBlendPerBodyEditor.h"

#include "GraphNodes/LbbEdGraphNode.h"
#include "GraphNodes/LbbEdGraphNode_Input.h"
#include "GraphNodes/LbbEdGraphNode_InputDetails.h"
#include "LbbGraphNodeRegistry.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UObject/UObjectHash.h"

#define LOCTEXT_NAMESPACE "FLayeredBlendPerBodyEditorModule"

namespace
{
	void RegisterAllLbbGraphNodes()
	{
		TArray<UClass*> NodeClasses;
		GetDerivedClasses(ULbbEdGraphNode::StaticClass(), NodeClasses, true);

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

			const ULbbEdGraphNode* NodeCDO = NodeClass->GetDefaultObject<ULbbEdGraphNode>();
			if (NodeCDO == nullptr)
			{
				continue;
			}

			const FLbbGraphNodeDescriptor Descriptor = NodeCDO->BuildGraphNodeDescriptor();
			if (Descriptor.NodeDataStruct == nullptr || Descriptor.NodeClass == nullptr)
			{
				continue;
			}

			RegisterLbbGraphNode(Descriptor);
		}
	}
}

void FLayeredBlendPerBodyEditorModule::StartupModule()
{
	ResetLbbGraphNodeRegistry();
	RegisterAllLbbGraphNodes();

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.RegisterCustomClassLayout(
		ULbbEdGraphNode_Input::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FLbbEdGraphNodeInputDetails::MakeInstance));
	PropertyEditorModule.NotifyCustomizationModuleChanged();
}

void FLayeredBlendPerBodyEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyEditorModule.UnregisterCustomClassLayout(ULbbEdGraphNode_Input::StaticClass()->GetFName());
		PropertyEditorModule.NotifyCustomizationModuleChanged();
	}

	ResetLbbGraphNodeRegistry();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLayeredBlendPerBodyEditorModule, LayeredBlendPerBodyEditor)
