// Fill out your copyright notice in the Description page of Project Settings.

#include "LbbDefinitionEditorToolkit.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "GraphEditor.h"
#include "IDetailsView.h"
#include "LbbBodyPartDefinitionDataAsset.h"
#include "LbbBodyPartDetailsObject.h"
#include "LbbInputListDetailsObject.h"
#include "LbbEdGraph.h"
#include "GraphNodes/LbbEdGraphNode.h"
#include "LbbGraphCompiler.h"
#include "LbbGraphNodeRegistry.h"
#include "LbbGraphSchema.h"
#include "LbbOperators.h"
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

#define LOCTEXT_NAMESPACE "LbbDefinitionEditorToolkit"

namespace
{
	static const FName LayeredBlendBodyPartEditorStyleSetName(TEXT("LbbPartEditorLocalStyle"));
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

	class FLbbDefinitionEditorCommands final : public TCommands<FLbbDefinitionEditorCommands>
	{
	public:
		FLbbDefinitionEditorCommands()
			: TCommands<FLbbDefinitionEditorCommands>(
				TEXT("LbbDefinitionEditorCommands"),
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

	static FString GetBodyPartLabel(const ULbbBodyPartDefinitionDataAsset* Definition, const int32 BodyPartIndex)
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
		const ULbbBodyPartDefinitionDataAsset* Definition,
		const ELbbGraphKind GraphKind,
		const int32 BodyPartIndex)
	{
		return GraphKind == ELbbGraphKind::Cache
			? TEXT("Cache Graph")
			: GetBodyPartLabel(Definition, BodyPartIndex);
	}

	static FName MakeUniqueBodyPartName(const ULbbBodyPartDefinitionDataAsset& Definition)
	{
		int32 Suffix = Definition.EditorModel.BodyPartGraphs.Num();
		while (true)
		{
			const FName Candidate(*FString::Printf(TEXT("BodyPart_%d"), Suffix));
			bool bExists = false;
			for (const FLbbPartGraphModel& GraphModel : Definition.EditorModel.BodyPartGraphs)
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

	static void RemapGraphGuids(FLbbGraphModelBase& GraphModel)
	{
		TMap<FGuid, FGuid> GuidRemap;
		GraphModel.GraphGuid = FGuid::NewGuid();

		for (FLbbGraphNodeModel& NodeModel : GraphModel.Nodes)
		{
			const FGuid OldGuid = NodeModel.NodeGuid;
			NodeModel.NodeGuid = FGuid::NewGuid();
			GuidRemap.Add(OldGuid, NodeModel.NodeGuid);
		}

		for (FLbbGraphLinkModel& LinkModel : GraphModel.Links)
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

	
	static bool AreCompiledBodyPartsEqual(
		const TArray<FLbbPart>& Left,
		const TArray<FLbbPart>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index].PartName != Right[Index].PartName)
			{
				return false;
			}

			if (Left[Index].Operators != Right[Index].Operators)
			{
				return false;
			}

#if WITH_EDITORONLY_DATA
			if (Left[Index].DebugColor != Right[Index].DebugColor)
			{
				return false;
			}
#endif
		}

		return true;
	}

	static bool AreCompiledDefinitionsEqual(
		const FLbbCompiledDefinitionData& CompiledDefinition,
		const ULbbBodyPartDefinitionDataAsset& Definition)
	{
		return CompiledDefinition.CacheProgram.NamedCacheNames == Definition.CacheProgram.NamedCacheNames
			&& (CompiledDefinition.CacheProgram.Operators == Definition.CacheProgram.Operators)
			&& AreCompiledBodyPartsEqual(CompiledDefinition.BodyParts, Definition.BodyParts);
	}

	static bool AreInputDefinitionsEqual(
		const TArray<FLbbInputPoseDefinition>& Left,
		const TArray<FLbbInputPoseDefinition>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index].InputName != Right[Index].InputName)
			{
				return false;
			}
		}

		return true;
	}

}

const FName FLbbDefinitionEditorToolkit::BodyPartListTabId(TEXT("LbbDefinitionEditorToolkit_BodyParts"));
const FName FLbbDefinitionEditorToolkit::GraphTabId(TEXT("LbbDefinitionEditorToolkit_Graph"));
const FName FLbbDefinitionEditorToolkit::DetailsTabId(TEXT("LbbDefinitionEditorToolkit_Details"));
const FName FLbbDefinitionEditorToolkit::InputsTabId(TEXT("LbbDefinitionEditorToolkit_Inputs"));
const FName FLbbDefinitionEditorToolkit::CompiledOperatorsPreviewTabId(TEXT("LbbDefinitionEditorToolkit_CompiledOperatorsPreview"));
const FName FLbbDefinitionEditorToolkit::CompileMessagesTabId(TEXT("LbbDefinitionEditorToolkit_CompileMessages"));
const FName FLbbDefinitionEditorToolkit::EditorAppName(TEXT("LbbDefinitionEditorApp"));

