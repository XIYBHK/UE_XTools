// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistInputProcessor.h"

#include "AssetViewUtils.h"
#include "BlueprintAssistCache.h"
#include "BlueprintAssistGlobals.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistModule.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistSettings_EditorFeatures.h"
#include "BlueprintAssistTabHandler.h"
#include "BlueprintAssistToolbar.h"
#include "ContentBrowserModule.h"
#include "EdGraphNode_Comment.h"
#include "IContentBrowserSingleton.h"
#include "K2Node_DynamicCast.h"
#include "SGraphPanel.h"
#include "BlueprintAssistObjects/BARootObject.h"
#include "Editor/ContentBrowser/Private/SContentBrowser.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Notifications/SNotificationList.h"

static TSharedPtr<FBAInputProcessor> BAInputProcessorInstance;

void FBAInputProcessor::Create()
{
	BAInputProcessorInstance = MakeShareable(new FBAInputProcessor());
	FSlateApplication::Get().RegisterInputPreProcessor(BAInputProcessorInstance);
}

FBAInputProcessor& FBAInputProcessor::Get()
{
	return *BAInputProcessorInstance;
}

FBAInputProcessor::FBAInputProcessor()
{
	GlobalActions.Init();
	TabActions.Init();
	ToolkitActions.Init();
	GraphActions.Init();
	NodeActions.Init();
	PinActions.Init();
	BlueprintActions.Init();

#if WITH_SLATE_DEBUGGING
	FSlateDebugging::InputEvent.AddRaw(this, &FBAInputProcessor::HandleSlateInputEvent);
#endif

	FSlateApplication::Get().OnApplicationActivationStateChanged().AddRaw(this, &FBAInputProcessor::OnWindowFocusChanged);

	CommandLists = {
		GlobalActions.GlobalCommands,
		TabActions.TabCommands,
		TabActions.ActionMenuCommands,
		ToolkitActions.ToolkitCommands,
		GraphActions.GraphCommands,
		GraphActions.GraphReadOnlyCommands,
		NodeActions.SingleNodeCommands,
		NodeActions.MultipleNodeCommands,
		NodeActions.MultipleNodeCommandsIncludingComments,
		NodeActions.MiscNodeCommands,
		PinActions.PinCommands,
		PinActions.PinEditCommands,
		BlueprintActions.BlueprintCommands
	};
}

FBAInputProcessor::~FBAInputProcessor() {}

void FBAInputProcessor::Cleanup()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(BAInputProcessorInstance);
	}

	BAInputProcessorInstance.Reset();
}

void FBAInputProcessor::Tick(
	const float DeltaTime,
	FSlateApplication& SlateApp,
	TSharedRef<ICursor> Cursor)
{
	bIsDisabled = FBAUtils::IsGamePlayingAndHasFocus();

	if (IsDisabled())
	{
		return;
	}

	TSharedPtr<FBAGraphHandler> GraphHandler = FBATabHandler::Get().GetActiveGraphHandler();

	FBATabHandler::Get().Tick(DeltaTime);

	if (UBARootObject* RootObject = FBlueprintAssistModule::Get().GetRootObject())
	{
		RootObject->Tick();
	}

	UpdateGroupMovement();
}

