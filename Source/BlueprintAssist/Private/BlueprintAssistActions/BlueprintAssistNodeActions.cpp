#include "BlueprintAssistActions/BlueprintAssistNodeActions.h"

#include "BlueprintAssistCache.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "BlueprintAssistGlobals.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "HAL/PlatformApplicationMisc.h"
#include "EdGraphUtilities.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Knot.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "SCommentBubble.h"
#include "ScopedTransaction.h"
#include "SGraphActionMenu.h"
#include "SGraphPanel.h"
#include "Algo/Transform.h"
#include "BlueprintAssistWidgets/BlueprintAssistGraphOverlay.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace MergeNodesTypes
{
	struct FMergeNodeTree
	{
		TArray<UEdGraphNode*> NodeTreeMap;

		FMergeNodeTree(const TArray<UEdGraphNode*>& NodeTree, const TSet<UEdGraphNode*>& SelectedNodes)
		{
			UEdGraphNode* TopRightMost = FBAUtils::GetTopMostWithFilter(NodeTree[0], EGPD_Output, [&SelectedNodes](UEdGraphNode* Node)
			{
				return SelectedNodes.Contains(Node);
			});

			TArray<UEdGraphNode*> PendingNodes;
			PendingNodes.Add(TopRightMost);

			while (PendingNodes.Num())
			{
				UEdGraphNode* CurrentNode = PendingNodes.Pop();
				NodeTreeMap.Add(CurrentNode);

				TArray<UEdGraphNode*> LinkedNodes = FBAUtils::GetLinkedNodes(CurrentNode, EGPD_MAX).FilterByPredicate([&NodeTree, this](UEdGraphNode* Node)
				{
					return NodeTree.Contains(Node) && !NodeTreeMap.Contains(Node);
				});

				for (UEdGraphNode* LinkedNode : LinkedNodes)
				{
					PendingNodes.Add(LinkedNode);
				}
			}
		}


		static TArray<FMergeNodeTree> MakeNodeTreesFromSelection(TSharedPtr<FBAGraphHandler> GraphHandler)
		{
			TArray<FMergeNodeTree> NodeTrees;

			TSet<UEdGraphNode*> SelectedNodes = GraphHandler->GetSelectedNodes(false);

			FScopedTransaction Transaction(INVTEXT("Merge nodes"));
			for (UEdGraphNode* SelectedNode : SelectedNodes)
			{
				SelectedNode->Modify();
			}

			TArray<UEdGraphNode*> PendingNodes = SelectedNodes.Array();
			while (PendingNodes.Num())
			{
				UEdGraphNode* NextNode = PendingNodes.Pop();

				const auto IsNodeSelected = [&SelectedNodes](UEdGraphPin* Pin)
				{
					return SelectedNodes.Contains(Pin->GetOwningNode());
				};

				const TArray<UEdGraphNode*> NodeTree = FBAUtils::GetNodeTreeWithFilter(NextNode, IsNodeSelected).Array();
				for (UEdGraphNode* Node : NodeTree)
				{
					PendingNodes.Remove(Node);
				}

				NodeTrees.Add(FMergeNodeTree(NodeTree, SelectedNodes));
			}

			return NodeTrees;
		}
	};

	struct FMergePinData
	{
		// TODO merge default values

		// links
		TArray<FBAGraphPinHandle> PendingLinks;
	};
}

bool FBANodeActionsBase::HasSingleNodeSelected() const
{
	return HasGraphNonReadOnly() ? (GetGraphHandler()->GetSelectedNode() != nullptr) : false;
}

bool FBANodeActionsBase::HasMultipleNodesSelected() const
{
	return HasGraphNonReadOnly() ? (GetGraphHandler()->GetSelectedNodes().Num() > 0) : false;
}

bool FBANodeActionsBase::HasMultipleNodesSelectedInclComments() const
{
	return HasGraphNonReadOnly() ? (GetGraphHandler()->GetSelectedNodes(true).Num() > 0) : false;
}

bool FBANodeActionsBase::HasHoveredNode() const
{
	return HasGraphNonReadOnly() ? FBAUtils::GetHoveredGraphNode(GetGraphHandler()->GetGraphPanel()).IsValid() : false;
}

bool FBANodeActionsBase::HasHoveredOrSelectedNodes() const
{
	if (!HasGraphNonReadOnly())
	{
		return false;
	}

	return GetGraphHandler()->GetSelectedNodes().Num() > 0 || HasHoveredNode();
}

bool FBANodeActionsBase::HasHoveredOrSingleSelectedNode() const
{
	return HasGraphNonReadOnly() ? (FBANodeActions::GetSingleHoveredOrSelectedNode() != nullptr) : false;
}