TSharedRef<FLbbDefinitionEditorToolkit> FLbbDefinitionEditorToolkit::CreateEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	ULbbBodyPartDefinitionDataAsset* InDefinition)
{
	TSharedRef<FLbbDefinitionEditorToolkit> NewEditor = MakeShareable(new FLbbDefinitionEditorToolkit());
	NewEditor->InitEditor(Mode, InitToolkitHost, InDefinition);
	return NewEditor;
}

FLbbDefinitionEditorToolkit::~FLbbDefinitionEditorToolkit()
{
	ClearCurrentGraphChangedHandler();
}

void FLbbDefinitionEditorToolkit::InitEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	ULbbBodyPartDefinitionDataAsset* InDefinition)
{
	if (!FLbbDefinitionEditorCommands::IsRegistered())
	{
		FLbbDefinitionEditorCommands::Register();
	}

	EditingDefinition = InDefinition;

	BodyPartCommandList = MakeShared<FUICommandList>();
	BodyPartDetailsObject = NewObject<ULbbBodyPartDetailsObject>(GetTransientPackage(), NAME_None, RF_Transactional);
	InputListDetailsObject = NewObject<ULbbInputListDetailsObject>(GetTransientPackage(), NAME_None, RF_Transactional);
	BindBodyPartCommands();

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.NotifyHook = nullptr;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->OnFinishedChangingProperties().AddSP(this, &FLbbDefinitionEditorToolkit::HandleDetailsFinishedChangingProperties);
	InputDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	InputDetailsView->OnFinishedChangingProperties().AddSP(this, &FLbbDefinitionEditorToolkit::HandleInputDetailsFinishedChangingProperties);

	RefreshBodyPartItems();
	if (BodyPartItems.Num() > 0)
	{
		CurrentBodyPartIndex = 0;
	}
	CurrentGraphKind = ELbbGraphKind::Cache;
	RefreshBodyPartDetailsObject();
	RefreshInputDetailsObject();
	if (InputDetailsView.IsValid())
	{
		InputDetailsView->SetObject(InputListDetailsObject.Get());
	}

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("Standalone_LbbDefinitionEditor_v3")
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
					->AddTab(InputsTabId, ETabState::OpenedTab)
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

	if (CurrentGraphKind == ELbbGraphKind::Cache)
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

FName FLbbDefinitionEditorToolkit::GetToolkitFName() const
{
	return FName(TEXT("LbbDefinitionEditor"));
}

FText FLbbDefinitionEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Layered Blend Body Definition Editor");
}

FString FLbbDefinitionEditorToolkit::GetWorldCentricTabPrefix() const
{
	return TEXT("LayeredBlendBodyDefinition");
}

FLinearColor FLbbDefinitionEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

void FLbbDefinitionEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(BodyPartListTabId, FOnSpawnTab::CreateSP(this, &FLbbDefinitionEditorToolkit::SpawnBodyPartListTab))
		.SetDisplayName(LOCTEXT("BodyPartListTabTitle", "Body Parts"))
		.SetGroup(GetWorkspaceMenuCategory());
	InTabManager->RegisterTabSpawner(GraphTabId, FOnSpawnTab::CreateSP(this, &FLbbDefinitionEditorToolkit::SpawnGraphTab))
		.SetDisplayName(LOCTEXT("GraphTabTitle", "Graph"))
		.SetGroup(GetWorkspaceMenuCategory());
	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FLbbDefinitionEditorToolkit::SpawnDetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTabTitle", "Details"))
		.SetGroup(GetWorkspaceMenuCategory());
	InTabManager->RegisterTabSpawner(InputsTabId, FOnSpawnTab::CreateSP(this, &FLbbDefinitionEditorToolkit::SpawnInputsTab))
		.SetDisplayName(LOCTEXT("InputsTabTitle", "Inputs"))
		.SetGroup(GetWorkspaceMenuCategory());
	InTabManager->RegisterTabSpawner(CompiledOperatorsPreviewTabId, FOnSpawnTab::CreateSP(this, &FLbbDefinitionEditorToolkit::SpawnCompiledOperatorsPreviewTab))
		.SetDisplayName(LOCTEXT("CompiledOperatorsPreviewTabTitle", "Compiled Operators Preview"))
		.SetGroup(GetWorkspaceMenuCategory());
	InTabManager->RegisterTabSpawner(CompileMessagesTabId, FOnSpawnTab::CreateSP(this, &FLbbDefinitionEditorToolkit::SpawnCompileMessagesTab))
		.SetDisplayName(LOCTEXT("CompileMessagesTabTitle", "Compile Messages"))
		.SetGroup(GetWorkspaceMenuCategory());
}

void FLbbDefinitionEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner(BodyPartListTabId);
	InTabManager->UnregisterTabSpawner(GraphTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(InputsTabId);
	InTabManager->UnregisterTabSpawner(CompiledOperatorsPreviewTabId);
	InTabManager->UnregisterTabSpawner(CompileMessagesTabId);
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

void FLbbDefinitionEditorToolkit::SaveAsset_Execute()
{
	if (!CompileDefinition(true))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("CompileBeforeSaveFailed", "The Layered Blend Body Definition has compile errors. Save has been blocked."));
		return;
	}

	FAssetEditorToolkit::SaveAsset_Execute();
}

