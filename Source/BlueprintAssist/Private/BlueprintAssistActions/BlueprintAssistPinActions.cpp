#include "BlueprintAssistActions/BlueprintAssistPinActions.h"

#include "BlueprintAssistCommands.h"
#include "BlueprintAssistGraphHandler.h"
#include "EdGraphUtilities.h"
#include "K2Node_Knot.h"
#include "ScopedTransaction.h"
#include "SGraphPanel.h"
#include "BlueprintAssistWidgets/LinkPinMenu.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Widgets/Notifications/SNotificationList.h"

bool FBAPinActionsBase::HasSelectedPin() const
{
	return HasGraphNonReadOnly() ? (GetGraphHandler()->GetSelectedPin() != nullptr) : false;
}

bool FBAPinActionsBase::HasEditablePin() const
{
	if (!CanExecuteActions())
	{
		return false;
	}

	const FSlateApplication& SlateApp = FSlateApplication::Get();
	TSharedPtr<SWidget> KeyboardFocusedWidget = SlateApp.GetKeyboardFocusedWidget();
	TSharedPtr<SWindow> Menu = SlateApp.GetActiveTopLevelWindow();

	return
		FBAUtils::IsUserInputWidget(KeyboardFocusedWidget) &&
		FBAUtils::GetParentWidgetOfType(KeyboardFocusedWidget, "SGraphPin").IsValid();
}

bool FBAPinActionsBase::HasHoveredPin() const
{
	return HasGraphNonReadOnly() ? (FBAUtils::GetHoveredGraphPin(GetGraphHandler()->GetGraphPanel()) != nullptr) : false;
}

bool FBAPinActionsBase::HasHoveredOrSelectedPin() const
{
	return HasHoveredPin() || HasSelectedPin();
}

void FBAPinActions::Init()
{
	PinCommands = MakeShareable(new FUICommandList());
	PinEditCommands = MakeShareable(new FUICommandList());

	////////////////////////////////////////////////////////////
	// Pin Commands
	////////////////////////////////////////////////////////////

	PinCommands->MapAction(
		FBACommands::Get().SwapConnectionUp,
		FExecuteAction::CreateRaw(this, &FBAPinActions::SwapPinConnection, true),
		FCanExecuteAction::CreateRaw(this, &FBAPinActions::HasSelectedPin)
	);

	PinCommands->MapAction(
		FBACommands::Get().SwapConnectionDown,
		FExecuteAction::CreateRaw(this, &FBAPinActions::SwapPinConnection, false),
		FCanExecuteAction::CreateRaw(this, &FBAPinActions::HasSelectedPin)
	);

	PinCommands->MapAction(
		FBACommands::Get().GetContextMenuForPin,
		FExecuteAction::CreateStatic(&FBANodeActions::OnGetContextMenuActions, true),
		FCanExecuteAction::CreateRaw(this, &FBAPinActions::HasSelectedPin)
	);

	PinCommands->MapAction(
		FBACommands::Get().LinkToHoveredPin,
		FExecuteAction::CreateRaw(this, &FBAPinActions::LinkToHoveredPin),
		FCanExecuteAction::CreateRaw(this, &FBAPinActions::HasSelectedPin)
	);

	PinCommands->MapAction(
		FBACommands::Get().LinkPinMenu,
		FExecuteAction::CreateRaw(this, &FBAPinActions::OpenPinLinkMenu),
		FCanExecuteAction::CreateRaw(this, &FBAPinActions::HasSelectedPin)
	);

	PinCommands->MapAction(
		FBACommands::Get().DuplicateNodeForEachLink,
		FExecuteAction::CreateRaw(this, &FBAPinActions::DuplicateNodeForEachLink),
		FCanExecuteAction::CreateRaw(this, &FBAPinActions::HasSelectedPin)
	);

	PinCommands->MapAction(
		FBACommands::Get().EditSelectedPinValue,
		FExecuteAction::CreateRaw(this, &FBAPinActions::OnEditSelectedPinValue),
		FCanExecuteAction::CreateRaw(this, &FBAPinActions::HasSelectedPin)
	);

	// has hovered or selected pin
	PinCommands->MapAction(
		FBACommands::Get().DisconnectPinLink,
		FExecuteAction::CreateRaw(this, &FBAPinActions::DisconnectPinOrWire),
		FCanExecuteAction::CreateRaw(this, &FBAPinActions::HasHoveredOrSelectedPin)
	);

	PinCommands->MapAction(
		FBACommands::Get().SplitPin,
		FExecuteAction::CreateRaw(this, &FBAPinActions::SplitPin),
		FCanExecuteAction::CreateRaw(this, &FBAPinActions::HasHoveredOrSelectedPin)
	);

	PinCommands->MapAction(
		FBACommands::Get().RecombinePin,
		FExecuteAction::CreateRaw(this, &FBAPinActions::RecombinePin),
		FCanExecuteAction::CreateRaw(this, &FBAPinActions::HasHoveredOrSelectedPin)
	);

	////////////////////////////////////////////////////////////
	// Pin Edit Commands
	////////////////////////////////////////////////////////////

	PinEditCommands->MapAction(
		FBACommands::Get().EditSelectedPinValue,
		FExecuteAction::CreateRaw(this, &FBAPinActions::OnEditSelectedPinValue),
		FCanExecuteAction::CreateRaw(this, &FBAPinActions::HasEditablePin)
	);
}