void FBANodeActions::Init()
{
	SingleNodeCommands = MakeShareable(new FUICommandList());
	MultipleNodeCommands = MakeShareable(new FUICommandList());
	MultipleNodeCommandsIncludingComments = MakeShareable(new FUICommandList());
	MiscNodeCommands = MakeShareable(new FUICommandList());

	////////////////////////////////////////////////////////////
	// Single Node Commands
	////////////////////////////////////////////////////////////

	SingleNodeCommands->MapAction(
		FBACommands::Get().ConnectUnlinkedPins,
		FExecuteAction::CreateRaw(this, &FBANodeActions::OnSmartWireSelectedNode),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasSingleNodeSelected)
	);

	SingleNodeCommands->MapAction(
		FBACommands::Get().ZoomToNodeTree,
		FExecuteAction::CreateRaw(this, &FBANodeActions::ZoomToNodeTree),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasSingleNodeSelected)
	);

	SingleNodeCommands->MapAction(
		FBACommands::Get().DisconnectAllNodeLinks,
		FExecuteAction::CreateRaw(this, &FBANodeActions::DisconnectAllNodeLinks),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasSingleNodeSelected)
	);

	SingleNodeCommands->MapAction(
		FBACommands::Get().SelectPinUp,
		FExecuteAction::CreateRaw(this, &FBANodeActions::SelectPinInDirection, 0, -1),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::CanSelectPinInDirection)
	);

	SingleNodeCommands->MapAction(
		FBACommands::Get().SelectPinDown,
		FExecuteAction::CreateRaw(this, &FBANodeActions::SelectPinInDirection, 0, 1),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::CanSelectPinInDirection)
	);

	SingleNodeCommands->MapAction(
		FBACommands::Get().SelectPinLeft,
		FExecuteAction::CreateRaw(this, &FBANodeActions::SelectPinInDirection, -1, 0),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::CanSelectPinInDirection)
	);

	SingleNodeCommands->MapAction(
		FBACommands::Get().SelectPinRight,
		FExecuteAction::CreateRaw(this, &FBANodeActions::SelectPinInDirection, 1, 0),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::CanSelectPinInDirection)
	);

	SingleNodeCommands->MapAction(
		FBACommands::Get().GetContextMenuForNode,
		FExecuteAction::CreateStatic(&FBANodeActions::OnGetContextMenuActions, false),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasSingleNodeSelected)
	);

	SingleNodeCommands->MapAction(
		FBACommands::Get().ReplaceNodeWith,
		FExecuteAction::CreateRaw(this, &FBANodeActions::ReplaceNodeWith),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasSingleNodeSelected)
	);

	SingleNodeCommands->MapAction(
		FBACommands::Get().RenameSelectedNode,
		FExecuteAction::CreateRaw(this, &FBANodeActions::RenameSelectedNode),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::CanRenameSelectedNode)
	);

	SingleNodeCommands->MapAction(
		FBACommands::Get().EditNodeComment,
		FExecuteAction::CreateRaw(this, &FBANodeActions::RenameCommentBubble),
		FCanExecuteAction::CreateLambda([&]()
		{
			if (HasSingleNodeSelected())
			{
				if (UEdGraphNode* SelectedNode = GetGraphHandler()->GetSelectedNode())
				{
					return SelectedNode->SupportsCommentBubble();
				}
			}

			return false;
		}));

	SingleNodeCommands->MapAction(
		FBACommands::Get().ToggleNodeAdvancedDisplay,
		FExecuteAction::CreateRaw(this, &FBANodeActions::ToggleNodeAdvancedDisplay),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::CanToggleNodeAdvancedDisplay)
	);

	////////////////////////////////////////////////////////////
	// Multiple Node Commands
	////////////////////////////////////////////////////////////

	MultipleNodeCommands->MapAction(
		FBACommands::Get().FormatNodes,
		FExecuteAction::CreateRaw(this, &FBANodeActions::FormatNodes),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasMultipleNodesSelected)
	);

	MultipleNodeCommands->MapAction(
		FBACommands::Get().FormatNodes_Selectively,
		FExecuteAction::CreateRaw(this, &FBANodeActions::FormatNodesSelectively),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasMultipleNodesSelected)
	);

	MultipleNodeCommands->MapAction(
		FBACommands::Get().FormatNodes_Helixing,
		FExecuteAction::CreateRaw(this, &FBANodeActions::FormatNodesWithHelixing),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasMultipleNodesSelected)
	);

	MultipleNodeCommands->MapAction(
		FBACommands::Get().FormatNodes_LHS,
		FExecuteAction::CreateRaw(this, &FBANodeActions::FormatNodesWithLHS),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasMultipleNodesSelected)
	);

	MultipleNodeCommands->MapAction(
		FBACommands::Get().LinkNodesBetweenWires,
		FExecuteAction::CreateRaw(this, &FBANodeActions::LinkNodesBetweenWires),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasMultipleNodesSelected)
	);

	MultipleNodeCommands->MapAction(
		FBACommands::Get().DisconnectNodeExecution,
		FExecuteAction::CreateRaw(this, &FBANodeActions::DisconnectExecutionOfSelectedNode),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasMultipleNodesSelected)
	);

	MultipleNodeCommands->MapAction(
		FBACommands::Get().SwapNodeLeft,
		FExecuteAction::CreateRaw(this, &FBANodeActions::SwapNodeInDirection, EGPD_Input),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasMultipleNodesSelected)
	);

	MultipleNodeCommands->MapAction(
		FBACommands::Get().SwapNodeRight,
		FExecuteAction::CreateRaw(this, &FBANodeActions::SwapNodeInDirection, EGPD_Output),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasMultipleNodesSelected)
	);

	MultipleNodeCommands->MapAction(
		FBACommands::Get().DeleteAndLink,
		FExecuteAction::CreateRaw(this, &FBANodeActions::DeleteAndLink),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasMultipleNodesSelected)
	);
	
	MultipleNodeCommands->MapAction(
		FBACommands::Get().CutAndLink,
		FExecuteAction::CreateRaw(this, &FBANodeActions::CutAndLink),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasMultipleNodesSelected)
	);

	MultipleNodeCommands->MapAction(
		FBACommands::Get().ToggleNode,
		FExecuteAction::CreateRaw(this, &FBANodeActions::ToggleNodes),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::CanToggleNodes)
	);

	MultipleNodeCommands->MapAction(
		FBACommands::Get().ToggleLockNode,
		FExecuteAction::CreateRaw(this, &FBANodeActions::ToggleLockNodes),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasMultipleNodesSelected)
	);

	MultipleNodeCommands->MapAction(
		FBACommands::Get().GroupNodes,
		FExecuteAction::CreateRaw(this, &FBANodeActions::GroupNodes),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasMultipleNodesSelected)
	);

	MultipleNodeCommands->MapAction(
		FBACommands::Get().UngroupNodes,
		FExecuteAction::CreateRaw(this, &FBANodeActions::UngroupNodes),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasMultipleNodesSelected)
	);

	MultipleNodeCommands->MapAction(
		FBACommands::Get().MergeSelectedNodes,
		FExecuteAction::CreateRaw(this, &FBANodeActions::MergeNodes),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::CanMergeNodes)
	);

	////////////////////////////////////////////////////////////
	// Multiple Node Including Comments Commands
	////////////////////////////////////////////////////////////

	MultipleNodeCommandsIncludingComments->MapAction(
		FBACommands::Get().RefreshNodeSizes,
		FExecuteAction::CreateRaw(this, &FBANodeActions::RefreshNodeSizes),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasMultipleNodesSelectedInclComments)
	);

	////////////////////////////////////////////////////////////
	// Hovered or Selected Node Commands
	////////////////////////////////////////////////////////////

	MiscNodeCommands->MapAction(
		FBACommands::Get().ExpandNodeSelection,
		FExecuteAction::CreateRaw(this, &FBANodeActions::ExpandSelection),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasHoveredOrSelectedNodes));

	MiscNodeCommands->MapAction(
		FBACommands::Get().ExpandSelectionLeft,
		FExecuteAction::CreateRaw(this, &FBANodeActions::ExpandNodeTreeInDirection, EGPD_Input),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasHoveredOrSingleSelectedNode));

	MiscNodeCommands->MapAction(
		FBACommands::Get().ExpandSelectionRight,
		FExecuteAction::CreateRaw(this, &FBANodeActions::ExpandNodeTreeInDirection, EGPD_Output),
		FCanExecuteAction::CreateRaw(this, &FBANodeActions::HasHoveredOrSingleSelectedNode));
}

void FBANodeActions::SmartWireNode(UEdGraphNode* Node)
{
	auto GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	if (!FBAUtils::IsGraphNode(Node))
	{
		return;
	}

	UEdGraph* Graph = GraphHandler->GetFocusedEdGraph();
	if (!Graph)
	{
		return;
	}

	TSet<UEdGraphNode*> LHSNodes, RHSNodes;
	TSet<UEdGraphPin*> LHSPins, RHSPins;
	FBAUtils::SortNodesOnGraphByDistance(Node, Graph, LHSNodes, RHSNodes, LHSPins, RHSPins);

	TArray<TArray<UEdGraphPin*>> PinsByType;
	TArray<UEdGraphPin*> ExecPins = FBAUtils::GetExecPins(Node);
	TArray<UEdGraphPin*> ParamPins = FBAUtils::GetParameterPins(Node);
	PinsByType.Add(ExecPins);
	PinsByType.Add(ParamPins);
	for (const TArray<UEdGraphPin*>& Pins : PinsByType)
	{
		for (UEdGraphPin* PinA : Pins)
		{
			// skip if pin is hidden or if the pin already is connected
			if (PinA->bHidden || PinA->LinkedTo.Num() > 0 || PinA->Direction == EGPD_MAX)
			{
				continue;
			}

			// check all pins to the left if we are an input pin
			// check all pins to the right if we are an output pin
			bool IsInputPin = PinA->Direction == EGPD_Input;
			for (UEdGraphPin* PinB : IsInputPin ? LHSPins : RHSPins)
			{
				// skip if has connection
				if (PinB->LinkedTo.Num() > 0)
				{
					continue;
				}

				// UE_LOG(LogBlueprintAssist, Warning, TEXT("Checking pins %s %s"), *FBAUtils::GetPinName(PinA), *FBAUtils::GetPinName(PinB));

				//bool bShouldOverrideLink = FBlueprintAssistUtils::IsExecPin(PinA);
				if (!FBAUtils::CanConnectPins(PinA, PinB, false, false, false))
				{
					// UE_LOG(LogBlueprintAssist, Warning, TEXT("\tSkipping"));
					continue;
				}

				TSharedPtr<FScopedTransaction> Transaction = MakeShareable(
					new FScopedTransaction(
						NSLOCTEXT("UnrealEd", "ConnectUnlinkedPins", "Connect Unlinked Pins")
					));

				FBAUtils::TryLinkPins(PinA, PinB);

				if (UBASettings::GetFormatterSettings(Graph).GetAutoFormatting() != EBAAutoFormatting::Never)
				{
					FEdGraphFormatterParameters FormatterParams;
					if (UBASettings::GetFormatterSettings(Graph).GetAutoFormatting() == EBAAutoFormatting::FormatSingleConnected)
					{
						FormatterParams.NodesToFormat.GetNodesWeak().Add(PinA->GetOwningNode());
						FormatterParams.NodesToFormat.GetNodesWeak().Add(PinB->GetOwningNode());
					}

					GraphHandler->AddPendingFormatNodes(PinA->GetOwningNode(), Transaction, FormatterParams);
				}
				else
				{
					Transaction.Reset();
				}

				return;
			}
		}
	}
}