bool FBAInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// ignore repeat keys
	if (InKeyEvent.IsRepeat())
	{
		return false;
	}

	if (OnKeyOrMouseDown(SlateApp, InKeyEvent.GetKey()))
	{
		return true;
	}

	// TODO: Perhaps implement a NavigationConfig, so users can't change focus on widgets
	// See FSlateApplication::SetNavigationConfig

	if (IsDisabled())
	{
		return false;
	}

	if (ProcessFolderBookmarkInput())
	{
		return true;
	}

	if (ProcessContentBrowserInput())
	{
		return true;
	}

	if (SlateApp.IsInitialized())
	{
		TSharedPtr<FBAGraphHandler> GraphHandler = FBATabHandler::Get().GetActiveGraphHandler();

		// process toolbar commands
		if (ProcessCommandBindings(FBAToolbar::Get().BlueprintAssistToolbarActions, InKeyEvent))
		{
			return true;
		}

		if (ProcessCommandBindings(GlobalActions.GlobalCommands, InKeyEvent))
		{
			return true;
		}

		if (BlueprintActions.HasOpenBlueprintEditor())
		{
			if (ProcessCommandBindings(BlueprintActions.BlueprintCommands, InKeyEvent))
			{
				return true;
			}
		}

		// try process toolkit hotkeys
		if (ProcessCommandBindings(ToolkitActions.ToolkitCommands, InKeyEvent))
		{
			return true;
		}

		if (!GraphHandler.IsValid())
		{
			//UE_LOG(LogBlueprintAssist, Warning, TEXT("Invalid graph handler"));
			return false;
		}

		// clear keyboard focus for single line editable text on Enter when editing a pin
		if (InKeyEvent.GetKey() == EKeys::Enter)
		{
			if (TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget())
			{
				if (FBAUtils::IsUserInputWidget(FocusedWidget))
				{
					if (FBAUtils::GetParentWidgetOfType(FocusedWidget, "SGraphPin", true))
					{
						GEditor->GetTimerManager()->SetTimerForNextTick([]()
						{
							FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
						});
					}
				}
			}
		}

		// cancel graph handler ongoing processes
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			GraphHandler->CancelActiveFormatting();
		}

		TSharedPtr<SDockTab> Tab = GraphHandler->GetTab();
		if (!Tab.IsValid() || !Tab->IsForeground())
		{
			//UE_LOG(LogBlueprintAssist, Warning, TEXT("Tab invalid or not foreground"));
			return false;
		}

		TSharedPtr<SWidget> KeyboardFocusedWidget = SlateApp.GetKeyboardFocusedWidget();
		// if (KeyboardFocusedWidget.IsValid())
		// {
		// 	UE_LOG(LogBlueprintAssist, Warning, TEXT("%s | %s"), *KeyboardFocusedWidget->GetTypeAsString(), *KeyboardFocusedWidget->ToString());
		// }
		// else
		// {
		// 	UE_LOG(LogBlueprintAssist, Warning, TEXT("No keyboard focused widget!"));
		// }

		// try process graph action menu hotkeys
		TSharedPtr<SWindow> Menu = SlateApp.GetActiveTopLevelWindow();
		if (Menu.IsValid())
		{
			if (GraphActions.HasOpenActionMenu())
			{
				if (ProcessCommandBindings(TabActions.ActionMenuCommands, InKeyEvent))
				{
					return true;
				}
			}
		}

		// get the keyboard focused widget
		if (!Menu.IsValid() || !KeyboardFocusedWidget.IsValid())
		{
			//UE_LOG(LogBlueprintAssist, Warning, TEXT("Focus graph panel"));

			TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();
			SlateApp.SetKeyboardFocus(GraphPanel);
			KeyboardFocusedWidget = GraphPanel;
		}

		// process commands for when you are editing a user input widget
		if (FBAUtils::IsUserInputWidget(KeyboardFocusedWidget))
		{
			if (FBAUtils::GetParentWidgetOfType(KeyboardFocusedWidget, "SGraphPin").IsValid())
			{
				if (ProcessCommandBindings(PinActions.PinEditCommands, InKeyEvent))
				{
					return true;
				}
			}

			if (InKeyEvent.GetKey() == EKeys::Escape)
			{
				SlateApp.SetKeyboardFocus(GraphHandler->GetGraphPanel());
			}

			return false;
		}

		// process commands for when the tab is open
		if (ProcessCommandBindings(TabActions.TabCommands, InKeyEvent))
		{
			return true;
		}

		//UE_LOG(LogBlueprintAssist, Warning, TEXT("Process tab commands"));

		if (!GraphHandler->IsWindowActive())
		{
			//TSharedPtr<SWindow> CurrentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
			//const FString CurrentWindowStr = CurrentWindow.IsValid()
			//	? CurrentWindow->GetTitle().ToString()
			//	: "nullptr";

			//TSharedPtr<SWindow> GHWindow = GraphHandler->GetOrFindWindow();
			//FString GHWindowStr = GHWindow.IsValid() ? GHWindow->GetTitle().ToString() : "Nullptr";
			//UE_LOG(
			//	LogBlueprintAssist,
			//	Warning,
			//	TEXT("Graph Handler window is not active %s current window | GH Window %s"),
			//	*CurrentWindowStr,
			//	*GHWindowStr);
			return false;
		}

		if (!GraphHandler->IsGraphPanelFocused())
		{
			return false;
		}

		// process commands for when the graph exists but is read only
		if (ProcessCommandBindings(GraphActions.GraphReadOnlyCommands, InKeyEvent))
		{
			return true;
		}

		// skip all other graph commands if read only
		if (GraphHandler->IsGraphReadOnly())
		{
			return false;
		}

		// process general graph commands
		if (ProcessCommandBindings(GraphActions.GraphCommands, InKeyEvent))
		{
			return true;
		}

		// process commands for which require a node to be selected
		if (GraphHandler->GetSelectedPin() != nullptr || FBAUtils::GetHoveredGraphPin(GraphHandler->GetGraphPanel()).IsValid())
		{
			if (ProcessCommandBindings(PinActions.PinCommands, InKeyEvent))
			{
				return true;
			}
		}

		if (ProcessCommandBindings(NodeActions.MiscNodeCommands, InKeyEvent))
		{
			return true;
		}

		// process commands for which require a single node to be selected
		if (GraphHandler->GetSelectedNode() != nullptr)
		{
			//UE_LOG(LogBlueprintAssist, Warning, TEXT("Process node commands"));
			if (ProcessCommandBindings(NodeActions.SingleNodeCommands, InKeyEvent))
			{
				return true;
			}

			if (InKeyEvent.GetKey() == EKeys::Escape)
			{
				GraphHandler->GetGraphPanel()->SelectionManager.ClearSelectionSet();
			}
		}

		// process commands for which require multiple nodes to be selected
		if (GraphHandler->GetSelectedNodes().Num() > 0)
		{
			if (ProcessCommandBindings(NodeActions.MultipleNodeCommands, InKeyEvent))
			{
				return true;
			}

			if (InKeyEvent.GetKey() == EKeys::Escape)
			{
				GraphHandler->GetGraphPanel()->SelectionManager.ClearSelectionSet();
			}
		}

		// process commands for which require multiple nodes (incl comments) to be selected
		if (GraphHandler->GetSelectedNodes(true).Num() > 0)
		{
			if (ProcessCommandBindings(NodeActions.MultipleNodeCommandsIncludingComments, InKeyEvent))
			{
				return true;
			}

			if (InKeyEvent.GetKey() == EKeys::Escape)
			{
				GraphHandler->GetGraphPanel()->SelectionManager.ClearSelectionSet();
			}
		}
	}
	else
	{
		UE_LOG(LogBlueprintAssist, Error, TEXT("HandleKeyDown: Slate App not initialized"));
	}
	return false;
}