void FLbbDefinitionEditorToolkit::ClearCurrentGraphChangedHandler()
{
	if (CurrentGraph != nullptr && CurrentGraphChangedHandle.IsValid())
	{
		CurrentGraph->RemoveOnGraphChangedHandler(CurrentGraphChangedHandle);
		CurrentGraphChangedHandle.Reset();
	}
}

void FLbbDefinitionEditorToolkit::ExtendToolbar()
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
		FToolBarExtensionDelegate::CreateSP(this, &FLbbDefinitionEditorToolkit::FillToolbar));

	AddToolbarExtender(ToolbarExtender);
}

void FLbbDefinitionEditorToolkit::FillToolbar(FToolBarBuilder& ToolbarBuilder)
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

void FLbbDefinitionEditorToolkit::BindBodyPartCommands()
{
	if (!BodyPartCommandList.IsValid())
	{
		return;
	}

	const FLbbDefinitionEditorCommands& Commands = FLbbDefinitionEditorCommands::Get();

	BodyPartCommandList->MapAction(
		Commands.AddBodyPart,
		FExecuteAction::CreateSP(this, &FLbbDefinitionEditorToolkit::HandleAddBodyPart),
		FCanExecuteAction::CreateSP(this, &FLbbDefinitionEditorToolkit::CanAddBodyPart));

	BodyPartCommandList->MapAction(
		Commands.DuplicateBodyPart,
		FExecuteAction::CreateSP(this, &FLbbDefinitionEditorToolkit::HandleDuplicateBodyPart),
		FCanExecuteAction::CreateSP(this, &FLbbDefinitionEditorToolkit::CanDuplicateBodyPart));

	BodyPartCommandList->MapAction(
		Commands.DeleteBodyPart,
		FExecuteAction::CreateSP(this, &FLbbDefinitionEditorToolkit::HandleDeleteBodyPart),
		FCanExecuteAction::CreateSP(this, &FLbbDefinitionEditorToolkit::CanDeleteBodyPart));

	BodyPartCommandList->MapAction(
		Commands.MoveBodyPartUp,
		FExecuteAction::CreateSP(this, &FLbbDefinitionEditorToolkit::HandleMoveBodyPartUp),
		FCanExecuteAction::CreateSP(this, &FLbbDefinitionEditorToolkit::CanMoveBodyPartUp));

	BodyPartCommandList->MapAction(
		Commands.MoveBodyPartDown,
		FExecuteAction::CreateSP(this, &FLbbDefinitionEditorToolkit::HandleMoveBodyPartDown),
		FCanExecuteAction::CreateSP(this, &FLbbDefinitionEditorToolkit::CanMoveBodyPartDown));
}