void FBANodeActions::DisconnectExecutionOfNodes(TArray<UEdGraphNode*> Nodes)
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	// TODO: Make this work for pure nodes
	FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "DisconnectExecutionForNodes", "Disconnect Execution for Nodes"));

	if (Nodes.Num() == 0)
	{
		Transaction.Cancel();
		return;
	}

	Nodes.Sort([](UEdGraphNode& A, UEdGraphNode& B)
	{
		return FBAUtils::IsNodeImpure(&A) > FBAUtils::IsNodeImpure(&B);  
	});

	const UEdGraphSchema* Schema = GraphHandler->GetFocusedEdGraph()->GetSchema();

	const int NumNodes = Nodes.Num();
	for (int i = 0; i < NumNodes; ++i)
	{
		// UE_LOG(LogTemp, Warning, TEXT("%d %d"), i, Nodes.Num());
		if (Nodes.Num() == 0)
		{
			break;
		}

		UEdGraphNode* NextNode = Nodes[0];
		// UE_LOG(LogTemp, Warning, TEXT("NODE TREE for %s"), *FBAUtils::GetNodeName(NextNode));

		const auto PinFilter = [&Nodes](const UEdGraphPin* Pin)
		{
			return Nodes.Contains(Pin->GetOwningNode());
		};

		TArray<UEdGraphNode*> FullNodeTree = FBAUtils::GetNodeTreeWithFilter(NextNode, PinFilter).Array();
		bool bIsExecTree = FullNodeTree.ContainsByPredicate(FBAUtils::IsNodeImpure); 

		TArray<FPinLink> LeafOutput;
		TArray<FPinLink> LeafInput;
		TArray<FPinLink> PinsToBreak;

		const auto PinLinkFilter = [&bIsExecTree, &LeafOutput, &LeafInput, &PinsToBreak, &FullNodeTree](const FPinLink& Link)
		{
			// UE_LOG(LogTemp, Warning, TEXT("Iterating %s"), *Link.ToString());

			// skip any parameter pins if we are an exec tree
			if (bIsExecTree && FBAUtils::IsParameterPin(Link.From))
			{
				// UE_LOG(LogTemp, Warning, TEXT("\tSKIP parameter'"));
				return false;
			}

			const bool bIsLeafNode = !FullNodeTree.Contains(Link.GetNode());
			if (bIsLeafNode)
			{
				PinsToBreak.Add(Link);

				// UE_LOG(LogTemp, Warning, TEXT("\tLeaf %s"), *Link.ToString());
				if (Link.GetDirection() == EGPD_Input)
				{
					LeafInput.Add(Link);
				}
				else
				{
					LeafOutput.Add(Link);
				}
			}

			return !bIsLeafNode;
		};

		const TArray<UEdGraphNode*> NodeTree = FBAUtils::IterateNodeTreeDepthFirst(NextNode, PinLinkFilter).Array();
		if (NodeTree.Num() > 0)
		{
			for (auto& Link : PinsToBreak)
			{
				Schema->BreakSinglePinLink(Link.GetFromPin(), Link.GetToPin());
			}

			for (FPinLink& InLink : LeafInput)
			{
				for (FPinLink& OutLink : LeafOutput)
				{
					UEdGraphPin* Input = InLink.GetToPin();
					UEdGraphPin* Output = OutLink.GetToPin();
					// UE_LOG(LogTemp, Warning, TEXT("Trying to link %s %s"), *FBAUtils::GetPinName(Input, true), *FBAUtils::GetPinName(Output, true));

					if (FBAUtils::CanConnectPins(Input, Output, false, false))
					{
						// UE_LOG(LogTemp, Warning, TEXT("Linked!"));
						Schema->TryCreateConnection(Input, Output);
					}
				}
			}
		}

		for (UEdGraphNode* Node : FullNodeTree)
		{
			// UE_LOG(LogTemp, Warning, TEXT("Remvoe %s"), *FBAUtils::GetNodeName(Node));
			Nodes.RemoveSwap(Node);
		}
	}
}

UEdGraphNode* FBANodeActions::GetSingleHoveredOrSelectedNode()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();

	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();
	if (!GraphPanel)
	{
		return nullptr;
	}

	// prefer hovered node
	if (UEdGraphNode* HoveredNode = FBAUtils::GetHoveredNode(GraphPanel))
	{
		return HoveredNode;
	}

	// otherwise use single selected node
	UEdGraphNode* SelectedNode = GraphHandler->GetSelectedNode();
	if (SelectedNode)
	{
		return SelectedNode;
	}

	return nullptr;
}

void FBANodeActions::OnSmartWireSelectedNode()
{
	UEdGraphNode* SelectedNode = GetGraphHandler()->GetSelectedNode();
	if (SelectedNode == nullptr)
	{
		return;
	}

	//const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "SmartWire", "Smart Wire Node"));
	SmartWireNode(SelectedNode);
}

void FBANodeActions::ZoomToNodeTree()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	UEdGraphNode* SelectedNode = GraphHandler->GetSelectedNode();
	if (SelectedNode == nullptr)
	{
		return;
	}

	TSet<UEdGraphNode*> NodeTree = FBAUtils::GetNodeTree(SelectedNode);

	// selecting a set of nodes requires the ptrs to be const
	TSet<const UEdGraphNode*> ConstNodeTree;
	for (UEdGraphNode* Node : NodeTree)
	{
		ConstNodeTree.Add(Node);
	}

	TSharedPtr<SGraphEditor> GraphEditor = GraphHandler->GetGraphEditor();
	GraphHandler->GetFocusedEdGraph()->SelectNodeSet(ConstNodeTree);
	GraphHandler->GetGraphEditor()->ZoomToFit(true);
}

void FBANodeActions::DisconnectAllNodeLinks()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	UEdGraphNode* SelectedNode = GraphHandler->GetSelectedNode();
	const UEdGraphSchema* Schema = GraphHandler->GetFocusedEdGraph()->GetSchema();
	if (SelectedNode != nullptr)
	{
		const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "DisconnectAllNodeLinks", "Disconnect All Node Links"));

		Schema->BreakNodeLinks(*SelectedNode);
	}
}

bool FBANodeActions::CanSelectPinInDirection()
{
	return HasSingleNodeSelected() && !FBAUtils::IsKnotNode(GetGraphHandler()->GetSelectedNode());
}

void FBANodeActions::SelectPinInDirection(int X, int Y) const
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	UEdGraphNode* SelectedNode = GraphHandler->GetSelectedNode();
	if (SelectedNode == nullptr)
	{
		GraphHandler->SetSelectedPin(nullptr);
		return;
	}

	if (FBAUtils::IsCommentNode(SelectedNode) || FBAUtils::IsKnotNode(SelectedNode))
	{
		GraphHandler->SetSelectedPin(nullptr);
		return;
	}

	const TArray<UEdGraphPin*> PinsOnSelectedNode = FBAUtils::GetPinsByDirection(SelectedNode);
	if (PinsOnSelectedNode.Num() == 0)
	{
		GraphHandler->SetSelectedPin(nullptr);
		return;
	}

	UEdGraphPin* SelectedPin = GraphHandler->GetSelectedPin();

	if (SelectedPin == nullptr)
	{
		GraphHandler->SetSelectedPin(FBAUtils::GetPinsByDirection(SelectedNode)[0]);
	}
	else
	{
		if (SelectedPin->GetOwningNode() != SelectedNode)
		{
			GraphHandler->SetSelectedPin(FBAUtils::GetPinsByDirection(SelectedNode)[0]);
		}
		else
		{
			const auto& IsPinVisibleAsAdvanced = [&](UEdGraphPin* Pin)
			{
				TSharedPtr<SGraphPin> GraphPin = FBAUtils::GetGraphPin(GraphHandler->GetGraphPanel(), Pin);
				return GraphPin.IsValid() &&
					GraphPin->IsPinVisibleAsAdvanced() == EVisibility::Visible;
			};

			if (X != 0) // x direction - switch to the opposite pins on the current node
			{
				// if we try to move the same direction as the selected pin, move to linked node instead
				if ((X < 0 && SelectedPin->Direction == EGPD_Input) ||
					(X > 0 && SelectedPin->Direction == EGPD_Output))
				{
					const TArray<UEdGraphPin*> LinkedToIgnoringKnots = FBAUtils::GetPinLinkedToIgnoringKnots(SelectedPin);
					if (LinkedToIgnoringKnots.Num() > 0)
					{
						GraphHandler->SetSelectedPin(LinkedToIgnoringKnots[0], true);
					}
					return;
				}

				auto Direction = UEdGraphPin::GetComplementaryDirection(SelectedPin->Direction);

				TArray<UEdGraphPin*> Pins = FBAUtils::GetPinsByDirection(SelectedNode, Direction).FilterByPredicate(IsPinVisibleAsAdvanced);

				if (Pins.Num() > 0)
				{
					const int32 PinIndex = FBAUtils::GetPinIndex(SelectedPin);

					if (PinIndex != -1)
					{
						const int32 NextPinIndex = FMath::Min(Pins.Num() - 1, PinIndex);
						if (Pins.Num() > 0)
						{
							GraphHandler->SetSelectedPin(Pins[NextPinIndex]);
						}
					}
					else
					{
						GraphHandler->SetSelectedPin(Pins[0]);
					}
				}
			}
			else if (Y != 0) // y direction - move the selected pin up / down
			{
				TArray<UEdGraphPin*> Pins =
					FBAUtils::GetPinsByDirection(SelectedNode, SelectedPin->Direction)
					.FilterByPredicate(IsPinVisibleAsAdvanced);

				if (Pins.Num() > 1)
				{
					int32 PinIndex;
					Pins.Find(SelectedPin, PinIndex);
					if (PinIndex != -1) // we couldn't find the pin index
					{
						int32 NextPinIndex = PinIndex + Y;

						if (NextPinIndex < 0)
						{
							NextPinIndex = Pins.Num() + NextPinIndex;
						}
						else
						{
							if (NextPinIndex >= Pins.Num())
							{
								if (SelectedNode->AdvancedPinDisplay == ENodeAdvancedPins::Hidden)
								{
									ToggleNodeAdvancedDisplay();

									// refresh pins
									Pins = FBAUtils::GetPinsByDirection(SelectedNode, SelectedPin->Direction)
										.FilterByPredicate(IsPinVisibleAsAdvanced);
								}
							}

							NextPinIndex = NextPinIndex % Pins.Num();
						}

						GraphHandler->SetSelectedPin(Pins[NextPinIndex]);
					}
				}
			}
		}
	}
}

