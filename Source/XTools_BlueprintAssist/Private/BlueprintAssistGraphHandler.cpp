// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistGraphHandler.h"

#include "BlueprintAssistCache.h"
#include "BlueprintAssistGlobals.h"
#include "BlueprintAssistInputProcessor.h"
#include "BlueprintAssistSettings.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistSettings_EditorFeatures.h"
#include "BlueprintAssistUtils.h"
#include "BlueprintEditor.h"
#include "EdGraphNode_Comment.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_InputKey.h"
#include "K2Node_Tunnel.h"
#include "K2Node_VariableGet.h"
#include "SCommentBubble.h"
#include "ScopedTransaction.h"
#include "SGraphActionMenu.h"
#include "SGraphPanel.h"
#include "Algo/Transform.h"
#include "BlueprintAssistFormatters/BAFormatterUtils.h"
#include "BlueprintAssistFormatters/EdGraphFormatter.h"
#include "BlueprintAssistMisc/BAControlRigUtils.h"
#include "BlueprintAssistMisc/BAMiscUtils.h"
#include "BlueprintAssistWidgets/BlueprintAssistGraphOverlay.h"
#include "BlueprintAssistWidgets/SBASizeProgress.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "Misc/FileHelper.h"
#include "BlueprintAssistMisc/BAGraphSchema.h"
#include "BlueprintAssistMisc/BAPrivate.h"
#include "UObject/SavePackage.h"
#include "Widgets/Docking/SDockTab.h"
#include "BAGraphHandler/BAFormatRequest.h"
#include "BAGraphHandler/BAGraphTasks.h"
#include "Widgets/Views/STreeView.h"
#include "GraphActionNode.h"
#include "BAGraphHandler/BAGraphOperation.h"


#if BA_UE_VERSION_OR_LATER(5, 0)
#include "UObject/ObjectSaveContext.h"
#endif

#if BA_UE_VERSION_OR_LATER(5, 1)
#include "Misc/TransactionObjectEvent.h"
#endif

BA_DEFINE_PRIVATE_MEMBER_PTR(TSharedPtr<SMyBlueprint>, GMyBlueprintWidget, FBlueprintEditor, MyBlueprintWidget);

BA_DEFINE_PRIVATE_MEMBER_PTR(TSharedPtr<STreeView<TSharedPtr<FGraphActionNode>>>, GTreeView, SGraphActionMenu, TreeView);

FBAGraphHandler::FBAGraphHandler(
	TWeakPtr<SDockTab> InTab,
	TWeakPtr<SGraphEditor> InGraphEditor)
	: GraphTasks(MakeUnique<FBAGraphTasks>(*this))
	, FormatRequest(MakeUnique<FBAFormatRequest>(*this))
	, CachedGraphEditor(InGraphEditor)
	, CachedTab(InTab)
{
	check(GetGraphEditor().IsValid());
	check(GetFocusedEdGraph() != nullptr);
	check(GetGraphPanel().IsValid());
	check(GetTab().IsValid());
	check(GetWindow().IsValid());

	FCoreUObjectDelegates::OnObjectTransacted.AddRaw(this, &FBAGraphHandler::OnObjectTransacted);
	FEditorDelegates::PostUndoRedo.AddRaw(this, &FBAGraphHandler::OnPostUndoRedo);
}

FBAGraphHandler::~FBAGraphHandler()
{
	if (OnGraphChangedHandle.IsValid())
	{
		if (auto EdGraph = GetFocusedEdGraph())
		{
			EdGraph->RemoveOnGraphChangedHandler(OnGraphChangedHandle);
		}
	}

	SelectedPinHandle = nullptr;
	PrevSelectedLink.Invalidate();
	LastSelectedNode = nullptr;
	LastNodes.Empty();
	ResetTransactions();

	FCoreUObjectDelegates::OnObjectTransacted.RemoveAll(this);
	FEditorDelegates::PostUndoRedo.RemoveAll(this);
}

void FBAGraphHandler::InitGraphHandler()
{
	Cleanup();

	DelayedGraphInitialized.StartDelay(2);
	DelayedGraphInitialized.SetOnDelayEnded(FBAOnDelayEnded::CreateRaw(this, &FBAGraphHandler::OnGraphInitializedDelayed));
	DelayedClearReplaceTransaction.SetOnDelayEnded(FBAOnDelayEnded::CreateRaw(this, &FBAGraphHandler::ResetReplaceNodeTransaction));
	DelayedDetectGraphChanges.SetOnDelayEnded(FBAOnDelayEnded::CreateRaw(this, &FBAGraphHandler::DetectGraphChanges));
	DelayedCacheSizeFinished.SetOnDelayEnded(FBAOnDelayEnded::CreateRaw(this, &FBAGraphHandler::OnDelayedCacheSizeFinished));

	NodeToReplace = nullptr;
	bInitialZoomFinished = false;
	LastSelectedNode = nullptr;
	bLerpViewport = false;
	bCenterWhileLerping = false;
	LastNodes.Empty();

	FormatRequest->Reset();
	GraphTasks->ClearTasks();

	ResetTransactions();

	CachedEdGraph.Reset();
	CachedEdGraph = GetFocusedEdGraph();

	CachedGraphPanel.Reset();

	GetGraphData().CleanupGraph(GetFocusedEdGraph());

	GetViewLocation(LastGraphView, LastZoom);

	if (OnGraphChangedHandle.IsValid())
	{
		GetFocusedEdGraph()->RemoveOnGraphChangedHandler(OnGraphChangedHandle);
	}

	OnGraphChangedHandle = GetFocusedEdGraph()->AddOnGraphChangedHandler(FOnGraphChanged::FDelegate::CreateRaw(this, &FBAGraphHandler::OnGraphChanged));

#if BA_UE_VERSION_OR_LATER(5, 0)
	FCoreUObjectDelegates::OnObjectPreSave.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectPreSave.AddRaw(this, &FBAGraphHandler::HandleObjectSaved);
#endif

	AddGraphPanelOverlay();

	SetSelectedPin(nullptr);
}

void FBAGraphHandler::AddGraphPanelOverlay()
{
	TSharedPtr<SGraphEditor> GraphEditor = GetGraphEditor();
	TSharedPtr<SOverlay> EditorOverlay = FBAUtils::GetChildWidgetCasted<SOverlay>(GraphEditor, "SOverlay");

	if (!EditorOverlay.IsValid())
	{
		return;
	}

	// remove the old graph overlay
	if (GraphOverlay.IsValid())
	{
		EditorOverlay->RemoveSlot(GraphOverlay.ToSharedRef());
	}

	EditorOverlay->AddSlot()
	[
		SAssignNew(GraphOverlay, SBlueprintAssistGraphOverlay, AsShared())
	];
}

void FBAGraphHandler::OnGainFocus()
{
	if (GraphTasks->CacheSizeTask.NodeSizeTimeout > 0)
	{
		GraphTasks->CacheSizeTask.ShowSizeTimeoutNotification();
	}

	if (TSharedPtr<SGraphPanel> GraphPanel = GetGraphPanel())
	{
		if (FSlateApplication::Get().IsDragDropping())
		{
			TSharedPtr<FDragDropOperation> DragDropOp = FSlateApplication::Get().GetDragDroppingContent();
			if (!DragDropOp.IsValid())
			{
				FSlateApplication::Get().SetKeyboardFocus(GraphPanel, EFocusCause::WindowActivate);
			}
		}
	}
}

void FBAGraphHandler::OnLoseFocus()
{
	CancelActiveFormatting();
}

void FBAGraphHandler::Cleanup()
{
	if (OnGraphChangedHandle.IsValid())
	{
		if (auto EdGraph = GetFocusedEdGraph())
		{
			EdGraph->RemoveOnGraphChangedHandler(OnGraphChangedHandle);
		}
	}

#if BA_UE_VERSION_OR_LATER(5, 0)
	FCoreUObjectDelegates::OnObjectPreSave.RemoveAll(this);
#endif

	FormatRequest->Reset();
	NodeToReplace = nullptr;
	bLerpViewport = false;
	NodeSizeChangeDataMap.Empty();
	FormattingChangeDataMap.Empty();
	PreFormatNodePositions.Empty();

	DelayedGraphInitialized.Cancel();
	DelayedViewportZoomIn.Cancel();
	DelayedClearReplaceTransaction.Cancel();
	DelayedDetectGraphChanges.Cancel();
	DelayedCacheSizeFinished.Cancel();

	CancelActiveFormatting();
}

void FBAGraphHandler::Tick(float DeltaTime)
{
	// on some asset types (control rig prefab) they switch out the graph editor
	AssignNewGraphEditorFromTab();

	TSharedPtr<SGraphPanel> GraphPanel = GetGraphPanel();

	// the panel's graph object can also change, check this too
	if (GraphPanel.IsValid() && CachedEdGraph != GraphPanel->GetGraphObj())
	{
		InitGraphHandler();
	}

	EarlyShowOverlay();

	if (GraphOperation)
	{
		return GraphOperation->Tick(DeltaTime);
	}

	if (IsGraphReadOnly())
	{
		return;
	}

	DelayedGraphInitialized.Tick();

	DelayedCacheSizeFinished.Tick();

	if (DelayedGraphInitialized.IsComplete())
	{
		// check if the graph's initial zooming phase has ended
		if (!bInitialZoomFinished)
		{
			if ((LastGraphView == GraphPanel->GetViewOffset()) && (LastZoom == GraphPanel->GetZoomAmount()))
			{
				bInitialZoomFinished = true;
			}

			GetViewLocation(LastGraphView, LastZoom);
		}
	}
	else // don't update until the graph handler has been initialized
	{
		return;
	}

	DelayedDetectGraphChanges.Tick();

	DelayedClearReplaceTransaction.Tick();

	UpdateSelectedNode();

	UpdateSelectedPin();

	FormatRequest->UpdateNodesRequiringFormatting();

	UpdateLerpViewport(DeltaTime);

	GraphTasks->TickTasks(DeltaTime);
}

bool FBAGraphHandler::OnKeyDown(const FKey& Key)
{
	if (GraphOperation)
	{
		if (Key == EKeys::Escape)
		{
			GraphOperation.Reset();
			return false;
		}

		return GraphOperation->OnKeyDown(Key);
	}

	return false;
}