void FBAPinActions::LinkToHoveredPin()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();
	if (!GraphPanel.IsValid())
	{
		return;
	}

	UEdGraphPin* SelectedPin = GraphHandler->GetSelectedPin();
	if (SelectedPin != nullptr)
	{
		TSharedPtr<SGraphPin> HoveredPin = FBAUtils::GetHoveredGraphPin(GraphPanel);
		if (HoveredPin.IsValid())
		{
			const FScopedTransaction Transaction(
				NSLOCTEXT("UnrealEd", "LinkToHoveredPin", "Link To Hovered Pin"));

			if (FBAUtils::CanConnectPins(SelectedPin, HoveredPin->GetPinObj(), true, false))
			{
				FBAUtils::TryLinkPins(SelectedPin, HoveredPin->GetPinObj());
			}
		}
	}
}

void FBAPinActions::OpenPinLinkMenu()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();
	if (!GraphPanel.IsValid())
	{
		return;
	}

	UEdGraphPin* Pin = GraphHandler->GetSelectedPin();
	check(Pin != nullptr)

	TSharedRef<SLinkPinMenu> Widget =
		SNew(SLinkPinMenu)
		.SourcePin(Pin)
		.GraphHandler(GraphHandler);

	FBAUtils::OpenPopupMenu(Widget, Widget->GetWidgetSize(), FVector2D(0, 0.4), FVector2D(0.5f, 1.0f));
}