bool FBAInputProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (OnKeyOrMouseUp(SlateApp, InKeyEvent.GetKey()))
	{
		return true;
	}

	return false;
}

bool FBAInputProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (OnKeyOrMouseDown(SlateApp, MouseEvent.GetEffectingButton()))
	{
		return true;
	}

	if (IsDisabled())
	{
		return false;
	}

	TSharedPtr<FBAGraphHandler> GraphHandler = FBATabHandler::Get().GetActiveGraphHandler();
	if (!GraphHandler)
	{
		return false;
	}

	if (TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel())
	{
		TSharedPtr<SGraphPin> HoveredPin = FBAUtils::GetHoveredGraphPin(GraphPanel);

		// select the hovered pin on LMB or RMB
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		{
			if (HoveredPin.IsValid())
			{
				UEdGraphPin* Pin = HoveredPin->GetPinObj();

				GraphHandler->SetSelectedPin(Pin);
			}
		}

		// Fix ongoing transactions being canceled via spawn node event on the graph. See FBlueprintEditor::OnSpawnGraphNodeByShortcut.
		if (GraphHandler->HasActiveTransaction())
		{
			if (GraphPanel->IsHovered())
			{
				return true;
			}
		}
	}

	return false;
}

bool FBAInputProcessor::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (OnKeyOrMouseUp(SlateApp, MouseEvent.GetEffectingButton()))
	{
		return true;
	}

	return false;
}

bool FBAInputProcessor::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (IsDisabled())
	{
		return false;
	}

	bool bBlocking = false;
	TSharedPtr<FBAGraphHandler> GraphHandler = FBATabHandler::Get().GetActiveGraphHandler();
	if (!GraphHandler.IsValid())
	{
		return false;
	}

	if (TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel())
	{
		const FVector2D NewMousePos = FBAUtils::SnapToGrid(FBAUtils::ScreenSpaceToPanelCoord(GraphPanel, MouseEvent.GetScreenSpacePosition()));
		const FVector2D Delta = NewMousePos - LastMousePos;

		bBlocking = OnMouseDrag(SlateApp, NewMousePos, Delta);

		LastMousePos = NewMousePos;
	}

	return bBlocking;
}

