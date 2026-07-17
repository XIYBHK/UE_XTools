// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistDelayedDelegate.h"
#include "BlueprintAssistNodeSizeChangeData.h"
#include "BlueprintAssistFormatters/GraphFormatterTypes.h"

class FBAGraphOperation;
class SGraphActionMenu;
class FBAGraphTasks;
class FBAFormatRequest;
struct FBAScopedGraphAction;
class UBAGraphSchema;
class IAssetEditorInstance;
class SBlueprintAssistGraphOverlay;
class SMyBlueprint;
class FBANodeSizeChangeData;
struct FFormatterInterface;
struct FBAGraphData;
struct FBANodeData;

class XTOOLS_BLUEPRINTASSIST_API FBAGraphHandler
	: public TSharedFromThis<FBAGraphHandler>
{
public:

	FBAGraphHandler(TWeakPtr<SDockTab> InTab, TWeakPtr<SGraphEditor> InGraphEditor);

	~FBAGraphHandler();

	void InitGraphHandler();

	void AddGraphPanelOverlay();

	void OnGainFocus();

	void OnLoseFocus();

	void Cleanup();

	void Tick(float DeltaTime);

	bool OnKeyDown(const FKey& Key);
	bool OnKeyUp(const FKey& Key);

	void UpdateSelectedNode();

	void UpdateSelectedPin();

	bool TrySelectFirstPinOnNode(UEdGraphNode* Node);
	bool TrySelectFirstValuePinOnNode(UEdGraphNode* Node);

	TSharedPtr<SWindow> GetWindow();

	bool IsWindowActive();

	bool IsGraphPanelFocused();

	bool IsGraphReadOnly();

	bool HasValidGraphReferences();

	bool TryAutoFormatNode(const TArray<UEdGraphNode*>& Nodes, TSharedPtr<FBAScopedGraphAction> PendingTransaction = {}, FEdGraphFormatterParameters Parameters = FEdGraphFormatterParameters());

	void RequestFormatNode(
		UEdGraphNode* Node,
		TSharedPtr<FBAScopedGraphAction> PendingTransaction = {},
		FEdGraphFormatterParameters FormatterParameters = FEdGraphFormatterParameters());

	void RequestFormatAll();
	void RequestFormatAllAndSave();

	void SetNodeToReplace(UEdGraphNode* Node, TSharedPtr<FBAScopedGraphAction> Transaction)
	{
		NodeToReplace = Node;
		ReplaceNewNodeTransaction = Transaction;
	}

	void ResetSingleNewNodeTransaction();

	void ResetReplaceNodeTransaction();

	int32 GetPinY(const UEdGraphPin* Pin);

	void SetSelectedPin(UEdGraphPin* Pin, bool bLerpIntoView = false);

	void UpdateLerpViewport(float DeltaTime);

	void BeginLerpViewport(FVector2D TargetView, bool bCenter = true);
	const FVector2D& GetTargetLerpLocation() const { return TargetLerpLocation; }
	bool IsLerpingViewport() const { return bLerpViewport; }

	/**
	 * Cancel active node size and formatting processes, also clear any active related notifications and transactions
	 */
	void CancelActiveFormatting();

	TSharedPtr<SDockTab> GetTab() const { return CachedTab.Pin(); }

	UEdGraph* GetFocusedEdGraph();

	TSharedPtr<SGraphEditor> GetGraphEditor();

	TSharedPtr<SGraphPanel> GetGraphPanel();

	TSharedPtr<SGraphActionMenu> GetGraphActionMenu();

	IAssetEditorInstance* GetAssetEditorInstance() const;

	const UBAGraphSchema& GetSchema();

	UBlueprint* GetBlueprint();

	TSharedPtr<SMyBlueprint> GetMyBlueprint();

	UEdGraphNode* GetSelectedNode(bool bAllowCommentNodes = false);

	TSet<UEdGraphNode*> GetSelectedNodes(bool bAllowCommentNodes = false);

	void SelectNodes(const TSet<UEdGraphNode*>& Nodes);

	FSlateRect GetCachedNodeBounds(UEdGraphNode* Node, bool bWithCommentBubble = true);

	UEdGraphPin* GetSelectedPin();

	TSharedPtr<SGraphNode> GetGraphNode(UEdGraphNode* Node);

	void RefreshNodeSize(UEdGraphNode* Node);

	bool AddPendingSize(UEdGraphNode* Node);

	void RefreshAllNodeSizes();

	void ResetTransactions();

	void ApplyGlobalCommentBubblePinned();

	void ApplyCommentBubblePinned(UEdGraphNode* Node);

	int32 GetNumberOfPendingNodesToCache() const;

	float GetPendingNodeSizeProgress() const;

	void ClearFormatters();

	bool HasActiveTransaction() const;

	void SelectNode(UEdGraphNode* Node, bool bLerpIntoView = true);

	void LerpNodeIntoView(UEdGraphNode* Node, bool bOnlyWhenOffscreen);

	TSharedPtr<SBlueprintAssistGraphOverlay> GetGraphOverlay() { return GraphOverlay; }

	FBAGraphData& GetGraphData();
	FBANodeData& GetNodeData(UEdGraphNode* Node);

	TMap<FGuid, TSet<TWeakObjectPtr<UEdGraphNode>>> NodeGroups;
	TSet<UEdGraphNode*> GetNodeGroup(const FGuid& GroupID);
	void AddToNodeGroup(FGuid GroupID, UEdGraphNode* Node);
	void ClearNodeGroup(UEdGraphNode* Node);
	void CleanupNodeGroups();
	TSet<UEdGraphNode*> GetGroupedNodes(const TSet<UEdGraphNode*>& NodeSet);

	void ToggleLockNodes(const TSet<UEdGraphNode*>& NodeSet);
	void GroupNodes(const TSet<UEdGraphNode*>& NodeSet);
	void UngroupNodes(const TSet<UEdGraphNode*>& NodeSet);

	const TMap<FGuid, FBAFormattingChangeData>& GetFormattingChangeData() const { return FormattingChangeDataMap; }

	void SetViewLocation(const FVector2D& NewLocation, float NewZoom);
	void GetViewLocation(FVector2D& OutLocation, float& OutZoom);
	void GetViewLocation(FVector2D& OutLocation);

	bool UpdateNodeSizesChanges(const TArray<UEdGraphNode*>& Nodes);

	void AutoLerpToNewlyCreatedNode(UEdGraphNode* Node);

	TSharedPtr<FBAScopedGraphAction> ReplaceNewNodeTransaction;

	void UpdateNodeChangeData(UEdGraphNode* Node);

	FBAGraphTasks& GetGraphTasks() const { return *GraphTasks; };
	FBAFormatRequest& GetFormatRequest() const { return *FormatRequest; };
	TSharedPtr<FBAGraphOperation> GetCurrentOperation() const { return GraphOperation; }

	bool BeginOperation(TSharedPtr<FBAGraphOperation> Op);
	void EndOperation() { GraphOperation.Reset(); }

	bool HasInitialZoomFinished() const { return bInitialZoomFinished; }

	bool IsAnyNodeBeingRenamed();

private:
	friend class FBAGraphTask_CacheNodeSizes;

	TUniquePtr<FBAGraphTasks> GraphTasks;
	TUniquePtr<FBAFormatRequest> FormatRequest;
	TSharedPtr<FBAGraphOperation> GraphOperation;

	TSharedPtr<SBlueprintAssistGraphOverlay> GraphOverlay;
	TWeakObjectPtr<UEdGraphNode> NodeToReplace = nullptr;

	TWeakPtr<SGraphPanel> CachedGraphPanel;
	TWeakPtr<SGraphEditor> CachedGraphEditor;
	TWeakPtr<SDockTab> CachedTab;
	TWeakPtr<SGraphActionMenu> CachedGraphActionMenu;

	TWeakObjectPtr<UEdGraph> CachedEdGraph;

	FBAGraphPinHandle SelectedPinHandle;
	FPinLink PrevSelectedLink;
	FBAGraphPinHandle PrevSelectedPinHandle;

	FBADelayedDelegate DelayedGraphInitialized;
	FBADelayedDelegate DelayedViewportZoomIn;
	FBADelayedDelegate DelayedClearReplaceTransaction;
	FBADelayedDelegate DelayedDetectGraphChanges;
	FBADelayedDelegate DelayedCacheSizeFinished;

	TWeakObjectPtr<UEdGraphNode> NewNodeForOverlayVisibility;
	void EarlyShowOverlay();

	bool bInitialZoomFinished = false;
	FVector2D LastGraphView;
	float LastZoom = 1.0f;

	TWeakObjectPtr<UEdGraphNode> LastSelectedNode;

	// lerp viewport position
	bool bLerpViewport = false;
	bool bCenterWhileLerping = false;
	FVector2D TargetLerpLocation;

	TArray<TWeakObjectPtr<UEdGraphNode>> LastNodes;

	FDelegateHandle OnGraphChangedHandle;

	TWeakPtr<SNotificationItem> SizeTimeoutNotification;

	void OnGraphInitializedDelayed();

	TMap<FGuid, FBANodeSizeChangeData> NodeSizeChangeDataMap;
	TMap<FGuid, FBAFormattingChangeData> FormattingChangeDataMap;
	TMap<FGuid, FIntPoint> PreFormatNodePositions;

	void OnSelectionChanged(UEdGraphNode* PreviousNode, UEdGraphNode* NewNode);

	bool TryInsertNewNode(UEdGraphNode* NewNode);

	bool LinkExecWhenCreatedFromParameter(UEdGraphNode* NodeCreated, bool bInsert);

	bool AutoInsertExecNode(UEdGraphNode* NodeCreated);

	bool AutoInsertParameterNode(UEdGraphNode* NodeCreated);

	void SetGraphEditor(TWeakPtr<SGraphEditor> NewGraphEditor);

	void ReplaceSavedSelectedNode(UEdGraphNode* NewNode);

	void MoveUnrelatedNodes(TSharedPtr<FFormatterInterface> Formatter);

	void OnGraphChanged(const FEdGraphEditAction& Action);

#if BA_UE_VERSION_OR_LATER(5, 0)
	void HandleObjectSaved(UObject* Obj, FObjectPreSaveContext PreSaveContext);
#endif

	void DetectGraphChanges();

	void OnNodesAdded(const TArray<UEdGraphNode*>& NewNodes);

	UEdGraphNode* DecideNodeToKeepStill(const TArray<UEdGraphNode*>& NewNodes);

	void CacheNodeSizes(const TArray<UEdGraphNode*>& Nodes);

	void FormatNewNodes(const TArray<UEdGraphNode*>& NewNodes, TSharedPtr<FBAScopedGraphAction> Transaction);

	void AutoAddParentNode(UEdGraphNode* NewNode);

	void OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event);

	void OnPostUndoRedo();

	void AutoZoomToNode(UEdGraphNode* Node);

	bool DoesNodeWantAutoFormatting(UEdGraphNode* Node);

	void RunSavePostFormatting();

	TSharedPtr<SGraphEditor> AssignNewGraphEditorFromTab();

	bool IsBlueprintRootNode(UEdGraphNode* Node, bool bOnlyOutputRoots);

	bool SavePackage(bool bCompile = true);

	void OnDelayedCacheSizeFinished();
};
