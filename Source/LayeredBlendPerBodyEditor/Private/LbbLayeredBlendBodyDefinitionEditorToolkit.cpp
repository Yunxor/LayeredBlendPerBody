// Fill out your copyright notice in the Description page of Project Settings.

#include "LbbLayeredBlendBodyDefinitionEditorToolkit.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "GraphEditor.h"
#include "IDetailsView.h"
#include "LbbLayeredBlendBodyDefinition.h"
#include "LbbLayeredBlendBodyBodyPartDetailsObject.h"
#include "LbbLayeredBlendBodyEdGraph.h"
#include "GraphNodes/LbbLayeredBlendBodyEdGraphNode.h"
#include "LbbLayeredBlendBodyGraphCompiler.h"
#include "LbbLayeredBlendBodyGraphNodeRegistry.h"
#include "LbbLayeredBlendBodyGraphSchema.h"
#include "LbbLayeredBlendBodyOperators.h"
#include "Logging/TokenizedMessage.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "LbbLayeredBlendBodyDefinitionEditorToolkit"

namespace
{
	static const FName LayeredBlendBodyPartEditorStyleSetName(TEXT("LbbLayeredBlendBodyPartEditorLocalStyle"));
	static const FName LargeDoubleUpArrowBrushName(TEXT("Symbols.DoubleUpArrow.Large"));
	static const FName LargeDoubleDownArrowBrushName(TEXT("Symbols.DoubleDownArrow.Large"));

	static const ISlateStyle& GetLayeredBlendBodyPartEditorStyle()
	{
		static TSharedPtr<FSlateStyleSet> StyleInstance;

		if (!StyleInstance.IsValid())
		{
			if (const ISlateStyle* ExistingStyle = FSlateStyleRegistry::FindSlateStyle(LayeredBlendBodyPartEditorStyleSetName))
			{
				return *ExistingStyle;
			}

			StyleInstance = MakeShared<FSlateStyleSet>(LayeredBlendBodyPartEditorStyleSetName);
			StyleInstance->SetParentStyleName(FAppStyle::GetAppStyleSetName());

			FSlateBrush* LargeUpBrush = new FSlateBrush(*FAppStyle::GetBrush(TEXT("Symbols.DoubleUpArrow")));
			LargeUpBrush->ImageSize = FVector2f(12.f, 12.f);
			StyleInstance->Set(LargeDoubleUpArrowBrushName, LargeUpBrush);

			FSlateBrush* LargeDownBrush = new FSlateBrush(*FAppStyle::GetBrush(TEXT("Symbols.DoubleDownArrow")));
			LargeDownBrush->ImageSize = FVector2f(12.f, 12.f);
			StyleInstance->Set(LargeDoubleDownArrowBrushName, LargeDownBrush);

			FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
		}

		return *StyleInstance.Get();
	}

	class FLbbLayeredBlendBodyDefinitionEditorCommands final : public TCommands<FLbbLayeredBlendBodyDefinitionEditorCommands>
	{
	public:
		FLbbLayeredBlendBodyDefinitionEditorCommands()
			: TCommands<FLbbLayeredBlendBodyDefinitionEditorCommands>(
				TEXT("LbbLayeredBlendBodyDefinitionEditorCommands"),
				LOCTEXT("BodyPartCommandsContext", "Layered Blend Body Definition Body Part Commands"),
				NAME_None,
				FAppStyle::GetAppStyleSetName())
		{
		}

		virtual void RegisterCommands() override
		{
			UI_COMMAND(AddBodyPart, "Add BodyPart", "Add BodyPart", EUserInterfaceActionType::Button, FInputChord(EKeys::Insert));
			UI_COMMAND(DuplicateBodyPart, "Duplicate", "Duplicate selected BodyPart", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::D));
			UI_COMMAND(DeleteBodyPart, "Delete", "Delete selected BodyPart", EUserInterfaceActionType::Button, FInputChord(EKeys::Delete));
			UI_COMMAND(MoveBodyPartUp, "Move Up", "Move selected BodyPart up", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::Up));
			UI_COMMAND(MoveBodyPartDown, "Move Down", "Move selected BodyPart down", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::Down));
		}

		TSharedPtr<FUICommandInfo> AddBodyPart;
		TSharedPtr<FUICommandInfo> DuplicateBodyPart;
		TSharedPtr<FUICommandInfo> DeleteBodyPart;
		TSharedPtr<FUICommandInfo> MoveBodyPartUp;
		TSharedPtr<FUICommandInfo> MoveBodyPartDown;
	};

	static FString GetSeverityLabel(const EMessageSeverity::Type Severity)
	{
		switch (Severity)
		{
		case EMessageSeverity::Info:
			return TEXT("Info");
		case EMessageSeverity::PerformanceWarning:
			return TEXT("Performance Warning");
		case EMessageSeverity::Warning:
			return TEXT("Warning");
		case EMessageSeverity::Error:
			return TEXT("Error");
		default:
			return TEXT("Unknown");
		}
	}

	static const FSlateBrush* GetSeverityBrush(const EMessageSeverity::Type Severity)
	{
		switch (Severity)
		{
		case EMessageSeverity::Error:
			return FCoreStyle::Get().GetBrush(TEXT("MessageLog.Error"));
		case EMessageSeverity::PerformanceWarning:
		case EMessageSeverity::Warning:
			return FCoreStyle::Get().GetBrush(TEXT("MessageLog.Warning"));
		case EMessageSeverity::Info:
		default:
			return FCoreStyle::Get().GetBrush(TEXT("MessageLog.Note"));
		}
	}

	static FSlateColor GetSeverityTextColor(const EMessageSeverity::Type Severity)
	{
		switch (Severity)
		{
		case EMessageSeverity::Error:
			return FSlateColor(FLinearColor(0.95f, 0.38f, 0.34f));
		case EMessageSeverity::PerformanceWarning:
			return FSlateColor(FLinearColor(0.88f, 0.63f, 0.22f));
		case EMessageSeverity::Warning:
			return FSlateColor(FLinearColor(0.96f, 0.75f, 0.27f));
		case EMessageSeverity::Info:
		default:
			return FSlateColor::UseForeground();
		}
	}

	static FString GetBodyPartLabel(const ULbbLayeredBlendBodyDefinition* Definition, const int32 BodyPartIndex)
	{
		if (Definition != nullptr
			&& Definition->EditorModel.BodyPartGraphs.IsValidIndex(BodyPartIndex)
			&& !Definition->EditorModel.BodyPartGraphs[BodyPartIndex].PartName.IsNone())
		{
			return Definition->EditorModel.BodyPartGraphs[BodyPartIndex].PartName.ToString();
		}

		return BodyPartIndex != INDEX_NONE
			? FString::Printf(TEXT("%d"), BodyPartIndex + 1)
			: TEXT("N/A");
	}

	static FString GetGraphLabel(
		const ULbbLayeredBlendBodyDefinition* Definition,
		const ELbbLayeredBlendBodyGraphKind GraphKind,
		const int32 BodyPartIndex)
	{
		return GraphKind == ELbbLayeredBlendBodyGraphKind::Cache
			? TEXT("Cache Graph")
			: GetBodyPartLabel(Definition, BodyPartIndex);
	}

	static FName MakeUniqueBodyPartName(const ULbbLayeredBlendBodyDefinition& Definition)
	{
		int32 Suffix = Definition.EditorModel.BodyPartGraphs.Num();
		while (true)
		{
			const FName Candidate(*FString::Printf(TEXT("BodyPart_%d"), Suffix));
			bool bExists = false;
			for (const FLbbLayeredBlendBodyPartGraphModel& GraphModel : Definition.EditorModel.BodyPartGraphs)
			{
				if (GraphModel.PartName == Candidate)
				{
					bExists = true;
					break;
				}
			}

			if (!bExists)
			{
				return Candidate;
			}

			++Suffix;
		}
	}

	static void RemapGraphGuids(FLbbLayeredBlendBodyGraphModelBase& GraphModel)
	{
		TMap<FGuid, FGuid> GuidRemap;
		GraphModel.GraphGuid = FGuid::NewGuid();

		for (FLbbLayeredBlendBodyGraphNodeModel& NodeModel : GraphModel.Nodes)
		{
			const FGuid OldGuid = NodeModel.NodeGuid;
			NodeModel.NodeGuid = FGuid::NewGuid();
			GuidRemap.Add(OldGuid, NodeModel.NodeGuid);
		}

		for (FLbbLayeredBlendBodyGraphLinkModel& LinkModel : GraphModel.Links)
		{
			if (const FGuid* NewFromGuid = GuidRemap.Find(LinkModel.FromNodeGuid))
			{
				LinkModel.FromNodeGuid = *NewFromGuid;
			}
			if (const FGuid* NewToGuid = GuidRemap.Find(LinkModel.ToNodeGuid))
			{
				LinkModel.ToNodeGuid = *NewToGuid;
			}
		}
	}

}