void FBANodeActions::OnGetContextMenuActions(const bool bUsePin)
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	UEdGraph* EdGraph = GraphHandler->GetFocusedEdGraph();
	if (EdGraph == nullptr)
	{
		return;
	}

	const UEdGraphSchema* Schema = EdGraph->GetSchema();
	if (Schema == nullptr)
	{
		return;
	}

	TSharedPtr<SGraphEditor> GraphEditor = GraphHandler->GetGraphEditor();
	const FVector2D MenuLocation = FSlateApplication::Get().GetCursorPos();
	const FVector2D SpawnLocation = GraphEditor->GetPasteLocation();

	UEdGraphNode* Node = GraphHandler->GetSelectedNode();

	UEdGraphPin* Pin = bUsePin
		? GraphHandler->GetSelectedPin()
		: nullptr;

	const TArray<UEdGraphPin*> DummyPins;
	GraphHandler->GetGraphPanel()->SummonContextMenu(MenuLocation, SpawnLocation, Node, Pin, DummyPins);
}

void FBANodeActions::ReplaceNodeWith()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	UEdGraphNode* SelectedNode = GraphHandler->GetSelectedNode();
	if (SelectedNode == nullptr || !SelectedNode->CanUserDeleteNode())
	{
		return;
	}

	TSharedPtr<SGraphEditor> GraphEditor = GraphHandler->GetGraphEditor();
	if (!GraphEditor.IsValid())
	{
		return;
	}

	const FVector2D MenuLocation = FSlateApplication::Get().GetCursorPos();
	const FVector2D SpawnLocation(SelectedNode->NodePosX, SelectedNode->NodePosY);

	TSharedPtr<FScopedTransaction> Transaction = MakeShareable(new FScopedTransaction(NSLOCTEXT("UnrealEd", "ReplaceNodeWith", "Replace Node With")));

	FBAGraphActions::OpenContextMenu(MenuLocation, SpawnLocation);

	GraphHandler->SetNodeToReplace(SelectedNode, Transaction);

	FSlateApplication& SlateApp = FSlateApplication::Get();
	if (SlateApp.IsInitialized())
	{
		TSharedPtr<SWindow> Menu = SlateApp.GetActiveTopLevelWindow();
		if (Menu.IsValid())
		{
			if (FBAUtils::GetGraphActionMenu().IsValid())
			{
#if ENGINE_MINOR_VERSION < 22 && ENGINE_MAJOR_VERSION == 4
				Menu->SetOnWindowClosed(FOnWindowClosed::CreateRaw(this, &FBAInputProcessor::OnReplaceNodeMenuClosed));
#else
				Menu->GetOnWindowClosedEvent().AddRaw(this, &FBANodeActions::OnReplaceNodeMenuClosed);
#endif
			}
		}
	}
}

void FBANodeActions::OnReplaceNodeMenuClosed(const TSharedRef<SWindow>& Window)
{
	GetGraphHandler()->ResetSingleNewNodeTransaction();
}

bool FBANodeActions::CanRenameSelectedNode()
{
	if (HasSingleNodeSelected())
	{
		UEdGraphNode* SelectedNode = GetGraphHandler()->GetSelectedNode();
		return SelectedNode->IsA(UK2Node_Variable::StaticClass()) ||
			SelectedNode->IsA(UK2Node_CallFunction::StaticClass()) ||
			SelectedNode->IsA(UK2Node_MacroInstance::StaticClass());
	}

	return false;
}

void FBANodeActions::RenameSelectedNode()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	UEdGraphNode* SelectedNode = GraphHandler->GetSelectedNode();

	FName ItemName;

	if (UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(SelectedNode))
	{
		ItemName = VariableNode->GetVarName();
	}
	else if (UK2Node_CallFunction* FunctionCall = Cast<UK2Node_CallFunction>(SelectedNode))
	{
		ItemName = FunctionCall->FunctionReference.GetMemberName();
	}
	else if (UK2Node_MacroInstance* Macro = Cast<UK2Node_MacroInstance>(SelectedNode))
	{
		ItemName = Macro->GetMacroGraph()->GetFName();
	}

	TSharedPtr<SGraphActionMenu> ActionMenu = FBAUtils::GetGraphActionMenu();
	if (!ActionMenu)
	{
		return;
	}

	if (!ItemName.IsNone())
	{
		ActionMenu->SelectItemByName(ItemName, ESelectInfo::OnKeyPress);
		if (ActionMenu->CanRequestRenameOnActionNode())
		{
			ActionMenu->OnRequestRenameOnActionNode();
		}
	}
}

void FBANodeActions::ToggleNodeAdvancedDisplay() const
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	if (UEdGraphNode* SelectedNode = GraphHandler->GetSelectedNode())
	{
		if (TSharedPtr<SGraphNode> GraphNode = FBAUtils::GetGraphNode(GraphHandler->GetGraphPanel(), SelectedNode))
		{
			if (SelectedNode->AdvancedPinDisplay != ENodeAdvancedPins::NoPins)
			{
				const bool bAdvancedPinsHidden = (SelectedNode->AdvancedPinDisplay == ENodeAdvancedPins::Hidden);
				SelectedNode->AdvancedPinDisplay = bAdvancedPinsHidden ? ENodeAdvancedPins::Shown : ENodeAdvancedPins::Hidden;

				// maybe optimal to call SGraphNode::OnAdvancedViewChanged but the function is protected
				GraphNode->UpdateGraphNode();
			}
		}
	}
}

bool FBANodeActions::CanToggleNodeAdvancedDisplay() const
{
	if (HasSingleNodeSelected())
	{
		UEdGraphNode* SelectedNode = GetGraphHandler()->GetSelectedNode();
		return SelectedNode->AdvancedPinDisplay != ENodeAdvancedPins::NoPins;
	}

	return false;
}

void FBANodeActions::RenameCommentBubble()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	UEdGraphNode* Node = GraphHandler->GetSelectedNode();
	if (Node->SupportsCommentBubble())
	{
		if (auto GraphNode = FBAUtils::GetGraphNode(GraphHandler->GetGraphPanel(), Node))
		{
			if (auto CommentBubble = FBAUtils::GetCommentBubble(GraphNode))
			{
				CommentBubble->OnCommentBubbleToggle(ECheckBoxState::Checked);
				if (TSharedPtr<SWidget> TextBox = FBAUtils::GetChildWidget(CommentBubble, "SMultiLineEditableTextBox"))
				{
					FBAUtils::InteractWithWidget(TextBox);
				}
			}
		}
	}
}

void FBANodeActions::FormatNodes()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	TSet<UEdGraphNode*> SelectedNodes = GraphHandler->GetSelectedNodes();
	TSharedPtr<FScopedTransaction> Transaction = MakeShareable(new FScopedTransaction(NSLOCTEXT("UnrealEd", "FormatNode", "Format Node")));
	for (UEdGraphNode* Node : SelectedNodes)
	{
		if (FBAUtils::IsGraphNode(Node))
		{
			GraphHandler->AddPendingFormatNodes(Node, Transaction);
		}
	}
}

void FBANodeActions::FormatNodesSelectively()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	TSet<UEdGraphNode*> SelectedNodes = GraphHandler->GetSelectedNodes();
	TSharedPtr<FScopedTransaction> Transaction = MakeShareable(new FScopedTransaction(NSLOCTEXT("UnrealEd", "FormatOnlySelectedNodes", "Format Only Selected Nodes")));

	if (SelectedNodes.Num() == 1)
	{
		UEdGraphNode* SelectedNode = SelectedNodes.Array()[0];

		// get the graph direction
		EEdGraphPinDirection GraphDirection = EGPD_Output;
		if (FBAFormatterSettings* FormatterSettings = UBASettings::FindFormatterSettings(SelectedNode->GetGraph()))
		{
			GraphDirection = FormatterSettings->FormatterDirection;
		}

		// can we assume pure nodes should always expand in the input direction?
		const EEdGraphPinDirection Direction = FBAUtils::IsNodeImpure(SelectedNode) ? GraphDirection : EGPD_Input;

		SelectedNodes = FBAUtils::GetNodeTree(SelectedNode, Direction, true);
	}

	for (UEdGraphNode* Node : SelectedNodes)
	{
		if (FBAUtils::IsGraphNode(Node))
		{
			FEdGraphFormatterParameters FormatterParameters;
			FormatterParameters.NodesToFormat.SetArray(SelectedNodes.Array());
			GraphHandler->AddPendingFormatNodes(Node, Transaction, FormatterParameters);
		}
	}
}