void FBAInputProcessor::HandleSlateInputEvent(const FSlateDebuggingInputEventArgs& EventArgs)
{
	if (EventArgs.InputEventType == ESlateDebuggingInputEvent::MouseButtonDoubleClick)
	{
		if (UBASettings::Get().bEnableDoubleClickGoToDefinition)
		{
			if (TSharedPtr<FBAGraphHandler> GraphHandler = FBATabHandler::Get().GetActiveGraphHandler())
			{
				// get the hovered graph node
				TSharedPtr<SGraphNode> GraphNode = FBAUtils::GetHoveredGraphNode(GraphHandler->GetGraphPanel());
				if (!GraphNode)
				{
					return;
				}

				// if we are a dynamic cast, jump to the definition
				if (UK2Node_DynamicCast* DynamicCast = Cast<UK2Node_DynamicCast>(GraphNode->GetNodeObj()))
				{
					TArray<UEdGraphPin*> OutputParameters = FBAUtils::GetParameterPins(DynamicCast, EGPD_Output);
					if (OutputParameters.Num())
					{
						TWeakObjectPtr<UObject> SubcategoryObject = OutputParameters[0]->PinType.PinSubCategoryObject;
						if (SubcategoryObject.IsValid())
						{
							// open using package if it is an asset
							if (SubcategoryObject->IsAsset()) 
							{
								if (UPackage* Outer = Cast<UPackage>(SubcategoryObject->GetOuter()))
								{
									GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Outer->GetName());
								}
							}
							else
							{
								GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(SubcategoryObject.Get());
							}
						}
					}
				}
			}
		}
	}
}

bool FBAInputProcessor::BeginGroupMovement(const FKey& Key)
{
	TSharedPtr<FBAGraphHandler> GraphHandler = FBATabHandler::Get().GetActiveGraphHandler();
	if (!GraphHandler.IsValid())
	{
		return false;
	}

	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();
	if (!GraphPanel.IsValid())
	{
		return false;
	}

	// UE_LOG(LogTemp, Warning, TEXT("Hovered node %s"), *HoveredNode->ToString());

	static const TSet<FName> BlockingWidgets = { "SButton", "SCheckBox" };

	bool bBlocking = false;

	// Select the node when pressing additional node drag chord
	if (Key == EKeys::LeftMouseButton)
	{
		TSet<UEdGraphNode*> SelectedNodes = GraphHandler->GetSelectedNodes(true);

		TSharedPtr<SGraphNode> HoveredNode = FBAUtils::GetHoveredGraphNode(GraphPanel);
		if (!HoveredNode.IsValid())
		{
			return false;
		}

		TSharedPtr<SGraphPin> HoveredPin = FBAUtils::GetHoveredGraphPin(GraphPanel);
		if (!HoveredPin && !FBAUtils::ContainsWidgetInFront(HoveredNode, BlockingWidgets))
		{
			UEdGraphNode* HoveredNodeObj = HoveredNode->GetNodeObj();

			// our lmb hook goes before the editor's selection, so the hovered node will be selected this tick
			if (SelectedNodes.Num() == 0)
			{
				SelectedNodes.Add(HoveredNodeObj);
			}

			TSet<UEdGraphNode*> NodesToMove;
			NodesToMove.Append(SelectedNodes);
			NodesToMove.Append(GraphHandler->GetGroupedNodes(SelectedNodes));

			// set the anchor node for group movement
			AnchorNode = HoveredNodeObj;
			LastAnchorPos = HoveredNode->GetPosition();
			DragNodeTransaction.Begin(NodesToMove, INVTEXT("Move Node(s)"), EBADragMethod::LMB);
		}
	}
	// Select the node when pressing additional node drag chord
	else if (IsAnyInputChordDown(UBASettings_EditorFeatures::Get().AdditionalDragNodesChords))
	{
		TSet<UEdGraphNode*> SelectedNodes = GraphHandler->GetSelectedNodes(true);

		TSharedPtr<SGraphNode> HoveredNode = FBAUtils::GetHoveredGraphNode(GraphPanel);
		if (!HoveredNode.IsValid())
		{
			return false;
		}

		if (!FBAUtils::ContainsWidgetInFront(HoveredNode, BlockingWidgets))
		{
			UEdGraphNode* HoveredNodeObj = HoveredNode->GetNodeObj();

			// also set the anchor node for group movement
			AnchorNode = HoveredNodeObj;
			LastAnchorPos = HoveredNode->GetPosition();

			if (!SelectedNodes.Contains(HoveredNodeObj))
			{
				GraphHandler->SelectNode(HoveredNodeObj);
				bBlocking = true;
			}

			TSet<UEdGraphNode*> NodeSet;
			NodeSet.Append(GraphHandler->GetSelectedNodes(true));
			NodeSet.Append(GraphHandler->GetGroupedNodes(GraphHandler->GetSelectedNodes()));

			// begin transaction
			DragNodeTransaction.Begin(NodeSet, INVTEXT("Move Node(s)"), EBADragMethod::AdditionalDragChord);
		}
	}

	return bBlocking;
}