const FName FLbbLayeredBlendBodyDefinitionEditorToolkit::BodyPartListTabId(TEXT("LbbLayeredBlendBodyDefinitionEditorToolkit_BodyParts"));
const FName FLbbLayeredBlendBodyDefinitionEditorToolkit::GraphTabId(TEXT("LbbLayeredBlendBodyDefinitionEditorToolkit_Graph"));
const FName FLbbLayeredBlendBodyDefinitionEditorToolkit::DetailsTabId(TEXT("LbbLayeredBlendBodyDefinitionEditorToolkit_Details"));
const FName FLbbLayeredBlendBodyDefinitionEditorToolkit::CompiledOperatorsPreviewTabId(TEXT("LbbLayeredBlendBodyDefinitionEditorToolkit_CompiledOperatorsPreview"));
const FName FLbbLayeredBlendBodyDefinitionEditorToolkit::CompileMessagesTabId(TEXT("LbbLayeredBlendBodyDefinitionEditorToolkit_CompileMessages"));
const FName FLbbLayeredBlendBodyDefinitionEditorToolkit::EditorAppName(TEXT("LbbLayeredBlendBodyDefinitionEditorApp"));

TSharedRef<FLbbLayeredBlendBodyDefinitionEditorToolkit> FLbbLayeredBlendBodyDefinitionEditorToolkit::CreateEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	ULbbLayeredBlendBodyDefinition* InDefinition)
{
	TSharedRef<FLbbLayeredBlendBodyDefinitionEditorToolkit> NewEditor = MakeShareable(new FLbbLayeredBlendBodyDefinitionEditorToolkit());
	NewEditor->InitEditor(Mode, InitToolkitHost, InDefinition);
	return NewEditor;
}

FLbbLayeredBlendBodyDefinitionEditorToolkit::~FLbbLayeredBlendBodyDefinitionEditorToolkit()
{
	ClearCurrentGraphChangedHandler();
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::InitEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	ULbbLayeredBlendBodyDefinition* InDefinition)
{
	if (!FLbbLayeredBlendBodyDefinitionEditorCommands::IsRegistered())
	{
		FLbbLayeredBlendBodyDefinitionEditorCommands::Register();
	}

	EditingDefinition = InDefinition;
	if (EditingDefinition.IsValid() && !EditingDefinition->EditorModel.CacheGraph.GraphGuid.IsValid())
	{
		EditingDefinition->Modify();
		FLbbLayeredBlendBodyGraphCompiler::CreateDefaultCacheGraph(EditingDefinition->EditorModel.CacheGraph);
	}

	BodyPartCommandList = MakeShared<FUICommandList>();
	BodyPartDetailsObject = NewObject<ULbbLayeredBlendBodyBodyPartDetailsObject>(GetTransientPackage(), NAME_None, RF_Transactional);
	BindBodyPartCommands();

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.NotifyHook = nullptr;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->OnFinishedChangingProperties().AddSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleDetailsFinishedChangingProperties);

	RefreshBodyPartItems();
	if (BodyPartItems.Num() > 0)
	{
		CurrentBodyPartIndex = 0;
	}
	CurrentGraphKind = ELbbLayeredBlendBodyGraphKind::Cache;
	RefreshBodyPartDetailsObject();

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("Standalone_LbbLayeredBlendBodyDefinitionEditor_v2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(0.78f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.18f)
					->SetHideTabWell(true)
					->AddTab(BodyPartListTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.57f)
					->SetHideTabWell(true)
					->AddTab(GraphTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.25f)
					->AddTab(DetailsTabId, ETabState::OpenedTab)
					->AddTab(CompiledOperatorsPreviewTabId, ETabState::OpenedTab)
					->SetForegroundTab(FTabId(DetailsTabId))
				)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.22f)
				->SetHideTabWell(true)
				->AddTab(CompileMessagesTabId, ETabState::OpenedTab)
			)
		);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	InitAssetEditor(Mode, InitToolkitHost, EditorAppName, Layout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InDefinition);
	ExtendToolbar();

	RegenerateMenusAndToolbars();

	if (CurrentGraphKind == ELbbLayeredBlendBodyGraphKind::Cache)
	{
		SelectCacheGraph(true);
	}
	else if (CurrentBodyPartIndex != INDEX_NONE)
	{
		SelectBodyPart(CurrentBodyPartIndex);
	}
	else
	{
		RebuildGraphWidget();
		RefreshDetailsPanel();
	}

	CompileDefinition(false);
}

FName FLbbLayeredBlendBodyDefinitionEditorToolkit::GetToolkitFName() const
{
	return FName(TEXT("LbbLayeredBlendBodyDefinitionEditor"));
}

FText FLbbLayeredBlendBodyDefinitionEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Layered Blend Body Definition Editor");
}

FString FLbbLayeredBlendBodyDefinitionEditorToolkit::GetWorldCentricTabPrefix() const
{
	return TEXT("LayeredBlendBodyDefinition");
}