void FBAPinActions::DuplicateNodeForEachLink()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	// Find the graph editor with focus
	UEdGraph* DestinationGraph = GraphHandler->GetFocusedEdGraph();
	if (DestinationGraph == nullptr)
	{
		return;
	}

	FBANodePinHandle SelectedPin(GraphHandler->GetSelectedPin());
	if (!SelectedPin.IsValid())
	{
		return;
	}

	// TODO: Make this work with multiple nodes
	UEdGraphNode* SelectedNode = GraphHandler->GetSelectedNode();
	if (SelectedNode == nullptr)
	{
		return;
	}

	if (!FBAUtils::IsBlueprintGraph(DestinationGraph))
	{
		FNotificationInfo Notification(FText::FromString("Duplicate Node For Each Link only supports Blueprint graphs"));
		Notification.ExpireDuration = 2.0f;
		FSlateNotificationManager::Get().AddNotification(Notification);
		return;
	}

	if (!FBAUtils::IsNodePure(SelectedNode))
	{
		FNotificationInfo Notification(FText::FromString("Duplicate Node For Each Link currently only supports pure nodes"));

		Notification.ExpireDuration = 2.0f;
		FSlateNotificationManager::Get().AddNotification(Notification);
		return;
	}

	const UEdGraphSchema* Schema = DestinationGraph->GetSchema();
	if (!Schema)
	{
		return;
	}

	TSharedPtr<FScopedTransaction> Transaction = MakeShareable(new FScopedTransaction(NSLOCTEXT("UnrealEd", "DuplicateNodesForEachLink", "Duplicate Node For Each Link")));

	DestinationGraph->Modify();

	// logic from FBlueprintEditor::PasteNodesHere
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(DestinationGraph);

	FGraphPanelSelectionSet SelectedNodes;

	const auto OwningNodeIsPure = [](UEdGraphPin* Pin)
	{
		return FBAUtils::IsNodePure(Pin->GetOwningNode());
	};

	const TSet<UEdGraphNode*> NodeTree = FBAUtils::GetNodeTreeWithFilter(SelectedNode, OwningNodeIsPure, EGPD_Input);

	for (UEdGraphNode* Node : NodeTree)
	{
		SelectedNodes.Emplace(Node);
	}

	SelectedNode->PrepareForCopying();
	FString ExportedText;
	FEdGraphUtilities::ExportNodesToText(SelectedNodes, ExportedText);

	struct FLocal
	{
		static void DeleteKnotsAndGetLinkedPins(
			UEdGraphPin* InPin,
			TArray<UEdGraphPin*>& LinkedPins)
		{
			/** Iterate across all linked pins */
			TArray<UEdGraphPin*> LinkedCopy = InPin->LinkedTo;
			for (UEdGraphPin* LinkedPin : LinkedCopy)
			{
				UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();

				if (FBAUtils::IsKnotNode(LinkedNode))
				{
					for (UEdGraphPin* Pin : FBAUtils::GetPinsByDirection(LinkedNode, InPin->Direction))
					{
						DeleteKnotsAndGetLinkedPins(Pin, LinkedPins);
					}
				}
				else
				{
					LinkedPins.Emplace(LinkedPin);
				}
			}

			/** Delete all connections for each knot node */
			if (UK2Node_Knot* KnotNode = Cast<UK2Node_Knot>(InPin->GetOwningNode()))
			{
				FBAUtils::DisconnectKnotNode(KnotNode);
				FBAUtils::DeleteNode(KnotNode);
			}
		}
	};

	TArray<UEdGraphPin*> LinkedPins;
	FLocal::DeleteKnotsAndGetLinkedPins(SelectedPin.GetPin(), LinkedPins);
	TArray<FBANodePinHandle> LinkedPinHandles = FBANodePinHandle::ConvertArray(LinkedPins);
	if (LinkedPinHandles.Num() <= 1)
	{
		return;
	}

	bool bNeedToModifyStructurally = false;

	SelectedPin.GetPin()->Modify();

	for (FBANodePinHandle& PinHandle : LinkedPinHandles)
	{
		PinHandle.GetPin()->Modify();

		// duplicate the node for each linked to pin
		Schema->BreakSinglePinLink(SelectedPin.GetPin(), PinHandle.GetPin());

		// import the nodes
		TSet<UEdGraphNode*> PastedNodes;
		FEdGraphUtilities::ImportNodesFromText(DestinationGraph, ExportedText, /*out*/ PastedNodes);

		for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
		{
			UEdGraphNode* Node = *It;

			// UE_LOG(LogBlueprintAssist, Warning, TEXT("Node %s %d | Selected node %s %d"),
			//        *FBAUtils::GetNodeName(Node), Node->GetUniqueID(),
			//        *FBAUtils::GetNodeName(SelectedNode), SelectedNode->GetUniqueID()
			// );

			auto OldGuid = Node->NodeGuid;
			Node->CreateNewGuid();

			Node->NodePosX = FBAUtils::GetPinPos(GraphHandler, PinHandle.GetPin()).X;

			if (OldGuid != SelectedNode->NodeGuid)
			{
				continue;
			}

			// Update the selected node
			UK2Node* K2Node = Cast<UK2Node>(Node);
			if (K2Node != nullptr && K2Node->NodeCausesStructuralBlueprintChange())
			{
				bNeedToModifyStructurally = true;
			}

			UEdGraphPin* ValuePin = FBAUtils::GetPinsByDirection(Node, EGPD_Output)[0];
			ValuePin->MakeLinkTo(PinHandle.GetPin());
		}
	}

	for (UEdGraphNode* Node : NodeTree)
	{
		Node->Modify();
		FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
	}

	if (bNeedToModifyStructurally)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
	else
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}

	// Update UI
	DestinationGraph->NotifyGraphChanged();

	auto AutoFormatting = UBASettings::GetFormatterSettings(DestinationGraph).GetAutoFormatting();

	if (AutoFormatting != EBAAutoFormatting::Never)
	{
		for (FBANodePinHandle& PinHandle : LinkedPinHandles)
		{
			GraphHandler->AddPendingFormatNodes(PinHandle.GetNode(), Transaction);
		}
	}
}