bool FBAInputProcessor::OnMouseDrag(FSlateApplication& SlateApp, const FVector2D& MousePos, const FVector2D& Delta)
{
	TSharedPtr<FBAGraphHandler> MyGraphHandler = FBATabHandler::Get().GetActiveGraphHandler();

	bool bBlocking = false;

	// process extra drag nodes
	if (AnchorNode.IsValid())
	{
		for (const FInputChord& Chord : UBASettings_EditorFeatures::Get().AdditionalDragNodesChords)
		{
			if (IsInputChordDown(Chord))
			{
				TSet<UEdGraphNode*> NodesToMove = MyGraphHandler->GetSelectedNodes();
				for (UEdGraphNode* Node : NodesToMove)
				{
					Node->NodePosX += Delta.X;
					Node->NodePosY += Delta.Y;
				}

				bBlocking = NodesToMove.Num() > 0;
				break;
			}
		}
	}

	UpdateGroupMovement();

	return bBlocking;
}

bool FBAInputProcessor::OnKeyOrMouseDown(FSlateApplication& SlateApp, const FKey& Key)
{
	KeysDown.Add(Key);
	KeysDownStartTime.Add(Key, FSlateApplication::Get().GetCurrentTime());

	if (IsDisabled())
	{
		return false;
	}

	if (ProcessorState.OnKeyOrMouseDown(Key))
	{
		return true;
	}

	bool bBlocking = false;
	bBlocking = BeginGroupMovement(Key);
	return bBlocking;
}

bool FBAInputProcessor::OnKeyOrMouseUp(FSlateApplication& SlateApp, const FKey& Key)
{
	bool bBlocking = false;

	if (ProcessorState.OnKeyOrMouseUp(Key))
	{
		return true;
	}

	// process extra drag nodes
	if (Key == EKeys::LeftMouseButton)
	{
		if (DragNodeTransaction.DragMethod == EBADragMethod::LMB)
		{
			GEditor->GetTimerManager()->SetTimerForNextTick([&]()
			{
				DragNodeTransaction.End(EBADragMethod::LMB);
				AnchorNode = nullptr;
			});
		}
	}

	if (IsAnyInputChordDown(UBASettings_EditorFeatures::Get().AdditionalDragNodesChords, Key))
	{
		if (DragNodeTransaction.DragMethod == EBADragMethod::AdditionalDragChord)
		{
			bBlocking = true;
			AnchorNode = nullptr;
	
			GEditor->GetTimerManager()->SetTimerForNextTick([&]()
			{
				DragNodeTransaction.End(EBADragMethod::AdditionalDragChord);
			});
		}
	}

	KeysDown.Remove(Key);
	KeysDownStartTime.Remove(Key);

	return bBlocking;
}

bool FBAInputProcessor::CanExecuteCommand(TSharedRef<const FUICommandInfo> Command) const
{
	for (TSharedPtr<FUICommandList> CommandList : CommandLists)
	{
		if (const FUIAction* Action = CommandList->GetActionForCommand(Command))
		{
			return Action->CanExecute();
		}
	}

	return false;
}

bool FBAInputProcessor::TryExecuteCommand(TSharedRef<const FUICommandInfo> Command)
{
	for (TSharedPtr<FUICommandList> CommandList : CommandLists)
	{
		if (const FUIAction* Action = CommandList->GetActionForCommand(Command))
		{
			if (Action->CanExecute())
			{
				return Action->Execute();
			}
		}
	}

	return false;
}

bool FBAInputProcessor::IsDisabled() const
{
	return bIsDisabled;
}