FLinearColor FLbbLayeredBlendBodyDefinitionEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(BodyPartListTabId, FOnSpawnTab::CreateSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::SpawnBodyPartListTab))
		.SetDisplayName(LOCTEXT("BodyPartListTabTitle", "Body Parts"))
		.SetGroup(GetWorkspaceMenuCategory());
	InTabManager->RegisterTabSpawner(GraphTabId, FOnSpawnTab::CreateSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::SpawnGraphTab))
		.SetDisplayName(LOCTEXT("GraphTabTitle", "Graph"))
		.SetGroup(GetWorkspaceMenuCategory());
	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::SpawnDetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTabTitle", "Details"))
		.SetGroup(GetWorkspaceMenuCategory());
	InTabManager->RegisterTabSpawner(CompiledOperatorsPreviewTabId, FOnSpawnTab::CreateSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::SpawnCompiledOperatorsPreviewTab))
		.SetDisplayName(LOCTEXT("CompiledOperatorsPreviewTabTitle", "Compiled Operators Preview"))
		.SetGroup(GetWorkspaceMenuCategory());
	InTabManager->RegisterTabSpawner(CompileMessagesTabId, FOnSpawnTab::CreateSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::SpawnCompileMessagesTab))
		.SetDisplayName(LOCTEXT("CompileMessagesTabTitle", "Compile Messages"))
		.SetGroup(GetWorkspaceMenuCategory());
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner(BodyPartListTabId);
	InTabManager->UnregisterTabSpawner(GraphTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(CompiledOperatorsPreviewTabId);
	InTabManager->UnregisterTabSpawner(CompileMessagesTabId);
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::SaveAsset_Execute()
{
	if (!CompileDefinition(true))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("CompileBeforeSaveFailed", "The Layered Blend Body Definition has compile errors. Save has been blocked."));
		return;
	}

	FAssetEditorToolkit::SaveAsset_Execute();
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::ClearCurrentGraphChangedHandler()
{
	if (CurrentGraph != nullptr && CurrentGraphChangedHandle.IsValid())
	{
		CurrentGraph->RemoveOnGraphChangedHandler(CurrentGraphChangedHandle);
		CurrentGraphChangedHandle.Reset();
	}
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::ExtendToolbar()
{
	if (ToolbarExtender.IsValid())
	{
		RemoveToolbarExtender(ToolbarExtender);
		ToolbarExtender.Reset();
	}

	ToolbarExtender = MakeShared<FExtender>();
	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::FillToolbar));

	AddToolbarExtender(ToolbarExtender);
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("LayeredBlendBodyCompile");
	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			CompileDefinition(true);
		})),
		NAME_None,
		LOCTEXT("ToolbarCompileLabel", "Compile"),
		LOCTEXT("ToolbarCompileTooltip", "Compile the layered blend body graphs into runtime BodyParts operators."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Recompile.Small"));
	ToolbarBuilder.EndSection();
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::BindBodyPartCommands()
{
	if (!BodyPartCommandList.IsValid())
	{
		return;
	}

	const FLbbLayeredBlendBodyDefinitionEditorCommands& Commands = FLbbLayeredBlendBodyDefinitionEditorCommands::Get();

	BodyPartCommandList->MapAction(
		Commands.AddBodyPart,
		FExecuteAction::CreateSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleAddBodyPart),
		FCanExecuteAction::CreateSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::CanAddBodyPart));

	BodyPartCommandList->MapAction(
		Commands.DuplicateBodyPart,
		FExecuteAction::CreateSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleDuplicateBodyPart),
		FCanExecuteAction::CreateSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::CanDuplicateBodyPart));

	BodyPartCommandList->MapAction(
		Commands.DeleteBodyPart,
		FExecuteAction::CreateSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleDeleteBodyPart),
		FCanExecuteAction::CreateSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::CanDeleteBodyPart));

	BodyPartCommandList->MapAction(
		Commands.MoveBodyPartUp,
		FExecuteAction::CreateSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleMoveBodyPartUp),
		FCanExecuteAction::CreateSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::CanMoveBodyPartUp));

	BodyPartCommandList->MapAction(
		Commands.MoveBodyPartDown,
		FExecuteAction::CreateSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleMoveBodyPartDown),
		FCanExecuteAction::CreateSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::CanMoveBodyPartDown));
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::RefreshBodyPartItems()
{
	BodyPartItems.Reset();

	if (const ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get())
	{
		for (int32 BodyPartIndex = 0; BodyPartIndex < Definition->EditorModel.BodyPartGraphs.Num(); ++BodyPartIndex)
		{
			BodyPartItems.Add(MakeShared<int32>(BodyPartIndex));
		}
	}

	if (BodyPartListView.IsValid())
	{
		BodyPartListView->RequestListRefresh();
		if (CurrentGraphKind == ELbbLayeredBlendBodyGraphKind::Cache)
		{
			BodyPartListView->ClearSelection();
		}
		else if (BodyPartItems.IsValidIndex(CurrentBodyPartIndex))
		{
			BodyPartListView->SetSelection(BodyPartItems[CurrentBodyPartIndex], ESelectInfo::Direct);
		}
		else
		{
			BodyPartListView->ClearSelection();
		}
	}
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::SelectCacheGraph(const bool bForceRebuild)
{
	if (!bForceRebuild && CurrentGraphKind == ELbbLayeredBlendBodyGraphKind::Cache && CurrentGraph != nullptr)
	{
		return;
	}

	SyncCurrentGraphToModel();
	CurrentGraphKind = ELbbLayeredBlendBodyGraphKind::Cache;
	RefreshBodyPartDetailsObject();
	RebuildCurrentGraph();
	RebuildGraphWidget();
	RefreshDetailsPanel();
	RefreshBodyPartItems();
	CompileDefinition(false, false);
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::SelectBodyPart(const int32 BodyPartIndex, const bool bForceRebuild)
{
	if (!bForceRebuild
		&& CurrentGraphKind == ELbbLayeredBlendBodyGraphKind::BodyPart
		&& CurrentBodyPartIndex == BodyPartIndex
		&& CurrentGraph != nullptr)
	{
		return;
	}

	SyncCurrentGraphToModel();
	CurrentGraphKind = ELbbLayeredBlendBodyGraphKind::BodyPart;
	CurrentBodyPartIndex = BodyPartIndex;
	RefreshBodyPartDetailsObject();
	RebuildCurrentGraph();
	RebuildGraphWidget();
	RefreshDetailsPanel();
	RefreshBodyPartItems();
	CompileDefinition(false, false);
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::ApplyCompileMessagesToCurrentGraph(
	const FLbbCompileResult& CompileResult)
{
	if (CurrentGraph == nullptr)
	{
		return;
	}

	TMap<FGuid, TArray<const FLbbCompileMessage*>> MessagesByNodeGuid;
	for (const FLbbCompileMessage& Message : CompileResult.Messages)
	{
		const bool bMatchesCurrentGraph = (Message.GraphKind == CurrentGraphKind)
			&& (Message.GraphKind == ELbbLayeredBlendBodyGraphKind::Cache || Message.BodyPartIndex == CurrentBodyPartIndex);
		if (bMatchesCurrentGraph && Message.NodeGuid.IsValid())
		{
			MessagesByNodeGuid.FindOrAdd(Message.NodeGuid).Add(&Message);
		}
	}

	TGuardValue<bool> Guard(bIsSynchronizing, true);
	for (UEdGraphNode* GraphNode : CurrentGraph->Nodes)
	{
		if (GraphNode == nullptr)
		{
			continue;
		}

		GraphNode->ClearCompilerMessage();
		GraphNode->ErrorType = EMessageSeverity::Info;
		GraphNode->ErrorMsg.Empty();

		const ULbbLayeredBlendBodyEdGraphNode* LbbNode = Cast<ULbbLayeredBlendBodyEdGraphNode>(GraphNode);
		if (LbbNode == nullptr)
		{
			continue;
		}

		const TArray<const FLbbCompileMessage*>* NodeMessages = MessagesByNodeGuid.Find(LbbNode->NodeGuid);
		if (NodeMessages == nullptr || NodeMessages->IsEmpty())
		{
			continue;
		}

		EMessageSeverity::Type HighestSeverity = EMessageSeverity::Info;
		FString ErrorText;
		for (const FLbbCompileMessage* Message : *NodeMessages)
		{
			if (Message == nullptr)
			{
				continue;
			}

			if (Message->Severity < HighestSeverity)
			{
				HighestSeverity = Message->Severity;
			}

			if (!ErrorText.IsEmpty())
			{
				ErrorText += TEXT("\n");
			}
			ErrorText += Message->Message;
		}

		GraphNode->bHasCompilerMessage = true;
		GraphNode->ErrorType = HighestSeverity;
		GraphNode->ErrorMsg = MoveTemp(ErrorText);
	}

	CurrentGraph->NotifyGraphChanged();
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::RebuildCurrentGraph()
{
	ClearCurrentGraphChangedHandler();
	CurrentGraph = nullptr;

	const ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get();
	if (Definition == nullptr)
	{
		return;
	}

	const FLbbLayeredBlendBodyGraphModelBase* GraphModel = nullptr;
	if (CurrentGraphKind == ELbbLayeredBlendBodyGraphKind::Cache)
	{
		GraphModel = &Definition->EditorModel.CacheGraph;
	}
	else if (Definition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex))
	{
		GraphModel = &Definition->EditorModel.BodyPartGraphs[CurrentBodyPartIndex];
	}

	if (GraphModel == nullptr)
	{
		return;
	}

	CurrentGraph = NewObject<ULbbLayeredBlendBodyEdGraph>(GetTransientPackage(), NAME_None, RF_Transactional);
	CurrentGraph->Schema = ULbbLayeredBlendBodyGraphSchema::StaticClass();
	CurrentGraph->EditingDefinition = EditingDefinition.Get();
	CurrentGraph->GraphKind = CurrentGraphKind;
	CurrentGraph->BodyPartIndex = CurrentGraphKind == ELbbLayeredBlendBodyGraphKind::BodyPart ? CurrentBodyPartIndex : INDEX_NONE;

	TMap<FGuid, ULbbLayeredBlendBodyEdGraphNode*> NodesByGuid;
	for (const FLbbLayeredBlendBodyGraphNodeModel& NodeModel : GraphModel->Nodes)
	{
		ULbbLayeredBlendBodyEdGraphNode* Node = CreateLbbLayeredBlendBodyNodeForDataStruct(
			CurrentGraph,
			NodeModel.NodeData.GetScriptStruct());
		if (Node == nullptr)
		{
			continue;
		}

		CurrentGraph->AddNode(Node, false, false);
		Node->ImportFromNodeModel(NodeModel);
		Node->AllocateDefaultPins();
		NodesByGuid.Add(Node->NodeGuid, Node);
	}

	for (const FLbbLayeredBlendBodyGraphLinkModel& LinkModel : GraphModel->Links)
	{
		ULbbLayeredBlendBodyEdGraphNode* const* FromNodePtr = NodesByGuid.Find(LinkModel.FromNodeGuid);
		ULbbLayeredBlendBodyEdGraphNode* const* ToNodePtr = NodesByGuid.Find(LinkModel.ToNodeGuid);
		if (FromNodePtr == nullptr || ToNodePtr == nullptr)
		{
			continue;
		}

		UEdGraphPin* FromPin = (*FromNodePtr)->FindPin(LinkModel.FromPinName);
		UEdGraphPin* ToPin = (*ToNodePtr)->FindPin(LinkModel.ToPinName);
		if (FromPin != nullptr && ToPin != nullptr)
		{
			FromPin->MakeLinkTo(ToPin);
		}
	}

	CurrentGraphChangedHandle = CurrentGraph->AddOnGraphChangedHandler(
		FOnGraphChanged::FDelegate::CreateSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleGraphChanged));
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::RebuildGraphWidget()
{
	if (!GraphHostBorder.IsValid())
	{
		return;
	}

	GraphEditorWidget.Reset();

	if (CurrentGraph == nullptr)
	{
		GraphHostBorder->SetContent(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(8.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoGraphSelected", "No graph selected."))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(8.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoGraphSelectedHint", "Create a BodyPart or switch to Cache Graph to start authoring."))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(8.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CreateFirstBodyPartButton", "Create BodyPart"))
					.OnClicked_Lambda([this]() -> FReply
					{
						HandleAddBodyPart();
						return FReply::Handled();
					})
				]
			]);
		return;
	}

	SGraphEditor::FGraphEditorEvents GraphEvents;
	GraphEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleGraphSelectionChanged);

	GraphEditorWidget = SNew(SGraphEditor)
		.GraphToEdit(CurrentGraph)
		.GraphEvents(GraphEvents)
		.ShowGraphStateOverlay(false);

	GraphHostBorder->SetContent(GraphEditorWidget.ToSharedRef());

	if (const ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get())
	{
		const FLbbLayeredBlendBodyGraphModelBase* GraphModel = nullptr;
		if (CurrentGraphKind == ELbbLayeredBlendBodyGraphKind::Cache)
		{
			GraphModel = &Definition->EditorModel.CacheGraph;
		}
		else if (Definition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex))
		{
			GraphModel = &Definition->EditorModel.BodyPartGraphs[CurrentBodyPartIndex];
		}

		if (GraphModel != nullptr)
		{
			GraphEditorWidget->SetViewLocation(FVector2f(GraphModel->SavedViewLocation), GraphModel->SavedZoomAmount);
		}
	}
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::RefreshBodyPartCompileSummaries(
	const FLbbCompileResult& CompileResult)
{
	const ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get();
	const int32 BodyPartCount = Definition != nullptr ? Definition->EditorModel.BodyPartGraphs.Num() : 0;

	CacheGraphCompileSummary = FBodyPartCompileSummary();
	BodyPartCompileSummaries.Reset();
	BodyPartCompileSummaries.SetNum(BodyPartCount);

	for (const FLbbCompileMessage& Message : CompileResult.Messages)
	{
		if (Message.GraphKind == ELbbLayeredBlendBodyGraphKind::Cache)
		{
			FBodyPartCompileSummary& Summary = CacheGraphCompileSummary;
			if (Message.Severity == EMessageSeverity::Error)
			{
				++Summary.ErrorCount;
			}
			else if (Message.Severity == EMessageSeverity::Warning || Message.Severity == EMessageSeverity::PerformanceWarning)
			{
				++Summary.WarningCount;
			}
			continue;
		}

		if (!BodyPartCompileSummaries.IsValidIndex(Message.BodyPartIndex))
		{
			continue;
		}

		FBodyPartCompileSummary& Summary = BodyPartCompileSummaries[Message.BodyPartIndex];
		if (Message.Severity == EMessageSeverity::Error)
		{
			++Summary.ErrorCount;
		}
		else if (Message.Severity == EMessageSeverity::Warning || Message.Severity == EMessageSeverity::PerformanceWarning)
		{
			++Summary.WarningCount;
		}
	}
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::RefreshDetailsPanel()
{
	if (!DetailsView.IsValid())
	{
		return;
	}

	if (GraphEditorWidget.IsValid())
	{
		FGraphPanelSelectionSet SelectionSet = GraphEditorWidget->GetSelectedNodes();
		if (SelectionSet.Num() > 0)
		{
			TArray<UObject*> SelectedObjects;
			SelectedObjects.Reserve(SelectionSet.Num());
			for (UObject* SelectedObject : SelectionSet)
			{
				SelectedObjects.Add(SelectedObject);
			}
			DetailsView->SetObjects(SelectedObjects);
			return;
		}
	}

	if (BodyPartDetailsObject != nullptr
		&& CurrentGraphKind == ELbbLayeredBlendBodyGraphKind::BodyPart
		&& EditingDefinition.IsValid()
		&& EditingDefinition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex))
	{
		DetailsView->SetObject(BodyPartDetailsObject);
	}
	else
	{
		DetailsView->SetObject(nullptr);
	}
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::SyncCurrentGraphToModel()
{
	if (bIsSynchronizing || CurrentGraph == nullptr)
	{
		return;
	}

	ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get();
	if (Definition == nullptr)
	{
		return;
	}

	TGuardValue<bool> Guard(bIsSynchronizing, true);

	Definition->Modify();
	FLbbLayeredBlendBodyGraphModelBase* GraphModel = nullptr;
	if (CurrentGraphKind == ELbbLayeredBlendBodyGraphKind::Cache)
	{
		GraphModel = &Definition->EditorModel.CacheGraph;
	}
	else if (Definition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex))
	{
		GraphModel = &Definition->EditorModel.BodyPartGraphs[CurrentBodyPartIndex];
	}

	if (GraphModel == nullptr)
	{
		return;
	}

	GraphModel->Nodes.Reset();
	GraphModel->Links.Reset();

	for (UEdGraphNode* GraphNode : CurrentGraph->Nodes)
	{
		if (ULbbLayeredBlendBodyEdGraphNode* Node = Cast<ULbbLayeredBlendBodyEdGraphNode>(GraphNode))
		{
			FLbbLayeredBlendBodyGraphNodeModel& NodeModel = GraphModel->Nodes.AddDefaulted_GetRef();
			Node->ExportToNodeModel(NodeModel);
		}
	}

	for (UEdGraphNode* GraphNode : CurrentGraph->Nodes)
	{
		ULbbLayeredBlendBodyEdGraphNode* Node = Cast<ULbbLayeredBlendBodyEdGraphNode>(GraphNode);
		if (Node == nullptr)
		{
			continue;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin == nullptr || Pin->Direction != EGPD_Output)
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (LinkedPin == nullptr)
				{
					continue;
				}

				ULbbLayeredBlendBodyEdGraphNode* LinkedNode = Cast<ULbbLayeredBlendBodyEdGraphNode>(LinkedPin->GetOwningNode());
				if (LinkedNode == nullptr)
				{
					continue;
				}

				FLbbLayeredBlendBodyGraphLinkModel& LinkModel = GraphModel->Links.AddDefaulted_GetRef();
				LinkModel.FromNodeGuid = Node->NodeGuid;
				LinkModel.FromPinName = Pin->PinName;
				LinkModel.ToNodeGuid = LinkedNode->NodeGuid;
				LinkModel.ToPinName = LinkedPin->PinName;
			}
		}
	}

	if (GraphEditorWidget.IsValid())
	{
		FVector2f ViewLocation = FVector2f::ZeroVector;
		float ZoomAmount = 1.f;
		GraphEditorWidget->GetViewLocation(ViewLocation, ZoomAmount);
		GraphModel->SavedViewLocation = FVector2D(ViewLocation);
		GraphModel->SavedZoomAmount = ZoomAmount;
	}

	Definition->MarkPackageDirty();
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::SyncBodyPartDetailsToModel()
{
	if (bIsSynchronizing || BodyPartDetailsObject == nullptr)
	{
		return;
	}

	ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get();
	if (CurrentGraphKind != ELbbLayeredBlendBodyGraphKind::BodyPart
		|| Definition == nullptr
		|| !Definition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex))
	{
		return;
	}

	Definition->Modify();
	FLbbLayeredBlendBodyPartGraphModel& GraphModel = Definition->EditorModel.BodyPartGraphs[CurrentBodyPartIndex];
	GraphModel.PartName = BodyPartDetailsObject->PartName;
	GraphModel.DebugColor = BodyPartDetailsObject->DebugColor;
	Definition->MarkPackageDirty();
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::RefreshBodyPartDetailsObject()
{
	if (BodyPartDetailsObject == nullptr)
	{
		return;
	}

	const ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get();
	if (CurrentGraphKind != ELbbLayeredBlendBodyGraphKind::BodyPart
		|| Definition == nullptr
		|| !Definition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex))
	{
		BodyPartDetailsObject->PartName = NAME_None;
		BodyPartDetailsObject->DebugColor = FLinearColor::Green;
		return;
	}

	const FLbbLayeredBlendBodyPartGraphModel& GraphModel = Definition->EditorModel.BodyPartGraphs[CurrentBodyPartIndex];
	BodyPartDetailsObject->PartName = GraphModel.PartName;
	BodyPartDetailsObject->DebugColor = GraphModel.DebugColor;
}