bool FBAGraphHandler::OnKeyUp(const FKey& Key)
{
	if (GraphOperation)
	{
		return GraphOperation->OnKeyUp(Key);
	}

	return false;
}

void FBAGraphHandler::UpdateSelectedNode()
{
	UEdGraphNode* CurrentSelectedNode = GetSelectedNode();

	UEdGraphNode* LastNode = LastSelectedNode.IsValid() ? LastSelectedNode.Get() : nullptr;
	if (CurrentSelectedNode != LastNode)
	{
		LastSelectedNode = CurrentSelectedNode;
		OnSelectionChanged(LastNode, CurrentSelectedNode);
	}
}

void FBAGraphHandler::UpdateSelectedPin()
{
	if (SelectedPinHandle.IsValid())
	{
		UEdGraphPin* SelectedPin = GetSelectedPin();
		if (!SelectedPin)
		{
			SetSelectedPin(nullptr);

			// on clearing try to select a valid pin
			if (UEdGraphNode* Node = GetSelectedNode())
			{
				TrySelectFirstPinOnNode(Node);
			}
		}
	}
	else
	{
		// while we have a selected node, always try to have a selected pin
		if (UEdGraphNode* Node = GetSelectedNode())
		{
			TrySelectFirstPinOnNode(Node);
		}
	}
}

bool FBAGraphHandler::TrySelectFirstPinOnNode(UEdGraphNode* NewNode)
{
	TSharedPtr<SGraphPanel> GraphPanel = GetGraphPanel();
	if (!GraphPanel)
	{
		return false;
	}

	const bool bSelectExecPin = UBASettings_EditorFeatures::Get().PinSelectionMethod_Execution == EBAPinSelectionMethod::Value && FBAUtils::IsNodeImpure(NewNode);
	const bool bSelectParamPin = UBASettings_EditorFeatures::Get().PinSelectionMethod_Parameter == EBAPinSelectionMethod::Value && FBAUtils::IsNodePure(NewNode);
	if (bSelectExecPin || bSelectParamPin)
	{
		if (TrySelectFirstValuePinOnNode(NewNode))
		{
			return true;
		}
	}

	TArray<UEdGraphPin*> Pins = FBAUtils::GetPinsByDirection(NewNode);
	Pins.RemoveAll([&GraphPanel](UEdGraphPin* Pin)
	{
		return !FBAUtils::IsPinVisible(GraphPanel, Pin);
	});

	if (Pins.Num() > 0)
	{
		EEdGraphPinDirection GraphDirection = EGPD_Output;
		if (FBAFormatterSettings* FormatterSettings = UBASettings::FindFormatterSettings(GetFocusedEdGraph()))
		{
			GraphDirection = FormatterSettings->FormatterDirection;
		}

		const auto& Sorter = [&](const UEdGraphPin& PinA, const UEdGraphPin& PinB)
		{
			// pins in graph direction first
			const uint8 bIsSameDirA = PinA.Direction == GraphDirection;
			const uint8 bIsSameDirB = PinB.Direction == GraphDirection;
			if (bIsSameDirA != bIsSameDirB)
			{
				return bIsSameDirA > bIsSameDirB;
			}

			// exec pins first
			const uint8 PinAExec = PinA.PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
			const uint8 PinBExec = PinB.PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
			if (PinAExec != PinBExec)
			{
				return PinAExec > PinBExec;
			}

			// sort by height
			return GetPinY(&PinA) < GetPinY(&PinB);
		};

		Pins.StableSort(Sorter);

		SetSelectedPin(Pins[0]);
		return true;
	}

	return false;
}

bool FBAGraphHandler::TrySelectFirstValuePinOnNode(UEdGraphNode* NewNode)
{
	if (!NewNode)
	{
		return false;
	}

	TSharedPtr<SGraphPanel> GraphPanel = GetGraphPanel();
	if (!GraphPanel)
	{
		return false;
	}

	// only have value pins in the input direction
	TArray<UEdGraphPin*> Pins = FBAUtils::GetParameterPins(NewNode, EGPD_Input);
	UEdGraphPin* SelfPin = FBAUtils::FindSelfPin(NewNode);
	Pins.RemoveAll([GraphPanel, SelfPin](UEdGraphPin* Pin)
	{
		if (Pin->LinkedTo.Num() > 0)
		{
			return true;
		}

		const TSharedPtr<SGraphPin> GraphPin = FBAUtils::GetGraphPin(GraphPanel, Pin);
		if (GraphPin.IsValid())
		{
			if (GraphPin->GetPinObj() == SelfPin)
			{
				return true;
			}

			if (!FBAUtils::IsPinVisible(GraphPin))
			{
				return true;
			}
		}

		return false;
	});

	if (Pins.Num() > 0)
	{
		const auto& Sorter = [&](const UEdGraphPin& PinA, const UEdGraphPin& PinB)
		{
			// sort by height
			return GetPinY(&PinA) < GetPinY(&PinB);
		};

		Pins.StableSort(Sorter);

		SetSelectedPin(Pins[0]);
		return true;
	}

	return false;
}

TSharedPtr<SWindow> FBAGraphHandler::GetWindow()
{
	return CachedTab.IsValid() ? FBAUtils::GetParentWindow(CachedTab.Pin()) : nullptr;
}

bool FBAGraphHandler::IsWindowActive()
{
	return GetWindow() == FSlateApplication::Get().GetActiveTopLevelWindow();
}

bool FBAGraphHandler::IsGraphPanelFocused()
{
	if (TSharedPtr<SGraphPanel> Panel = GetGraphPanel())
	{
		if (Panel->HasAnyUserFocus().IsSet())
		{
			return true;
		}

		// check if the focused widget is a child of the graph panel
		if (TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetUserFocusedWidget(0))
		{
			TSharedPtr<SWidget> FoundGraphPanel = FBAUtils::ScanParentContainersForTypes(FocusedWidget, { "SGraphPanel" }, "SDockingTabStack");
			if (FoundGraphPanel == Panel)
			{
				return true;
			}
		}
	}

	return false;
}

bool FBAGraphHandler::IsGraphReadOnly()
{
	if (!HasValidGraphReferences())
	{
		return true;
	}

	return FBlueprintEditorUtils::IsGraphReadOnly(GetFocusedEdGraph()) || !GetGraphPanel()->IsGraphEditable();
}

bool FBAGraphHandler::HasValidGraphReferences()
{
	return GetFocusedEdGraph() && GetGraphPanel() && GetGraphEditor();
}

bool FBAGraphHandler::TryAutoFormatNode(const TArray<UEdGraphNode*>& Nodes, TSharedPtr<FBAScopedGraphAction> InPendingTransaction, FEdGraphFormatterParameters Parameters)
{
	if (Nodes.IsEmpty())
	{
		return false;
	}

	const auto AutoFormatting = UBASettings::GetFormatterSettings(GetFocusedEdGraph()).GetAutoFormatting();
	if (AutoFormatting == EBAAutoFormatting::Never)
	{
		return false;
	}

	const bool bApplyFormatAll = UBASettings::Get().bAutoPositionEventNodes
		&& FBAUtils::IsBlueprintGraph(GetFocusedEdGraph())
		&& Nodes.ContainsByPredicate([&](UEdGraphNode* Node) { return FBAUtils::IsEventNode(Node); });

	if (bApplyFormatAll)
	{
		FormatRequest->RequestFormatAll();
	}
	else
	{
		if (AutoFormatting == EBAAutoFormatting::FormatSingleConnected)
		{
			// only format the new nodes and the node to keep still (if there is one)
			Parameters.LimitedNodes.Append(Nodes);
			if (UEdGraphNode* NodeToKeepStill = Parameters.NodeToKeepStill.Get())
			{
				Parameters.LimitedNodes.Add(NodeToKeepStill);
			}
		}

		FormatRequest->RequestFormatNode(Nodes[0], InPendingTransaction, Parameters);
		return true;
	}

	return false;
}

void FBAGraphHandler::EarlyShowOverlay()
{
	if (!NewNodeForOverlayVisibility.IsValid())
	{
		return;
	}

	if (!GraphOverlay)
	{
		return;
	}

	UEdGraphNode* Node = NewNodeForOverlayVisibility.Get();
	if (!IsAnyNodeBeingRenamed() && !Node->GetCanRenameNode())
	{
		const EBAAutoFormatting AutoFormatting = UBASettings::GetFormatterSettings(GetFocusedEdGraph()).GetAutoFormatting();
		if (AutoFormatting != EBAAutoFormatting::Never || UBASettings::Get().bDetectNewNodesAndCacheNodeSizes)
		{
			UE_LOG(LogBlueprintAssist, Verbose, TEXT("[%hs] Early show overlay"), __FUNCTION__);
			GraphOverlay->SizeProgressWidget->ShowOverlay();
		}
	}

	NewNodeForOverlayVisibility.Reset();
}

void FBAGraphHandler::OnGraphInitializedDelayed()
{
	LastNodes = FBAMiscUtils::AsWeakObjectPtrArray(GetFocusedEdGraph()->Nodes);

	if (UBASettings::Get().bDetectNewNodesAndCacheNodeSizes)
	{
		CacheNodeSizes(GetFocusedEdGraph()->Nodes);
	}

	for (UEdGraphNode* Node : GetFocusedEdGraph()->Nodes)
	{
		NodeSizeChangeDataMap.Add(Node->NodeGuid, FBANodeSizeChangeData(Node));

		const FBANodeData& NodeData = GetNodeData(Node);
		if (NodeData.NodeGroup.IsValid())
		{
			// initialize the node groups
			NodeGroups.FindOrAdd(NodeData.NodeGroup).Add(Node);
		}

		// if the node hasn't moved from the last formatted location, assume the formatting data should be saved now
		if (Node->NodePosX == NodeData.Last.X && Node->NodePosY == NodeData.Last.Y)
		{
			UpdateNodeChangeData(Node);
		}
	}
}