void FLbbDefinitionEditorToolkit::RefreshBodyPartItems()
{
	BodyPartItems.Reset();

	if (const ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get())
	{
		for (int32 BodyPartIndex = 0; BodyPartIndex < Definition->EditorModel.BodyPartGraphs.Num(); ++BodyPartIndex)
		{
			BodyPartItems.Add(MakeShared<int32>(BodyPartIndex));
		}
	}

	if (BodyPartListView.IsValid())
	{
		BodyPartListView->RequestListRefresh();
		if (CurrentGraphKind == ELbbGraphKind::Cache)
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

void FLbbDefinitionEditorToolkit::SelectCacheGraph(const bool bForceRebuild)
{
	if (!bForceRebuild && CurrentGraphKind == ELbbGraphKind::Cache && CurrentGraph != nullptr)
	{
		return;
	}

	SyncCurrentGraphToModel();
	CurrentGraphKind = ELbbGraphKind::Cache;
	RefreshBodyPartDetailsObject();
	RefreshInputDetailsObject();
	RebuildCurrentGraph();
	RebuildGraphWidget();
	RefreshDetailsPanel();
	RefreshBodyPartItems();
	CompileDefinition(false, false);
}

void FLbbDefinitionEditorToolkit::SelectBodyPart(const int32 BodyPartIndex, const bool bForceRebuild)
{
	if (!bForceRebuild
		&& CurrentGraphKind == ELbbGraphKind::BodyPart
		&& CurrentBodyPartIndex == BodyPartIndex
		&& CurrentGraph != nullptr)
	{
		return;
	}

	SyncCurrentGraphToModel();
	CurrentGraphKind = ELbbGraphKind::BodyPart;
	CurrentBodyPartIndex = BodyPartIndex;
	RefreshBodyPartDetailsObject();
	RefreshInputDetailsObject();
	RebuildCurrentGraph();
	RebuildGraphWidget();
	RefreshDetailsPanel();
	RefreshBodyPartItems();
	CompileDefinition(false, false);
}

void FLbbDefinitionEditorToolkit::ApplyCompileMessagesToCurrentGraph(
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
			&& (Message.GraphKind == ELbbGraphKind::Cache || Message.BodyPartIndex == CurrentBodyPartIndex);
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

		const ULbbEdGraphNode* LbbNode = Cast<ULbbEdGraphNode>(GraphNode);
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

void FLbbDefinitionEditorToolkit::RebuildCurrentGraph()
{
	ClearCurrentGraphChangedHandler();
	CurrentGraph = nullptr;

	const ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get();
	if (Definition == nullptr)
	{
		return;
	}

	const FLbbGraphModelBase* GraphModel = nullptr;
	if (CurrentGraphKind == ELbbGraphKind::Cache)
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

	CurrentGraph = NewObject<ULbbEdGraph>(GetTransientPackage(), NAME_None, RF_Transactional);
	CurrentGraph->Schema = ULbbGraphSchema::StaticClass();
	CurrentGraph->EditingDefinition = EditingDefinition.Get();
	CurrentGraph->GraphKind = CurrentGraphKind;
	CurrentGraph->BodyPartIndex = CurrentGraphKind == ELbbGraphKind::BodyPart ? CurrentBodyPartIndex : INDEX_NONE;

	TMap<FGuid, ULbbEdGraphNode*> NodesByGuid;
	for (const FLbbGraphNodeModel& NodeModel : GraphModel->Nodes)
	{
		ULbbEdGraphNode* Node = CreateLbbNodeForDataStruct(
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

	for (const FLbbGraphLinkModel& LinkModel : GraphModel->Links)
	{
		ULbbEdGraphNode* const* FromNodePtr = NodesByGuid.Find(LinkModel.FromNodeGuid);
		ULbbEdGraphNode* const* ToNodePtr = NodesByGuid.Find(LinkModel.ToNodeGuid);
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
		FOnGraphChanged::FDelegate::CreateSP(this, &FLbbDefinitionEditorToolkit::HandleGraphChanged));
}

void FLbbDefinitionEditorToolkit::RebuildGraphWidget()
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
	GraphEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FLbbDefinitionEditorToolkit::HandleGraphSelectionChanged);

	GraphEditorWidget = SNew(SGraphEditor)
		.GraphToEdit(CurrentGraph)
		.GraphEvents(GraphEvents)
		.ShowGraphStateOverlay(false);

	GraphHostBorder->SetContent(GraphEditorWidget.ToSharedRef());

	if (const ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get())
	{
		const FLbbGraphModelBase* GraphModel = nullptr;
		if (CurrentGraphKind == ELbbGraphKind::Cache)
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

void FLbbDefinitionEditorToolkit::RefreshBodyPartCompileSummaries(
	const FLbbCompileResult& CompileResult)
{
	const ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get();
	const int32 BodyPartCount = Definition != nullptr ? Definition->EditorModel.BodyPartGraphs.Num() : 0;

	CacheGraphCompileSummary = FBodyPartCompileSummary();
	BodyPartCompileSummaries.Reset();
	BodyPartCompileSummaries.SetNum(BodyPartCount);

	for (const FLbbCompileMessage& Message : CompileResult.Messages)
	{
		if (Message.GraphKind == ELbbGraphKind::Cache)
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

void FLbbDefinitionEditorToolkit::RefreshDetailsPanel()
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
		&& CurrentGraphKind == ELbbGraphKind::BodyPart
		&& EditingDefinition.IsValid()
		&& EditingDefinition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex))
	{
		DetailsView->SetObject(BodyPartDetailsObject.Get());
	}
	else
	{
		DetailsView->SetObject(nullptr);
	}
}

void FLbbDefinitionEditorToolkit::SyncCurrentGraphToModel()
{
	if (bIsSynchronizing || CurrentGraph == nullptr)
	{
		return;
	}

	ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get();
	if (Definition == nullptr)
	{
		return;
	}

	TGuardValue<bool> Guard(bIsSynchronizing, true);
	FLbbGraphModelBase* GraphModel = nullptr;
	if (CurrentGraphKind == ELbbGraphKind::Cache)
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

	TArray<FLbbGraphNodeModel> NewNodes;
	TArray<FLbbGraphLinkModel> NewLinks;
	NewNodes.Reserve(CurrentGraph->Nodes.Num());

	for (UEdGraphNode* GraphNode : CurrentGraph->Nodes)
	{
		if (ULbbEdGraphNode* Node = Cast<ULbbEdGraphNode>(GraphNode))
		{
			FLbbGraphNodeModel& NodeModel = NewNodes.AddDefaulted_GetRef();
			Node->ExportToNodeModel(NodeModel);
		}
	}

	for (UEdGraphNode* GraphNode : CurrentGraph->Nodes)
	{
		ULbbEdGraphNode* Node = Cast<ULbbEdGraphNode>(GraphNode);
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

				ULbbEdGraphNode* LinkedNode = Cast<ULbbEdGraphNode>(LinkedPin->GetOwningNode());
				if (LinkedNode == nullptr)
				{
					continue;
				}

				FLbbGraphLinkModel& LinkModel = NewLinks.AddDefaulted_GetRef();
				LinkModel.FromNodeGuid = Node->NodeGuid;
				LinkModel.FromPinName = Pin->PinName;
				LinkModel.ToNodeGuid = LinkedNode->NodeGuid;
				LinkModel.ToPinName = LinkedPin->PinName;
			}
		}
	}

	FVector2D NewViewLocation = GraphModel->SavedViewLocation;
	float NewZoomAmount = GraphModel->SavedZoomAmount;
	if (GraphEditorWidget.IsValid())
	{
		FVector2f ViewLocation = FVector2f(NewViewLocation);
		GraphEditorWidget->GetViewLocation(ViewLocation, NewZoomAmount);
		NewViewLocation = FVector2D(ViewLocation);
	}

	const bool bGraphChanged = (GraphModel->Nodes != NewNodes)
		|| (GraphModel->Links != NewLinks)
		|| !GraphModel->SavedViewLocation.Equals(NewViewLocation, KINDA_SMALL_NUMBER)
		|| !FMath::IsNearlyEqual(GraphModel->SavedZoomAmount, NewZoomAmount, KINDA_SMALL_NUMBER);

	if (!bGraphChanged)
	{
		return;
	}

	Definition->Modify();
	if (!GraphModel->GraphGuid.IsValid())
	{
		GraphModel->GraphGuid = FGuid::NewGuid();
	}
	GraphModel->Nodes = MoveTemp(NewNodes);
	GraphModel->Links = MoveTemp(NewLinks);
	GraphModel->SavedViewLocation = NewViewLocation;
	GraphModel->SavedZoomAmount = NewZoomAmount;
	Definition->MarkPackageDirty();
}

void FLbbDefinitionEditorToolkit::SyncBodyPartDetailsToModel()
{
	if (bIsSynchronizing || BodyPartDetailsObject == nullptr)
	{
		return;
	}

	ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get();
	if (CurrentGraphKind != ELbbGraphKind::BodyPart
		|| Definition == nullptr
		|| !Definition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex))
	{
		return;
	}

	FLbbPartGraphModel& GraphModel = Definition->EditorModel.BodyPartGraphs[CurrentBodyPartIndex];
	if (GraphModel.PartName == BodyPartDetailsObject->PartName
		&& GraphModel.DebugColor == BodyPartDetailsObject->DebugColor)
	{
		return;
	}

	Definition->Modify();
	GraphModel.PartName = BodyPartDetailsObject->PartName;
	GraphModel.DebugColor = BodyPartDetailsObject->DebugColor;
	Definition->MarkPackageDirty();
}

void FLbbDefinitionEditorToolkit::SyncInputDefinitionsToModel()
{
	if (bIsSynchronizing || InputListDetailsObject == nullptr)
	{
		return;
	}

	ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get();
	if (Definition == nullptr)
	{
		return;
	}

	InputListDetailsObject->NormalizeInputDefinitions();

	if (AreInputDefinitionsEqual(Definition->InputPoseDefinitions, InputListDetailsObject->InputDefinitions))
	{
		return;
	}

	Definition->Modify();
	Definition->InputPoseDefinitions = InputListDetailsObject->InputDefinitions;
	Definition->MarkPackageDirty();
}

void FLbbDefinitionEditorToolkit::RefreshBodyPartDetailsObject()
{
	if (BodyPartDetailsObject == nullptr)
	{
		return;
	}

	TGuardValue<bool> Guard(bIsSynchronizing, true);

	const ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get();
	if (CurrentGraphKind != ELbbGraphKind::BodyPart
		|| Definition == nullptr
		|| !Definition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex))
	{
		BodyPartDetailsObject->PartName = NAME_None;
		BodyPartDetailsObject->DebugColor = FLinearColor::Green;
		return;
	}

	const FLbbPartGraphModel& GraphModel = Definition->EditorModel.BodyPartGraphs[CurrentBodyPartIndex];
	BodyPartDetailsObject->PartName = GraphModel.PartName;
	BodyPartDetailsObject->DebugColor = GraphModel.DebugColor;
}