bool FLbbLayeredBlendBodyDefinitionEditorToolkit::CompileDefinition(const bool bMarkDirty, const bool bForceRefreshMessages)
{
	ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get();
	if (Definition == nullptr)
	{
		return false;
	}

	SyncCurrentGraphToModel();
	SyncBodyPartDetailsToModel();

	const FLbbCompileResult CompileResult
		= FLbbLayeredBlendBodyGraphCompiler::Compile(*Definition);

	RefreshBodyPartCompileSummaries(CompileResult);
	ApplyCompileMessagesToCurrentGraph(CompileResult);

	if (bForceRefreshMessages)
	{
		RefreshCompileMessages(CompileResult);
	}

	if (CompileResult.bSuccess && bMarkDirty)
	{
		Definition->Modify();
		Definition->MarkPackageDirty();
	}

	RefreshCompiledOperatorsPreview();
	RefreshBodyPartItems();
	return CompileResult.bSuccess;
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::RefreshCompileMessages(
	const FLbbCompileResult& CompileResult)
{
	CompileMessageItems.Reset();

	if (CompileResult.Messages.IsEmpty())
	{
		TSharedPtr<FCompileMessageListItem> Item = MakeShared<FCompileMessageListItem>();
		Item->Severity = CompileResult.bSuccess
			? EMessageSeverity::Info
			: EMessageSeverity::Error;
		Item->DisplayText = CompileResult.bSuccess
			? LOCTEXT("CompileSucceededMessage", "Compile succeeded.")
			: LOCTEXT("CompileFailedMessage", "Compile failed.");
		Item->ToolTipText = Item->DisplayText;
		CompileMessageItems.Add(Item);
	}
	else
	{
		const ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get();
		for (const FLbbCompileMessage& Message : CompileResult.Messages)
		{
			const FString GraphLabel = GetGraphLabel(Definition, Message.GraphKind, Message.BodyPartIndex);

			TSharedPtr<FCompileMessageListItem> Item = MakeShared<FCompileMessageListItem>();
			Item->Severity = Message.Severity;
			Item->GraphKind = Message.GraphKind;
			Item->BodyPartIndex = Message.BodyPartIndex;
			Item->NodeGuid = Message.NodeGuid;
			Item->bCanNavigate = Message.GraphKind == ELbbLayeredBlendBodyGraphKind::Cache || Message.BodyPartIndex != INDEX_NONE;
			Item->DisplayText = FText::FromString(FString::Printf(
				TEXT("[%s] [%s] %s"),
				*GetSeverityLabel(Message.Severity),
				*GraphLabel,
				*Message.Message));
			Item->ToolTipText = Item->bCanNavigate
				? FText::FromString(FString::Printf(
					TEXT("[%s] [%s] %s\nClick to locate this message in the graph."),
					*GetSeverityLabel(Message.Severity),
					*GraphLabel,
					*Message.Message))
				: Item->DisplayText;
			CompileMessageItems.Add(Item);
		}
	}

	if (CompileMessagesListView.IsValid())
	{
		CompileMessagesListView->ClearSelection();
		CompileMessagesListView->RequestListRefresh();
	}
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::RefreshCompiledOperatorsPreview()
{
	if (CompiledOperatorsPreviewTextBox.IsValid())
	{
		CompiledOperatorsPreviewTextBox->SetText(FText::FromString(BuildCompiledOperatorsPreviewText()));
	}
}

FString FLbbLayeredBlendBodyDefinitionEditorToolkit::BuildCompiledOperatorsPreviewText() const
{
	const ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get();
	if (Definition == nullptr)
	{
		return TEXT("No definition is being edited.");
	}

	TArray<FString> Lines;
	Lines.Reserve(64);
	Lines.Add(TEXT("Preview Source: Runtime Cache Graph + BodyParts stored on this asset."));
	Lines.Add(TEXT("This panel reflects the current compiled uasset data, not the transient editor graph."));

	const FString CachePrefix = CurrentGraphKind == ELbbLayeredBlendBodyGraphKind::Cache ? TEXT(">> ") : TEXT("   ");
	Lines.Add(TEXT(""));
	Lines.Add(FString::Printf(TEXT("%s[Cache Graph]"), *CachePrefix));
	Lines.Add(FString::Printf(TEXT("Named Caches: %d"), Definition->CacheProgram.NamedCacheNames.Num()));
	Lines.Add(FString::Printf(TEXT("Operators: %d"), Definition->CacheProgram.Operators.Num()));
	if (Definition->CacheProgram.Operators.IsEmpty())
	{
		Lines.Add(TEXT("  <No operators>"));
	}
	else
	{
		for (int32 OperatorIndex = 0; OperatorIndex < Definition->CacheProgram.Operators.Num(); ++OperatorIndex)
		{
			FString OperatorText = TEXT("<Invalid Operator>");
			if (const FLbbLayeredBlendBodyPartOperatorBase* Operator = Definition->CacheProgram.Operators[OperatorIndex].GetPtr<FLbbLayeredBlendBodyPartOperatorBase>())
			{
				OperatorText = Operator->ToString();
			}

			Lines.Add(FString::Printf(TEXT("  %d. %s"), OperatorIndex + 1, *OperatorText));
		}
	}

	if (Definition->BodyParts.IsEmpty())
	{
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("No compiled BodyParts are currently stored on the asset."));
		return FString::Join(Lines, TEXT("\n"));
	}

	for (int32 BodyPartIndex = 0; BodyPartIndex < Definition->BodyParts.Num(); ++BodyPartIndex)
	{
		const FLbbLayeredBlendBodyPart& BodyPart = Definition->BodyParts[BodyPartIndex];
		const FString PartName = BodyPart.PartName.IsNone()
			? FString::Printf(TEXT("BodyPart_%d"), BodyPartIndex + 1)
			: BodyPart.PartName.ToString();
		const FString BodyPartPrefix = (CurrentGraphKind == ELbbLayeredBlendBodyGraphKind::BodyPart && BodyPartIndex == CurrentBodyPartIndex) ? TEXT(">> ") : TEXT("   ");

		if (BodyPartIndex > 0)
		{
			Lines.Add(TEXT(""));
		}

		Lines.Add(FString::Printf(TEXT("%s[BodyPart %d] %s"), *BodyPartPrefix, BodyPartIndex + 1, *PartName));
		Lines.Add(FString::Printf(TEXT("Operators: %d"), BodyPart.Operators.Num()));

		if (BodyPart.Operators.IsEmpty())
		{
			Lines.Add(TEXT("  <No operators>"));
			continue;
		}

		for (int32 OperatorIndex = 0; OperatorIndex < BodyPart.Operators.Num(); ++OperatorIndex)
		{
			FString OperatorText = TEXT("<Invalid Operator>");
			if (const FLbbLayeredBlendBodyPartOperatorBase* Operator = BodyPart.Operators[OperatorIndex].GetPtr<FLbbLayeredBlendBodyPartOperatorBase>())
			{
				OperatorText = Operator->ToString();
			}

			Lines.Add(FString::Printf(
				TEXT("  %d. %s"),
				OperatorIndex + 1,
				*OperatorText));
		}
	}

	return FString::Join(Lines, TEXT("\n"));
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::NavigateToCompileMessage(const FCompileMessageListItem& Item)
{
	if (!Item.bCanNavigate)
	{
		return;
	}

	if (Item.GraphKind == ELbbLayeredBlendBodyGraphKind::Cache)
	{
		SelectCacheGraph();
	}
	else if (Item.BodyPartIndex != INDEX_NONE && Item.BodyPartIndex != CurrentBodyPartIndex)
	{
		SelectBodyPart(Item.BodyPartIndex);
	}

	if (!Item.NodeGuid.IsValid() || !GraphEditorWidget.IsValid())
	{
		return;
	}

	if (ULbbLayeredBlendBodyEdGraphNode* Node = FindCurrentGraphNodeByGuid(Item.NodeGuid))
	{
		GraphEditorWidget->ClearSelectionSet();
		GraphEditorWidget->SetNodeSelection(Node, true);
		GraphEditorWidget->JumpToNode(Node, false, true);
		RefreshDetailsPanel();
	}
}

ULbbLayeredBlendBodyEdGraphNode* FLbbLayeredBlendBodyDefinitionEditorToolkit::FindCurrentGraphNodeByGuid(const FGuid& NodeGuid) const
{
	if (CurrentGraph == nullptr || !NodeGuid.IsValid())
	{
		return nullptr;
	}

	for (UEdGraphNode* GraphNode : CurrentGraph->Nodes)
	{
		ULbbLayeredBlendBodyEdGraphNode* LbbNode = Cast<ULbbLayeredBlendBodyEdGraphNode>(GraphNode);
		if (LbbNode != nullptr && LbbNode->NodeGuid == NodeGuid)
		{
			return LbbNode;
		}
	}

	return nullptr;
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleGraphChanged(const FEdGraphEditAction& GraphEditAction)
{
	if (bIsSynchronizing)
	{
		return;
	}

	SyncCurrentGraphToModel();
	CompileDefinition(true);
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleGraphSelectionChanged(const TSet<UObject*>& NewSelection)
{
	RefreshDetailsPanel();
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleDetailsFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (bIsSynchronizing)
	{
		return;
	}

	if (GraphEditorWidget.IsValid() && GraphEditorWidget->GetSelectedNodes().Num() > 0)
	{
		SyncCurrentGraphToModel();
		CompileDefinition(true);
		return;
	}

	SyncBodyPartDetailsToModel();
	RefreshBodyPartItems();
	CompileDefinition(true);
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleAddBodyPart()
{
	ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get();
	if (Definition == nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddBodyPartTransaction", "Add Layered Blend Body Part"));
	Definition->Modify();

	FLbbLayeredBlendBodyPartGraphModel& NewGraphModel = Definition->EditorModel.BodyPartGraphs.AddDefaulted_GetRef();
	FLbbLayeredBlendBodyGraphCompiler::CreateDefaultBodyPartGraph(NewGraphModel, MakeUniqueBodyPartName(*Definition));
	Definition->MarkPackageDirty();

	RefreshBodyPartItems();
	SelectBodyPart(Definition->EditorModel.BodyPartGraphs.Num() - 1);
	CompileDefinition(true);
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleDuplicateBodyPart()
{
	ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get();
	if (Definition == nullptr || !Definition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DuplicateBodyPartTransaction", "Duplicate Layered Blend Body Part"));
	Definition->Modify();

	FLbbLayeredBlendBodyPartGraphModel DuplicatedGraphModel = Definition->EditorModel.BodyPartGraphs[CurrentBodyPartIndex];
	RemapGraphGuids(DuplicatedGraphModel);
	DuplicatedGraphModel.PartName = MakeUniqueBodyPartName(*Definition);
	Definition->EditorModel.BodyPartGraphs.Insert(DuplicatedGraphModel, CurrentBodyPartIndex + 1);
	Definition->MarkPackageDirty();

	RefreshBodyPartItems();
	SelectBodyPart(CurrentBodyPartIndex + 1);
	CompileDefinition(true);
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleDeleteBodyPart()
{
	ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get();
	if (Definition == nullptr || !Definition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DeleteBodyPartTransaction", "Delete Layered Blend Body Part"));
	Definition->Modify();

	const int32 RemovedBodyPartIndex = CurrentBodyPartIndex;
	Definition->EditorModel.BodyPartGraphs.RemoveAt(RemovedBodyPartIndex);
	Definition->MarkPackageDirty();
	ClearCurrentGraphChangedHandler();
	CurrentGraph = nullptr;

	if (Definition->EditorModel.BodyPartGraphs.IsEmpty())
	{
		CurrentBodyPartIndex = INDEX_NONE;
	}
	else
	{
		CurrentBodyPartIndex = FMath::Clamp(RemovedBodyPartIndex, 0, Definition->EditorModel.BodyPartGraphs.Num() - 1);
	}

	RefreshBodyPartItems();
	if (CurrentBodyPartIndex != INDEX_NONE)
	{
		SelectBodyPart(CurrentBodyPartIndex, true);
	}
	else
	{
		RebuildGraphWidget();
		RefreshDetailsPanel();
	}
	CompileDefinition(true);
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleMoveBodyPartUp()
{
	ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get();
	if (Definition == nullptr || CurrentBodyPartIndex <= 0 || !Definition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("MoveBodyPartUpTransaction", "Move Layered Blend Body Part Up"));
	Definition->Modify();
	Definition->EditorModel.BodyPartGraphs.Swap(CurrentBodyPartIndex, CurrentBodyPartIndex - 1);
	--CurrentBodyPartIndex;
	Definition->MarkPackageDirty();

	RefreshBodyPartItems();
	SelectBodyPart(CurrentBodyPartIndex, true);
	CompileDefinition(true);
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleMoveBodyPartDown()
{
	ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get();
	if (Definition == nullptr || !Definition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex + 1))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("MoveBodyPartDownTransaction", "Move Layered Blend Body Part Down"));
	Definition->Modify();
	Definition->EditorModel.BodyPartGraphs.Swap(CurrentBodyPartIndex, CurrentBodyPartIndex + 1);
	++CurrentBodyPartIndex;
	Definition->MarkPackageDirty();

	RefreshBodyPartItems();
	SelectBodyPart(CurrentBodyPartIndex, true);
	CompileDefinition(true);
}

bool FLbbLayeredBlendBodyDefinitionEditorToolkit::CanAddBodyPart() const
{
	return EditingDefinition.IsValid();
}

bool FLbbLayeredBlendBodyDefinitionEditorToolkit::HasValidBodyPartSelection() const
{
	const ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get();
	return CurrentGraphKind == ELbbLayeredBlendBodyGraphKind::BodyPart
		&& Definition != nullptr
		&& Definition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex);
}

bool FLbbLayeredBlendBodyDefinitionEditorToolkit::CanDuplicateBodyPart() const
{
	return HasValidBodyPartSelection();
}

bool FLbbLayeredBlendBodyDefinitionEditorToolkit::CanDeleteBodyPart() const
{
	return HasValidBodyPartSelection();
}

bool FLbbLayeredBlendBodyDefinitionEditorToolkit::CanMoveBodyPartUp() const
{
	return HasValidBodyPartSelection() && CurrentBodyPartIndex > 0;
}

bool FLbbLayeredBlendBodyDefinitionEditorToolkit::CanMoveBodyPartDown() const
{
	const ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get();
	return CurrentGraphKind == ELbbLayeredBlendBodyGraphKind::BodyPart
		&& Definition != nullptr
		&& Definition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex)
		&& Definition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex + 1);
}

TSharedRef<SDockTab> FLbbLayeredBlendBodyDefinitionEditorToolkit::SpawnBodyPartListTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
	[
		BuildBodyPartListTabContent()
	];
}

TSharedRef<SDockTab> FLbbLayeredBlendBodyDefinitionEditorToolkit::SpawnGraphTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
	[
		BuildGraphTabContent()
	];
}

TSharedRef<SDockTab> FLbbLayeredBlendBodyDefinitionEditorToolkit::SpawnDetailsTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
	[
		DetailsView.IsValid()
			? DetailsView.ToSharedRef()
			: SNullWidget::NullWidget
	];
}

TSharedRef<SDockTab> FLbbLayeredBlendBodyDefinitionEditorToolkit::SpawnCompiledOperatorsPreviewTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> Tab = SNew(SDockTab)
	[
		SAssignNew(CompiledOperatorsPreviewTextBox, SMultiLineEditableTextBox)
		.IsReadOnly(true)
		.AutoWrapText(false)
		.Text(FText::FromString(BuildCompiledOperatorsPreviewText()))
	];

	RefreshCompiledOperatorsPreview();
	return Tab;
}

TSharedRef<SDockTab> FLbbLayeredBlendBodyDefinitionEditorToolkit::SpawnCompileMessagesTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
	[
		BuildCompileMessagesTabContent()
	];
}

TSharedRef<ITableRow> FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleGenerateBodyPartRow(
	TSharedPtr<int32> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	const ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get();
	const FLbbLayeredBlendBodyPartGraphModel* GraphModel = (Definition != nullptr && Item.IsValid() && Definition->EditorModel.BodyPartGraphs.IsValidIndex(*Item))
		? &Definition->EditorModel.BodyPartGraphs[*Item]
		: nullptr;

	const FText RowText = GraphModel != nullptr
		? FText::FromName(GraphModel->PartName)
		: LOCTEXT("InvalidBodyPartRow", "Invalid BodyPart");

	const FLinearColor RowColor = GraphModel != nullptr ? GraphModel->DebugColor : FLinearColor::Gray;
	const FBodyPartCompileSummary Summary =
		(Item.IsValid() && BodyPartCompileSummaries.IsValidIndex(*Item))
		? BodyPartCompileSummaries[*Item]
		: FBodyPartCompileSummary();

	return SNew(STableRow<TSharedPtr<int32>>, OwnerTable)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.f)
		[
			SNew(SBorder)
			.BorderBackgroundColor(RowColor)
			.Padding(FMargin(8.f, 6.f))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		.Padding(6.f, 2.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(RowText)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
					.Text(FText::FromString(FString::Printf(TEXT("Index: %d"), Item.IsValid() ? (*Item + 1) : 0)))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SSpacer)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SHorizontalBox)
					.Visibility((Summary.ErrorCount > 0 || Summary.WarningCount > 0) ? EVisibility::Visible : EVisibility::Collapsed)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.Visibility(Summary.ErrorCount > 0 ? EVisibility::Visible : EVisibility::Collapsed)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.ErrorWithColor"))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(4.f, 0.f, 0.f, 0.f)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(FText::FromString(FString::Printf(TEXT(" Errors:  %d"), Summary.ErrorCount)))
							]
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(10.f, 0.f, 0.f, 0.f)
					[
						SNew(SBox)
						.Visibility(Summary.WarningCount > 0 ? EVisibility::Visible : EVisibility::Collapsed)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.WarningWithColor"))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(4.f, 0.f, 0.f, 0.f)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(FText::FromString(FString::Printf(TEXT("Warnings: %d"), Summary.WarningCount)))
							]
						]
					]
				]
			]
		]
	];
}