void FBANodeActions::FormatNodesWithHelixing()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	TSet<UEdGraphNode*> SelectedNodes = GraphHandler->GetSelectedNodes();
	TSharedPtr<FScopedTransaction> Transaction = MakeShareable(new FScopedTransaction(NSLOCTEXT("UnrealEd", "FormatNodeHelixing", "Format Node with Helixing")));
	for (UEdGraphNode* Node : SelectedNodes)
	{
		if (FBAUtils::IsGraphNode(Node))
		{
			FEdGraphFormatterParameters FormatterParameters;
			FormatterParameters.OverrideFormattingStyle = MakeShareable(new EBAParameterFormattingStyle(EBAParameterFormattingStyle::Helixing));
			GraphHandler->AddPendingFormatNodes(Node, Transaction, FormatterParameters);
		}
	}
}

void FBANodeActions::FormatNodesWithLHS()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	TSet<UEdGraphNode*> SelectedNodes = GraphHandler->GetSelectedNodes();
	TSharedPtr<FScopedTransaction> Transaction = MakeShareable(new FScopedTransaction(NSLOCTEXT("UnrealEd", "FormatNodeLHS", "Format Node with LHS")));
	for (UEdGraphNode* Node : SelectedNodes)
	{
		if (FBAUtils::IsGraphNode(Node))
		{
			FEdGraphFormatterParameters FormatterParameters;
			FormatterParameters.OverrideFormattingStyle = MakeShareable(new EBAParameterFormattingStyle(EBAParameterFormattingStyle::LeftSide));
			GraphHandler->AddPendingFormatNodes(Node, Transaction, FormatterParameters);
		}
	}
}

void FBANodeActions::LinkNodesBetweenWires()
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

	FPinLink HoveredWire = FBAUtils::GetHoveredPinLink(GraphHandler->GetGraphPanel());
	UEdGraphPin* PinForHoveredWire = HoveredWire.From;
	if (!PinForHoveredWire)
	{
		return;
	}

	TArray<UEdGraphNode*> SelectedNodes = GraphHandler->GetSelectedNodes().Array();

	// if we have exec nodes selected, then we should only consider linking our exec nodes and ignore the parameter nodes
	const bool bHasExecNodes = SelectedNodes.ContainsByPredicate(FBAUtils::IsNodeImpure);
	if (bHasExecNodes)
	{
		SelectedNodes.RemoveAll(FBAUtils::IsNodePure);
	}

	if (SelectedNodes.Num() == 0)
	{
		return;
	}

	const auto LeftMostSort = [](const UEdGraphNode& NodeA, const UEdGraphNode& NodeB)
	{
		return NodeA.NodePosX < NodeB.NodePosX;
	};
	SelectedNodes.Sort(LeftMostSort);

	const auto IsSelected = [&SelectedNodes](UEdGraphNode* Node)
	{
		return SelectedNodes.Contains(Node);
	};

	UEdGraphNode* LeftMostNode =
		FBAUtils::GetTopMostWithFilter(SelectedNodes[0], EGPD_Input, IsSelected);

	UEdGraphNode* RightMostNode =
		FBAUtils::GetTopMostWithFilter(SelectedNodes[0], EGPD_Output, IsSelected);

	TSharedPtr<FScopedTransaction> Transaction =
		MakeShareable(
			new FScopedTransaction(
				NSLOCTEXT("UnrealEd", "LinkNodesBetweenWires", "Link Nodes Between Wires")));

	UEdGraphNode* First = PinForHoveredWire->Direction == EGPD_Output
		? LeftMostNode
		: RightMostNode;

	bool bCancelTransaction = true;

	TArray<FPinLink> PendingLinks;
	PendingLinks.Reserve(2);

	for (UEdGraphPin* Pin : First->Pins)
	{
		if (FBAUtils::CanConnectPins(PinForHoveredWire, Pin, true, false, false))
		{
			PendingLinks.Add(FPinLink(Pin, PinForHoveredWire));
			break;
		}
	}

	UEdGraphPin* ConnectedPin = HoveredWire.To;

	if (!ConnectedPin && PinForHoveredWire->LinkedTo.Num() > 0)
	{
		ConnectedPin = PinForHoveredWire->LinkedTo[0];
	}

	if (ConnectedPin != nullptr)
	{
		UEdGraphNode* ConnectedNode =
			PinForHoveredWire->Direction == EGPD_Output ? RightMostNode : LeftMostNode;

		for (UEdGraphPin* Pin : ConnectedNode->Pins)
		{
			if (FBAUtils::CanConnectPins(ConnectedPin, Pin, true, false, false))
			{
				PendingLinks.Add(FPinLink(Pin, ConnectedPin));
				break;
			}
		}
	}

	FEdGraphFormatterParameters FormatterParams;
	if (UBASettings::GetFormatterSettings(Graph).GetAutoFormatting() == EBAAutoFormatting::FormatSingleConnected)
	{
		FormatterParams.NodesToFormat.GetNodesWeak().Append(SelectedNodes);
		FormatterParams.NodesToFormat.GetNodesWeak().Add(PinForHoveredWire->GetOwningNode());
	}

	for (FPinLink& Link : PendingLinks)
	{
		const bool bMadeLink = FBAUtils::TryCreateConnection(Link.From, Link.To, EBABreakMethod::Default);
		if (bMadeLink)
		{
			if (UBASettings::GetFormatterSettings(Graph).GetAutoFormatting() != EBAAutoFormatting::Never)
			{
				GraphHandler->AddPendingFormatNodes(Link.GetFromNode(), Transaction, FormatterParams);
				GraphHandler->AddPendingFormatNodes(Link.GetToNode(), Transaction, FormatterParams);
			}

			bCancelTransaction = false;
		}
	}

	if (bCancelTransaction)
	{
		Transaction->Cancel();
	}
}

void FBANodeActions::DisconnectExecutionOfSelectedNode()
{
	TArray<UEdGraphNode*> SelectedNodes = GetGraphHandler()->GetSelectedNodes().Array();
	DisconnectExecutionOfNodes(SelectedNodes);
}