void FLbbDefinitionEditorToolkit::RefreshInputDetailsObject()
{
	if (InputListDetailsObject == nullptr)
	{
		return;
	}

	TGuardValue<bool> Guard(bIsSynchronizing, true);

	const ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get();
	if (Definition == nullptr)
	{
		InputListDetailsObject->InputDefinitions.Reset();
		InputListDetailsObject->NormalizeInputDefinitions();
		if (InputDetailsView.IsValid())
		{
			InputDetailsView->SetObject(InputListDetailsObject.Get());
		}
		return;
	}

	InputListDetailsObject->InputDefinitions = Definition->InputPoseDefinitions;
	InputListDetailsObject->NormalizeInputDefinitions();

	if (InputDetailsView.IsValid())
	{
		InputDetailsView->SetObject(InputListDetailsObject.Get());
	}
}

bool FLbbDefinitionEditorToolkit::CompileDefinition(const bool bMarkDirty, const bool bForceRefreshMessages)
{
	ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get();
	if (Definition == nullptr)
	{
		return false;
	}

	if (bMarkDirty)
	{
		SyncCurrentGraphToModel();
		SyncBodyPartDetailsToModel();
	}

	const FLbbCompileResult CompileResult
		= FLbbGraphCompiler::Compile(*Definition);

	if (CompileResult.bSuccess)
	{
		CompiledDefinitionPreview = CompileResult.CompiledDefinition;
		bHasCompiledDefinitionPreview = true;
	}
	else
	{
		CompiledDefinitionPreview = FLbbCompiledDefinitionData();
		bHasCompiledDefinitionPreview = false;
	}

	RefreshBodyPartCompileSummaries(CompileResult);
	ApplyCompileMessagesToCurrentGraph(CompileResult);

	if (bForceRefreshMessages)
	{
		RefreshCompileMessages(CompileResult);
	}

	if (CompileResult.bSuccess && bMarkDirty)
	{
		if (!AreCompiledDefinitionsEqual(CompileResult.CompiledDefinition, *Definition))
		{
			Definition->Modify();
			FLbbGraphCompiler::ApplyCompiledDefinition(CompileResult.CompiledDefinition, *Definition);
			Definition->MarkPackageDirty();
		}
	}

	RefreshCompiledOperatorsPreview();
	RefreshBodyPartItems();
	return CompileResult.bSuccess;
}