TSharedRef<ITableRow> FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleGenerateCompileMessageRow(
	TSharedPtr<FCompileMessageListItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	const EMessageSeverity::Type Severity =
		Item.IsValid()
		? Item->Severity
		: EMessageSeverity::Info;

	return SNew(STableRow<TSharedPtr<FCompileMessageListItem>>, OwnerTable)
		.ToolTipText(Item.IsValid() ? Item->ToolTipText : FText::GetEmpty())
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
		.Padding(FMargin(8.f, 6.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			.Padding(0.f, 1.f, 8.f, 0.f)
			[
				SNew(SImage)
				.Image(GetSeverityBrush(Severity))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Item.IsValid() ? Item->DisplayText : FText::GetEmpty())
				.ColorAndOpacity(GetSeverityTextColor(Severity))
				.AutoWrapText(true)
			]
		]
	];
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleBodyPartSelectionChanged(TSharedPtr<int32> Item, ESelectInfo::Type SelectInfo)
{
	if (!Item.IsValid())
	{
		return;
	}

	SelectBodyPart(*Item);
}

void FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleCompileMessageSelectionChanged(
	TSharedPtr<FCompileMessageListItem> Item,
	ESelectInfo::Type SelectInfo)
{
	if (!Item.IsValid())
	{
		return;
	}

	NavigateToCompileMessage(*Item);

	if (CompileMessagesListView.IsValid())
	{
		CompileMessagesListView->ClearSelection();
	}
}