void FBANodeActions::SwapNodeInDirection(EEdGraphPinDirection Direction)
{
	// PinA: Linked to pin in direction
	// PinB: Linked to pin opposite
	// PinC: Linked to PinA's Node in direction

	struct FDebugLocal
	{
		static void DrawPin(TSharedPtr<FBAGraphHandler> GraphHandler, UEdGraphPin* Pin, const FString& Text)
		{
			if (!UBASettings::HasDebugSetting("dSwapNodes"))
			{
				return;
			}

			if (!Pin)
			{
				UE_LOG(LogBlueprintAssist, Warning, TEXT("Pin %s is null"), *Text);
				return;
			}

			if (TSharedPtr<SGraphPin> GraphPin = FBAUtils::GetGraphPin(GraphHandler->GetGraphPanel(), Pin))
			{
				FBAGraphOverlayTextParams Params;
				Params.Text = FText::FromString(Text);
				Params.Widget = GraphPin;
				Params.WidgetBounds = FBAUtils::GetPinBounds(GraphPin);
				GraphHandler->GetGraphOverlay()->DrawTextOverWidget(Params);
			}
			// else
			// {
			// 	UE_LOG(LogTemp, Error, TEXT("Graph Pin %s | %s is null"), *FBAUtils::GetPinName(Pin), *Text);
			// }
		}
	};

	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	bool bRunConnections = true;

	if (UBASettings::HasDebugSetting("dSwapNodes"))
	{
		// when debugging, only run connections on the second call of this function
		if (!GraphHandler->GetGraphOverlay()->IsDrawingTextOverWidgets())
		{
			bRunConnections = false;
		}
	}

	auto GraphHandlerCapture = GraphHandler;
	const auto TopMostPinSort = [GraphHandlerCapture](UEdGraphPin& PinA, UEdGraphPin& PinB)
	{
		return GraphHandlerCapture->GetPinY(&PinA) < GraphHandlerCapture->GetPinY(&PinB);
	};

	TArray<UEdGraphNode*> SelectedNodes = GraphHandler->GetSelectedNodes().Array();

	if (SelectedNodes.Num() == 0)
	{
		return;
	}

	const UEdGraphSchema* Schema = GraphHandler->GetFocusedEdGraph()->GetSchema();
	if (!Schema)
	{
		return;
	}

	const auto IsSelectedAndPure = [&SelectedNodes](UEdGraphNode* Node)
	{
		return FBAUtils::IsNodeImpure(Node) && SelectedNodes.Contains(Node) && FBAUtils::HasExecInOut(Node);
	};

	UEdGraphNode* LeftMostNode = FBAUtils::GetTopMostWithFilter(SelectedNodes[0], EGPD_Input, IsSelectedAndPure);

	UEdGraphNode* RightMostNode = FBAUtils::GetTopMostWithFilter(SelectedNodes[0], EGPD_Output, IsSelectedAndPure);

	UEdGraphNode* NodeInDirection = Direction == EGPD_Input ? LeftMostNode : RightMostNode;
	UEdGraphNode* NodeOpposite = Direction == EGPD_Input ? RightMostNode : LeftMostNode;

	// Process NodeInDirection
	TArray<UEdGraphPin*> LinkedPins =
		FBAUtils::GetLinkedPins(NodeInDirection, Direction).FilterByPredicate(FBAUtils::IsExecPin);

	if (LinkedPins.Num() == 0)
	{
		return;
	}

	// keep track of these pins
	TMap<FBANodePinHandle, bool> InitialLoopingState;
	for (UEdGraphNode* SelectedNode : SelectedNodes)
	{
		TArray<UEdGraphPin*> ExecLinks = FBAUtils::GetLinkedPins(SelectedNode, Direction).FilterByPredicate(FBAUtils::IsExecPin);
		for (UEdGraphPin* Pin : ExecLinks)
		{
			// should this be checking execution in the graph direction? (can you even have looping nodes on a non-bp graph?)
			UEdGraphPin* LinkedTo = Pin->LinkedTo[0];
			bool bNewLoopingState = FBAUtils::DoesNodeHaveExecutionTo(LinkedTo->GetOwningNode(), Pin->GetOwningNode(), EGPD_Output);
			InitialLoopingState.Add(FBANodePinHandle(Pin), bNewLoopingState);
		}
	}

	FBANodePinHandle PinInDirection(LinkedPins[0]);
	if (PinInDirection.GetPin()->LinkedTo.Num() == 0)
	{
		return;
	}

	// Process NodeOpposite
	const auto OppositeDirection = UEdGraphPin::GetComplementaryDirection(Direction);
	TArray<UEdGraphPin*> PinsOpposite = FBAUtils::GetPinsByDirection(NodeOpposite, OppositeDirection).FilterByPredicate(FBAUtils::IsExecPin);
	if (PinsOpposite.Num() == 0)
	{
		return;
	}

	FBANodePinHandle PinOpposite = PinsOpposite[0];

	FDebugLocal::DrawPin(GraphHandler, PinInDirection.GetPin(), "PinInDir");
	FDebugLocal::DrawPin(GraphHandler, PinOpposite.GetPin(), "PinOpposite");
	// UE_LOG(LogBlueprintAssist, Warning, TEXT("PinInDirection %s"), *FBAUtils::GetPinName(PinInDirection.GetPin(), true));
	// UE_LOG(LogBlueprintAssist, Warning, TEXT("PinOpposite %s"), *FBAUtils::GetPinName(PinOpposite.GetPin(), true));

	// Process NodeA
	auto PinInDLinkedTo = FBAUtils::GetPinLinkedToIgnoringKnots(PinInDirection.GetPin());
	if (!PinInDLinkedTo.Num())
	{
		// TODO should we handle this case (where we are linked to a knot node with no links)?
		return;
	}

	PinInDLinkedTo.StableSort(TopMostPinSort);
	// for (UEdGraphPin* InDLinkedTo : PinInDLinkedTo)
	// {
	// 	UE_LOG(LogBlueprintAssist, Warning, TEXT("\tPinInD LinkedTo %s"), *FBAUtils::GetPinName(InDLinkedTo, true));
	// }
	FBANodePinHandle PinA(PinInDLinkedTo[0]);

	FDebugLocal::DrawPin(GraphHandler, PinA.GetPin(), "PinA");
	// UE_LOG(LogBlueprintAssist, Warning, TEXT("PinA %s"), *FBAUtils::GetPinName(PinA.GetPin(), true));

	UEdGraphNode* NodeA = PinA.GetNode();

	if (!FBAUtils::HasExecInOut(NodeA))
	{
		return;
	}

	// For the linked pins on NodeA, do not create any looping pins
	{
		TArray<UEdGraphPin*> NodeA_LinkedPins = FBAUtils::GetLinkedPins(NodeA, PinA->Direction).FilterByPredicate(FBAUtils::IsExecPin);
		for (UEdGraphPin* Pin : NodeA_LinkedPins)
		{
			UEdGraphPin* LinkedPin = Pin->LinkedTo[0];

			// should this be checking execution in the graph direction? (can you even have looping nodes on a non-bp graph?)
			bool bNewLoopingState = FBAUtils::DoesNodeHaveExecutionTo(LinkedPin->GetOwningNode(), Pin->GetOwningNode(), EGPD_Output);
			InitialLoopingState.Add(FBANodePinHandle(Pin), bNewLoopingState);
		}
	}

	TArray<FPinLink> PendingConnections;
	TArray<FPinLink> PendingDisconnects;

	const FText TransactionDesc = Direction == EGPD_Output ? INVTEXT("Swap Node(s) Right") : INVTEXT("Swap Node(s) Left"); 
	TSharedPtr<FScopedTransaction> Transaction = MakeShareable(new FScopedTransaction(TransactionDesc));

	FBANodePinHandle PinAInDirection;
	{
		TArray<UEdGraphPin*> PinsAInDirection = FBAUtils::GetPinsByDirection(NodeA, Direction).FilterByPredicate(FBAUtils::IsExecPin);
		if (PinsAInDirection.Num() > 0)
		{
			PinAInDirection = PinsAInDirection[0];
			FDebugLocal::DrawPin(GraphHandler, PinAInDirection.GetPin(), "PinAInDirection");
			// UE_LOG(LogBlueprintAssist, Warning, TEXT("PinAInDirection %s"), *FBAUtils::GetPinName(PinAInDirection, true));

			PendingConnections.Add(FPinLink(PinAInDirection.GetPin(), PinOpposite.GetPin()));

			// Optional PinB
			if (PinAInDirection->LinkedTo.Num() > 0)
			{
				PinAInDirection->LinkedTo.StableSort(TopMostPinSort);
				for (int i = 0; i < PinAInDirection->LinkedTo.Num(); ++i)
				{
					UEdGraphPin* PinB = PinAInDirection->LinkedTo[i];
					if (PinB->GetOwningNode() != PinInDirection->GetOwningNode())
					{
						FDebugLocal::DrawPin(GraphHandler, PinB, FString::Printf(TEXT("PinB_%d"), i));
						// UE_LOG(LogBlueprintAssist, Warning, TEXT("PinB %s"), *FBAUtils::GetPinName(PinB, true));
						PendingConnections.Add(FPinLink(PinB, PinInDirection.GetPin()));
						PendingDisconnects.Add(FPinLink(PinB, PinAInDirection.GetPin()));
					}
				}
			}
		}
	}

	{
		// Optional PinC
		TArray<UEdGraphPin*> LinkedToPinOpposite = PinOpposite.GetPin()->LinkedTo;
		if (LinkedToPinOpposite.Num() > 0)
		{
			LinkedToPinOpposite.StableSort(TopMostPinSort);

			for (int i = 0; i < LinkedToPinOpposite.Num(); ++i)
			{
				UEdGraphPin* PinC = PinOpposite.GetPin()->LinkedTo[i];
				if (PinC->GetOwningNode() != PinA->GetOwningNode())
				{
					FDebugLocal::DrawPin(GraphHandler, PinC, FString::Printf(TEXT("PinC_%d"), i));
					// UE_LOG(LogBlueprintAssist, Warning, TEXT("PinC %s"), *FBAUtils::GetPinName(PinC, true));

					PendingConnections.Add(FPinLink(PinC, PinA.GetPin()));
					PendingDisconnects.Add(FPinLink(PinC, PinOpposite.GetPin()));
				}
			}
		}
	}

	// Get PinInDirection links and link them to PinAInDirection
	for (UEdGraphPin* Pin : PinInDirection->LinkedTo)
	{
		if (Pin->GetOwningNode() != NodeA)
		{
			PendingDisconnects.Add(FPinLink(PinInDirection.GetPin(), Pin));

			if (PinAInDirection.GetPin())
			{
				PendingConnections.Add(FPinLink(PinAInDirection.GetPin(), Pin));
			}
		}
	}

	// Get PinA links and link them to PinOpposite
	for (UEdGraphPin* Pin : PinA->LinkedTo)
	{
		if (!SelectedNodes.Contains(Pin->GetOwningNode()))
		{
			PendingDisconnects.Add(FPinLink(PinA.GetPin(), Pin));

			if (PinOpposite.IsValid())
			{
				PendingConnections.Add(FPinLink(PinOpposite.GetPin(), Pin));
			}
		}
	}

	if (PendingConnections.Num() == 0 || !bRunConnections)
	{
		Transaction->Cancel();
		return;
	}

	GraphHandler->GetGraphOverlay()->ClearAllTextOverWidgets();

	PendingDisconnects.Add(FPinLink(PinInDirection.GetPin(), PinA.GetPin()));

	for (FPinLink& Link : PendingDisconnects)
	{
		if (!Link.HasBothPins())
		{
			continue;
		}

		// UE_LOG(LogBlueprintAssist, Log, TEXT("Breaking %s"), *Link.ToString());
		Schema->BreakSinglePinLink(Link.GetFromPin(), Link.GetToPin());
	}

	for (FPinLink& Link : PendingConnections)
	{
		if (!Link.HasBothPins())
		{
			continue;
		}

		// UE_LOG(LogBlueprintAssist, Log, TEXT("Linking %s (%s)"), *Link.ToString(), *Response.Message.ToString());
		Schema->TryCreateConnection(Link.GetFromPin(), Link.GetToPin());
	}

	auto AutoFormatting = UBASettings::GetFormatterSettings(GraphHandler->GetFocusedEdGraph()).GetAutoFormatting();

	if (AutoFormatting != EBAAutoFormatting::Never)
	{
		FEdGraphFormatterParameters FormatterParams;
		if (AutoFormatting == EBAAutoFormatting::FormatSingleConnected)
		{
			FormatterParams.NodesToFormat.GetNodesWeak().Append(SelectedNodes);
			FormatterParams.NodesToFormat.GetNodesWeak().Add(PinInDirection.GetNode());
		}

		GraphHandler->AddPendingFormatNodes(NodeInDirection, Transaction, FormatterParams);
	}

	UEdGraphNode* SelectedNodeToUse = Direction == EGPD_Output ? NodeOpposite : NodeInDirection;

	const int32 PinPosY_Selected = GraphHandler->GetPinY(PinInDirection.GetPin());
	const int32 PinPosY_A = GraphHandler->GetPinY(PinAInDirection.GetPin());

	int32 DeltaX_Selected = NodeA->NodePosX - SelectedNodeToUse->NodePosX;
	int32 DeltaY_Selected = PinPosY_A - PinPosY_Selected;

	int32 DeltaX_A = SelectedNodeToUse->NodePosX - NodeA->NodePosX;
	int32 DeltaY_A = PinPosY_Selected - PinPosY_A;

	// Selected nodes: move node and parameters
	for (UEdGraphNode* SelectedNode : SelectedNodes)
	{
		TArray<UEdGraphNode*> NodeAndParams = FBAUtils::GetNodeAndParameters(SelectedNode);
		for (UEdGraphNode* Node : NodeAndParams)
		{
			Node->Modify();
			Node->NodePosX += DeltaX_Selected;
			Node->NodePosY += DeltaY_Selected;
		}
	}

	// NodeA: move node and parameters
	for (UEdGraphNode* Node : FBAUtils::GetNodeAndParameters(NodeA))
	{
		Node->Modify();
		Node->NodePosX += DeltaX_A;
		Node->NodePosY += DeltaY_A;
	}

	if (UBASettings_Advanced::Get().bRemoveLoopingCausedBySwapping)
	{
		// TODO the additional transaction does not work if auto-formatting is enabled since the previous transaction still exists in the graph handler
		Transaction.Reset();
		Transaction = MakeShareable(new FScopedTransaction(INVTEXT("Disconnect Looping Swap Nodes")));

		for (TTuple<FBANodePinHandle, bool>& LoopingState : InitialLoopingState)
		{
			bool bOldLoopingState = LoopingState.Value;
			UEdGraphPin* Pin = LoopingState.Key.GetPin();
			if (Pin->LinkedTo.Num())
			{
				UEdGraphPin* LinkedPin = Pin->LinkedTo[0];
				bool bNewLoopingState = FBAUtils::DoesNodeHaveExecutionTo(LinkedPin->GetOwningNode(), Pin->GetOwningNode(), EGPD_Output);

				// UE_LOG(LogTemp, Warning, TEXT("%s: %d -> %d"), *FBAUtils::GetPinName(Pin, true), bOldLoopingState, bNewLoopingState);

				// if swapping our connection caused the node to become looping but was not before, then we should disconnect this pin
				if (bNewLoopingState && !bOldLoopingState)
				{
					Pin->Modify();
					Schema->BreakSinglePinLink(Pin, LinkedPin);
				}
			}
		}
	}
}