void FLbbDefinitionEditorToolkit::RefreshCompileMessages(
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
		const ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get();
		for (const FLbbCompileMessage& Message : CompileResult.Messages)
		{
			const FString GraphLabel = GetGraphLabel(Definition, Message.GraphKind, Message.BodyPartIndex);

			TSharedPtr<FCompileMessageListItem> Item = MakeShared<FCompileMessageListItem>();
			Item->Severity = Message.Severity;
			Item->GraphKind = Message.GraphKind;
			Item->BodyPartIndex = Message.BodyPartIndex;
			Item->NodeGuid = Message.NodeGuid;
			Item->bCanNavigate = Message.GraphKind == ELbbGraphKind::Cache || Message.BodyPartIndex != INDEX_NONE;
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

void FLbbDefinitionEditorToolkit::RefreshCompiledOperatorsPreview()
{
	if (CompiledOperatorsPreviewTextBox.IsValid())
	{
		CompiledOperatorsPreviewTextBox->SetText(FText::FromString(BuildCompiledOperatorsPreviewText()));
	}
}

FString FLbbDefinitionEditorToolkit::BuildCompiledOperatorsPreviewText() const
{
	if (!bHasCompiledDefinitionPreview)
	{
		return TEXT("No compiled preview is available. The current editor graph has compile errors or has not been compiled yet.");
	}

	const FLbbCompiledDefinitionData& CompiledDefinition = CompiledDefinitionPreview;

	TArray<FString> Lines;
	Lines.Reserve(64);
	Lines.Add(TEXT("Preview Source: transient compile result from the current editor graph."));
	Lines.Add(TEXT("This panel does not read back runtime data from the stored uasset."));

	const FString CachePrefix = CurrentGraphKind == ELbbGraphKind::Cache ? TEXT(">> ") : TEXT("   ");
	Lines.Add(TEXT(""));
	Lines.Add(FString::Printf(TEXT("%s[Cache Graph]"), *CachePrefix));
	Lines.Add(FString::Printf(TEXT("Named Caches: %d"), CompiledDefinition.CacheProgram.NamedCacheNames.Num()));
	Lines.Add(FString::Printf(TEXT("Operators: %d"), CompiledDefinition.CacheProgram.Operators.Num()));
	if (CompiledDefinition.CacheProgram.Operators.IsEmpty())
	{
		Lines.Add(TEXT("  <No operators>"));
	}
	else
	{
		for (int32 OperatorIndex = 0; OperatorIndex < CompiledDefinition.CacheProgram.Operators.Num(); ++OperatorIndex)
		{
			FString OperatorText = TEXT("<Invalid Operator>");
			if (const FLbbPartOperatorBase* Operator = CompiledDefinition.CacheProgram.Operators[OperatorIndex].GetPtr<FLbbPartOperatorBase>())
			{
				OperatorText = Operator->ToString();
			}

			Lines.Add(FString::Printf(TEXT("  %d. %s"), OperatorIndex + 1, *OperatorText));
		}
	}

	if (CompiledDefinition.BodyParts.IsEmpty())
	{
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("No compiled BodyParts were produced by the current graph."));
		return FString::Join(Lines, TEXT("\n"));
	}

	for (int32 BodyPartIndex = 0; BodyPartIndex < CompiledDefinition.BodyParts.Num(); ++BodyPartIndex)
	{
		const FLbbPart& BodyPart = CompiledDefinition.BodyParts[BodyPartIndex];
		const FString PartName = BodyPart.PartName.IsNone()
			? FString::Printf(TEXT("BodyPart_%d"), BodyPartIndex + 1)
			: BodyPart.PartName.ToString();
		const FString BodyPartPrefix = (CurrentGraphKind == ELbbGraphKind::BodyPart && BodyPartIndex == CurrentBodyPartIndex) ? TEXT(">> ") : TEXT("   ");

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
			if (const FLbbPartOperatorBase* Operator = BodyPart.Operators[OperatorIndex].GetPtr<FLbbPartOperatorBase>())
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

void FLbbDefinitionEditorToolkit::NavigateToCompileMessage(const FCompileMessageListItem& Item)
{
	if (!Item.bCanNavigate)
	{
		return;
	}

	if (Item.GraphKind == ELbbGraphKind::Cache)
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

	if (ULbbEdGraphNode* Node = FindCurrentGraphNodeByGuid(Item.NodeGuid))
	{
		GraphEditorWidget->ClearSelectionSet();
		GraphEditorWidget->SetNodeSelection(Node, true);
		GraphEditorWidget->JumpToNode(Node, false, true);
		RefreshDetailsPanel();
	}
}

ULbbEdGraphNode* FLbbDefinitionEditorToolkit::FindCurrentGraphNodeByGuid(const FGuid& NodeGuid) const
{
	if (CurrentGraph == nullptr || !NodeGuid.IsValid())
	{
		return nullptr;
	}

	for (UEdGraphNode* GraphNode : CurrentGraph->Nodes)
	{
		ULbbEdGraphNode* LbbNode = Cast<ULbbEdGraphNode>(GraphNode);
		if (LbbNode != nullptr && LbbNode->NodeGuid == NodeGuid)
		{
			return LbbNode;
		}
	}

	return nullptr;
}

void FLbbDefinitionEditorToolkit::HandleGraphChanged(const FEdGraphEditAction& GraphEditAction)
{
	if (bIsSynchronizing)
	{
		return;
	}

	SyncCurrentGraphToModel();
	CompileDefinition(true);
}

void FLbbDefinitionEditorToolkit::HandleGraphSelectionChanged(const TSet<UObject*>& NewSelection)
{
	RefreshDetailsPanel();
}

void FLbbDefinitionEditorToolkit::HandleDetailsFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
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

void FLbbDefinitionEditorToolkit::HandleInputDetailsFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (bIsSynchronizing)
	{
		return;
	}

	SyncInputDefinitionsToModel();
	CompileDefinition(true);
}

void FLbbDefinitionEditorToolkit::HandleAddBodyPart()
{
	ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get();
	if (Definition == nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddBodyPartTransaction", "Add Layered Blend Body Part"));
	Definition->Modify();

	FLbbPartGraphModel& NewGraphModel = Definition->EditorModel.BodyPartGraphs.AddDefaulted_GetRef();
	FLbbGraphCompiler::CreateDefaultBodyPartGraph(NewGraphModel, MakeUniqueBodyPartName(*Definition));
	Definition->MarkPackageDirty();

	RefreshBodyPartItems();
	SelectBodyPart(Definition->EditorModel.BodyPartGraphs.Num() - 1);
	CompileDefinition(true);
}

void FLbbDefinitionEditorToolkit::HandleDuplicateBodyPart()
{
	ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get();
	if (Definition == nullptr || !Definition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DuplicateBodyPartTransaction", "Duplicate Layered Blend Body Part"));
	Definition->Modify();

	FLbbPartGraphModel DuplicatedGraphModel = Definition->EditorModel.BodyPartGraphs[CurrentBodyPartIndex];
	RemapGraphGuids(DuplicatedGraphModel);
	DuplicatedGraphModel.PartName = MakeUniqueBodyPartName(*Definition);
	Definition->EditorModel.BodyPartGraphs.Insert(DuplicatedGraphModel, CurrentBodyPartIndex + 1);
	Definition->MarkPackageDirty();

	RefreshBodyPartItems();
	SelectBodyPart(CurrentBodyPartIndex + 1);
	CompileDefinition(true);
}

void FLbbDefinitionEditorToolkit::HandleDeleteBodyPart()
{
	ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get();
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

void FLbbDefinitionEditorToolkit::HandleMoveBodyPartUp()
{
	ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get();
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

void FLbbDefinitionEditorToolkit::HandleMoveBodyPartDown()
{
	ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get();
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

bool FLbbDefinitionEditorToolkit::CanAddBodyPart() const
{
	return EditingDefinition.IsValid();
}

bool FLbbDefinitionEditorToolkit::HasValidBodyPartSelection() const
{
	const ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get();
	return CurrentGraphKind == ELbbGraphKind::BodyPart
		&& Definition != nullptr
		&& Definition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex);
}

bool FLbbDefinitionEditorToolkit::CanDuplicateBodyPart() const
{
	return HasValidBodyPartSelection();
}

bool FLbbDefinitionEditorToolkit::CanDeleteBodyPart() const
{
	return HasValidBodyPartSelection();
}

bool FLbbDefinitionEditorToolkit::CanMoveBodyPartUp() const
{
	return HasValidBodyPartSelection() && CurrentBodyPartIndex > 0;
}

bool FLbbDefinitionEditorToolkit::CanMoveBodyPartDown() const
{
	const ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get();
	return CurrentGraphKind == ELbbGraphKind::BodyPart
		&& Definition != nullptr
		&& Definition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex)
		&& Definition->EditorModel.BodyPartGraphs.IsValidIndex(CurrentBodyPartIndex + 1);
}

TSharedRef<SDockTab> FLbbDefinitionEditorToolkit::SpawnBodyPartListTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
	[
		BuildBodyPartListTabContent()
	];
}

TSharedRef<SDockTab> FLbbDefinitionEditorToolkit::SpawnGraphTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
	[
		BuildGraphTabContent()
	];
}

TSharedRef<SDockTab> FLbbDefinitionEditorToolkit::SpawnDetailsTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
	[
		DetailsView.IsValid()
			? DetailsView.ToSharedRef()
			: SNullWidget::NullWidget
	];
}

TSharedRef<SDockTab> FLbbDefinitionEditorToolkit::SpawnInputsTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
	[
		InputDetailsView.IsValid()
			? InputDetailsView.ToSharedRef()
			: SNullWidget::NullWidget
	];
}

TSharedRef<SDockTab> FLbbDefinitionEditorToolkit::SpawnCompiledOperatorsPreviewTab(const FSpawnTabArgs& Args)
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

TSharedRef<SDockTab> FLbbDefinitionEditorToolkit::SpawnCompileMessagesTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
	[
		BuildCompileMessagesTabContent()
	];
}