void FBAPinActions::SwapPinConnection(const bool bUp)
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	UEdGraph* Graph = GraphHandler->GetFocusedEdGraph();
	if (!Graph)
	{
		return;
	}

	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (!Schema)
	{
		return;
	}

	FBANodePinHandle PinA(GraphHandler->GetSelectedPin());
	if (PinA == nullptr)
	{
		return;
	}

	// Get the pin to swap to (PinB)
	FBANodePinHandle PinB;

	TArray<UEdGraphPin*> PinsOnSide = FBAUtils::GetPinsByDirection(PinA->GetOwningNode(), PinA->Direction);
	const int32 PinIndex = FBAUtils::GetPinIndex(PinA.GetPin());
	if (bUp)
	{
		for (int i = PinIndex - 1; i >= 0; --i)
		{
			if (PinsOnSide[i]->PinType == PinA->PinType)
			{
				PinB = PinsOnSide[i];
				break;
			}
		}
	}
	else
	{
		for (int i = PinIndex + 1; i < PinsOnSide.Num(); ++i)
		{
			if (PinsOnSide[i]->PinType == PinA->PinType)
			{
				PinB = PinsOnSide[i];
				break;
			}
		}
	}

	if (!PinB.IsValid())
	{
		return;
	}

	FScopedTransaction Transaction(INVTEXT("Swap connections"));

	TArray<FBANodePinHandle> LinkedTo_PinB = FBANodePinHandle::ConvertArray(PinB->LinkedTo);
	TArray<FBANodePinHandle> LinkedTo_PinA = FBANodePinHandle::ConvertArray(PinA->LinkedTo);

	PinA->Modify();
	PinB->Modify();

	FString DefaultValue_A = PinA->DefaultValue;
	FText DefaultTextValue_A = PinA->DefaultTextValue;
	UObject* DefaultObject_A = PinA->DefaultObject;

	FString DefaultValue_B = PinB->DefaultValue;
	FText DefaultTextValue_B = PinB->DefaultTextValue;
	UObject* DefaultObject_B = PinB->DefaultObject;

	const bool DefaultValueDifferent = !DefaultValue_A.Equals(DefaultValue_B, ESearchCase::CaseSensitive);
	const bool DefaultTextDifferent = !DefaultTextValue_A.IdenticalTo(DefaultTextValue_B);
	const bool DefaultObjectDifferent = DefaultObject_A != DefaultObject_B;

	PinA->BreakAllPinLinks();
	PinB->BreakAllPinLinks();

	if (LinkedTo_PinA.Num())
	{
		// connect LinkedTo_PinA -> PinB
		for (FBANodePinHandle& Pin : LinkedTo_PinA)
		{
			FBAUtils::TryCreateConnection(Pin.GetPin(), PinB.GetPin(), EBABreakMethod::Default);
		}
	}
	else
	{
		// copy the default values onto PinB from PinA
		if (DefaultValueDifferent)
			Schema->TrySetDefaultValue(*PinB.GetPin(), DefaultValue_A);
		if (DefaultTextDifferent)
			Schema->TrySetDefaultText(*PinB.GetPin(), DefaultTextValue_A);
		if (DefaultObjectDifferent)
			Schema->TrySetDefaultObject(*PinB.GetPin(), DefaultObject_A);
	}

	// connect LinkedTo_PinB -> PinA
	if (LinkedTo_PinB.Num())
	{
		for (FBANodePinHandle& Pin : LinkedTo_PinB)
		{
			FBAUtils::TryCreateConnection(Pin.GetPin(), PinA.GetPin(), true);
		}
	}
	else
	{
		// copy the default values onto PinA from PinB
		if (DefaultValueDifferent)
			Schema->TrySetDefaultValue(*PinA.GetPin(), DefaultValue_B);
		if (DefaultTextDifferent)
			Schema->TrySetDefaultText(*PinA.GetPin(), DefaultTextValue_B);
		if (DefaultObjectDifferent)
			Schema->TrySetDefaultObject(*PinA.GetPin(), DefaultObject_B);
	}

	GetGraphHandler()->SetSelectedPin(PinB.GetPin());
}

