// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "LbbLayeredBlendBodyGraphCompiler.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Widgets/Views/SListView.h"

class IDetailsView;
class FUICommandList;
class SGraphEditor;
class SBorder;
class SMultiLineEditableTextBox;
class ULbbLayeredBlendBodyDefinition;
class ULbbLayeredBlendBodyEdGraph;
class ULbbLayeredBlendBodyEdGraphNode;
class ULbbLayeredBlendBodyBodyPartDetailsObject;
class ULbbLayeredBlendBodyInputListDetailsObject;

class FLbbLayeredBlendBodyDefinitionEditorToolkit : public FAssetEditorToolkit
{
public:
	static TSharedRef<FLbbLayeredBlendBodyDefinitionEditorToolkit> CreateEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		ULbbLayeredBlendBodyDefinition* InDefinition);

	virtual ~FLbbLayeredBlendBodyDefinitionEditorToolkit() override;

	void InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, ULbbLayeredBlendBodyDefinition* InDefinition);

	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual void SaveAsset_Execute() override;

private:
	struct FBodyPartCompileSummary
	{
		int32 ErrorCount = 0;
		int32 WarningCount = 0;
	};

	struct FCompileMessageListItem
	{
		EMessageSeverity::Type Severity = EMessageSeverity::Info;
		ELbbLayeredBlendBodyGraphKind GraphKind = ELbbLayeredBlendBodyGraphKind::BodyPart;
		int32 BodyPartIndex = INDEX_NONE;
		FGuid NodeGuid;
		FText DisplayText;
		FText ToolTipText;
		bool bCanNavigate = false;
	};

	static const FName BodyPartListTabId;
	static const FName GraphTabId;
	static const FName DetailsTabId;
	static const FName InputsTabId;
	static const FName CompiledOperatorsPreviewTabId;
	static const FName CompileMessagesTabId;
	static const FName EditorAppName;

	void ApplyCompileMessagesToCurrentGraph(const FLbbCompileResult& CompileResult);
	void ClearCurrentGraphChangedHandler();
	void ExtendToolbar();
	void FillToolbar(class FToolBarBuilder& ToolbarBuilder);
	void BindBodyPartCommands();
	void RefreshBodyPartCompileSummaries(const FLbbCompileResult& CompileResult);
	void RefreshBodyPartItems();
	void SelectCacheGraph(bool bForceRebuild = false);
	void SelectBodyPart(int32 BodyPartIndex, bool bForceRebuild = false);
	void RebuildCurrentGraph();
	void RebuildGraphWidget();
	void RefreshDetailsPanel();
	void SyncCurrentGraphToModel();
	void SyncBodyPartDetailsToModel();
	void SyncInputDefinitionsToModel();
	void RefreshBodyPartDetailsObject();
	void RefreshInputDetailsObject();
	bool CompileDefinition(bool bMarkDirty, bool bForceRefreshMessages = true);
	void RefreshCompileMessages(const FLbbCompileResult& CompileResult);
	void RefreshCompiledOperatorsPreview();
	FString BuildCompiledOperatorsPreviewText() const;
	void NavigateToCompileMessage(const FCompileMessageListItem& Item);
	ULbbLayeredBlendBodyEdGraphNode* FindCurrentGraphNodeByGuid(const FGuid& NodeGuid) const;

	void HandleGraphChanged(const struct FEdGraphEditAction& GraphEditAction);
	void HandleGraphSelectionChanged(const TSet<UObject*>& NewSelection);
	void HandleDetailsFinishedChangingProperties(const struct FPropertyChangedEvent& PropertyChangedEvent);
	void HandleInputDetailsFinishedChangingProperties(const struct FPropertyChangedEvent& PropertyChangedEvent);

	void HandleAddBodyPart();
	void HandleDuplicateBodyPart();
	void HandleDeleteBodyPart();
	void HandleMoveBodyPartUp();
	void HandleMoveBodyPartDown();
	bool CanAddBodyPart() const;
	bool HasValidBodyPartSelection() const;
	bool CanDuplicateBodyPart() const;
	bool CanDeleteBodyPart() const;
	bool CanMoveBodyPartUp() const;
	bool CanMoveBodyPartDown() const;
	bool IsCacheGraphSelected() const { return CurrentGraphKind == ELbbLayeredBlendBodyGraphKind::Cache; }

	TSharedRef<class SDockTab> SpawnBodyPartListTab(const class FSpawnTabArgs& Args);
	TSharedRef<class SDockTab> SpawnGraphTab(const class FSpawnTabArgs& Args);
	TSharedRef<class SDockTab> SpawnDetailsTab(const class FSpawnTabArgs& Args);
	TSharedRef<class SDockTab> SpawnInputsTab(const class FSpawnTabArgs& Args);
	TSharedRef<class SDockTab> SpawnCompiledOperatorsPreviewTab(const class FSpawnTabArgs& Args);
	TSharedRef<class SDockTab> SpawnCompileMessagesTab(const class FSpawnTabArgs& Args);
	TSharedRef<class ITableRow> HandleGenerateBodyPartRow(TSharedPtr<int32> Item, const TSharedRef<class STableViewBase>& OwnerTable);
	TSharedRef<class ITableRow> HandleGenerateCompileMessageRow(TSharedPtr<FCompileMessageListItem> Item, const TSharedRef<class STableViewBase>& OwnerTable);
	void HandleBodyPartSelectionChanged(TSharedPtr<int32> Item, ESelectInfo::Type SelectInfo);
	void HandleCompileMessageSelectionChanged(TSharedPtr<FCompileMessageListItem> Item, ESelectInfo::Type SelectInfo);
	FReply HandleBodyPartListKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent);
	TSharedPtr<SWidget> HandleBodyPartListContextMenuOpening();
	TSharedRef<SWidget> BuildBodyPartListTabContent();
	TSharedRef<SWidget> BuildGraphTabContent();
	TSharedRef<SWidget> BuildCompileMessagesTabContent();