void FBANodeActions::DeleteAndLink()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	const auto& ShouldDeleteNode = [](UEdGraphNode* Node)
	{
		return Node->CanUserDeleteNode();
	};

	TArray<UEdGraphNode*> NodesToDelete = GraphHandler->GetSelectedNodes().Array().FilterByPredicate(ShouldDeleteNode);
	if (NodesToDelete.Num() > 0)
	{
		const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "DeleteAndLink", "Delete and link"));

		DisconnectExecutionOfNodes(NodesToDelete);
		for (int i = NodesToDelete.Num() - 1; i >= 0; --i)
		{
			FBAUtils::SafeDelete(GraphHandler, NodesToDelete[i]);
		}
	}
}

void FBANodeActions::CutAndLink()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	const auto& ShouldCutNode = [](UEdGraphNode* Node)
	{
		return ((Node != nullptr) && Node->CanDuplicateNode() && Node->CanUserDeleteNode());
	};

	TArray<UEdGraphNode*> NodesToCut = GraphHandler->GetSelectedNodes(true).Array().FilterByPredicate(ShouldCutNode);
	if (NodesToCut.Num() > 0)
	{
		const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "CutAndLink", "Cut and link"));

		DisconnectExecutionOfNodes(NodesToCut);

		// Prepare nodes for copying
		TSet<UObject*> NodesToCopy;
		for (int i = NodesToCut.Num() - 1; i >= 0; --i)
		{
			NodesToCut[i]->PrepareForCopying();
			NodesToCopy.Add(NodesToCut[i]);
		}
		
		// Copy to clipboard
		FString ExportedText;
		FEdGraphUtilities::ExportNodesToText(NodesToCopy, ExportedText);
		FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
		
		// Delete nodes
		for (int i = NodesToCut.Num() - 1; i >= 0; --i)
		{
			FBAUtils::SafeDelete(GraphHandler, NodesToCut[i]);
		}
	}
}

bool FBANodeActions::CanToggleNodes()
{
	return HasMultipleNodesSelected() && GetGraphHandler()->GetBlueprint() != nullptr;
}

// TODO: figure out a nice way to make this work for non-bp graphs as well
void FBANodeActions::ToggleNodes()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	TSet<UEdGraphNode*> SelectedNodes = GraphHandler->GetSelectedNodes();

	auto OnlyPureNodes = [](UEdGraphNode* Node)
	{
		return !FBAUtils::IsKnotNode(Node) && !FBAUtils::IsCommentNode(Node) && FBAUtils::IsNodeImpure(Node);
	};

	TArray<UEdGraphNode*> FilteredNodes = SelectedNodes.Array().FilterByPredicate(OnlyPureNodes);

	if (FilteredNodes.Num() == 0)
	{
		return;
	}

	FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "ToggleNodes", "Toggle Nodes"));

	bool bAllNodesDisabled = true;
	for (UEdGraphNode* Node : FilteredNodes)
	{
		if (Node->GetDesiredEnabledState() != ENodeEnabledState::Disabled)
		{
			bAllNodesDisabled = false;
			break;
		}
	}

	for (UEdGraphNode* Node : FilteredNodes)
	{
		if (bAllNodesDisabled) // Set nodes to their default state
		{
			ENodeEnabledState DefaultEnabledState = ENodeEnabledState::Enabled;

			if (UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(Node))
			{
				if (UFunction* Function = CallFunctionNode->GetTargetFunction())
				{
					if (Function->HasMetaData(FBlueprintMetadata::MD_DevelopmentOnly))
					{
						DefaultEnabledState = ENodeEnabledState::DevelopmentOnly;
					}
				}
			}

			Node->Modify();
			Node->SetEnabledState(DefaultEnabledState);
		}
		else // Set all nodes to disabled
		{
			if (Node->GetDesiredEnabledState() != ENodeEnabledState::Disabled)
			{
				Node->Modify();
				Node->SetEnabledState(ENodeEnabledState::Disabled);
			}
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GraphHandler->GetBlueprint());
}

void FBANodeActions::ToggleLockNodes()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	GraphHandler->ToggleLockNodes(GraphHandler->GetSelectedNodes());
}

void FBANodeActions::GroupNodes()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	GraphHandler->GroupNodes(GraphHandler->GetSelectedNodes());
}

void FBANodeActions::UngroupNodes()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	GraphHandler->UngroupNodes(GraphHandler->GetSelectedNodes());
}