void FBAPinActions::OnEditSelectedPinValue()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	UEdGraphPin* SelectedPin = GraphHandler->GetSelectedPin();
	if (SelectedPin == nullptr)
	{
		return;
	}

	// if we have an invalid pin for editing (exec or delegate) try select the first value pin
	if (FBAUtils::IsExecPin(SelectedPin) || FBAUtils::IsDelegatePin(SelectedPin))
	{
		if (UEdGraphPin* ValuePin = FBAUtils::GetFirstValuePinOnNode(GraphHandler, SelectedPin->GetOwningNode()))
		{
			SelectedPin = ValuePin;
			GraphHandler->SetSelectedPin(ValuePin);
		}
	}

	TSharedPtr<SGraphPin> GraphPin = FBAUtils::GetGraphPin(GraphHandler->GetGraphPanel(), SelectedPin);
	if (!GraphPin.IsValid())
	{
		return;
	}

	struct FLocal
	{
		static void GetEditableWidgets(TSharedPtr<SWidget> Widget, TArray<TSharedPtr<SWidget>>& EditableWidgets, TArray<TSharedPtr<SWidget>>& ClickableWidgets)
		{
			if (Widget.IsValid())
			{
				if (FBAUtils::IsUserInputWidget(Widget))
				{
					EditableWidgets.Add(Widget);
				}
				else if (FBAUtils::IsClickableWidget(Widget))
				{
					ClickableWidgets.Add(Widget);
				}

				// iterate through children
				if (FChildren* Children = Widget->GetChildren())
				{
					for (int i = 0; i < Children->Num(); i++)
					{
						GetEditableWidgets(Children->GetChildAt(i), EditableWidgets, ClickableWidgets);
					}
				}
			}
		}
	};

	TArray<TSharedPtr<SWidget>> EditableWidgets;
	TArray<TSharedPtr<SWidget>> ClickableWidgets;
	FLocal::GetEditableWidgets(GraphPin, EditableWidgets, ClickableWidgets);

	if (EditableWidgets.Num() > 0)
	{
		TSharedPtr<SWidget> CurrentlyFocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
		const int32 CurrentIndex = EditableWidgets.IndexOfByKey(CurrentlyFocusedWidget);

		if (CurrentIndex == -1)
		{
			FSlateApplication::Get().SetKeyboardFocus(EditableWidgets[0], EFocusCause::Navigation);
		}
		else
		{
			const int32 NextIndex = (CurrentIndex + 1) % (EditableWidgets.Num());
			FSlateApplication::Get().SetKeyboardFocus(EditableWidgets[NextIndex], EFocusCause::Navigation);
		}
	}
	else if (ClickableWidgets.Num() > 0)
	{
		FBAUtils::InteractWithWidget(ClickableWidgets[0]);
	}
}

void FBAPinActions::DisconnectPinOrWire()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();

	if (GraphPanel.IsValid())
	{
		TSharedPtr<SGraphPin> HoveredPin = FBAUtils::GetHoveredGraphPin(GraphPanel);
		if (HoveredPin.IsValid())
		{
			if (UEdGraphPin* Pin = HoveredPin->GetPinObj())
			{
				const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "DisconnectPinLink", "Disconnect Pin Link"));
				GraphPanel->GetGraphObj()->GetSchema()->BreakPinLinks(*Pin, true);
				return;
			}
		}
	}

	UEdGraphPin* SelectedPin = GraphHandler->GetSelectedPin();
	if (SelectedPin != nullptr)
	{
		const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "DisconnectPinLink", "DisconectPinLink"));
		GraphPanel->GetGraphObj()->GetSchema()->BreakPinLinks(*SelectedPin, true);
	}
}

void FBAPinActions::SplitPin()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();
	if (!GraphPanel.IsValid())
	{
		return;
	}

	auto EdGraph = GraphHandler->GetFocusedEdGraph();
	if (!EdGraph)
	{
		return;
	}

	UEdGraphPin* PinToUse = nullptr;

	TSharedPtr<SGraphPin> HoveredPin = FBAUtils::GetHoveredGraphPin(GraphPanel);
	if (HoveredPin.IsValid())
	{
		PinToUse = HoveredPin->GetPinObj();
	}

	if (PinToUse == nullptr)
	{
		PinToUse = GraphHandler->GetSelectedPin();
	}

	if (PinToUse != nullptr)
	{
		if (const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(EdGraph->GetSchema()))
		{
			if (Schema->CanSplitStructPin(*PinToUse))
			{
				const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "SplitPin", "Split Pin"));

				Schema->SplitPin(PinToUse);

				if (PinToUse->SubPins.Num())
				{
					GraphHandler->SetSelectedPin(PinToUse->SubPins[0]);
				}
				else
				{
					GraphHandler->SetSelectedPin(nullptr);
				}
			}
		}
	}
}

void FBAPinActions::RecombinePin()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();
	if (!GraphPanel.IsValid())
	{
		return;
	}

	auto EdGraph = GraphHandler->GetFocusedEdGraph();
	if (!EdGraph)
	{
		return;
	}

	UEdGraphPin* PinToUse = nullptr;

	TSharedPtr<SGraphPin> HoveredPin = FBAUtils::GetHoveredGraphPin(GraphPanel);
	if (HoveredPin.IsValid())
	{
		PinToUse = HoveredPin->GetPinObj();
	}

	if (PinToUse == nullptr)
	{
		PinToUse = GraphHandler->GetSelectedPin();
	}

	if (PinToUse != nullptr)
	{
		const UEdGraphSchema* Schema = EdGraph->GetSchema();

		if (PinToUse->ParentPin != nullptr)
		{
			const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "RecombinePin", "Recombine Pin"));
			GraphHandler->SetSelectedPin(PinToUse->ParentPin);
			Schema->RecombinePin(PinToUse);
		}
	}
}