void FBAGraphHandler::OnSelectionChanged(UEdGraphNode* PreviousNode, UEdGraphNode* NewNode)
{
	if (NewNode == nullptr)
	{
		SetSelectedPin(nullptr);
		return;
	}

	if (FBAUtils::IsCommentNode(NewNode) || FBAUtils::IsKnotNode(NewNode))
	{
		SetSelectedPin(nullptr);
		return;
	}

	TSharedPtr<SGraphPanel> GraphPanel = GetGraphPanel();
	if (!GraphPanel)
	{
		SetSelectedPin(nullptr);
		return;
	}

	UEdGraphPin* SelectedPin = GetSelectedPin();

	const bool bKeepCurrentPin = (SelectedPin != nullptr) && (SelectedPin->GetOwningNode() == NewNode);
	if (bKeepCurrentPin)
	{
		return;
	}

	if (!TrySelectFirstPinOnNode(NewNode))
	{
		SetSelectedPin(nullptr);
	}
}

bool FBAGraphHandler::TryInsertNewNode(UEdGraphNode* NewNode)
{
	const bool bInsertKeyDown = FBAInputProcessor::Get().IsInputChordDown(UBASettings_EditorFeatures::Get().InsertNewNodeKeyChord);
	const auto& BASettings = UBASettings_EditorFeatures::Get();
	if (FBAUtils::IsNodeImpure(NewNode))
	{
		if (bInsertKeyDown != BASettings.bAlwaysConnectExecutionFromParameter)
		{
			return LinkExecWhenCreatedFromParameter(NewNode, true);
		}

		if (bInsertKeyDown != BASettings.bAlwaysInsertFromExecution)
		{
			return AutoInsertExecNode(NewNode);
		}
	}
	else if (FBAUtils::IsNodePure(NewNode))
	{
		if (bInsertKeyDown != BASettings.bAlwaysInsertFromParameter)
		{
			return AutoInsertParameterNode(NewNode);
		}
	}

	return false;
}