private:
	TWeakObjectPtr<ULbbLayeredBlendBodyDefinition> EditingDefinition;
	TObjectPtr<ULbbLayeredBlendBodyEdGraph> CurrentGraph = nullptr;
	TObjectPtr<ULbbLayeredBlendBodyBodyPartDetailsObject> BodyPartDetailsObject = nullptr;
	TObjectPtr<ULbbLayeredBlendBodyInputListDetailsObject> InputListDetailsObject = nullptr;
	TSharedPtr<SGraphEditor> GraphEditorWidget;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<IDetailsView> InputDetailsView;
	TSharedPtr<FUICommandList> BodyPartCommandList;
	TSharedPtr<SMultiLineEditableTextBox> CompiledOperatorsPreviewTextBox;
	TSharedPtr<SListView<TSharedPtr<FCompileMessageListItem>>> CompileMessagesListView;
	TSharedPtr<SListView<TSharedPtr<int32>>> BodyPartListView;
	TSharedPtr<SBorder> GraphHostBorder;
	TSharedPtr<class FExtender> ToolbarExtender;
	TArray<TSharedPtr<int32>> BodyPartItems;
	FBodyPartCompileSummary CacheGraphCompileSummary;
	TArray<FBodyPartCompileSummary> BodyPartCompileSummaries;
	TArray<TSharedPtr<FCompileMessageListItem>> CompileMessageItems;
	FLbbCompiledDefinitionData CompiledDefinitionPreview;
	FDelegateHandle CurrentGraphChangedHandle;
	ELbbLayeredBlendBodyGraphKind CurrentGraphKind = ELbbLayeredBlendBodyGraphKind::Cache;
	int32 CurrentBodyPartIndex = INDEX_NONE;
	bool bHasCompiledDefinitionPreview = false;
	bool bIsSynchronizing = false;
};