void FBAInputProcessor::UpdateGroupMovement()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = FBATabHandler::Get().GetActiveGraphHandler();
	if (!GraphHandler || !AnchorNode.IsValid())
	{
		return;
	}

	TSet<UEdGraphNode*> SelectedNodes = GraphHandler->GetSelectedNodes();
	if (!SelectedNodes.Contains(AnchorNode.Get()))
	{
		return;
	}

	const FVector2D NewNodePos(AnchorNode->NodePosX, AnchorNode->NodePosY);
	const FVector2D Delta = NewNodePos - LastAnchorPos;
	LastAnchorPos = NewNodePos;

	if (Delta.SizeSquared() == 0)
	{
		return;
	}
	
	TSet<UEdGraphNode*> NodesToMove;
	EEdGraphPinDirection Direction = EGPD_MAX;
	bool bMoveGroupOrSubtree = false;
	bool bMoveGraphHandledGroup = false;
	
	// Using right subtree movement key
	if (IsAnyInputChordDown(UBASettings_EditorFeatures::Get().RightSubTreeMovementChords))
	{
		Direction = EGPD_Output;
		bMoveGroupOrSubtree = true;
	}
	// Using left subtree movement key
	else if (IsAnyInputChordDown(UBASettings_EditorFeatures::Get().LeftSubTreeMovementChords))
	{
		Direction = EGPD_Input;
		bMoveGroupOrSubtree = true;
	}
	// Using group movement key
	else if (IsAnyInputChordDown(UBASettings_EditorFeatures::Get().GroupMovementChords))
	{
		Direction = EGPD_MAX;
		bMoveGroupOrSubtree = true;
	}
	// Otherwise use graph handler
	else if (KeysDown.Num() < 2)
	{
		bMoveGraphHandledGroup = true;
	}

	// Are we trying to move anything?
	if (!bMoveGroupOrSubtree && !bMoveGraphHandledGroup)
	{
		return;
	}
	
	// Group/subtree movement
	if (bMoveGroupOrSubtree)
	{
		// Get the graph nodes in the desired direction(s)
		for (UEdGraphNode* SelectedNode : SelectedNodes)
		{
			auto RelevantTree = FBAUtils::GetNodeTreeWithFilter(SelectedNode, [](UEdGraphPin* Pin)
			{
				return !FBAUtils::IsDelegatePin(Pin);
			}, Direction);
			NodesToMove.Append(RelevantTree);
		}
		// Add relevant input nodes for rightward subtrees
		if (Direction == EGPD_Output)
		{
			TSet<UEdGraphNode*> AdditionalNodesToMove;
			for (UEdGraphNode* SelectedNode : NodesToMove)
			{
				auto LinkedNodes = FBAUtils::GetLinkedNodes(SelectedNode, EGPD_Input);
				for (UEdGraphNode* Node : LinkedNodes)
				{
					auto ExecPins = FBAUtils::GetExecPins(Node, EGPD_Output);
					if (ExecPins.Num() == 0)
					{
						auto NonExecNodes = FBAUtils::GetNodeTreeWithFilter(Node, [](UEdGraphPin* Pin)
						{
							return FBAUtils::IsNodePure(Pin->GetOwningNode());
						}, EGPD_Input);
						AdditionalNodesToMove.Append(NonExecNodes);
					}
				}
			}
			NodesToMove.Append(AdditionalNodesToMove);
		}
	}
	// Group movement using graph handler
	else if (bMoveGraphHandledGroup)
	{
		NodesToMove = GraphHandler->GetGroupedNodes(SelectedNodes);
	}
	
	// Move nodes
	GroupMoveNodes(Delta, NodesToMove);
}

void FBAInputProcessor::GroupMoveSelectedNodes(const FVector2D& Delta)
{
	TSharedPtr<FBAGraphHandler> GraphHandler = FBATabHandler::Get().GetActiveGraphHandler();

	TSet<UEdGraphNode*> NodesToMove;

	// grab all linked nodes to move from the selected nodes
	TSet<UEdGraphNode*> SelectedNodes = GraphHandler->GetSelectedNodes();
	for (UEdGraphNode* SelectedNode : SelectedNodes)
	{
		NodesToMove.Append(FBAUtils::GetNodeTree(SelectedNode));
	}

	for (UEdGraphNode* Node : NodesToMove)
	{
		if (!SelectedNodes.Contains(Node))
		{
			Node->Modify(false);
			Node->NodePosX += Delta.X; 
			Node->NodePosY += Delta.Y;
		}
	}
}

void FBAInputProcessor::GroupMoveNodes(const FVector2D& Delta, TSet<UEdGraphNode*>& Nodes)
{
	TSharedPtr<FBAGraphHandler> GraphHandler = FBATabHandler::Get().GetActiveGraphHandler();
	TSet<UEdGraphNode*> SelectedNodes = GraphHandler->GetSelectedNodes();
	TSet<UEdGraphNode*> IgnoredNodes(SelectedNodes);

	if (SelectedNodes.Num() == 1)
	{
		if (UEdGraphNode_Comment* DraggedComment = Cast<UEdGraphNode_Comment>(SelectedNodes.Array()[0]))
		{
			for (UEdGraphNode* Node : FBAUtils::GetNodesUnderComment(DraggedComment))
			{
				IgnoredNodes.Add(Node);
			}
		}
	}

	for (UEdGraphNode* Node : Nodes)
	{
		if (IgnoredNodes.Contains(Node))
		{
			continue;
		}

		Node->Modify(false);
		Node->NodePosX += Delta.X;
		Node->NodePosY += Delta.Y;
	}
}