bool FBAGraphHandler::LinkExecWhenCreatedFromParameter(UEdGraphNode* NodeCreated, bool bInsert)
{
	TArray<UEdGraphPin*> LinkedPins = FBAUtils::GetLinkedPins(NodeCreated);

	// if we drag off a parameter pin, link the exec pin too (if it exists)
	const auto IsPinOwningNodeImpure = [](UEdGraphPin* Pin)
	{
		return FBAUtils::IsNodeImpure(Pin->GetOwningNode());
	};

	const auto IsLinkedToImpureNode = [IsPinOwningNodeImpure](UEdGraphPin* Pin)
	{
		// skip delegate pins
		return !FBAUtils::IsDelegatePin(Pin) && Pin->LinkedTo.FilterByPredicate(IsPinOwningNodeImpure).Num() > 0;
	};

	TArray<UEdGraphPin*> PinsLinkedToImpureNodes = LinkedPins.FilterByPredicate(IsLinkedToImpureNode);

	if (PinsLinkedToImpureNodes.Num() == 1)
	{
		UEdGraphPin* MyLinkedPin = PinsLinkedToImpureNodes[0];
		if (MyLinkedPin->LinkedTo.Num() == 1)
		{
			UEdGraphPin* OtherLinkedPin = MyLinkedPin->LinkedTo[0];

			if (OtherLinkedPin != nullptr)
			{
				UEdGraphNode* OtherLinkedNode = OtherLinkedPin->GetOwningNode();

				if (FBAUtils::IsNodeImpure(OtherLinkedNode))
				{
					TArray<UEdGraphPin*> ExecPins = FBAUtils::GetExecPins(NodeCreated, MyLinkedPin->Direction);
					if (ExecPins.Num() == 0)
					{
						return false;
					}

					FBANodePinHandle FirstPin(ExecPins[0]);

					const int NumLinkedExecPins = ExecPins.FilterByPredicate(FBAUtils::IsPinLinked).Num();

					if (NumLinkedExecPins == 0)
					{
						TArray<UEdGraphPin*> OtherExecPins = FBAUtils::GetExecPins(OtherLinkedNode, UEdGraphPin::GetComplementaryDirection(MyLinkedPin->Direction));
						if (OtherExecPins.Num() > 0)
						{
							FBANodePinHandle OtherExecPin = OtherExecPins[0];
							if (OtherExecPin->LinkedTo.Num() > 0)
							{
								UEdGraphPin* FirstLinkedTo = OtherExecPin->LinkedTo[0];

								// if we aren't inserting and the exec pin has links, then don't do anything
								if (!bInsert)
								{
									return false;
								}

								TArray<UEdGraphPin*> MyPinsInDirection = FBAUtils::GetExecPins(NodeCreated, OtherExecPin->Direction);
								if (MyPinsInDirection.Num() > 0)
								{
									FBAUtils::TryCreateConnectionUnsafe(FirstLinkedTo, MyPinsInDirection[0], EBABreakMethod::Always);
								}
							}

							FBAUtils::TryCreateConnection(FirstPin, OtherExecPin, EBABreakMethod::Always);
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}

bool FBAGraphHandler::AutoInsertExecNode(UEdGraphNode* NodeCreated)
{
	if (GetSelectedPin() == nullptr)
	{
		return false;
	}

	// output direction already has inbuilt insertion, skip this case
	if (PrevSelectedLink.GetDirection() == EGPD_Output)
	{
		return false;
	}

	TArray<UEdGraphPin*> LinkedExecPins = FBAUtils::GetLinkedPins(NodeCreated).FilterByPredicate(FBAUtils::IsExecPin);
	if (LinkedExecPins.Num() != 1)
	{
		return false;
	}

	// when dragging from exec input pin B in the in the chain A->B
	// makes it so we create A->C->B (by default it create A->B | C<-B)
	if (UEdGraphPin* PinToLinkTo = PrevSelectedLink.GetToPin())
	{
		// try to link one of our pins to the pin to link to
		for (UEdGraphPin* Pin : FBAUtils::GetExecPins(NodeCreated, PrevSelectedLink.GetDirection()))
		{
			if (FBAUtils::TryCreateConnectionUnsafe(Pin, PinToLinkTo, EBABreakMethod::Always))
			{
				return true;
			}
		}
	}

	return false;
}

bool FBAGraphHandler::AutoInsertParameterNode(UEdGraphNode* NodeCreated)
{
	// when dragging from a pin creating node C in a chain A->B
	// makes it so we create A->C->B (by default it creates A->B | A->C)
	TArray<UEdGraphPin*> LinkedParameterPins = FBAUtils::GetLinkedPins(NodeCreated).FilterByPredicate(FBAUtils::IsParameterPin);

	if (LinkedParameterPins.Num() > 0)
	{
		if (UEdGraphPin* PinToLinkTo = PrevSelectedLink.GetToPin())
		{
			// try to link one of our pins to the pin to link to
			for (UEdGraphPin* Pin : FBAUtils::GetParameterPins(NodeCreated, PrevSelectedLink.GetDirection()))
			{
				if (FBAUtils::TryCreateConnectionUnsafe(Pin, PinToLinkTo, EBABreakMethod::Always))
				{
					return true;
				}
			}
		}
	}

	return false;
}

void FBAGraphHandler::SetGraphEditor(TWeakPtr<SGraphEditor> NewGraphEditor)
{
	CachedGraphEditor = NewGraphEditor;
	InitGraphHandler();
}

void FBAGraphHandler::ReplaceSavedSelectedNode(UEdGraphNode* NewNode)
{
	if (!NodeToReplace.IsValid())
	{
		return;
	}

	TArray<UEdGraphPin*> NodeToReplacePins = NodeToReplace->Pins;

	NodeToReplacePins.StableSort([](UEdGraphPin& PinA, UEdGraphPin& PinB)
	{
		return PinA.Direction > PinB.Direction;
	});

	TArray<FPinLink> PinsToLink;

	TArray<UEdGraphPin*> NewNodePins = NewNode->Pins;

	TSet<UEdGraphPin*> PinsConnected;

	// loop through our pins and check which pins can be connected to the new node
	for (int i = 0; i < 2; ++i)
	{
		for (UEdGraphPin* Pin : NodeToReplacePins)
		{
			if (Pin->LinkedTo.Num() == 0)
			{
				continue;
			}

			if (PinsConnected.Contains(Pin))
			{
				continue;
			}

			for (UEdGraphPin* NewNodePin : NewNodePins)
			{
				if (PinsConnected.Contains(NewNodePin))
				{
					continue;
				}

				// on the first run (i = 0), we only use pins which have the same name
				if (FBAUtils::GetPinName(Pin) == FBAUtils::GetPinName(NewNodePin) || i > 0)
				{
					TArray<UEdGraphPin*> LinkedTo = Pin->LinkedTo;

					bool bConnected = false;
					for (UEdGraphPin* LinkedPin : LinkedTo)
					{
						if (FBAUtils::CanConnectPins(LinkedPin, NewNodePin, true, false))
						{
							PinsToLink.Add(FPinLink(LinkedPin, NewNodePin));
							PinsConnected.Add(Pin);
							PinsConnected.Add(NewNodePin);
							bConnected = true;
						}
					}

					if (bConnected)
					{
						break;
					}
				}
			}
		}
	}

	// link the pins marked in the last two loops
	for (auto& PinToLink : PinsToLink)
	{
		for (UEdGraphPin* Pin : NewNode->Pins)
		{
			if (Pin->PinId == PinToLink.To->PinId)
			{
				FBAUtils::TryCreateConnectionUnsafe(PinToLink.From, Pin, EBABreakMethod::Default);
				break;
				//UE_LOG(LogBlueprintAssist, Warning, TEXT("\tConnected"));
			}
		}
	}

	// insert the new node into correct comment boxes
	const TArray<UEdGraphNode_Comment*> AllComments = FBAUtils::GetCommentNodesFromGraph(GetFocusedEdGraph());
	TArray<UEdGraphNode_Comment*> ContainingComments = FBAUtils::GetContainingCommentNodes(AllComments, NodeToReplace.Get());
	for (UEdGraphNode_Comment* Comment : ContainingComments)
	{
		Comment->AddNodeUnderComment(NewNode);
	}

	FBAUtils::SafeDelete(AsShared(), NodeToReplace.Get());

	NodeToReplace = nullptr;

	FEdGraphFormatterParameters Parameters;
	const bool bPendingFormatting = TryAutoFormatNode({NewNode});

	DelayedClearReplaceTransaction.Cancel();

	// when we format we will reset the transaction
	if (!bPendingFormatting)
	{
		ReplaceNewNodeTransaction.Reset();
	}
}

void FBAGraphHandler::MoveUnrelatedNodes(TSharedPtr<FFormatterInterface> Formatter)
{
	// only move unrelated if we have an event node as our root node
	if (!FBAUtils::IsEventNode(Formatter->GetRootNode()))
	{
		return;
	}

	const TSet<UEdGraphNode*> FormattedNodes = Formatter->GetFormattedNodes();
	const FSlateRect FormatterBounds = FBAUtils::GetNodeArrayBounds(FormattedNodes.Array());

	UEdGraph* Graph = GetFocusedEdGraph();
	if (Graph == nullptr)
	{
		return;
	}

	float CHECK_INFINITE_LOOP = 0;

	// check all nodes on the graph
	TArray<UEdGraphNode*> Nodes = Graph->Nodes;

	while (Nodes.Num())
	{
		const auto NextNode = Nodes.Pop();

		if (FBAUtils::IsCommentNode(NextNode))
		{
			continue;
		}

		const auto NodeTree = FBAUtils::GetNodeTree(NextNode);

		const bool bSkipNodeTree = NodeTree.Array().ContainsByPredicate([&FormattedNodes](UEdGraphNode* Node)
		{
			return FormattedNodes.Contains(Node);
		});

		if (bSkipNodeTree)
		{
			continue;
		}

		const FSlateRect NodeTreeBounds = FBAUtils::GetNodeArrayBounds(NodeTree.Array());
		float OffsetX = 0;
		if (FSlateRect::DoRectanglesIntersect(FormatterBounds, NodeTreeBounds))
		{
			OffsetX = FormatterBounds.Bottom - NodeTreeBounds.Top + 20;
		}

		for (auto Node : NodeTree)
		{
			if (OffsetX != 0)
			{
				Node->Modify();
				Node->NodePosY += OffsetX;
			}

			Nodes.Remove(Node);
		}

		if (CHECK_INFINITE_LOOP++ > 10000)
		{
			UE_LOG(LogBlueprintAssist, Error, TEXT("Infinite loop detected in MoveUnrelatedNodes"));
			break;
		}
	}
}

void FBAGraphHandler::OnGraphChanged(const FEdGraphEditAction& Action)
{
	// handle early show overlay, only trigger on blueprint graph type as this doesn't work on most other graph types
	if (FBAUtils::IsBlueprintGraph(GetFocusedEdGraph()) && UBASettings_Advanced::Get().bImmediatelyShowGraphOverlay)
	{
		// GRAPHACTION_AddNode does not trigger on copy paste
		if ((Action.Action & GRAPHACTION_AddNode) != 0 && Action.Nodes.Num() == 1)
		{
			if (UEdGraphNode* Node = const_cast<UEdGraphNode*>(Action.Nodes.Get(FSetElementId::FromInteger(0))))
			{
				if (!FBAUtils::IsKnotNode(Node))
				{
					NewNodeForOverlayVisibility = Node;
				}
			}
		}
	}

	DelayedDetectGraphChanges.StartDelay(1);
}

#if BA_UE_VERSION_OR_LATER(5, 0)
void FBAGraphHandler::HandleObjectSaved(UObject* Obj, FObjectPreSaveContext PreSaveContext)
{
	if (!UBASettings::Get().bFormatAllAfterSaving)
	{
		return;
	}

	// check we saved our graph
	if (Obj != GetFocusedEdGraph())
	{
		return;
	}

	// skip if we aren't active
	if (FBAUtils::GetCurrentGraphHandler().Get() != this)
	{
		return;
	}

	// skip if it was not a user save
	const bool bAutosave = (PreSaveContext.GetSaveFlags() & SAVE_FromAutosave) != 0;
	if (PreSaveContext.IsProceduralSave() || bAutosave)
	{
		return;
	}

	FormatRequest->RequestFormatAllAndSave();
}
#endif

void FBAGraphHandler::RequestFormatNode(UEdGraphNode* Node, TSharedPtr<FBAScopedGraphAction> InPendingTransaction, FEdGraphFormatterParameters InFormatterParameters)
{
	FormatRequest->RequestFormatNode(Node, InPendingTransaction, InFormatterParameters);
}

void FBAGraphHandler::ResetSingleNewNodeTransaction()
{
	DelayedClearReplaceTransaction.StartDelay(2);
}

void FBAGraphHandler::ResetReplaceNodeTransaction()
{
	if (ReplaceNewNodeTransaction.IsValid())
	{
		ReplaceNewNodeTransaction->Cancel();
		ReplaceNewNodeTransaction.Reset();
	}
}

int32 FBAGraphHandler::GetPinY(const UEdGraphPin* Pin)
{
	if (!Pin) { return 0; }

	UEdGraphNode* OwningNode = Pin->GetOwningNode();
	if (!OwningNode)
	{
		return 0;
	}

	const FBANodeData& FoundNodeData = GetNodeData(OwningNode);
	if (const float* FoundPinOffset = FoundNodeData.CachedPins.Find(Pin->PinId))
	{
		return FMath::RoundToInt(OwningNode->NodePosY + *FoundPinOffset);
	}

	// cache pin offset
	TSharedPtr<SGraphPanel> GraphPanel = GetGraphPanel();
	if (GraphPanel.IsValid())
	{
		TSharedPtr<SGraphNode> GraphNode = GetGraphNode(OwningNode);
		if (GraphNode.IsValid())
		{
			TSharedPtr<SGraphPin> GraphPin = GraphNode->FindWidgetForPin(const_cast<UEdGraphPin*>(Pin));
			if (GraphPin.IsValid())
			{
				if (GraphPin->GetPinObj() != nullptr)
				{
					return FMath::RoundToInt(OwningNode->NodePosY + GraphPin->GetNodeOffset().Y);
				}
			}
		}
	}

	return OwningNode->NodePosY;
}

void FBAGraphHandler::SetSelectedPin(UEdGraphPin* NewPin, bool bLerpIntoView)
{
	// clear the highlight on the previous pin
	if (SelectedPinHandle.IsValid() && SelectedPinHandle != FBAGraphPinHandle(NewPin))
	{
		if (GraphOverlay)
		{
			GraphOverlay->RemoveHighlightedPin(SelectedPinHandle);
		}

		// clear keyboard focus if we were focusing something inside the previous selected pin
		TSharedPtr<SGraphPin> PrevGraphPin = FBAUtils::GetGraphPin(GetGraphPanel(), SelectedPinHandle.GetPin());
		if (PrevGraphPin)
		{
			if (TSharedPtr<SWidget> KeyboardFocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget())
			{
				if (FBAUtils::IsParentWidget(PrevGraphPin, KeyboardFocusedWidget))
				{
					FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
				}
			}
		}

		PrevSelectedPinHandle = SelectedPinHandle;
	}

	if (NewPin)
	{
		// if the node is not already selected, select it
		if (UEdGraphNode* OwningNode = NewPin->GetOwningNodeUnchecked())
		{
			if (!GetSelectedNodes().Contains(OwningNode))
			{
				SelectNode(OwningNode, bLerpIntoView);
			}
		}

		SelectedPinHandle = FBAGraphPinHandle(NewPin);

		UEdGraphPin* ToPin = nullptr;
		if (NewPin->LinkedTo.Num())
		{
			TArray<UEdGraphPin*> Sorted = NewPin->LinkedTo;
			Sorted.Sort([this](UEdGraphPin& A, UEdGraphPin& B)
			{
				return GetPinY(&A) < GetPinY(&B);
			});

			ToPin = Sorted[0];
		}

		PrevSelectedLink = FPinLink(NewPin, ToPin);

		// highlight the pin
		if (GraphOverlay)
		{
			GraphOverlay->AddHighlightedPin(SelectedPinHandle, UBASettings_EditorFeatures::Get().SelectedPinHighlightColor);
		}
	}
	else
	{
		SelectedPinHandle.Invalidate();
		PrevSelectedLink.Invalidate();
	}
}

void FBAGraphHandler::UpdateLerpViewport(const float DeltaTime)
{
	if (bLerpViewport)
	{
		FVector2D CurrentView;
		float CurrentZoom;
		GetViewLocation(CurrentView, CurrentZoom);

		TSharedPtr<SGraphPanel> GraphPanel = GetGraphPanel();
		if (!GraphPanel.IsValid())
		{
			return;
		}

		FVector2D TargetView = TargetLerpLocation;
		if (bCenterWhileLerping)
		{
			const FGeometry Geometry = GraphPanel->GetTickSpaceGeometry();
			const FVector2D HalfOfScreenInGraphSpace = 0.5f * Geometry.Size / GraphPanel->GetZoomAmount();
			TargetView -= HalfOfScreenInGraphSpace;
		}

		if (FVector2D::Distance(CurrentView, TargetView) > 10.f)
		{
			const FVector2D NewView = FMath::Vector2DInterpTo(CurrentView, TargetView, DeltaTime, 10.f);
			SetViewLocation(NewView, CurrentZoom);
		}
		else
		{
			bLerpViewport = false;
		}
	}
}

void FBAGraphHandler::BeginLerpViewport(const FVector2D TargetView, const bool bCenter)
{
	TargetLerpLocation = TargetView;
	bLerpViewport = true;
	bCenterWhileLerping = bCenter;
}

void FBAGraphHandler::CancelActiveFormatting()
{
	FormatRequest->Reset();

	GraphOperation.Reset();

	ResetTransactions();

	GraphTasks->ClearTasks();

	if (GraphOverlay)
	{
		GraphOverlay->SizeProgressWidget->HideOverlay();
	}
}

UEdGraph* FBAGraphHandler::GetFocusedEdGraph()
{
	if (CachedEdGraph.IsValid())
	{
		return CachedEdGraph.Get();
	}

	TSharedPtr<SGraphEditor> GraphEditor = GetGraphEditor();
	if (GraphEditor)
	{
		UEdGraph* Graph = GraphEditor->GetCurrentGraph();
		CachedEdGraph = Graph;
		return GraphEditor->GetCurrentGraph();
	}

	TSharedPtr<SGraphPanel> GraphPanel = GetGraphPanel();
	if (GraphPanel.IsValid())
	{
		UEdGraph* Graph = GraphPanel->GetGraphObj();
		CachedEdGraph = Graph;
		return Graph;
	}

	return nullptr;
}

TSharedPtr<SGraphEditor> FBAGraphHandler::GetGraphEditor()
{
	if (CachedGraphEditor.IsValid())
	{
		return CachedGraphEditor.Pin();
	}

	// if our cached graph editor is not valid try to assign a new one
	return AssignNewGraphEditorFromTab();
}

TSharedPtr<SGraphPanel> FBAGraphHandler::GetGraphPanel()
{
	if (CachedGraphPanel.IsValid())
	{
		return CachedGraphPanel.Pin();
	}

	TSharedPtr<SGraphEditor> GraphEditor = GetGraphEditor();
	if (!GraphEditor.IsValid())
	{
		return nullptr;
	}

	// try to grab the graph panel from the graph editor
	// check all children as the graph panel can use invalidation (see material graph)
	TSharedPtr<SGraphPanel> GraphPanelWidget = FIND_CHILD_WIDGET_ALL_CHILDREN(GraphEditor, SGraphPanel);
	if (GraphPanelWidget.IsValid())
	{
		CachedGraphPanel = GraphPanelWidget;
		return CachedGraphPanel.Pin();
	}

	return nullptr;
}

TSharedPtr<SGraphActionMenu> FBAGraphHandler::GetGraphActionMenu()
{
	if (CachedGraphActionMenu.IsValid())
	{
		return CachedGraphActionMenu.Pin();
	}

	if (TSharedPtr<SGraphActionMenu> ActionMenu = FIND_CHILD_WIDGET(GetWindow(), SGraphActionMenu))
	{
		CachedGraphActionMenu = ActionMenu;
		return ActionMenu;
	}

	return nullptr;
}

IAssetEditorInstance* FBAGraphHandler::GetAssetEditorInstance() const
{
	return UBAAssetEditorHandlerObject::Get()->GetEditorFromTab(GetTab());
}

const UBAGraphSchema& FBAGraphHandler::GetSchema()
{
	return UBAGraphSchema::Get(GetFocusedEdGraph());
}

UBlueprint* FBAGraphHandler::GetBlueprint()
{
	if (UEdGraph* Graph = GetFocusedEdGraph())
	{
		return Graph->GetTypedOuter<UBlueprint>();
	}

	return nullptr;
}

TSharedPtr<SMyBlueprint> FBAGraphHandler::GetMyBlueprint()
{
	if (const FBlueprintEditor* BPEditor = FBAUtils::GetBlueprintEditorForGraph(GetFocusedEdGraph()))
	{
		return BPEditor->*GMyBlueprintWidget;
	}

	return nullptr;
}

UEdGraphNode* FBAGraphHandler::GetSelectedNode(bool bAllowCommentNodes)
{
	TArray<UEdGraphNode*> SelectedNodes = GetSelectedNodes(bAllowCommentNodes).Array();
	return SelectedNodes.Num() == 1 ? SelectedNodes[0] : nullptr;
}

TSet<UEdGraphNode*> FBAGraphHandler::GetSelectedNodes(bool bAllowCommentNodes)
{
	TSet<UEdGraphNode*> SelectedNodes;

	auto GraphEditor = GetGraphEditor();
	if (GraphEditor.IsValid())
	{
		for (UObject* Obj : GraphEditor->GetSelectedNodes())
		{
			if (UEdGraphNode* Node = Cast<UEdGraphNode>(Obj))
			{
				if (FBAUtils::IsGraphNode(Node) && (bAllowCommentNodes || !FBAUtils::IsCommentNode(Node)))
				{
					SelectedNodes.Emplace(Node);
				}
			}
		}
	}

	return SelectedNodes;
}

void FBAGraphHandler::SelectNodes(const TSet<UEdGraphNode*>& Nodes)
{
	UEdGraph* Graph = GetFocusedEdGraph();
	if (!Graph)
		return;

	TSet<const UEdGraphNode*> SelectionSet;
	Algo::Transform(Nodes, SelectionSet, [](UEdGraphNode* Node){ return Node; });
	Graph->SelectNodeSet(SelectionSet);
}

FSlateRect FBAGraphHandler::GetCachedNodeBounds(UEdGraphNode* Node, bool bWithCommentBubble)
{
	if (!Node)
	{
		return FSlateRect();
	}

	FVector2D Pos(Node->NodePosX, Node->NodePosY);

	FIntPoint Size(300, 150);
	if (FBAUtils::IsKnotNode(Node))
	{
		Size = FIntPoint(FBAUtils::GetKnotNodeSize().X, FBAUtils::GetKnotNodeSize().Y);
	}
	else
	{
		FBANodeData& FoundNodeData = GetNodeData(Node);
		if (FoundNodeData.HasSize())
		{
			Size.X = FoundNodeData.GetNodeSize().X;
			Size.Y = FoundNodeData.GetNodeSize().Y;
		}
		else
		{
			if (TSharedPtr<SGraphNode> GraphNode = FBAUtils::GetGraphNode(GetGraphPanel(), Node))
			{
				const auto DesiredSize = GraphNode->GetDesiredSize();
				Size.X = FMath::RoundToInt(DesiredSize.X);
				Size.Y = FMath::RoundToInt(DesiredSize.Y);
			}
		}
	}

	if (!FBAUtils::IsCommentNode(Node))
	{
		const FBANodeData& FoundNodeData = GetNodeData(Node);
		if (FoundNodeData.HasCommentBubbleSize())
		{
			const FIntPoint& BubbleSize = FoundNodeData.GetCommentBubbleSize();
			if (bWithCommentBubble && Node->bCommentBubbleVisible)
			{
				Pos.Y -= BubbleSize.Y;
				Size.Y += BubbleSize.Y;
				Size.X = FMath::Max(Size.X, BubbleSize.X);
			}
		}
	}

	return FSlateRect::FromPointAndExtent(Pos, Size);
}

UEdGraphPin* FBAGraphHandler::GetSelectedPin()
{
	if (!SelectedPinHandle.IsValid())
	{
		return nullptr;
	}

	TSharedPtr<SGraphPanel> GraphPanel = GetGraphPanel();
	if (!GraphPanel.IsValid())
	{
		return nullptr;
	}

	UEdGraphPin* PinObj = SelectedPinHandle.GetPin(false);
	if (!PinObj || PinObj->bHidden || PinObj->bWasTrashed || PinObj->bOrphanedPin)
	{
		return nullptr;
	}

	return PinObj;
}

TSharedPtr<SGraphNode> FBAGraphHandler::GetGraphNode(UEdGraphNode* Node)
{
	if (!Node)
	{
		return nullptr;
	}

	TSharedPtr<SGraphPanel> GraphPanel = GetGraphPanel();
	if (GraphPanel.IsValid())
	{
		return GraphPanel->GetNodeWidgetFromGuid(Node->NodeGuid);
	}

	return nullptr;
}

void FBAGraphHandler::RefreshNodeSize(UEdGraphNode* Node)
{
	if (FBAUtils::IsKnotNode(Node))
	{
		return;
	}

	if (FBAUtils::IsGraphNode(Node))
	{
		GetNodeData(Node).ResetSize();
		AddPendingSize(Node);
	}
	else if (FBAUtils::IsCommentNode(Node))
	{
		AddPendingSize(Node);
	}
}

bool FBAGraphHandler::AddPendingSize(UEdGraphNode* Node)
{
	UE_LOG(LogBlueprintAssist, Verbose, TEXT("[%hs] Request cache node size: %s"), __FUNCTION__, *FBAUtils::GetNodeName(Node));
	if (!GraphTasks->CacheSizeTask.PendingSize.Contains(Node))
	{
		GraphTasks->CacheSizeTask.PendingSize.Add(Node);
		return true;
	}

	return false;
}

void FBAGraphHandler::RefreshAllNodeSizes()
{
	for (UEdGraphNode* Node : GetFocusedEdGraph()->Nodes)
	{
		RefreshNodeSize(Node);
	}
}

void FBAGraphHandler::ResetTransactions()
{
	ReplaceNewNodeTransaction.Reset();
	FormatRequest->ResetTransactions();
}

void FBAGraphHandler::RequestFormatAll()
{
	FormatRequest->RequestFormatAll();
}

bool FBAGraphHandler::BeginOperation(TSharedPtr<FBAGraphOperation> Op)
{
	if (GraphOperation.IsValid())
	{
		return false;
	}

	GraphOperation = Op;

	return true;
}

bool FBAGraphHandler::IsAnyNodeBeingRenamed()
{
	if (TSharedPtr<SGraphActionMenu> GraphActionMenu = GetGraphActionMenu())
	{
		if (TSharedPtr<STreeView<TSharedPtr<FGraphActionNode>>> TreeView = GraphActionMenu.Get()->*GTreeView)
		{
			TArray<TSharedPtr<FGraphActionNode>> SelectedItems = TreeView->GetSelectedItems();
			for (TSharedPtr<FGraphActionNode> Item : SelectedItems)
			{
				if (Item->IsRenameRequestPending())
				{
					return true;
				}
			}
		}
	}

	return false;
}

void FBAGraphHandler::ApplyGlobalCommentBubblePinned()
{
	if (!UBASettings_EditorFeatures::Get().bEnableGlobalCommentBubblePinned)
	{
		return;
	}

	if (UEdGraph* EdGraph = GetFocusedEdGraph())
	{
		for (UEdGraphNode* Node : EdGraph->Nodes)
		{
			ApplyCommentBubblePinned(Node);
		}
	}
}

void FBAGraphHandler::ApplyCommentBubblePinned(UEdGraphNode* Node)
{
	if (!UBASettings_EditorFeatures::Get().bEnableGlobalCommentBubblePinned)
	{
		return;
	}

	// let the AutoSizeComment plugin handle comment nodes
	if (FBAUtils::IsCommentNode(Node))
	{
		return;
	}

	Node->bCommentBubblePinned = UBASettings_EditorFeatures::Get().bGlobalCommentBubblePinnedValue;
}

int32 FBAGraphHandler::GetNumberOfPendingNodesToCache() const
{
	return GraphTasks->CacheSizeTask.PendingSize.Num();
}

float FBAGraphHandler::GetPendingNodeSizeProgress() const
{
	return GraphTasks->CacheSizeTask.GetProgress();
}

void FBAGraphHandler::ClearFormatters()
{
	FormattingChangeDataMap.Empty();
}

bool FBAGraphHandler::HasActiveTransaction() const
{
	const bool bHasReplaceNewNodeTransaction = ReplaceNewNodeTransaction.IsValid() && ReplaceNewNodeTransaction->IsOutstanding();
	return bHasReplaceNewNodeTransaction || FormatRequest->HasActiveTransaction();
}

void FBAGraphHandler::SelectNode(UEdGraphNode* NodeToSelect, bool bLerpIntoView)
{
	TSharedPtr<SGraphPanel> GraphPanel = GetGraphPanel();
	if (!NodeToSelect)
	{
		GraphPanel->SelectionManager.ClearSelectionSet();
		return;
	}

	// select the owning node when it is not the only selected node
	if (!GraphPanel->SelectionManager.IsNodeSelected(NodeToSelect) || GraphPanel->SelectionManager.GetSelectedNodes().Num() > 1)
	{
		GraphPanel->SelectionManager.SelectSingleNode(NodeToSelect);
	}

	if (bLerpIntoView)
	{
		LerpNodeIntoView(NodeToSelect, true);
	}
}

void FBAGraphHandler::LerpNodeIntoView(UEdGraphNode* Node, bool bOnlyWhenOffscreen)
{
	TSharedPtr<SGraphPanel> GraphPanel = GetGraphPanel();

	// if the node selected is not visible, then we lerp the viewport
	const FSlateRect NodeBounds = FBAUtils::GetNodeBounds(Node);
	if (!bOnlyWhenOffscreen || !GraphPanel->IsRectVisible(NodeBounds.GetTopLeft(), NodeBounds.GetBottomRight()))
	{
		BeginLerpViewport(NodeBounds.GetCenter());
	}
}

FBAGraphData& FBAGraphHandler::GetGraphData()
{
	return FBACache::Get().GetGraphData(GetFocusedEdGraph());
}

FBANodeData& FBAGraphHandler::GetNodeData(UEdGraphNode* Node)
{
	return GetGraphData().GetNodeData(Node);
}

TSet<UEdGraphNode*> FBAGraphHandler::GetNodeGroup(const FGuid& GroupID)
{
	TSet<UEdGraphNode*> OutNodeGroup;
	for (TWeakObjectPtr<UEdGraphNode> WeakNode : NodeGroups.FindOrAdd(GroupID))
	{
		if (WeakNode.IsValid() && !FBAUtils::IsNodeDeletedFromGraph(GetFocusedEdGraph(), WeakNode.Get()))
		{
			OutNodeGroup.Add(WeakNode.Get());
		}
	}

	return OutNodeGroup;
}

void FBAGraphHandler::AddToNodeGroup(FGuid GroupID, UEdGraphNode* Node)
{
	FBANodeData& NodeData = GetNodeData(Node);
	if (NodeData.NodeGroup.IsValid())
	{
		// remove from old node group
		if (auto Group = NodeGroups.Find(NodeData.NodeGroup))
		{
			Group->Remove(Node);
		}
	}

	// add to mapping
	NodeGroups.FindOrAdd(GroupID).Add(Node);

	// set new group id
	NodeData.NodeGroup = GroupID;
}

void FBAGraphHandler::ClearNodeGroup(UEdGraphNode* Node)
{
	FBANodeData& NodeData = GetNodeData(Node);
	if (NodeData.NodeGroup.IsValid())
	{
		// remove from old node group
		if (auto Group = NodeGroups.Find(NodeData.NodeGroup))
		{
			Group->Remove(Node);
		}

		NodeData.NodeGroup.Invalidate();
	}
}

void FBAGraphHandler::CleanupNodeGroups()
{
	TSet<FGuid> KeysToRemove;
	for (const auto& Group : NodeGroups)
	{
		if (Group.Value.Num() <= 1)
		{
			KeysToRemove.Add(Group.Key);
		}
	}

	for (const auto& Key : KeysToRemove)
	{
		NodeGroups.Remove(Key);
	}
}

TSet<UEdGraphNode*> FBAGraphHandler::GetGroupedNodes(const TSet<UEdGraphNode*>& NodeSet)
{
	TSet<UEdGraphNode*> OutNodes;

	for (UEdGraphNode* Node : NodeSet)
	{
		const FBANodeData& NodeData = GetNodeData(Node);
		if (NodeData.NodeGroup.IsValid())
		{
			for (UEdGraphNode* NodeInGroup : GetNodeGroup(NodeData.NodeGroup))
			{
				OutNodes.Add(NodeInGroup);
			}
		}
	}

	return OutNodes;
}

void FBAGraphHandler::ToggleLockNodes(const TSet<UEdGraphNode*>& NodeSet)
{
	TArray<UEdGraphNode*> Nodes = NodeSet.Array();

	const bool bAnyUnlocked = Nodes.ContainsByPredicate([&](UEdGraphNode* Node)
	{
		return !GetNodeData(Node).bLocked;
	});

	for (UEdGraphNode* SelectedNode : Nodes)
	{
		FBANodeData& NodeData = GetNodeData(SelectedNode);
		NodeData.bLocked = bAnyUnlocked;
	}
}

void FBAGraphHandler::GroupNodes(const TSet<UEdGraphNode*>& NodeSet)
{
	const FGuid NewGroup = FGuid::NewGuid();
	for (UEdGraphNode* Node : NodeSet)
	{
		AddToNodeGroup(NewGroup, Node);
	}

	CleanupNodeGroups();
}

void FBAGraphHandler::UngroupNodes(const TSet<UEdGraphNode*>& NodeSet)
{
	for (UEdGraphNode* Node : NodeSet)
	{
		ClearNodeGroup(Node);
	}

	CleanupNodeGroups();
}

void FBAGraphHandler::SetViewLocation(const FVector2D& NewLocation, float NewZoom)
{
	if (auto Editor = GetGraphEditor())
	{
#if BA_UE_VERSION_OR_LATER(5, 6)
		Editor->SetViewLocation(FVector2f(NewLocation), NewZoom);
#else
		Editor->SetViewLocation(NewLocation, NewZoom);
#endif
	}
}

void FBAGraphHandler::GetViewLocation(FVector2D& OutLocation, float& OutZoom)
{
	if (auto Editor = GetGraphEditor())
	{
#if BA_UE_VERSION_OR_LATER(5, 6)
		FVector2f Loc2f;
		Editor->GetViewLocation(Loc2f, OutZoom);
		OutLocation.X = Loc2f.X;
		OutLocation.Y = Loc2f.Y;
#else
		Editor->GetViewLocation(OutLocation, OutZoom);
#endif
	}
}

void FBAGraphHandler::GetViewLocation(FVector2D& OutLocation)
{
	float Zoom;
	return GetViewLocation(OutLocation, Zoom);
}

bool FBAGraphHandler::SavePackage(bool bCompile)
{
	if (UEdGraph* Graph = GetFocusedEdGraph())
	{
		if (UPackage* Package = FBAUtils::GetPackage(Graph))
		{
			FString const PackageName = Package->GetName();
			FString const PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

			if (bCompile)
			{
				if (FBlueprintEditor* BPEditor = FBAUtils::GetBlueprintEditorForGraph(Graph))
				{
					BPEditor->Compile();
				}
			}

			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Standalone;
			Args.SaveFlags = SAVE_FromAutosave;
			UPackage::SavePackage(Package, nullptr, *PackageFileName, Args);
			return true;
		}
	}

	return false;
}

void FBAGraphHandler::RequestFormatAllAndSave()
{
	FormatRequest->RequestFormatAllAndSave();
}

void FBAGraphHandler::UpdateNodeChangeData(UEdGraphNode* Node)
{
	if (Node)
	{
		FBAFormattingChangeData& ChangeData = FormattingChangeDataMap.FindOrAdd(FBAUtils::GetNodeGuid(Node));
		ChangeData.UpdateNode(Node);
	}
}

void FBAGraphHandler::DetectGraphChanges()
{
	TArray<UEdGraphNode*> NewNodes;
	for (UEdGraphNode* NewNode : GetFocusedEdGraph()->Nodes)
	{
		// Not sure how this can occur, but fixing crash regardless
		// Do we have to check every time we access this array?
		// I assume it only hits this edge case since this runs immediately after OnGraphChanged delegate
		if (!NewNode)
		{
			continue;
		}

		if (FBAUtils::IsCommentNode(NewNode) || FBAUtils::IsKnotNode(NewNode))
		{
			continue;
		}

		if (!LastNodes.Contains(NewNode))
		{
			NewNodes.Add(NewNode);
		}
	}

	LastNodes = FBAMiscUtils::AsWeakObjectPtrArray(GetFocusedEdGraph()->Nodes);

	if (NewNodes.Num() > 0)
	{
		GraphTasks->RenameTask.AddNodes(NewNodes);

		// if we have nodes to be renamed, then wait for renaming to be finished before we process the nodes
		if (GraphTasks->RenameTask.IsRunning())
		{
			auto OnRenamedFinished = [this, NewNodes = GraphTasks->RenameTask.Nodes](bool bCancelled)
			{
				TArray<UEdGraphNode*> FreshNodes;
				CopyFromWeakArray(FreshNodes, NewNodes);
				OnNodesAdded(FreshNodes);
				GraphTasks->RenameTask.OnTaskEnded.Unbind();
			};

			GraphTasks->RenameTask.OnTaskEnded.BindLambda(OnRenamedFinished);

			GraphOverlay->SizeProgressWidget->HideOverlay();
		}
		else // otherwise just call nodes added
		{
			OnNodesAdded(NewNodes);
		}
	}
}

void FBAGraphHandler::OnNodesAdded(const TArray<UEdGraphNode*>& NewNodes)
{
	if (NewNodes.IsEmpty())
	{
		return;
	}

	TSharedRef<FBAScopedGraphAction> NodeAddedTransaction = MakeShared<FBAScopedGraphAction>(GetFocusedEdGraph(), "Format Node Added");

	for (UEdGraphNode* Node : NewNodes)
	{
		NodeSizeChangeDataMap.Add(Node->NodeGuid, FBANodeSizeChangeData(Node));
	}

	if (UBASettings::Get().bDetectNewNodesAndCacheNodeSizes)
	{
		CacheNodeSizes(NewNodes);
	}

	if (GetDefault<UBASettings_Advanced>()->bGenerateUniqueGUIDForMaterialExpressions)
	{
		const UEdGraph* Graph = GetFocusedEdGraph();

		// if the Material Graph has a node with the same GUID, we need to generate a new one
		for (const UEdGraphNode* Node : NewNodes)
		{
			if (const UMaterialGraphNode* MaterialNode = Cast<UMaterialGraphNode>(Node))
			{
				check(Node->GetGraph());

				const bool bHasDuplicateGUID = Graph->Nodes.ContainsByPredicate([&MaterialNode](UEdGraphNode* NodeB)
				{
					if (NodeB != MaterialNode)
					{
						if (UMaterialGraphNode* MaterialNodeB = Cast<UMaterialGraphNode>(NodeB))
						{
							return MaterialNode->MaterialExpression->MaterialExpressionGuid == MaterialNodeB->MaterialExpression->MaterialExpressionGuid;
						}
					}

					return false;
				});

				if (bHasDuplicateGUID)
				{
					MaterialNode->MaterialExpression->UpdateMaterialExpressionGuid(true, true);
				}
			}
		}
	}

	bool bDisableAutoFormatting = false;

	if (NewNodes.Num() == 1)
	{
		UEdGraphNode* SingleNewNode = NewNodes[0];

		ReplaceSavedSelectedNode(SingleNewNode);

		const bool bInsertedNode = TryInsertNewNode(SingleNewNode);

		// skip auto-formatting if we break the previously selected node
		// this only occurs when formatting parameter nodes in the input direction
		// (ignore if the new node was successfully inserted in the chain)
		bDisableAutoFormatting = UBASettings::Get().bSkipAutoFormattingAfterBreakingPins && !bInsertedNode && FBAUtils::IsNodePure(SingleNewNode) && PrevSelectedLink.HasBothPins() && !PrevSelectedLink.IsLinked();

		// TODO: which non-blueprint graphs even have parent nodes...?
		if (FBAUtils::IsBlueprintGraph(GetFocusedEdGraph()))
		{
			AutoAddParentNode(SingleNewNode);
		}

		AutoZoomToNode(SingleNewNode);

		if (GetSelectedNode() != SingleNewNode)
		{
			// select newly promoted variable nodes
			TSharedPtr<SGraphPanel> GraphPanel = GetGraphPanel();
			if (IsValid(SingleNewNode) && FBAUtils::IsVarNode(SingleNewNode) && GraphPanel && !GraphPanel->SelectionManager.IsNodeSelected(SingleNewNode))
			{
				GraphPanel->SelectionManager.SelectSingleNode(SingleNewNode);
			}

			// for some reason on the GameplayAbilityGraph, new nodes aren't selected correctly
			GraphPanel->SelectionManager.SelectSingleNode(SingleNewNode);
		}

		const bool bSelectedFirstValuePin = UBASettings_EditorFeatures::Get().bSelectValuePinWhenCreatingNewNodes && TrySelectFirstValuePinOnNode(SingleNewNode);
		if (!bSelectedFirstValuePin)
		{
			if (!TrySelectFirstPinOnNode(SingleNewNode))
			{
				SetSelectedPin(nullptr);
			}
		}
	}

	// Select a pin when we spawn a node and it comes in pairs, e.g
	// - SetSphereRadius (SphereComponent)
	// - Assign Delegate
	if (NewNodes.Num() == 2)
	{
		UEdGraphNode* ExecNode = nullptr;
		bool bHasVariableGet = false;

		UEdGraphNode* CustomEvent = nullptr;
		bool bHasAssignDelegate = false;

		for (UEdGraphNode* Node : NewNodes)
		{
			if (Node->IsA(UK2Node_VariableGet::StaticClass()))
			{
				bHasVariableGet = true;
			}
			else if (Node->IsA(UK2Node_AssignDelegate::StaticClass()))
			{
				bHasAssignDelegate = true;
			}
			else if (Node->IsA(UK2Node_CustomEvent::StaticClass()))
			{
				CustomEvent = Node;
			}

			if (FBAUtils::IsNodeImpure(Node))
			{
				ExecNode = Node;
			}
		}

		if (bHasVariableGet && ExecNode)
		{
			TrySelectFirstPinOnNode(ExecNode);
		}

		if (bHasAssignDelegate && CustomEvent)
		{
			TrySelectFirstPinOnNode(CustomEvent);
		}
	}

	if (!bDisableAutoFormatting)
	{
		FormatNewNodes(NewNodes, NodeAddedTransaction);
	}

	if (GraphOverlay)
	{
		if (!GraphTasks->CacheSizeTask.IsRunning())
		{
			GraphOverlay->SizeProgressWidget->HideOverlay();
		}
	}
}

UEdGraphNode* FBAGraphHandler::DecideNodeToKeepStill(const TArray<UEdGraphNode*>& NewNodes)
{
	if (NewNodes.IsEmpty())
	{
		return nullptr;
	}

	TSet<UEdGraphNode*> ExistingLeafNodes;
	FBAUtils::IterateNodeTreeDepthFirst(NewNodes[0], [&](const FPinLink& Link)
	{
		UEdGraphNode* NextNode = Link.GetToNodeUnsafe();
		if (!NewNodes.Contains(NextNode))
		{
			ExistingLeafNodes.Add(NextNode);
			return false;
		}

		return true;
	});

	// if there is only 1 leaf node then use that
	if (ExistingLeafNodes.Num() == 1)
	{
		return ExistingLeafNodes[FSetElementId::FromInteger(0)];
	}

	// select from the pin we dragged off
	if (UEdGraphPin* PreviousPin = PrevSelectedPinHandle.GetPin())
	{
		UEdGraphNode* PrevOwningNode = PreviousPin->GetOwningNode();
		if (ExistingLeafNodes.Contains(PrevOwningNode))
		{
			return PreviousPin->GetOwningNode();
		}
	}

	// select any leaf node
	if (ExistingLeafNodes.Num())
	{
		return ExistingLeafNodes[FSetElementId::FromInteger(0)];
	}

	// if there are multiple new nodes then prefer the first impure node
	if (NewNodes.Num() > 1)
	{
		TArray<UEdGraphNode*> ImpureNodes = NewNodes.FilterByPredicate(FBAUtils::IsNodeImpure);
		if (ImpureNodes.Num())
		{
			return ImpureNodes[0];
		}
	}

	// otherwise return the 1st new node
	return NewNodes[0];
}

void FBAGraphHandler::CacheNodeSizes(const TArray<UEdGraphNode*>& Nodes)
{
	for (UEdGraphNode* Node : Nodes)
	{
		if (FBAUtils::IsKnotNode(Node) || (!FBAUtils::IsGraphNode(Node) && !FBAUtils::IsCommentNode(Node)))
		{
			continue;
		}

		// if the node size hasn't been cached, add the node to be calculated
		if (!GetNodeData(Node).HasSize())
		{
			AddPendingSize(Node);
		}
	}
}

void FBAGraphHandler::FormatNewNodes(const TArray<UEdGraphNode*>& NewNodes, TSharedPtr<FBAScopedGraphAction> Transaction)
{
	const auto AutoFormatting = UBASettings::GetFormatterSettings(GetFocusedEdGraph()).GetAutoFormatting();
	if (AutoFormatting == EBAAutoFormatting::Never)
	{
		return;
	}

	if (NewNodes.IsEmpty())
	{
		return;
	}

	TSet<UEdGraphNode*> ExistingLeafNodes;
	FBAUtils::IterateNodeTreeDepthFirst(NewNodes[0], [&](const FPinLink& Link)
	{
		UEdGraphNode* NextNode = Link.GetToNodeUnsafe();
		if (!NewNodes.Contains(NextNode))
		{
			ExistingLeafNodes.Add(NextNode);
			return false;
		}

		return true;
	});

	// don't format if our new nodes aren't connected to any existing nodes
	// in particular, when we copy and paste
	if (ExistingLeafNodes.Num() == 0)
	{
		return;
	}

	// Check if we want to format all
	bool bHandledAlwaysFormatAll = false;
	if (UBASettings::Get().bAlwaysFormatAll)
	{
		TArray<UEdGraphNode*> PendingNodes = NewNodes;
		int32 ErrorCount = 0;
		while (PendingNodes.Num() > 0)
		{
			ErrorCount += 1;
			if (ErrorCount > 1000)
			{
				UE_LOG(LogBlueprintAssist, Error, TEXT("BlueprintAssist: Error infinite loop detected in FBAGraphHandler::FormatNewNodes"));
				break;
			}

			UEdGraphNode* CurrentNode = PendingNodes.Pop();
			TArray<UEdGraphNode*> NodeTree = FBAUtils::GetNodeTree(CurrentNode).Array();

			auto FilterEvents = [](UEdGraphNode* Node)
			{
				return FBAUtils::IsEventNode(Node, EGPD_Output);
			};

			if (NodeTree.FilterByPredicate(FilterEvents).Num() > 0)
			{
				FormatRequest->RequestFormatAll();
				bHandledAlwaysFormatAll = true;
				break;
			}

			PendingNodes.RemoveAllSwap([&NodeTree](UEdGraphNode* Node) { return NodeTree.Contains(Node); });
		}
	}

	if (bHandledAlwaysFormatAll)
	{
		return;
	}

	FEdGraphFormatterParameters Parameters;
	Parameters.NodeToKeepStill = DecideNodeToKeepStill(NewNodes);

	TryAutoFormatNode(NewNodes, Transaction, Parameters);
}

void FBAGraphHandler::AutoAddParentNode(UEdGraphNode* NewNode)
{
	if (!UBASettings_EditorFeatures::Get().bAutoAddParentNode)
	{
		return;
	}

	if (!FBAUtils::IsEventNode(NewNode))
	{
		return;
	}

	const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(GetFocusedEdGraph()->GetSchema());
	if (!Schema)
	{
		return;
	}

	// See FBlueprintEditor::OnAddParentNode
	FFunctionFromNodeHelper FunctionFromNode(NewNode);
	if (FunctionFromNode.Function && FunctionFromNode.Node)
	{
		UFunction* ValidParent = Schema->GetCallableParentFunction(FunctionFromNode.Function);
		UEdGraph* TargetGraph = FunctionFromNode.Node->GetGraph();
		if (ValidParent && TargetGraph)
		{
			// blueprint implementable events don't have parent c++ functions
			if (FBAMiscUtils::IsBlueprintImplementableEvent(ValidParent) && !ValidParent->IsInBlueprint())
			{
				return;
			}

			TSharedPtr<FScopedTransaction> Transaction = MakeShareable(new FScopedTransaction(FText::FromString("Auto-Add Parent Function Call")));
			TargetGraph->Modify();

			FGraphNodeCreator<UK2Node_CallParentFunction> FunctionNodeCreator(*TargetGraph);
			UK2Node_CallParentFunction* ParentFunctionNode = FunctionNodeCreator.CreateNode();
			ParentFunctionNode->SetFromFunction(ValidParent);
			ParentFunctionNode->AllocateDefaultPins();

			int32 NodeSizeY = 15;
			if (UK2Node* Node = Cast<UK2Node>(NewNode))
			{
				NodeSizeY += Node->DEPRECATED_NodeWidget.IsValid() ? static_cast<int32>(Node->DEPRECATED_NodeWidget.Pin()->GetDesiredSize().Y) : 0;
			}
			ParentFunctionNode->NodePosX = FunctionFromNode.Node->NodePosX;
			ParentFunctionNode->NodePosY = FunctionFromNode.Node->NodePosY + NodeSizeY;

			FunctionNodeCreator.Finalize();

			// The original event node may be linked, check linked to pins
			auto NodeLinkedToPins = FBAUtils::GetLinkedToPins(NewNode, EGPD_Output);
			for (auto OutputPin : FBAUtils::GetPinsByDirection(ParentFunctionNode, EGPD_Output))
			{
				for (auto Pin : NodeLinkedToPins)
				{
					// Can use unsafe cos we are breaking here!
					if (FBAUtils::TryCreateConnectionUnsafe(OutputPin, Pin, EBABreakMethod::Never))
					{
						break;
					}
				}
			}

			// Link the original node to the parent
			for (auto OutputPin : FBAUtils::GetPinsByDirection(NewNode, EGPD_Output))
			{
				const bool bIsExecPin = FBAUtils::IsExecPin(OutputPin);
				for (auto InputPin : FBAUtils::GetPinsByDirection(ParentFunctionNode, EGPD_Input))
				{
					if (!bIsExecPin)
					{
						// for parameter pins, make sure the pin name is the same!
						if (OutputPin->GetFName() != InputPin->GetFName())
						{
							continue;
						}
					}

					// Can use unsafe cos we are breaking here!
					if (FBAUtils::TryCreateConnectionUnsafe(OutputPin, InputPin, EBABreakMethod::Never))
					{
						break;
					}
				}
			}

			// We don't want to process the parent node as a new node, add it to last nodes so it will be ignored in the next check
			LastNodes.Add(ParentFunctionNode);

			// Always format this new custom event node (even if auto formatting is disabled)
			FormatRequest->RequestFormatNode(NewNode);
		}
	}
}

void FBAGraphHandler::OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event)
{
	static const FName NodesChangedName(TEXT("Nodes"));

	if (Event.GetEventType() == ETransactionObjectEventType::UndoRedo)
	{
		if ((Event.GetChangedProperties().Num() == 1) && Event.GetChangedProperties()[0].IsEqual(NodesChangedName))
		{
			if (UEdGraph* Graph = Cast<UEdGraph>(Object))
			{
				if (Graph == GetFocusedEdGraph())
				{
					LastNodes = FBAMiscUtils::AsWeakObjectPtrArray(GetFocusedEdGraph()->Nodes);
				}
			}
		}
	}
}

void FBAGraphHandler::OnPostUndoRedo()
{
	// Control Rig nodes don't use OnObjectTransacted, instead force update last nodes here
	if (auto Graph = GetFocusedEdGraph())
	{
		if (FBAControlRigUtils::IsControlRigGraph(Graph))
		{
			LastNodes = FBAMiscUtils::AsWeakObjectPtrArray(Graph->Nodes);
		}
	}
}

bool FBAGraphHandler::UpdateNodeSizesChanges(const TArray<UEdGraphNode*>& Nodes)
{
	bool bAddedSize = false;

	TSet<UEdGraphNode*> NodesToCheck;
	for (UEdGraphNode* Node : Nodes)
	{
		if (!FBAUtils::IsGraphNode(Node) || FBAUtils::IsKnotNode(Node))
		{
			continue;
		}

		NodesToCheck.Add(Node);
	}

	// also get comment nodes
	TArray<UEdGraphNode_Comment*> Comments;
	GetFocusedEdGraph()->GetNodesOfClass(Comments);
	for (UEdGraphNode* Node : Nodes)
	{
		for (UEdGraphNode_Comment* Comment : Comments)
		{
			if (Comment->GetNodesUnderComment().Contains(Node))
			{
				NodesToCheck.Add(Comment);
			}
		}
	}

	for (auto Node : NodesToCheck)
	{
		// refresh node sizes for nodes which have changed in size
		if (FBANodeSizeChangeData* ChangeData = NodeSizeChangeDataMap.Find(Node->NodeGuid))
		{
			if (ChangeData->HasNodeChanged(Node))
			{
				AddPendingSize(Node);
				bAddedSize = true;
			}

			ChangeData->UpdateNode(Node);
		}
		else
		{
			NodeSizeChangeDataMap.Add(Node->NodeGuid, FBANodeSizeChangeData(Node));
		}

		// calculate size for all connected nodes which don't have a valid size
		const bool bHasValidSize = GetNodeData(Node).HasSize();
		if (!bHasValidSize && AddPendingSize(Node))
		{
			bAddedSize = true;
		}
	}

	return bAddedSize;
}

void FBAGraphHandler::AutoLerpToNewlyCreatedNode(UEdGraphNode* Node)
{
	if (UBASettings_EditorFeatures::Get().AutoZoomToNodeBehavior == EBAAutoZoomToNode::Outside_Viewport)
	{
		if (FBAUtils::IsNodeVisible(GetGraphPanel(), Node))
		{
			return;
		}
	}

	FVector2D NodePos(Node->NodePosX, Node->NodePosY);
	BeginLerpViewport(NodePos);
}

void FBAGraphHandler::AutoZoomToNode(UEdGraphNode* Node)
{
	const EBAAutoZoomToNode AutoZoomToNode = UBASettings_EditorFeatures::Get().AutoZoomToNodeBehavior;
	if (AutoZoomToNode == EBAAutoZoomToNode::Never)
	{
		return;
	}

	if (DoesNodeWantAutoFormatting(Node))
	{
		// delay the zooming until post formatting
		FormatRequest->ZoomToTargetPostFormatting = Node;
	}
	else
	{
		if (AutoZoomToNode == EBAAutoZoomToNode::Outside_Viewport)
		{
			if (FBAUtils::IsNodeVisible(GetGraphPanel(), Node))
			{
				return;
			}
		}

		const FVector2D NodePos(Node->NodePosX, Node->NodePosY);
		BeginLerpViewport(NodePos);
	}
}

bool FBAGraphHandler::DoesNodeWantAutoFormatting(UEdGraphNode* Node)
{
	const auto AutoFormatting = UBASettings::GetFormatterSettings(GetFocusedEdGraph()).GetAutoFormatting();
	if (AutoFormatting == EBAAutoFormatting::Never)
	{
		return false;
	}

	if (FBAUtils::GetLinkedNodes(Node).Num() == 0)
	{
		return false;
	}

	return true;
}

void FBAGraphHandler::RunSavePostFormatting()
{
#if BA_UE_VERSION_OR_LATER(5, 0)
	// only support blueprint graph types for now
	if (UBlueprint* BP = GetBlueprint())
	{
		if (UPackage* const Package = BP->GetPackage())
		{
			FString const PackageName = Package->GetName();
			FString const PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Standalone;
			Args.SaveFlags = SAVE_FromAutosave;
			if (FBlueprintEditor* BPEditor = FBAUtils::GetBlueprintEditorForGraph(GetFocusedEdGraph()))
			{
				BPEditor->Compile();
			}

			UPackage::SavePackage(Package, nullptr, *PackageFileName, Args);
		}
	}
#endif
}

TSharedPtr<SGraphEditor> FBAGraphHandler::AssignNewGraphEditorFromTab()
{
	if (CachedTab.IsValid())
	{
		const TSharedRef<SWidget> TabContent = CachedTab.Pin()->GetContent();

		// grab the graph editor from the tab
		TSharedPtr<SGraphEditor> TabContentAsGraphEditor = FBAUtils::GetChildWidgetByTypesCasted<SGraphEditor>(TabContent, UBASettings::Get().SupportedGraphEditors);
		if (TabContentAsGraphEditor.IsValid())
		{
			if (CachedGraphEditor != TabContentAsGraphEditor)
			{
				SetGraphEditor(TWeakPtr<SGraphEditor>(TabContentAsGraphEditor));
				return CachedGraphEditor.Pin();
			}
		}
	}

	return nullptr;
}

bool FBAGraphHandler::IsBlueprintRootNode(UEdGraphNode* Node, bool bOnlyOutputRoots)
{
	// blueprint nodes
	{
		if (Node->IsA(UK2Node_Event::StaticClass()))
		{
			return true;
		}

		if (FBAUtils::IsInputNode(Node))
		{
			return true;
		}
	}

	// function entry / result
	{
		if (Node->IsA(UK2Node_FunctionEntry::StaticClass()))
		{
			return true;
		}

		if (!bOnlyOutputRoots && Node->IsA(UK2Node_FunctionResult::StaticClass()))
		{
			return true;
		}
	}

	// macros
	if (Node->GetClass() == UK2Node_Tunnel::StaticClass())
	{
		// tunnel node could be instance of a macro, to find the "root" node of a macro check if it can't be removed from the graph
		if (!Node->CanUserDeleteNode())
		{
			if (!bOnlyOutputRoots)
			{
				return true;
			}

			return FBAUtils::GetLinkedPins(Node, EGPD_Output).Num() > 0;
		}
	}

	return false;
}

void FBAGraphHandler::OnDelayedCacheSizeFinished()
{
	UE_LOG(LogBlueprintAssist, VeryVerbose, TEXT("[%hs]"), __FUNCTION__)
	if (GraphOverlay)
	{
		GraphOverlay->SizeProgressWidget->HideOverlay();
	}
}