TSharedRef<ITableRow> FLbbDefinitionEditorToolkit::HandleGenerateBodyPartRow(
	TSharedPtr<int32> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	const ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get();
	const FLbbPartGraphModel* GraphModel = (Definition != nullptr && Item.IsValid() && Definition->EditorModel.BodyPartGraphs.IsValidIndex(*Item))
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

TSharedRef<ITableRow> FLbbDefinitionEditorToolkit::HandleGenerateCompileMessageRow(
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

void FLbbDefinitionEditorToolkit::HandleBodyPartSelectionChanged(TSharedPtr<int32> Item, ESelectInfo::Type SelectInfo)
{
	if (!Item.IsValid())
	{
		return;
	}

	SelectBodyPart(*Item);
}

void FLbbDefinitionEditorToolkit::HandleCompileMessageSelectionChanged(
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

FReply FLbbDefinitionEditorToolkit::HandleBodyPartListKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (BodyPartCommandList.IsValid() && BodyPartCommandList->ProcessCommandBindings(KeyEvent))
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedPtr<SWidget> FLbbDefinitionEditorToolkit::HandleBodyPartListContextMenuOpening()
{
	if (!BodyPartCommandList.IsValid())
	{
		return nullptr;
	}

	const FLbbDefinitionEditorCommands& Commands = FLbbDefinitionEditorCommands::Get();
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

TSharedRef<SWidget> FLbbDefinitionEditorToolkit::BuildBodyPartListTabContent()
{
	auto MakeBodyPartToolBarWidget = [this]() -> TSharedRef<SWidget>
	{
		FToolBarBuilder ToolbarBuilder(BodyPartCommandList, FMultiBoxCustomization::None);
		ToolbarBuilder.SetStyle(&FAppStyle::Get(), "AssetEditorToolbar");
		ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

		const FLbbDefinitionEditorCommands& Commands = FLbbDefinitionEditorCommands::Get();
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
						const ULbbBodyPartDefinitionDataAsset* Definition = EditingDefinition.Get();
						const int32 NodeCount = Definition != nullptr ? Definition->EditorModel.CacheGraph.Nodes.Num() : 0;
						const int32 NamedCacheCount = bHasCompiledDefinitionPreview
							? CompiledDefinitionPreview.CacheProgram.NamedCacheNames.Num()
							: (Definition != nullptr ? Definition->CacheProgram.NamedCacheNames.Num() : 0);
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
			.OnGenerateRow(this, &FLbbDefinitionEditorToolkit::HandleGenerateBodyPartRow)
			.OnSelectionChanged(this, &FLbbDefinitionEditorToolkit::HandleBodyPartSelectionChanged)
			.OnContextMenuOpening(this, &FLbbDefinitionEditorToolkit::HandleBodyPartListContextMenuOpening)
			.OnKeyDownHandler(this, &FLbbDefinitionEditorToolkit::HandleBodyPartListKeyDown)
		];

	RefreshBodyPartItems();
	return Widget;
}

TSharedRef<SWidget> FLbbDefinitionEditorToolkit::BuildGraphTabContent()
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

TSharedRef<SWidget> FLbbDefinitionEditorToolkit::BuildCompileMessagesTabContent()
{
	TSharedRef<SWidget> Widget =
		SNew(SBox)
		.MinDesiredHeight(148.f)
		[
			SAssignNew(CompileMessagesListView, SListView<TSharedPtr<FCompileMessageListItem>>)
			.ListItemsSource(&CompileMessageItems)
			.OnGenerateRow(this, &FLbbDefinitionEditorToolkit::HandleGenerateCompileMessageRow)
			.OnSelectionChanged(this, &FLbbDefinitionEditorToolkit::HandleCompileMessageSelectionChanged)
			.SelectionMode(ESelectionMode::Single)
		];

	if (CompileMessagesListView.IsValid())
	{
		CompileMessagesListView->RequestListRefresh();
	}

	return Widget;
}

#undef LOCTEXT_NAMESPACE