bool FBAInputProcessor::IsInputChordDown(const FInputChord& Chord)
{
	if (!Chord.Key.IsValid())
	{
		return false;
	}

	const FModifierKeysState ModKeysState = FSlateApplication::Get().GetModifierKeys();
	const bool AreModifiersDown = ModKeysState.AreModifersDown(EModifierKey::FromBools(Chord.bCtrl, Chord.bAlt, Chord.bShift, Chord.bCmd));
	return KeysDown.Contains(Chord.Key) && AreModifiersDown;
}

bool FBAInputProcessor::IsAnyInputChordDown(const TArray<FInputChord>& Chords)
{
	for (const FInputChord& Chord : Chords)
	{
		if (IsInputChordDown(Chord))
		{
			return true;
		}
	}

	return false;
}

bool FBAInputProcessor::IsInputChordDown(const FInputChord& Chord, const FKey Key)
{
	if (!Key.IsValid())
	{
		return false;
	}

	const FModifierKeysState ModKeysState = FSlateApplication::Get().GetModifierKeys();
	const bool AreModifiersDown = ModKeysState.AreModifersDown(EModifierKey::FromBools(Chord.bCtrl, Chord.bAlt, Chord.bShift, Chord.bCmd));
	return (Chord.Key == Key) && AreModifiersDown;
}

bool FBAInputProcessor::IsAnyInputChordDown(const TArray<FInputChord>& Chords, const FKey Key)
{
	for (const FInputChord& Chord : Chords)
	{
		if (IsInputChordDown(Chord, Key))
		{
			return true;
		}
	}

	return false;
}

bool FBAInputProcessor::IsKeyDown(const FKey Key)
{
	return KeysDownStartTime.Contains(Key);
}

double FBAInputProcessor::GetKeyDownDuration(const FKey Key)
{
	if (const double* FoundTime = KeysDownStartTime.Find(Key))
	{
		return FSlateApplication::Get().GetCurrentTime() - (*FoundTime);
	}

	return -1.0f;
}

bool FBAInputProcessor::ProcessFolderBookmarkInput()
{
	const UBASettings& BASettings = UBASettings::Get();

	for (int i = 0; i < BASettings.FolderBookmarks.Num(); ++i)
	{
		const FKey& BookmarkKey = BASettings.FolderBookmarks[i];

		if (IsInputChordDown(FInputChord(EModifierKey::Control | EModifierKey::Shift, BookmarkKey)))
		{
			if (FIND_PARENT_WIDGET(FSlateApplication::Get().GetUserFocusedWidget(0), SContentBrowser))
			{
				FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
				IContentBrowserSingleton& ContentBrowser = ContentBrowserModule.Get();

#if BA_UE_VERSION_OR_LATER(5, 0)
				const FString FolderPath = ContentBrowser.GetCurrentPath().GetInternalPathString();
#else
				const FString FolderPath = ContentBrowser.GetCurrentPath();
#endif
				FBACache::Get().SetBookmarkedFolder(FolderPath, i);

				FNotificationInfo Notification(FText::FromString(FString::Printf(TEXT("Saved bookmark %s to %s"), *BookmarkKey.ToString().ToUpper(), *FolderPath)));
				Notification.ExpireDuration = 3.0f;
				FSlateNotificationManager::Get().AddNotification(Notification);
				break;
			}
		}

		if (IsInputChordDown(FInputChord(EModifierKey::Control, BookmarkKey)))
		{
			if (FIND_PARENT_WIDGET(FSlateApplication::Get().GetUserFocusedWidget(0), SContentBrowser))
			{
				FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
				IContentBrowserSingleton& ContentBrowser = ContentBrowserModule.Get();

				if (TOptional<FString> FolderPath = FBACache::Get().FindBookmarkedFolder(i))
				{
					if (!FolderPath.GetValue().IsEmpty())
					{
						ContentBrowser.SetSelectedPaths({ FolderPath.GetValue() });
					}
				}
				break;
			}
		}
	}

	return false;
}