FReply FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleBodyPartListKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (BodyPartCommandList.IsValid() && BodyPartCommandList->ProcessCommandBindings(KeyEvent))
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedPtr<SWidget> FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleBodyPartListContextMenuOpening()
{
	if (!BodyPartCommandList.IsValid())
	{
		return nullptr;
	}

	const FLbbLayeredBlendBodyDefinitionEditorCommands& Commands = FLbbLayeredBlendBodyDefinitionEditorCommands::Get();
	FMenuBuilder MenuBuilder(true, BodyPartCommandList);

	MenuBuilder.BeginSection("BodyPartEdit", LOCTEXT("BodyPartContextMenuEditSection", "Body Part"));
	MenuBuilder.AddMenuEntry(Commands.AddBodyPart, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"));
	MenuBuilder.AddMenuEntry(Commands.DuplicateBodyPart, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Duplicate"));
	MenuBuilder.AddMenuEntry(Commands.DeleteBodyPart, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"));
	MenuBuilder.AddSeparator();
	MenuBuilder.AddMenuEntry(Commands.MoveBodyPartUp, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Symbols.DoubleUpArrow"));
	MenuBuilder.AddMenuEntry(Commands.MoveBodyPartDown, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Symbols.DoubleDownArrow"));
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> FLbbLayeredBlendBodyDefinitionEditorToolkit::BuildBodyPartListTabContent()
{
	auto MakeBodyPartToolBarWidget = [this]() -> TSharedRef<SWidget>
	{
		FToolBarBuilder ToolbarBuilder(BodyPartCommandList, FMultiBoxCustomization::None);
		ToolbarBuilder.SetStyle(&FAppStyle::Get(), "AssetEditorToolbar");
		ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

		const FLbbLayeredBlendBodyDefinitionEditorCommands& Commands = FLbbLayeredBlendBodyDefinitionEditorCommands::Get();
		const ISlateStyle& LocalStyle = GetLayeredBlendBodyPartEditorStyle();

		ToolbarBuilder.BeginSection("BodyPartActions");
		{
			ToolbarBuilder.AddToolBarButton(Commands.AddBodyPart, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"));
			ToolbarBuilder.AddToolBarButton(Commands.DuplicateBodyPart, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Duplicate"));
			ToolbarBuilder.AddToolBarButton(Commands.DeleteBodyPart, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"));

			ToolbarBuilder.AddSeparator();

			ToolbarBuilder.AddToolBarButton(Commands.MoveBodyPartUp, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(LocalStyle.GetStyleSetName(), LargeDoubleUpArrowBrushName));
			ToolbarBuilder.AddToolBarButton(Commands.MoveBodyPartDown, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(LocalStyle.GetStyleSetName(), LargeDoubleDownArrowBrushName));
		}
		ToolbarBuilder.EndSection();

		return SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("AssetEditorToolbar.Background"))
			.Padding(FMargin(0.0f))
			[
				ToolbarBuilder.MakeWidget()
		];
	};

	auto MakeCacheGraphButton = [this]() -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ButtonStyle(&FAppStyle::Get(), "FlatButton")
			.ContentPadding(FMargin(10.f, 8.f))
			.OnClicked_Lambda([this]() -> FReply
			{
				SelectCacheGraph();
				return FReply::Handled();
			})
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CacheGraphLabel", "Cache Graph"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.75f, 0.95f)))
					.Text_Lambda([this]() -> FText
					{
						const ULbbLayeredBlendBodyDefinition* Definition = EditingDefinition.Get();
						const int32 NodeCount = Definition != nullptr ? Definition->EditorModel.CacheGraph.Nodes.Num() : 0;
						const int32 NamedCacheCount = Definition != nullptr ? Definition->CacheProgram.NamedCacheNames.Num() : 0;
						return FText::FromString(FString::Printf(
							TEXT("Nodes: %d  Named Caches: %d  Errors: %d  Warnings: %d"),
							NodeCount,
							NamedCacheCount,
							CacheGraphCompileSummary.ErrorCount,
							CacheGraphCompileSummary.WarningCount));
					})
				]
			];
	};

	TSharedRef<SWidget> Widget =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f)
		[
			MakeCacheGraphButton()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f)
		[
			MakeBodyPartToolBarWidget()
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.Padding(4.f)
		[
			SAssignNew(BodyPartListView, SListView<TSharedPtr<int32>>)
			.ListItemsSource(&BodyPartItems)
			.OnGenerateRow(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleGenerateBodyPartRow)
			.OnSelectionChanged(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleBodyPartSelectionChanged)
			.OnContextMenuOpening(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleBodyPartListContextMenuOpening)
			.OnKeyDownHandler(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleBodyPartListKeyDown)
		];

	RefreshBodyPartItems();
	return Widget;
}

TSharedRef<SWidget> FLbbLayeredBlendBodyDefinitionEditorToolkit::BuildGraphTabContent()
{
	TSharedRef<SWidget> Widget =
		SNew(SBorder)
		.Padding(2.f)
		[
			SAssignNew(GraphHostBorder, SBorder)
		];

	RebuildGraphWidget();
	RefreshDetailsPanel();
	return Widget;
}

TSharedRef<SWidget> FLbbLayeredBlendBodyDefinitionEditorToolkit::BuildCompileMessagesTabContent()
{
	TSharedRef<SWidget> Widget =
		SNew(SBox)
		.MinDesiredHeight(148.f)
		[
			SAssignNew(CompileMessagesListView, SListView<TSharedPtr<FCompileMessageListItem>>)
			.ListItemsSource(&CompileMessageItems)
			.OnGenerateRow(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleGenerateCompileMessageRow)
			.OnSelectionChanged(this, &FLbbLayeredBlendBodyDefinitionEditorToolkit::HandleCompileMessageSelectionChanged)
			.SelectionMode(ESelectionMode::Single)
		];

	if (CompileMessagesListView.IsValid())
	{
		CompileMessagesListView->RequestListRefresh();
	}

	return Widget;
}

#undef LOCTEXT_NAMESPACE