void FBANodeActions::MergeNodes()
{
	using namespace MergeNodesTypes;

	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	TArray<FMergeNodeTree> NodeTrees = FMergeNodeTree::MakeNodeTreesFromSelection(GraphHandler);

	TSet<UEdGraphNode*> SelectedNodes = GraphHandler->GetSelectedNodes(false);

	FScopedTransaction Transaction(INVTEXT("Merge nodes"));
	for (UEdGraphNode* SelectedNode : SelectedNodes)
	{
		SelectedNode->Modify();
	}

	// GraphHandler->GetGraphOverlay()->ClearAllTextOverWidgets();

	TMap<int, TMap<FString, FMergePinData>> PinDataMapping;
	TArray<FPinLink> PendingBreakLinks;

	for (const FMergeNodeTree& Tree : NodeTrees)
	{
		for (int i = 0; i < Tree.NodeTreeMap.Num(); ++i)
		{
			UEdGraphNode* Node = Tree.NodeTreeMap[i];

			// debug draw
			{
				// FBAGraphOverlayTextParams Params;
				// TSharedPtr<SGraphNode> GraphNode = FBAUtils::GetGraphNode(GraphHandler->GetGraphPanel(), Node);
				// Params.Widget = GraphNode;
				// Params.WidgetBounds = FBAUtils::GetNodeBounds(GraphNode);
				// Params.Text = FText::FromString(FString::FromInt(i));
				// GraphHandler->GetGraphOverlay()->DrawTextOverWidget(Params);
			}

			for (UEdGraphPin* Pin : Node->Pins)
			{
				FString PinName = FBAUtils::GetPinName(Pin);

				// gather all pins which are linked to unselected nodes

				for (int j = Pin->LinkedTo.Num() - 1; j >= 0; --j)
				{
					UEdGraphPin* LinkedTo = Pin->LinkedTo[j];

					// update the pin data
					if (!SelectedNodes.Contains(LinkedTo->GetOwningNode()))
					{
						FMergePinData& PinData = PinDataMapping.FindOrAdd(i).FindOrAdd(PinName);
						PinData.PendingLinks.Add(FBAGraphPinHandle(LinkedTo));

						PendingBreakLinks.Add(FPinLink(Pin, LinkedTo));

						// UE_LOG(LogTemp, Warning, TEXT("Added (%d %s) %s > %s"),
						// 	j, *PinName,
						// 	*FBAUtils::GetPinName(Pin, true),
						// 	*FBAUtils::GetPinName(LinkedTo, true)
						// );
					}
				}
			}
		}
	}

	for (FPinLink& PinLink : PendingBreakLinks)
	{
		if (PinLink.HasBothPins())
		{
			PinLink.GetFromPin()->BreakLinkTo(PinLink.GetToPin());
		}
	}

	FMergeNodeTree& MainNodeTree = NodeTrees[0];
	for (int i = 0; i < MainNodeTree.NodeTreeMap.Num(); ++i)
	{
		if (TMap<FString, FMergePinData>* Mapping = PinDataMapping.Find(i))
		{
			if (UEdGraphNode* Node = MainNodeTree.NodeTreeMap[i])
			{
				for (UEdGraphPin* Pin : Node->Pins)
				{
					FString PinName = FBAUtils::GetPinName(Pin);
					if (FMergePinData* PinData = Mapping->Find(PinName))
					{
						for (FBAGraphPinHandle& PendingLink : PinData->PendingLinks)
						{
							// UE_LOG(LogTemp, Warning, TEXT("Pending link %s > %s"), *PinName, *FBAUtils::GetPinName(PendingLink.GetPin()));
							if (UEdGraphPin* TargetPin = PendingLink.GetPin())
							{
								FBAUtils::TryCreateConnection(Pin, TargetPin, EBABreakMethod::Default, false);
							}
						}
					}
				}
			}
		}
	}

	for (int i = 1; i < NodeTrees.Num(); ++i)
	{
		for (UEdGraphNode* Node : NodeTrees[i].NodeTreeMap)
		{
			FBAUtils::SafeDelete(GraphHandler, Node);
		}
	}
}

bool FBANodeActions::CanMergeNodes() const
{
	using namespace MergeNodesTypes;

	if (!HasMultipleNodesSelected())
	{
		return false;
	}

	TArray<FMergeNodeTree> NodeTrees = FMergeNodeTree::MakeNodeTreesFromSelection(GetGraphHandler());
	TArray<FString> NodeNames;

	// can only merge nodes if we have multiple node trees selected
	if (NodeTrees.Num() < 2)
	{
		return false;
	}

	// we need to check if the selected node trees all have the same nodes (with the same links)
	// check by name of the node
	const FMergeNodeTree& MainNodeTree = NodeTrees[0];

	// fail if the node types are different
	for (int i = 0; i < MainNodeTree.NodeTreeMap.Num(); ++i)
	{
		FString NodeName = FBAUtils::GetNodeName(MainNodeTree.NodeTreeMap[i]);

		// check against the other node trees
		for (int j = 1; j < NodeTrees.Num(); ++j)
		{
			FMergeNodeTree& OtherNodeTree = NodeTrees[j];
			if (!OtherNodeTree.NodeTreeMap.IsValidIndex(i))
			{
				return false;
			}

			FString OtherNodeName = FBAUtils::GetNodeName(OtherNodeTree.NodeTreeMap[i]);
			if (NodeName != OtherNodeName)
			{
				return false;
			}
		}
	}

	return true;
}

void FBANodeActions::RefreshNodeSizes()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	TSet<UEdGraphNode*> SelectedNodes = GraphHandler->GetSelectedNodes(true);

	auto Graph = GraphHandler->GetFocusedEdGraph();

	auto AutoFormatting = UBASettings::GetFormatterSettings(Graph).GetAutoFormatting();

	if (SelectedNodes.Num() > 0)
	{
		TSharedPtr<FScopedTransaction> Transaction = MakeShareable(new FScopedTransaction(NSLOCTEXT("UnrealEd", "RefreshNodeSize", "Refresh Node Size")));

		FEdGraphFormatterParameters FormatterParams;

		if (AutoFormatting == EBAAutoFormatting::FormatSingleConnected)
		{
			TSet<UEdGraphNode*> NodeSet;
			for (UEdGraphNode* Node : SelectedNodes)
			{
				if (FBAUtils::IsGraphNode(Node))
				{
					NodeSet.Add(Node);
					if (UEdGraphNode* Linked = FBAUtils::GetFirstLinkedNodePreferringInput(Node))
					{
						NodeSet.Add(Linked);
					}
				}
			}

			FormatterParams.NodesToFormat.SetArray(NodeSet.Array());
		}

		for (UEdGraphNode* Node : SelectedNodes)
		{
			GraphHandler->RefreshNodeSize(Node);

			if (AutoFormatting != EBAAutoFormatting::Never)
			{
				GraphHandler->AddPendingFormatNodes(Node, Transaction, FormatterParams);
			}
			else
			{
				Transaction->Cancel();
			}
		}
	}
}

void FBANodeActions::ExpandSelection()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler->HasValidGraphReferences())
	{
		return;
	}

	TSharedPtr<SGraphNode> HoveredNode = FBAUtils::GetHoveredGraphNode(GraphHandler->GetGraphPanel());
	if (!HoveredNode)
		return;

	UEdGraph* Graph = GraphHandler->GetFocusedEdGraph();

	TSet<UEdGraphNode*> NodeTree = FBAUtils::GetNodeTree(HoveredNode->GetNodeObj());
	TSet<const UEdGraphNode*> SelectionSet;
	Algo::Transform(NodeTree, SelectionSet, [](UEdGraphNode* Node){ return Node; });

	// TODO actually expand selection instead of selecting the entire node tree
	Graph->SelectNodeSet(SelectionSet);
}

void FBANodeActions::ExpandNodeTreeInDirection(EEdGraphPinDirection Direction)
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler->HasValidGraphReferences())
	{
		return;
	}

	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();
	UEdGraphNode* HoveredNode = GetSingleHoveredOrSelectedNode();
	if (!HoveredNode)
	{
		return;
	}

	if (FBAUtils::IsNodeImpure(HoveredNode))
	{
		TSet<UEdGraphNode*> OriginalSelection = GraphHandler->GetSelectedNodes(true);

		// expand execution nodes
		TSet<UEdGraphNode*> NewExecSelection = OriginalSelection;
		NewExecSelection.Append(FBAUtils::GetExecTree(HoveredNode, Direction));

		TSet<UEdGraphNode*> NewSelection = NewExecSelection;

		// add all parameter nodes
		for (UEdGraphNode* Node : NewExecSelection)
		{
			NewSelection.Append(FBAUtils::GetParameterTree(Node));
		}

		// TODO look into why subtract doesn't work here
		// subtract from selection if control is down
		// if (FSlateApplication::Get().GetModifierKeys().IsControlDown())
		// {
		// 	TSet<UEdGraphNode*> Subtracted = OriginalSelection.Difference(NewSelection);
		// 	GraphHandler->SelectNodes(Subtracted);
		// }
		{
			GraphHandler->SelectNodes(NewSelection);
		}
	}
	else
	{
		TSet<UEdGraphNode*> OriginalSelection = GraphHandler->GetSelectedNodes(true);

		// expand execution nodes
		TSet<UEdGraphNode*> NewExecSelection = OriginalSelection;
		NewExecSelection.Append(FBAUtils::GetParameterTree(HoveredNode, Direction, true));

		TSet<UEdGraphNode*> NewSelection = NewExecSelection;
		GraphHandler->SelectNodes(NewSelection);
	}
}