// TODO move these into FBACommands
bool FBAInputProcessor::ProcessContentBrowserInput()
{
	if (TSharedPtr<SContentBrowser> ContentBrowserWidget = FIND_PARENT_WIDGET(FSlateApplication::Get().GetUserFocusedWidget(0), SContentBrowser))
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		IContentBrowserSingleton& ContentBrowser = ContentBrowserModule.Get();

		// copy
		if (IsInputChordDown(FInputChord(EModifierKey::Control, EKeys::C)))
		{
			CutAssets.Reset();
			return false;
		}

		// cut
		if (IsInputChordDown(FInputChord(EModifierKey::Control, EKeys::X)))
		{
			TArray<FAssetData> SelectedAssets;
			ContentBrowser.GetSelectedAssets(SelectedAssets);

			CutAssets.Reset();
			for (FAssetData& SelectedAsset : SelectedAssets)
			{
				CutAssets.Add(SelectedAsset);
			}

			return CutAssets.Num() > 0;
		}

		// paste
		if (IsInputChordDown(FInputChord(EModifierKey::Control, EKeys::V)))
		{
			if (CutAssets.Num())
			{
#if BA_UE_VERSION_OR_LATER(5, 0)
				const FContentBrowserItemPath BrowserPath = ContentBrowser.GetCurrentPath();
				const FString Path = BrowserPath.HasInternalPath() ? ContentBrowser.GetCurrentPath().GetInternalPathString() : FString();
#else
				const FString Path = ContentBrowser.GetCurrentPath();
#endif

				TArray<UObject*> AssetsToMove;
				for (const FAssetData& AssetData : CutAssets)
				{
					const bool bSameFolder = Path.Equals(AssetData.PackagePath.ToString());
					if (!bSameFolder)
					{
						if (UObject* Asset = AssetData.GetAsset())
						{
							AssetsToMove.Add(Asset);
						}
					}
				}

				if (!AssetsToMove.Num())
				{
					return false;
				}

				// TODO why do transactions not work when moving assets? (there's no undo when moving with drag / drop)
				// const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "CutPaste_BlueprintAssist", "Cut And Paste"));
				// for (UObject* ToMove : AssetsToMove)
				// {
				// 	ToMove->Modify();
				// }

				AssetViewUtils::MoveAssets(AssetsToMove, Path);

				CutAssets.Reset();
				return true;
			}
		}
	}

	return false;
}

void FBAInputProcessor::OnWindowFocusChanged(bool bIsFocused)
{
	if (!bIsFocused)
	{
		TSet<FKey> CurrentKeysDown = KeysDown;
		for (const FKey& Key : CurrentKeysDown)
		{
			OnKeyOrMouseUp(FSlateApplication::Get(), Key);
		}

		KeysDown.Reset();
		KeysDownStartTime.Reset();
	}
}

// See logic from FUICommandList::ConditionalProcessCommandBindings
bool FBAInputProcessor::ProcessCommandBindings(TSharedPtr<FUICommandList> CommandList, const FKeyEvent& KeyEvent)
{
	if (FSlateApplication::Get().IsDragDropping())
	{
		return false;
	}

	FModifierKeysState ModifierKeysState = FSlateApplication::Get().GetModifierKeys();
	const FInputChord CheckChord(KeyEvent.GetKey(), EModifierKey::FromBools(
		ModifierKeysState.IsControlDown(),
		ModifierKeysState.IsAltDown(),
		ModifierKeysState.IsShiftDown(),
		ModifierKeysState.IsCommandDown()));

	const auto& DisabledCommands = GetDefault<UBASettings_Advanced>()->DisabledCommands;

	const FInputBindingManager& InputBindingManager = FInputBindingManager::Get();

	TArray<TSharedPtr<FUICommandInfo>> LocalCommandInfos;
	InputBindingManager.GetCommandInfosFromContext(FBACommands::Get().GetContextName(), LocalCommandInfos);

	// Only active chords process commands
	constexpr bool bCheckDefault = false;

	static const TArray<FName> ContextNames = { FBACommands::Get().GetContextName(), FBAToolbarCommands::Get().GetContextName() };

	// Check to see if there is any command in the context activated by the chord
	for (const FName& ContextName : ContextNames)
	{
		TSharedPtr<FUICommandInfo> Command = FInputBindingManager::Get().FindCommandInContext(ContextName, CheckChord, bCheckDefault);

		if (Command.IsValid() && Command->HasActiveChord(CheckChord))
		{
			// Find the bound action for this command
			const FUIAction* Action = CommandList->GetActionForCommand(Command);

			// If there is no Action mapped to this command list, continue to the next context
			if (Action)
			{
				if (Action->CanExecute() && (!KeyEvent.IsRepeat() || Action->CanRepeat()))
				{
					// Block the command if we have disabled it in the settings
					if (!DisabledCommands.Contains(Command->GetCommandName()))
					{
						// If the action was found and can be executed, do so now
						return Action->Execute();
					}
				}
			}
		}
	}

	return false;
}