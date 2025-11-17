// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistUtils.h"

#include "BlueprintAssistGlobals.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistModule.h"
#include "BlueprintAssistSettings.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistSettings_EditorFeatures.h"
#include "BlueprintAssistStats.h"
#include "BlueprintAssistTabHandler.h"
#include "BlueprintEditor.h"
#include "EdGraphUtilities.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_InputAction.h"
#include "K2Node_InputActionEvent.h"
#include "K2Node_InputAxisEvent.h"
#include "K2Node_InputAxisKeyEvent.h"
#include "K2Node_InputKey.h"
#include "K2Node_InputKeyEvent.h"
#include "K2Node_InputTouch.h"
#include "K2Node_InputTouchEvent.h"
#include "K2Node_InputVectorAxisEvent.h"
#include "K2Node_Knot.h"
#include "K2Node_Timeline.h"
#include "K2Node_Variable.h"
#include "LevelEditor.h"
#include "MaterialGraphNode_Knot.h"
#include "SCommentBubble.h"
#include "SGraphActionMenu.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BlueprintAssistObjects/BARootObject.h"
#include "BlueprintAssistFormatters/BlueprintAssistCommentHandler.h"
#include "BlueprintAssistFormatters/GraphFormatterTypes.h"
#include "EdGraph/EdGraphSchema.h"
#include "Editor/BlueprintGraph/Classes/EdGraphSchema_K2.h"
#include "Editor/BlueprintGraph/Classes/K2Node.h"
#include "Editor/GraphEditor/Public/SGraphNode.h"
#include "Editor/GraphEditor/Public/SGraphPanel.h"
#include "Editor/GraphEditor/Public/SNodePanel.h"
#include "Editor/UnrealEd/Public/EdGraphNode_Comment.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/SlateRect.h"
#include "Misc/HotReloadInterface.h"
#include "Runtime/Core/Public/Containers/Queue.h"
#include "Runtime/Engine/Classes/EdGraph/EdGraph.h"
#include "Sound/SoundCue.h"
#include "SoundCueGraph/SoundCueGraph.h"
#include "UObject/MetaData.h"
#include "Widgets/SViewport.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#if WITH_LIVE_CODING
#include "ILiveCodingModule.h"
#endif

#if !BA_UE_VERSION_OR_LATER(5, 0)
#include "K2Node_Composite.h"
#endif

void FBAUtils::DeleteNode(UEdGraphNode* Node)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FBAUtils::DeleteNode"), STAT_FBAUtils_DeleteNode, STATGROUP_BA_EdGraphFormatter);
	if (!Node)
	{
		return;
	}

	if (UEdGraph* EdGraph = Node->GetGraph())
	{
		if (auto Schema = EdGraph->GetSchema())
		{
			Schema->SafeDeleteNodeFromGraph(EdGraph, Node);
		}
	}
}

bool FBAUtils::IsNodeDeleted(UEdGraphNode* Node)
{
	if (!Node)
	{
		return true;
	}

	return IsNodeDeletedFromGraph(Node->GetGraph(), Node);
}

bool FBAUtils::IsNodeDeletedFromGraph(UEdGraph* Graph, UEdGraphNode* Node)
{
	if (!Node || !Graph)
	{
		return true;
	}

	return !Graph->Nodes.Contains(Node);
}

TArray<UEdGraphNode*> FBAUtils::GetLinkedNodes(
	UEdGraphNode* Node,
	const EEdGraphPinDirection Direction)
{
	if (!Node) return TArray<UEdGraphNode*>();

	TSet<UEdGraphNode*> Nodes;
	for (UEdGraphPin* LinkedPin : GetLinkedPins(Node, Direction))
	{
		if (LinkedPin->LinkedTo.Num() > 0)
		{
			for (UEdGraphPin* Linked2LinkedPin : LinkedPin->LinkedTo)
			{
				Nodes.Add(Linked2LinkedPin->GetOwningNode());
			}
		}
	}

	return Nodes.Array();
}

TArray<UEdGraphNode*> FBAUtils::GetLinkedNodesFromPins(TArray<UEdGraphPin*> Pins)
{
	TSet<UEdGraphNode*> Nodes;
	for (UEdGraphPin* Pin : Pins)
	{
		for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			Nodes.Add(LinkedPin->GetOwningNode());
		}
	}

	return Nodes.Array();
}

TArray<UEdGraphNode*> FBAUtils::GetLinkedNodesFromPin(const UEdGraphPin* Pin)
{
	if (!Pin) return TArray<UEdGraphNode*>();

	TSet<UEdGraphNode*> Nodes;
	for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
	{
		Nodes.Add(LinkedPin->GetOwningNode());
	}

	return Nodes.Array();
}

bool FBAUtils::IsNodeLinked(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
	return GetLinkedPins(Node, Direction).Num() > 0;
}

TArray<UEdGraphPin*> FBAUtils::GetLinkedPins(
	UEdGraphNode* Node,
	const EEdGraphPinDirection Direction)
{
	if (!Node) return TArray<UEdGraphPin*>();

	const auto Pred = [](UEdGraphPin* Pin)
	{
		return Pin->LinkedTo.Num() > 0;
	};
	return GetPinsByDirection(Node, Direction).FilterByPredicate(Pred);
}

TArray<UEdGraphPin*> FBAUtils::GetLinkedToPins(
	UEdGraphNode* Node,
	const EEdGraphPinDirection Direction)
{
	if (!Node) return TArray<UEdGraphPin*>();

	TSet<UEdGraphPin*> LinkedToPins;

	for (UEdGraphPin* Pin : GetLinkedPins(Node, Direction))
	{
		LinkedToPins.Append(Pin->LinkedTo);
	}

	return LinkedToPins.Array();
}

TArray<UEdGraphNode*> FBAUtils::GetNodeAndParameters(UEdGraphNode* Node)
{
	struct FLocal
	{
		static bool AddNode(UEdGraphNode* Node, UEdGraphNode* ParentNode, TSet<UEdGraphNode*>& VisitedNodes, TSet<UEdGraphNode*>& OutNodes)
		{
			const bool bIsParentNode = Node == ParentNode;

			// fail if we are linked to an impure node (which is not the root node)
			if (IsNodeImpure(Node) && !bIsParentNode)
			{
				return false;
			}

			// don't check linked nodes if this node has been visited
			if (VisitedNodes.Contains(Node))
			{
				return true;
			}

			VisitedNodes.Add(Node);

			const TArray<UEdGraphNode*> LinkedNodes = GetLinkedNodes(Node);
			for (UEdGraphNode* LinkedNode : LinkedNodes)
			{
				// if the node fails (when it is an impure node), then the whole branch should fail  
				if (!AddNode(LinkedNode, ParentNode, VisitedNodes, OutNodes))
				{
					if (!bIsParentNode)
					{
						return false;
					}
				}
			}

			OutNodes.Add(Node);
			return true;
		}
	};

	TArray<UEdGraphNode*> PendingNodes;
	PendingNodes.Append(GetLinkedNodes(Node));

	TSet<UEdGraphNode*> VisitedNodes;
	TSet<UEdGraphNode*> OutNodes;

	FLocal::AddNode(Node, Node, VisitedNodes, OutNodes);
	return OutNodes.Array();
}

TArray<UEdGraphPin*> FBAUtils::GetPinsByDirection(
	UEdGraphNode* Node,
	const EEdGraphPinDirection Direction)
{
	if (!Node) return TArray<UEdGraphPin*>();

	const auto Pred = [Direction](UEdGraphPin* Pin)
	{
		return !FBAUtils::IsPinHidden(Pin) && (Pin->Direction == Direction || Direction ==
			EGPD_MAX);
	};

	return Node->Pins.FilterByPredicate(Pred);
}

TArray<UEdGraphPin*> FBAUtils::GetExecPins(UEdGraphNode* Node, const EEdGraphPinDirection Direction)
{
	if (!Node) return TArray<UEdGraphPin*>();

	const auto Pred = [Direction](UEdGraphPin* Pin)
	{
		return !FBAUtils::IsPinHidden(Pin) && IsExecPin(Pin) && (Pin->Direction == Direction || Direction == EGPD_MAX);
	};

	return Node->Pins.FilterByPredicate(Pred);
}

TArray<UEdGraphPin*> FBAUtils::GetExecOrDelegatePins(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
	if (!Node) return TArray<UEdGraphPin*>();

	const auto Pred = [Direction](UEdGraphPin* Pin)
	{
		return !FBAUtils::IsPinHidden(Pin) && IsExecOrDelegatePin(Pin) && (Pin->Direction == Direction || Direction == EGPD_MAX);
	};

	return Node->Pins.FilterByPredicate(Pred);
}

TArray<UEdGraphPin*> FBAUtils::GetParameterPins(
	UEdGraphNode* Node,
	const EEdGraphPinDirection Direction)
{
	if (!Node) return TArray<UEdGraphPin*>();

	const auto Pred = [&Direction](UEdGraphPin* Pin)
	{
		return !FBAUtils::IsPinHidden(Pin) && IsParameterPin(Pin) && (Pin->Direction == Direction || Direction == EGPD_MAX);
	};

	return Node->Pins.FilterByPredicate(Pred);
}

TArray<UEdGraphPin*> FBAUtils::GetPinsByCategory(
	UEdGraphNode* Node,
	const FName Category,
	const EEdGraphPinDirection Direction)
{
	if (!Node) return TArray<UEdGraphPin*>();

	const auto Pred = [&Category, &Direction](UEdGraphPin* Pin)
	{
		return (Pin->PinType.PinCategory == Category) && (Pin->Direction == Direction
			|| Direction == EGPD_MAX);
	};

	return Node->Pins.FilterByPredicate(Pred);
}

FSlateRect FBAUtils::GetGroupedBounds(const TArray<FSlateRect>& Rects)
{
	TOptional<FSlateRect> Bounds;
	for (const FSlateRect& Rect : Rects)
	{
		Bounds = Bounds.IsSet() ? Bounds->Expand(Rect) : Rect;
	}

	return Bounds.Get(FSlateRect());
}

FVector2D FBAUtils::GetNodePosition(UEdGraphNode* Node)
{
	return FVector2D(Node->NodePosX, Node->NodePosY);
}

FSlateRect FBAUtils::GetNodeBounds(UEdGraphNode* Node)
{
	if (Node == nullptr)
	{
		return FSlateRect();
	}

	const TWeakPtr<SGraphNode> GraphNode = Node->DEPRECATED_NodeWidget;
	return GetNodeBounds(GraphNode.Pin());
}

FSlateRect FBAUtils::GetNodeBounds(TSharedPtr<SGraphNode> GraphNode)
{
	if (GraphNode)
	{
		return FSlateRect::FromPointAndExtent(FBAUtils::GetGraphNodePos(GraphNode), GraphNode->GetDesiredSize());
	}

	return FSlateRect();
}

FSlateRect FBAUtils::GetNodeArrayBounds(const TArray<UEdGraphNode*>& Nodes)
{
	TOptional<FSlateRect> Bounds;
	for (UEdGraphNode* Node : Nodes)
	{
		if (!Node)
		{
			continue;
		}

		Bounds = !Bounds.IsSet()
			? Bounds = GetNodeBounds(Node)
			: Bounds.GetValue().Expand(GetNodeBounds(Node));
	}

	return Bounds.Get(FSlateRect());
}

FSlateRect FBAUtils::GetPinBounds(TSharedPtr<SGraphPanel> GraphPanel, UEdGraphPin* Pin)
{
	if (Pin == nullptr)
	{
		return FSlateRect();
	}

	const TSharedPtr<SGraphPin> GraphPin = FBAUtils::GetGraphPin(GraphPanel, Pin);
	if (!GraphPin)
	{
		return FSlateRect();
	}

	return GetPinBounds(GraphPin);
}

FSlateRect FBAUtils::GetPinBounds(TSharedPtr<SGraphPin> GraphPin)
{
	if (!GraphPin)
	{
		return FSlateRect();
	}

	const FVector2D Pos = FBAUtils::GetPinPos(GraphPin);
	const FVector2D Size = GraphPin->GetDesiredSize();
	return FSlateRect::FromPointAndExtent(Pos, Size);
}

FSlateRect FBAUtils::GetCachedNodeBounds(
	TSharedPtr<FBAGraphHandler> GraphHandler,
	UEdGraphNode* Node,
	bool bWithCommentBubble)
{
	return GraphHandler->GetCachedNodeBounds(Node, bWithCommentBubble);
}

FSlateRect FBAUtils::GetCachedNodeArrayBounds(
	TSharedPtr<FBAGraphHandler> GraphHandler,
	TArray<UEdGraphNode*> Nodes)
{
	TOptional<FSlateRect> Bounds;
	for (UEdGraphNode* Node : Nodes)
	{
		if (!Node)
			continue;

		const FSlateRect NodeBounds = GetCachedNodeBounds(GraphHandler, Node);
		Bounds = !Bounds.IsSet() ? NodeBounds : Bounds.GetValue().Expand(NodeBounds);
	}

	return Bounds.Get(FSlateRect());
}

FVector2D FBAUtils::GetPinPos(TSharedPtr<FBAGraphHandler> GraphHandler, UEdGraphPin* Pin)
{
	if (Pin)
	{
		UEdGraphNode* OwningNode = Pin->GetOwningNode();

		const FSlateRect NodeBounds = GetCachedNodeBounds(GraphHandler, OwningNode, false);

		// use node left and right for the pin pos x
		return FVector2D(
			Pin->Direction == EGPD_Input ? NodeBounds.Left : NodeBounds.Right,
			GraphHandler->GetPinY(Pin));
	}

	return FVector2D::ZeroVector;
}

FVector2D FBAUtils::GetPinPos(TSharedPtr<SGraphPin> Pin)
{
	if (Pin)
	{
		UEdGraphNode* OwningNode = Pin->GetPinObj()->GetOwningNode();
		if (OwningNode)
		{
			return FVector2D(OwningNode->NodePosX, OwningNode->NodePosY) + Pin->GetNodeOffset();
		}
	}

	return FVector2D::ZeroVector;
}

void FBAUtils::SortNodesOnGraphByDistance(
	UEdGraphNode* RelativeNode,
	const UEdGraph* Graph,
	TSet<UEdGraphNode*>& LHSNodes,
	TSet<UEdGraphNode*>& RHSNodes,
	TSet<UEdGraphPin*>& LHSPins,
	TSet<UEdGraphPin*>& RHSPins)
{
	if (Graph == nullptr || RelativeNode == nullptr)
	{
		return;
	}

	// Add nodes to LHS or RHS depending on X position
	for (UEdGraphNode* Other : Graph->Nodes)
	{
		// ignore the same node
		if (Other == RelativeNode)
		{
			continue;
		}

		// ignore nodes too far away
		// TODO: make this an option parameter
		const float DeltaX = FMath::Abs(Other->NodePosX - RelativeNode->NodePosX);
		const float DeltaY = FMath::Abs(Other->NodePosY - RelativeNode->NodePosY);
		if (DeltaX > 600 || DeltaY > 400)
		{
			continue;
		}

		(RelativeNode->NodePosX >= Other->NodePosX ? LHSNodes : RHSNodes).Add(Other);
	}

	// sort nodes by distance to Node
	UEdGraphNode* NodeLValue = RelativeNode;
	const auto& Sorter = [NodeLValue](UEdGraphNode& A, UEdGraphNode& B)-> bool
	{
		float DeltaX = A.NodePosX - NodeLValue->NodePosX;
		float DeltaY = A.NodePosY - NodeLValue->NodePosY;
		const float DistA = DeltaX * DeltaX + 4.f * DeltaY * DeltaY;

		DeltaX = B.NodePosX - NodeLValue->NodePosX;
		DeltaY = B.NodePosY - NodeLValue->NodePosY;
		const float DistB = DeltaX * DeltaX + 4.f * DeltaY * DeltaY;

		return DistA < DistB;
	};

	LHSNodes.Sort(Sorter);
	RHSNodes.Sort(Sorter);

	for (UEdGraphNode* Node : LHSNodes)
	{
		for (UEdGraphPin* OtherPin : Node->Pins)
		{
			if (OtherPin->Direction == EGPD_Output)
			{
				LHSPins.Add(OtherPin);
			}
		}
	}

	for (UEdGraphNode* Node : RHSNodes)
	{
		for (UEdGraphPin* OtherPin : Node->Pins)
		{
			if (OtherPin->Direction == EGPD_Input)
			{
				RHSPins.Add(OtherPin);
			}
		}
	}
}

FVector2D FBAUtils::GetKnotNodeSize()
{
#if BA_UE_VERSION_OR_LATER(5, 1)
	const static FVector2D NodeSpacerSize(42.f, 20.0f);
#else
	const static FVector2D NodeSpacerSize(42.f, 16.0f);
#endif

	if (UBASettings_EditorFeatures::Get().bEnableInvisibleKnotNodes)
	{
		const static FVector2D InvisibleNodeSpacerSize(42.f, 8.0f);
		return InvisibleNodeSpacerSize;
	}
	else
	{
		return NodeSpacerSize;
	}
}

UK2Node_Knot* FBAUtils::CreateKnotNode(
	UEdGraph* Graph,
	const FVector2D& Position,
	UEdGraphPin* PinA,
	UEdGraphPin* PinB)
{
	if (!Graph)
	{
		return nullptr;
	}

	UK2Node_Knot* NewKnot = NewObject<UK2Node_Knot>(Graph);
	if (Graph->HasAnyFlags(RF_Transactional))
	{
		NewKnot->SetFlags(RF_Transactional);
	}

	Graph->AddNode(NewKnot, false, false);
	NewKnot->CreateNewGuid();
	NewKnot->PostPlacedNewNode();
	NewKnot->AllocateDefaultPins();

	LinkKnotNodeBetween(NewKnot, Position, PinA, PinB);
	return NewKnot;
}

void FBAUtils::RemoveLinkedKnotNodes(UEdGraphNode* InitialNode, TSet<UEdGraphNode*>& RemovedNodes)
{
	struct FLocal
	{
		static void Delete(UEdGraphNode* InNode, TSet<UEdGraphNode*>& VisitedNodes)
		{
			if (VisitedNodes.Contains(InNode))
			{
				return;
			}

			/** Iterate across all linked node */
			for (UEdGraphNode* Node : GetLinkedNodes(InNode))
			{
				if (IsKnotNode(Node) && !VisitedNodes.Contains(Node))
				{
					if (VisitedNodes.Contains(Node))
					{
						continue;
					}

					VisitedNodes.Emplace(Node);
					Delete(Node, VisitedNodes);
				}
			}

			/** Delete all connections for each knot node */
			if (IsKnotNode(InNode))
			{
				DisconnectKnotNode(InNode);
				DeleteNode(InNode);
			}
		}
	};

	FLocal::Delete(InitialNode, RemovedNodes);
}

TArray<UEdGraphPin*> FBAUtils::GetLinkedToPinsIgnoringKnots(UEdGraphNode* InNode, EEdGraphPinDirection Direction)
{
	TArray<UEdGraphNode*> PendingNodes = { InNode };
	TArray<UEdGraphPin*> OutPins;
	TSet<UEdGraphNode*> VisitedNodes;

	while (PendingNodes.Num() > 0)
	{
		UEdGraphNode* Node = PendingNodes.Pop();
		if (VisitedNodes.Contains(Node))
		{
			continue;
		}

		VisitedNodes.Add(Node);

		for (UEdGraphPin* Pin : GetLinkedToPins(Node, Direction))
		{
			if (UK2Node_Knot* KnotNode = Cast<UK2Node_Knot>(Pin->GetOwningNode()))
			{
				PendingNodes.Push(KnotNode);
			}
			else
			{
				OutPins.Add(Pin);
			}
		}
	}

	return OutPins;
}

TArray<UEdGraphPin*> FBAUtils::GetPinLinkedToIgnoringKnots(const UEdGraphPin* InitialPin)
{
	if (!InitialPin)
	{
		return {};
	}

	TArray<const UEdGraphPin*> PendingPins = { InitialPin };
	TArray<UEdGraphPin*> OutPins;
	EEdGraphPinDirection Dir = InitialPin->Direction;
	TSet<const UEdGraphPin*> VisitedPins;

	while (PendingPins.Num() > 0)
	{
		const UEdGraphPin* CurrPin = PendingPins.Pop();
		if (VisitedPins.Contains(CurrPin))
		{
			continue;
		}

		VisitedPins.Add(CurrPin);

		for (UEdGraphPin* LinkedPin : CurrPin->LinkedTo)
		{
			if (UK2Node_Knot* KnotNode = Cast<UK2Node_Knot>(LinkedPin->GetOwningNode()))
			{
				PendingPins.Push(FBAUtils::GetKnotPinByDirection(KnotNode, Dir));
			}
			else
			{
				OutPins.Add(LinkedPin);
			}
		}
	}

	return OutPins;
}

void FBAUtils::LinkKnotNodeBetween(UK2Node_Knot* KnotNode, const FVector2D& Position, UEdGraphPin* PinA, UEdGraphPin* PinB)
{
	if (!PinA)
	{
		UE_LOG(LogBlueprintAssist, Warning, TEXT("[%hs] Tried to call with null PinA parameter"), __FUNCTION__);
		return;
	}

	// This constant is duplicated from inside of SGraphNodeKnot
	const FVector2D KnotTopLeft = Position - FBAUtils::GetKnotNodeSize() * 0.5f;
	KnotNode->NodePosX = KnotTopLeft.X;
	KnotNode->NodePosY = KnotTopLeft.Y;

	// Move the connections across (only notifying the knot, as the other two didn't really change)
	if (PinB != nullptr)
	{
		PinA->BreakLinkTo(PinB);
	}
	PinA->MakeLinkTo(PinA->Direction == EGPD_Output ? KnotNode->GetInputPin() : KnotNode->GetOutputPin());

	if (PinB != nullptr)
	{
		PinB->MakeLinkTo(PinB->Direction == EGPD_Output ? KnotNode->GetInputPin() : KnotNode->GetOutputPin());
	}

	// Blueprint stuff
	KnotNode->PostReconstructNode();
}

void FBAUtils::DisconnectKnotNode(UEdGraphNode* Node)
{
	if (!Node)
	{
		return;
	}

	UEdGraphPin* InputPin = GetPinsByDirection(Node, EGPD_Input)[0];
	UEdGraphPin* OutputPin = GetPinsByDirection(Node, EGPD_Output)[0];

	// link connections of the knot node
	if (InputPin->LinkedTo.Num() > 0 && OutputPin->LinkedTo.Num() > 0)
	{
		TArray<UEdGraphPin*> InputPinsLinked = InputPin->LinkedTo;
		TArray<UEdGraphPin*> OutputPinsLinked = OutputPin->LinkedTo;

		for (UEdGraphPin* PinA : InputPinsLinked)
		{
			InputPin->BreakLinkTo(PinA);

			for (UEdGraphPin* PinB : OutputPinsLinked)
			{
				OutputPin->BreakLinkTo(PinB);

				PinA->MakeLinkTo(PinB);
			}
		}
	}

	Node->BreakAllNodeLinks();
}

bool FBAUtils::IsNodePure(UEdGraphNode* Node)
{
	return !IsNodeImpure(Node);
}

bool FBAUtils::IsNodeImpure(UEdGraphNode* Node)
{
	return Node->Pins.ContainsByPredicate(IsExecPin);
}

bool FBAUtils::IsPinLinked(const UEdGraphPin* Pin)
{
	return Pin->LinkedTo.Num() > 0;
}

bool FBAUtils::IsPinUnlinked(const UEdGraphPin* Pin)
{
	return !IsPinLinked(Pin);
}

bool FBAUtils::IsExecPin(const UEdGraphPin* Pin)
{
	UEdGraph* Graph = Pin->GetOwningNodeUnchecked()->GetGraph();

	if (FBAFormatterSettings* FormatterSettings = UBASettings::FindFormatterSettings(Graph))
	{
		if (!FormatterSettings->ExecPinName.IsNone())
		{
			return DoesPinTypeCategoryObjectMatchName(Pin, FormatterSettings->ExecPinName);
		}
	}

	if (IsBlueprintGraph(Graph))
	{
		return Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
	}

	return true;
}

bool FBAUtils::IsExecPinLink(FPinLink& PinLink)
{
	return IsExecPin(PinLink.GetFromPin());
}

bool FBAUtils::IsExecPinLinkConst(const FPinLink& PinLink)
{
	return IsExecPin(PinLink.From);
}

bool FBAUtils::IsDelegatePin(const UEdGraphPin* Pin)
{
	return Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Delegate;
}

bool FBAUtils::IsExecOrDelegatePin(const UEdGraphPin* Pin)
{
	if (!Pin)
		return false;

	if (IsExecPin(Pin))
		return true;

	if (UBASettings::Get().bTreatDelegatesAsExecutionPins)
	{
		if (UK2Node_Knot* KnotDelegate = Cast<UK2Node_Knot>(Pin->GetOwningNodeUnchecked()))
		{
			// if it is a delegate knot pin, check both the input and output
			if (IsDelegatePinLinkedToCustomEvent(KnotDelegate->GetInputPin()) ||
				IsDelegatePinLinkedToCustomEvent(KnotDelegate->GetOutputPin()))
			{
				return true;
			}
		}
		else
		{
			if (IsDelegatePinLinkedToCustomEvent(Pin))
			{
				return true;
			}
		}
	}

	return false;
}

bool FBAUtils::IsDelegatePinLinkedToCustomEvent(const UEdGraphPin* Pin)
{
	if (IsDelegatePin(Pin))
	{
		// only apply this to to custom events
		if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Pin->GetOwningNode()))
		{
			return true;
		}

		// or pins linked to custom events pins
		for (UEdGraphPin* LinkedPin : FBAUtils::GetPinLinkedToIgnoringKnots(Pin))
		{
			if (auto CustomEvent = Cast<UK2Node_CustomEvent>(LinkedPin->GetOwningNode()))
			{
				return true;
			}
		}
	}

	return false;
}

bool FBAUtils::IsExecOrDelegatePinLink(const FPinLink& PinLink)
{
	return IsExecOrDelegatePin(PinLink.From);
}

bool FBAUtils::IsParameterPin(const UEdGraphPin* Pin)
{
	if (IsDelegatePin(Pin))
	{
		return !IsDelegatePinLinkedToCustomEvent(Pin);
	}

	return !IsExecPin(Pin) && !IsDelegatePin(Pin);
}

int FBAUtils::GetPinIndex(UEdGraphPin* Pin)
{
	int PinIndex = -1;
	GetPinsByDirection(Pin->GetOwningNode(), Pin->Direction).Find(Pin, PinIndex);
	return PinIndex;
}

int FBAUtils::GetLinkedPinIndex(UEdGraphPin* Pin)
{
	int PinIndex;
	GetLinkedPins(Pin->GetOwningNode(), Pin->Direction).Find(Pin, PinIndex);
	return PinIndex;
}

bool FBAUtils::DoesNodeHaveExecutionTo(UEdGraphNode* InNodeA, UEdGraphNode* InNodeB, EEdGraphPinDirection Direction)
{
	if (!InNodeA || !InNodeB)
	{
		return false;
	}

	TSet<UEdGraphNode*> NodeTree;
	TSet<FPinLink> VisitedLinks;
	TQueue<UEdGraphNode*> NodeQueue;

	UEdGraphNode* NodeA = InNodeA;
	UEdGraphNode* NodeB = InNodeB;

	if (FBAUtils::IsNodePure(NodeA))
	{
		if (UEdGraphNode* ExecNode = GetExecutingNode(NodeA))
		{
			NodeA = ExecNode;
		}
	}

	if (FBAUtils::IsNodePure(NodeB))
	{
		if (UEdGraphNode* ExecNode = GetExecutingNode(NodeB))
		{
			NodeB = ExecNode;
		}
	}

	NodeQueue.Enqueue(NodeA);

	while (!NodeQueue.IsEmpty())
	{
		UEdGraphNode* NextNode;
		NodeQueue.Dequeue(NextNode);
		NodeTree.Add(NextNode);

		if (NextNode == NodeB)
		{
			return true;
		}

		TArray<UEdGraphPin*> MyLinkedPins = FBAUtils::GetLinkedPins(NextNode, Direction);
		if (IsNodeImpure(NextNode))
		{
			MyLinkedPins = MyLinkedPins.FilterByPredicate(FBAUtils::IsExecOrDelegatePin);
		}

		for (auto Pin : MyLinkedPins)
		{
			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();

				FPinLink Link(Pin, LinkedPin);

				if (VisitedLinks.Contains(Link))
				{
					continue;
				}

				VisitedLinks.Add(Link);
				NodeQueue.Enqueue(LinkedNode);
			}
		}
	}

	return false;
}

bool FBAUtils::DoesNodeHaveExecutionTo(UEdGraphNode* InNode, const TSet<UEdGraphNode*>& NodeSet, EEdGraphPinDirection Direction)
{
	if (!InNode || NodeSet.Num() == 0)
	{
		return false;
	}

	TSet<UEdGraphNode*> NodeTree;
	TSet<FPinLink> VisitedLinks;
	TQueue<UEdGraphNode*> NodeQueue;

	UEdGraphNode* NodeA = InNode;

	if (FBAUtils::IsNodePure(NodeA))
	{
		if (UEdGraphNode* ExecNode = GetExecutingNode(NodeA))
		{
			NodeA = ExecNode;
		}
	}

	NodeQueue.Enqueue(NodeA);

	while (!NodeQueue.IsEmpty())
	{
		UEdGraphNode* NextNode;
		NodeQueue.Dequeue(NextNode);
		NodeTree.Add(NextNode);

		if (NodeSet.Contains(NextNode))
		{
			return true;
		}

		TArray<UEdGraphPin*> MyLinkedPins = FBAUtils::GetLinkedPins(NextNode, Direction);
		if (IsNodeImpure(NextNode))
		{
			MyLinkedPins = MyLinkedPins.FilterByPredicate(FBAUtils::IsExecOrDelegatePin);
		}

		for (auto Pin : MyLinkedPins)
		{
			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();

				FPinLink Link(Pin, LinkedPin);

				if (VisitedLinks.Contains(Link))
				{
					continue;
				}

				VisitedLinks.Add(Link);
				NodeQueue.Enqueue(LinkedNode);
			}
		}
	}

	return false;
}

bool FBAUtils::IsLoopingPinLink(FPinLink& PinLink, EEdGraphPinDirection Direction)
{
	return FBAUtils::DoesNodeHaveExecutionTo(PinLink.GetToNode(), PinLink.GetFromNode(), Direction);
}

UEdGraphNode* FBAUtils::GetExecutingNode(UEdGraphNode* Node)
{
	if (FBAUtils::IsNodeImpure(Node))
	{
		return Node;
	}

	const TArray<UEdGraphNode*> LinkedOutNodes = FBAUtils::GetLinkedNodes(Node, EGPD_Output);
	const TArray<UEdGraphNode*> LinkedPureNodes = LinkedOutNodes.FilterByPredicate(IsNodeImpure);
	if (LinkedPureNodes.Num() > 0)
	{
		return LinkedPureNodes[0];
	}

	for (UEdGraphNode* OutNode : LinkedOutNodes.FilterByPredicate(IsNodePure))
	{
		if (UEdGraphNode* ExecutingNode = GetExecutingNode(OutNode))
		{
			return ExecutingNode;
		}
	}

	return nullptr;
}

TSet<UEdGraphNode*> FBAUtils::GetNodeTreeWithFilter(UEdGraphNode* InitialNode, TFunctionRef<bool(UEdGraphPin*)> Pred, EEdGraphPinDirection Direction, bool bOnlyInitialDirection)
{
	TSet<UEdGraphNode*> NodeTree;
	if (!InitialNode)
	{
		return NodeTree;
	}

	TSet<UEdGraphNode*> VisitedNodes;
	VisitedNodes.Add(InitialNode);
	TQueue<UEdGraphNode*> NodeQueue;

	NodeQueue.Enqueue(InitialNode);

	while (!NodeQueue.IsEmpty())
	{
		UEdGraphNode* NextNode;
		NodeQueue.Dequeue(NextNode);
		NodeTree.Add(NextNode);

		EEdGraphPinDirection PinsDirection = Direction;

		if (bOnlyInitialDirection && NextNode != InitialNode)
		{
			PinsDirection = EGPD_MAX;
		}

		for (UEdGraphPin* LinkedPin : GetLinkedToPins(NextNode, PinsDirection).FilterByPredicate(Pred))
		{
			UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();

			if (VisitedNodes.Contains(LinkedNode))
			{
				continue;
			}

			NodeQueue.Enqueue(LinkedNode);
			VisitedNodes.Add(LinkedNode);
		}
	}

	return NodeTree;
}

TSet<UEdGraphNode*> FBAUtils::GetNodeTreeWithFilter(UEdGraphNode* InitialNode, TFunctionRef<bool(const FPinLink&)> Pred,
													EEdGraphPinDirection Direction, bool bOnlyInitialDirection)
{
	TSet<UEdGraphNode*> NodeTree;
	TSet<UEdGraphNode*> VisitedNodes;
	VisitedNodes.Add(InitialNode);
	TQueue<UEdGraphNode*> NodeQueue;

	NodeQueue.Enqueue(InitialNode);

	while (!NodeQueue.IsEmpty())
	{
		UEdGraphNode* NextNode;
		NodeQueue.Dequeue(NextNode);
		NodeTree.Add(NextNode);

		EEdGraphPinDirection PinsDirection = Direction;

		if (bOnlyInitialDirection && NextNode != InitialNode)
		{
			PinsDirection = EGPD_MAX;
		}

		auto PinLinks = GetPinLinks(NextNode, PinsDirection).FilterByPredicate(Pred);

		for (const FPinLink& PinLink : PinLinks)
		{
			UEdGraphPin* LinkedPin = PinLink.To;

			UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
			if (VisitedNodes.Contains(LinkedNode))
			{
				continue;
			}

			NodeQueue.Enqueue(LinkedNode);
			VisitedNodes.Add(LinkedNode);
		}
	}

	return NodeTree;
}

TSet<UEdGraphNode*> FBAUtils::IterateNodeTreeDepthFirst(
	UEdGraphNode* InitialNode,
	TFunctionRef<bool(const FPinLink&)> Pred,
	const EEdGraphPinDirection Direction,
	const bool bOnlyInitialDirection)
{
	struct FLocal
	{
		static void IterateNodeTreeDepthFirst_Recursive(
			const FPinLink& RootLink,
			TFunctionRef<bool(const FPinLink&)> Pred,
			TSet<UEdGraphNode*>& VisitedNodes,
			EEdGraphPinDirection Direction = EGPD_MAX,
			bool bOnlyInitialDirection = false)
		{
			UEdGraphNode* NextNode = RootLink.GetNode();
			VisitedNodes.Add(NextNode);

			EEdGraphPinDirection PinsDirection = Direction;

			TArray<FPinLink> ExecPinLinks;
			TArray<FPinLink> ParamPinLinks;

			{
				TArray<FPinLink> PinLinks = GetPinLinks(NextNode, PinsDirection);

				if (PinsDirection == EGPD_MAX)
				{
					// output first
					PinLinks.StableSort([](const FPinLink& A, const FPinLink& B)
					{
						return A.GetDirection() > B.GetDirection();
					});
				}

				for (const FPinLink& PinLink : PinLinks)
				{
					if (FBAUtils::IsExecPin(PinLink.From))
					{
						ExecPinLinks.Add(PinLink);
					}
					else
					{
						ParamPinLinks.Add(PinLink);
					}
				}
			}

			// process the exec pins first!
			for (TArray<FPinLink>* Links : { &ExecPinLinks, &ParamPinLinks })
			{
				for (FPinLink& PinLink : *Links)
				{
					UEdGraphNode* LinkedNode = PinLink.GetNode();

					if (Pred(PinLink) && !VisitedNodes.Contains(LinkedNode))
					{
						// UE_LOG(LogTemp, Warning, TEXT("Added %s"), *FBAUtils::GetNodeName(LinkedNode));
						// NodeStack.Add(LinkedNode);
						IterateNodeTreeDepthFirst_Recursive(PinLink, Pred, VisitedNodes, Direction, bOnlyInitialDirection);
						VisitedNodes.Add(LinkedNode);
					}
				}
			}
		}
	};

	TSet<UEdGraphNode*> VisitedNodes;

	const FPinLink RootLink(nullptr, nullptr, InitialNode);

	FLocal::IterateNodeTreeDepthFirst_Recursive(RootLink, Pred, VisitedNodes, Direction, bOnlyInitialDirection);

	return VisitedNodes; 
}

TSet<UEdGraphNode*> FBAUtils::GetNodeTree(UEdGraphNode* InitialNode, const EEdGraphPinDirection Direction, bool bOnlyInitialDirection)
{
	return GetNodeTreeWithFilter(InitialNode, [](UEdGraphPin*) { return true; }, Direction, bOnlyInitialDirection);
}

TSet<UEdGraphNode*> FBAUtils::GetExecTree(UEdGraphNode* Node, EEdGraphPinDirection Direction, bool bOnlyInitialDirection)
{
	return GetExecutionTreeWithFilter(Node, [](UEdGraphNode* Node) { return true; }, Direction, bOnlyInitialDirection);
}

TSet<UEdGraphNode*> FBAUtils::GetExecutionTreeWithFilter(UEdGraphNode* InitialNode, TFunctionRef<bool(UEdGraphNode*)> Pred, EEdGraphPinDirection Direction, bool bOnlyInitialDirection)
{
	TSet<UEdGraphNode*> NodeTree;
	if (!InitialNode)
	{
		return NodeTree;
	}

	TSet<UEdGraphNode*> VisitedNodes;
	VisitedNodes.Add(InitialNode);
	TQueue<UEdGraphNode*> NodeQueue;

	NodeQueue.Enqueue(InitialNode);

	while (!NodeQueue.IsEmpty())
	{
		UEdGraphNode* NextNode;
		NodeQueue.Dequeue(NextNode);
		NodeTree.Add(NextNode);

		const EEdGraphPinDirection PinsDirection = Direction;

		if (bOnlyInitialDirection && NextNode != InitialNode)
		{
			Direction = EGPD_MAX;
		}

		TArray<UEdGraphNode*> LinkedNodes = GetLinkedNodesFromPins(GetExecPins(NextNode, PinsDirection));

		for (UEdGraphNode* LinkedNode : LinkedNodes)
		{
			if (VisitedNodes.Contains(LinkedNode))
			{
				continue;
			}

			if (!Pred(LinkedNode))
			{
				continue;
			}

			NodeQueue.Enqueue(LinkedNode);
			VisitedNodes.Add(LinkedNode);
		}
	}

	return NodeTree;
}

TSet<UEdGraphNode*> FBAUtils::GetParameterTree(UEdGraphNode* Node, EEdGraphPinDirection Direction, bool bOnlyInitialDirection)
{
	const auto Filter = [](UEdGraphPin* Pin)
	{
		UEdGraphNode* Node = Pin->GetOwningNode();
		return Node && !IsKnotNode(Node) && IsNodePure(Node) && IsParameterPin(Pin);
	};

	return GetNodeTreeWithFilter(Node, Filter, Direction, bOnlyInitialDirection);
}

TSet<UEdGraphNode*> FBAUtils::GetEdGraphNodeTree(
	UEdGraphNode* InitialNode,
	const EEdGraphPinDirection Direction)
{
	// if the initial node is pure, find the first impure node in the tree
	if (IsNodePure(InitialNode))
	{
		TQueue<UEdGraphNode*> NodeQueue;
		TSet<UEdGraphNode*> VisitedNodes;

		VisitedNodes.Add(InitialNode);
		NodeQueue.Enqueue(InitialNode);

		while (!NodeQueue.IsEmpty())
		{
			UEdGraphNode* NextNode;
			NodeQueue.Dequeue(NextNode);

			TArray<UEdGraphNode*> LinkedOutputNodes = GetLinkedNodes(NextNode);
			bool bFoundExecNode = false;
			for (UEdGraphNode* LinkedNode : LinkedOutputNodes)
			{
				if (VisitedNodes.Contains(LinkedNode))
				{
					continue;
				}

				VisitedNodes.Add(LinkedNode);

				if (IsNodeImpure(LinkedNode))
				{
					InitialNode = LinkedNode;
					bFoundExecNode = true;
					break;
				}

				NodeQueue.Enqueue(LinkedNode);
			}

			if (bFoundExecNode)
			{
				break;
			}
		}
	}

	TSet<UEdGraphNode*> NodeTree;
	TSet<UEdGraphNode*> VisitedNodes;
	TQueue<UEdGraphNode*> NodeQueue;

	NodeQueue.Enqueue(InitialNode);

	// find all connected impure nodes
	while (!NodeQueue.IsEmpty())
	{
		UEdGraphNode* NextNode;
		NodeQueue.Dequeue(NextNode);

		NodeTree.Add(NextNode);

		// add the input parameter tree
		TQueue<UEdGraphNode*> ParameterQueue;
		TSet<UEdGraphNode*> VisitedParameters;
		ParameterQueue.Enqueue(NextNode);
		while (!ParameterQueue.IsEmpty())
		{
			UEdGraphNode* ParameterNode;
			ParameterQueue.Dequeue(ParameterNode);

			TArray<UEdGraphNode*> LinkedPureNodes =
				GetLinkedNodes(ParameterNode).FilterByPredicate(IsNodePure);
			for (UEdGraphNode* LinkedNode : LinkedPureNodes)
			{
				if (VisitedParameters.Contains(LinkedNode))
				{
					continue;
				}

				VisitedParameters.Add(LinkedNode);

				ParameterQueue.Enqueue(LinkedNode);
				NodeTree.Add(LinkedNode);
			}
		}

		// iterate over linked impure nodes
		TArray<UEdGraphPin*> LinkedExecPins =
			GetLinkedPins(NextNode, Direction).FilterByPredicate(IsExecPin);
		for (UEdGraphPin* LinkedPin : LinkedExecPins)
		{
			for (UEdGraphPin* LinkedTo : LinkedPin->LinkedTo)
			{
				UEdGraphNode* LinkedNode = LinkedTo->GetOwningNode();
				if (VisitedNodes.Contains(LinkedNode))
				{
					continue;
				}

				NodeQueue.Enqueue(LinkedNode);
				VisitedNodes.Add(LinkedNode);
			}
		}
	}

	return NodeTree;
}

UEdGraphPin* FBAUtils::GetFirstLinkedPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
	TArray<UEdGraphPin*> Pins = GetLinkedPins(Node, Direction);
	return Pins.Num() == 0 ? nullptr : Pins[0];
}

UEdGraphPin* FBAUtils::GetFirstExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
	TArray<UEdGraphPin*> Pins = GetExecPins(Node, Direction);
	return Pins.Num() == 0 ? nullptr : Pins[0];
}

UEdGraphNode* FBAUtils::GetFirstLinkedNode(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
	if (UEdGraphPin* ExecPin = GetFirstExecPin(Node, Direction))
	{
		if (ExecPin->LinkedTo.Num() > 0)
		{
			return ExecPin->LinkedTo[0]->GetOwningNode();
		}
	}

	return nullptr;
}

void FBAUtils::PrintNodeInfo(UEdGraphNode* Node)
{
	if (Node == nullptr)
	{
		return;
	}

	if (auto GraphHandler = GetCurrentGraphHandler())
	{
		FSlateRect NodeBounds = FBAUtils::GetCachedNodeBounds(GraphHandler, Node);
		UE_LOG(LogBlueprintAssist, Log, TEXT("%s"), *NodeBounds.ToString());
	}

	UE_LOG(LogBlueprintAssist, Log, TEXT("### BEGIN print node <%d> <%s (%s)> <%s> <%s> <%d> info"),
			FBAUtils::IsNodePure(Node),
			*Node->GetName(),
			*FBAUtils::GetObjectClassName(Node).ToString(),
			*Node->NodeGuid.ToString(),
			*Node->GetGraph()->GetClass()->GetName(),
			Node->GetUniqueID()
	);

	for (UEdGraphPin* Pin : Node->Pins)
	{
		FString SubCategoryObjectName = "nullptr";
		FString SubCategoryDataStruct = "nullptr";
		if (Pin->PinType.PinSubCategoryObject.IsValid())
		{
			SubCategoryObjectName = Pin->PinType.PinSubCategoryObject->GetName();
			if (Pin->PinType.PinSubCategoryObject->GetSparseClassDataStruct() != nullptr)
			{
				SubCategoryDataStruct = Pin->PinType.PinSubCategoryObject->GetSparseClassDataStruct()->GetClass()->GetName();
			}
		}
		
		UE_LOG(LogBlueprintAssist, Log, TEXT("\tPin <%s> | <%d> | <%s> | <%s> | <%s> | <%s> | <%s> | <%s>"),
				*Pin->GetDisplayName().ToString(),
				FBAUtils::IsExecPin(Pin),
				*FString(Pin->Direction == EGPD_Input ? "In" : "Out"),
				*Pin->PinType.PinCategory.ToString(),
				*Pin->PinType.PinSubCategory.ToString(),
				*SubCategoryObjectName,
				*SubCategoryDataStruct,
				*Pin->PinId.ToString());
	}
	UE_LOG(LogBlueprintAssist, Log, TEXT("### END print node <%s> info"), *Node->GetName());
}

void FBAUtils::PrintNodeArray(const TArray<UEdGraphNode*>& Nodes, const FString& InitialMessage)
{
	UE_LOG(LogTemp, Warning, TEXT("PRINTNODEARRAY {%s}"), *InitialMessage);

	for (auto Node : Nodes)
	{
		UE_LOG(LogTemp, Warning, TEXT("\t%s"), *FBAUtils::GetNodeName(Node));
	}
}

bool FBAUtils::IsKnotNode(UEdGraphNode* Node)
{
	if (!Node)
	{
		return false;
	}

	if (Node->IsA(UK2Node_Knot::StaticClass()))
	{
		return true;
	}

	if (Node->IsA(UMaterialGraphNode_Knot::StaticClass()))
	{
		return true;
	}

	return false;
}

bool FBAUtils::IsCommentNode(const UEdGraphNode* Node)
{
	const UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(Node);
	return CommentNode != nullptr;
}

bool FBAUtils::IsVarNode(UEdGraphNode* Node)
{
	const UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node);
	return VarNode != nullptr;
}

bool FBAUtils::IsK2Node(UEdGraphNode* Node)
{
	if (Node == nullptr)
	{
		return false;
	}

	const UK2Node* K2Node = Cast<UK2Node>(Node);
	return K2Node != nullptr;
}

bool FBAUtils::IsValidGraph(UEdGraph* Graph)
{
	if (!Graph)
	{
		return false;
	}

	if (IsBlueprintGraph(Graph))
	{
		return true;
	}

	if (UBASettings::FindFormatterSettings(Graph))
	{
		return true;
	}

	// UE_LOG(LogBlueprintAssist, Warning, TEXT("Graph not valid `%s` | %s"), *Graph->GetClass()->GetName(), *Graph->GetSchema()->GetClass()->GetName());

	return false;
}

bool FBAUtils::IsValidPin(UEdGraphPin* Pin)
{
	return Pin != nullptr && !Pin->bWasTrashed;
}

bool FBAUtils::IsPinHidden(UEdGraphPin* Pin)
{
	if (!Pin)
	{
		return true;
	}

	// certain pins can be marked as hidden but not be hidden when linked such as the self-pin
	return Pin->bHidden && Pin->LinkedTo.Num() == 0;
}

bool FBAUtils::IsBlueprintGraph(UEdGraph* Graph, bool bCheckFormatterSettings)
{
	if (!Graph)
	{
		return false;
	}

	const FName GraphClassName = GetObjectClassName(Graph);
	if (UBASettings::Get().UseBlueprintFormattingForTheseGraphs.Contains(GraphClassName))
	{
		return true;
	}

	if (bCheckFormatterSettings)
	{
		if (FBAFormatterSettings* Settings = UBASettings::FindFormatterSettings(Graph))
		{
			if (Settings->bEnabled)
			{
				return Settings->FormatterType == EBAFormatterType::Blueprint;
			}
		}
	}

	return false;
}

bool FBAUtils::IsGraphNode(UEdGraphNode* Node)
{
	return Node && IsValid(Node) && !Node->IsUnreachable();
}

bool FBAUtils::IsEventNode(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
	// only has exec pins in formatter direction
	const bool bHasPinDirection = GetExecPins(Node, Direction).Num() > 0;
	const bool bNoPinOpposite = GetExecPins(Node, UEdGraphPin::GetComplementaryDirection(Direction)).Num() == 0;
	return bHasPinDirection && bNoPinOpposite;
}

bool FBAUtils::HasExecInOut(UEdGraphNode* Node)
{
	return GetExecPins(Node, EGPD_Input).Num() > 0 && GetExecPins(Node, EGPD_Output).Num() > 0;
}

bool FBAUtils::TryCreateConnection(FPinLink& PinLink, EBABreakMethod BreakMethod, bool bConversionAllowed, bool bTryHidden)
{
	if (PinLink.HasBothPins())
	{
		return TryCreateConnection(PinLink.FromNodePinHandle, PinLink.ToNodePinHandle, BreakMethod, bConversionAllowed, bTryHidden);
	}

	return false;
}

bool FBAUtils::TryCreateConnection(FBANodePinHandle& PinA, FBANodePinHandle& PinB, EBABreakMethod BreakMethod, bool bConversionAllowed, bool bTryHidden)
{
	if (!PinA.IsValid() || !PinB.IsValid())
	{
		return false;
	}

	if (!bTryHidden && PinB->bHidden)
	{
		return false;
	}

	const UEdGraphSchema* Schema = GetSchema(PinA.GetPin());
	check(Schema);
	const FPinConnectionResponse Response = Schema->CanCreateConnection(PinA.GetPin(), PinB.GetPin());
	bool bModified = false;

	TArray<UEdGraphPin*> PreviouslyLinked = PinA->LinkedTo;

	ECanCreateConnectionResponse NewResponse = Response.Response;

	switch (BreakMethod)
	{
		case EBABreakMethod::Always:
		{
			if (Response.Response == CONNECT_RESPONSE_MAKE ||
				Response.Response == CONNECT_RESPONSE_BREAK_OTHERS_A ||
				Response.Response == CONNECT_RESPONSE_BREAK_OTHERS_B)
			{
				NewResponse = CONNECT_RESPONSE_BREAK_OTHERS_AB;
			}

			break;
		}
		case EBABreakMethod::Never:
		{
			if (Response.Response == CONNECT_RESPONSE_BREAK_OTHERS_AB ||
				Response.Response == CONNECT_RESPONSE_BREAK_OTHERS_A ||
				Response.Response == CONNECT_RESPONSE_BREAK_OTHERS_B)
			{
				return false;
			}

			break;
		}
		case EBABreakMethod::Default: // default means that we just follow what the response tells us
		default: ;
	}

	switch (NewResponse)
	{
		case CONNECT_RESPONSE_MAKE:
		{
			SchemaTryCreateConnection_Internal(PinA, PinB);
			bModified = true;
			break;
		}

		case CONNECT_RESPONSE_BREAK_OTHERS_A:
		{
			SchemaBreakPinLinks(PinA, false);
			SchemaTryCreateConnection_Internal(PinA, PinB);
			bModified = true;
			break;
		}

		case CONNECT_RESPONSE_BREAK_OTHERS_B:
		{
			SchemaBreakPinLinks(PinB, false);
			SchemaTryCreateConnection_Internal(PinA, PinB);
			bModified = true;
			break;
		}

		case CONNECT_RESPONSE_BREAK_OTHERS_AB:
		{
			SchemaBreakPinLinks(PinA, false);
			SchemaBreakPinLinks(PinB, false);
			SchemaTryCreateConnection_Internal(PinA, PinB);
			bModified = true;
			break;
		}
		case CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE:
			if (bConversionAllowed)
			{
				bModified = Schema->CreateAutomaticConversionNodeAndConnections(PinA.GetPin(), PinB.GetPin());
			}
			break;
		case CONNECT_RESPONSE_DISALLOW:
			break;
		default:
			break;
	}

#if WITH_EDITOR
	if (bModified)
	{
		if (UEdGraphNode* NodeA = PinA.GetNode())
		{
			NodeA->PinConnectionListChanged(PinA.GetPin());
			NodeA->NodeConnectionListChanged();
		}

		if (UEdGraphNode* NodeB = PinB.GetNode())
		{
			NodeB->PinConnectionListChanged(PinB.GetPin());
			NodeB->NodeConnectionListChanged();
		}
	}
#endif

	return bModified;
}

bool FBAUtils::TryCreateConnectionUnsafe(UEdGraphPin* PinA, UEdGraphPin* PinB, EBABreakMethod BreakMethod, bool bConversionAllowed, bool bTryHidden)
{
	FBANodePinHandle HandleA(PinA);
	FBANodePinHandle HandleB(PinB);
	return TryCreateConnection(HandleA, HandleB, BreakMethod, bConversionAllowed, bTryHidden);
}

FString FBAUtils::GetNodeName(const UEdGraphNode* Node)
{
	if (Node == nullptr)
	{
		return FString("nullptr");
	}

	if (const UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node))
	{
		return Comment->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	}

	return Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
}

FString FBAUtils::GetGraphDisplayName(const UEdGraph* Graph)
{
	if (!Graph)
	{
		return FString("Null");
	}

	if (const UEdGraphSchema* GraphSchema = Graph->GetSchema())
	{
		FGraphDisplayInfo DisplayInfo;
		GraphSchema->GetGraphDisplayInformation(*Graph, DisplayInfo);
		return DisplayInfo.DisplayName.ToString();
	}

	return Graph->GetName();
}

FSlateRect FBAUtils::FSlateRectFromVectors(const FVector2D& A, const FVector2D& B)
{
	return FSlateRect(
		FMath::Min(A.X, B.X),
		FMath::Min(A.Y, B.Y),
		FMath::Max(A.X, B.X),
		FMath::Max(A.Y, B.Y));
}

int FBAUtils::DistanceSquaredBetweenNodes(UEdGraphNode* NodeA, UEdGraphNode* NodeB)
{
	const int DeltaX = NodeA->NodePosX - NodeB->NodePosX;
	const int DeltaY = NodeA->NodePosY - NodeB->NodePosY;
	return DeltaX * DeltaX + DeltaY * DeltaY;
}

bool FBAUtils::LineRectIntersection(
	const FSlateRect& Rect,
	const FVector2D& Start,
	const FVector2D& End)
{
	const FVector Min3D(Rect.GetTopLeft(), -1.f);
	const FVector Max3D(Rect.GetBottomRight(), 1.f);
	const FBox Box(Min3D, Max3D);

	const FVector Start3D(Start, 0.f);
	const FVector End3D(End, 0.f);
	const FVector StartToEnd3D = End3D - Start3D;

	return FMath::LineBoxIntersection(Box, Start3D, End3D, StartToEnd3D);
}

void FBAUtils::SetNodePosY(
	TSharedPtr<FBAGraphHandler> GraphHandler,
	UEdGraphNode* Node,
	const int NewY)
{
	const FSlateRect NodeBounds = GetCachedNodeBounds(GraphHandler, Node);
	const float DeltaY = NewY - NodeBounds.Top;
	Node->NodePosY += DeltaY;
}

void FBAUtils::SetNodePos(
	TSharedPtr<FBAGraphHandler> GraphHandler,
	UEdGraphNode* Node,
	const int NewX,
	const int NewY)
{
	Node->NodePosX = NewX;
	SetNodePosY(GraphHandler, Node, NewY);
}

void FBAUtils::SetNodePos(
	TSharedPtr<FBAGraphHandler> GraphHandler,
	UEdGraphNode* Node,
	const FVector2D& NewPos)
{
	SetNodePos(GraphHandler, Node, NewPos.X, NewPos.Y);
}

FString FBAUtils::GraphTypeToString(const EGraphType GraphType)
{
	switch (GraphType)
	{
		case GT_Function:
			return FString("Function");
		case GT_Ubergraph:
			return FString("Graph");
		case GT_Macro:
			return FString("Macro");
		case GT_Animation:
			return FString("Animation");
		case GT_StateMachine:
			return FString("StateMachine");
		case GT_MAX:
			return FString("MAX");
		default:
			return FString("BlueprintAssistUtils::GraphTypeToString: ERROR");
	}
}

EGraphType FBAUtils::GetGraphType(UEdGraph* Graph)
{
	if (Graph)
	{
		if (const UEdGraphSchema* Schema = Graph->GetSchema())
		{
			return Schema->GetGraphType(Graph);
		}
	}

	return EGraphType::GT_MAX;
}

bool FBAUtils::IsInputNode(UEdGraphNode* Node)
{
	return
		Cast<UK2Node_InputAction>(Node) || Cast<UK2Node_InputActionEvent>(Node) ||
		Cast<UK2Node_InputTouch>(Node) || Cast<UK2Node_InputTouchEvent>(Node) ||
		Cast<UK2Node_InputAxisEvent>(Node) ||
		Cast<UK2Node_InputAxisKeyEvent>(Node) ||
		Cast<UK2Node_InputKey>(Node) || Cast<UK2Node_InputKeyEvent>(Node) ||
		Cast<UK2Node_InputVectorAxisEvent>(Node) ||
		(Node && (GetObjectClassName(Node) == "K2Node_EnhancedInputAction"));
}

int FBAUtils::StraightenPin(
	TSharedPtr<FBAGraphHandler> GraphHandler,
	UEdGraphPin* SourcePin,
	UEdGraphPin* PinToAlign)
{
	if (!SourcePin || !PinToAlign)
	{
		return 0;
	}

	UEdGraphNode* NodeToAlign = PinToAlign->GetOwningNode();
	const int DeltaY = GraphHandler->GetPinY(SourcePin) - GraphHandler->GetPinY(PinToAlign);
	NodeToAlign->NodePosY += DeltaY;
	return DeltaY;
}

float FBAUtils::StraightenPin(
	TSharedPtr<FBAGraphHandler> GraphHandler,
	const FPinLink& PinLink)
{
	return StraightenPin(GraphHandler, PinLink.From, PinLink.To);
}

bool FBAUtils::ArePinsStraightened(TSharedPtr<FBAGraphHandler> GraphHandler, FPinLink& PinLink)
{
	UEdGraphPin* FromPin = PinLink.GetFromPin();
	UEdGraphPin* ToPin = PinLink.GetToPin();

	if (!FromPin || !ToPin)
	{
		return false;
	}

	const FVector2D FromPos = GetPinPos(GraphHandler, FromPin);
	const FVector2D ToPos = GetPinPos(GraphHandler, ToPin);

	// check the y position is correct
	if (PinLink.GetDirection() == EGPD_Output)
	{
		if (FMath::IsNearlyEqual(FromPos.Y, ToPos.Y) && (FromPos.X <= ToPos.X))
		{
			return true;
		}
	}
	else
	{
		if (FMath::IsNearlyEqual(FromPos.Y, ToPos.Y) && (FromPos.X >= ToPos.X))
		{
			return true;
		}
	}

	return false;
}

FSlateRect FBAUtils::GetGraphPanelBounds(TSharedPtr<SGraphPanel> GraphPanel)
{
	if (!GraphPanel)
	{
		return FSlateRect();
	}

	return FSlateRect(
		PanelCoordToGraphCoord(GraphPanel, FVector2D::ZeroVector),
		PanelCoordToGraphCoord(GraphPanel, GraphPanel->GetCachedGeometry().GetLocalSize()));
}

bool FBAUtils::IsNodeVisible(TSharedPtr<SGraphPanel> GraphPanel, UEdGraphNode* Node)
{
	if (!GraphPanel.IsValid())
	{
		return false;
	}

	const FSlateRect Bounds = GetGraphPanelBounds(GraphPanel);
	if (!Bounds.ContainsPoint(FVector2D(Node->NodePosX, Node->NodePosY)))
	{
		return false;
	}

	// TODO for some reason this seems inaccurate, so we use this point check earlier
	const FSlateRect NodeBounds = GetNodeBounds(Node);
	return GraphPanel->IsRectVisible(NodeBounds.GetTopLeft(), NodeBounds.GetBottomRight());
}

FString FBAUtils::GetPinName(UEdGraphPin* Pin, bool bIncludeOwningNode)
{
	if (Pin == nullptr)
	{
		return FString("nullptr");
	}

	FString PinName;

	if (Pin->PinFriendlyName.IsEmpty())
	{
		PinName = Pin->GetName().Replace(TEXT(" "), TEXT(""));
	}
	else
	{
		PinName = Pin->PinFriendlyName.ToString();
	}

	if (bIncludeOwningNode)
	{
		return FString::Printf(TEXT("%s (%s)"), *PinName, *GetNodeName(Pin->GetOwningNodeUnchecked()));
	}

	return PinName;
}

bool FBAUtils::IsWidgetOfType(TSharedPtr<SWidget> Widget, const FString& WidgetTypeName, bool bCheckContains)
{
	if (!Widget.IsValid())
	{
		return false;
	}

	return bCheckContains ? Widget->GetTypeAsString().Contains(WidgetTypeName) : Widget->GetTypeAsString() == WidgetTypeName;
}

bool FBAUtils::IsWidgetOfTypeFast(TSharedPtr<SWidget> Widget, const FName& WidgetTypeName)
{
	return Widget.IsValid() ? Widget->GetType() == WidgetTypeName : false; 
}

bool FBAUtils::IsWidgetOfAnyType(TSharedPtr<SWidget> Widget, const TArray<FName>& Types)
{
	for (const FName& Type : Types)
	{
		if (IsWidgetOfTypeFast(Widget, Type))
		{
			return true;
		}
	}

	return false;
}

bool FBAUtils::IsWidgetOfAnyType(TSharedPtr<SWidget> Widget, const TSet<FName>& Types)
{
	if (!Widget.IsValid())
	{
		return false;
	}

	return Types.Contains(Widget->GetType());
}

TSharedPtr<SWidget> FBAUtils::GetChildWidget(
	TSharedPtr<SWidget> Widget,
	const FString& WidgetClassName,
	bool bCheckContains)
{
	if (Widget.IsValid())
	{
		if (IsWidgetOfType(Widget, WidgetClassName, bCheckContains))
		{
			return Widget;
		}

		// iterate through children
		if (FChildren* Children = Widget->GetChildren())
		{
			for (int i = 0; i < Children->Num(); i++)
			{
				TSharedPtr<SWidget> ReturnWidget = GetChildWidget(Children->GetChildAt(i), WidgetClassName, bCheckContains);
				if (ReturnWidget.IsValid())
				{
					return ReturnWidget;
				}
			}
		}
	}

	return nullptr;
}

TSharedPtr<SWidget> FBAUtils::GetChildWidgetFast(TSharedPtr<SWidget> Widget, const FName& WidgetClassName)
{
	if (Widget.IsValid())
	{
		if (IsWidgetOfTypeFast(Widget, WidgetClassName))
		{
			return Widget;
		}

		// iterate through children
		if (FChildren* Children = Widget->GetChildren())
		{
			for (int i = 0; i < Children->Num(); i++)
			{
				TSharedPtr<SWidget> ReturnWidget = GetChildWidgetFast(Children->GetChildAt(i), WidgetClassName);
				if (ReturnWidget.IsValid())
				{
					return ReturnWidget;
				}
			}
		}
	}

	return nullptr;
}

TSharedPtr<SWidget> FBAUtils::GetChildWidgetByTypes(TSharedPtr<SWidget> Widget, const TArray<FName>& WidgetClassNames)
{
	if (Widget.IsValid())
	{
		if (IsWidgetOfAnyType(Widget, WidgetClassNames))
		{
			return Widget;
		}

		// iterate through children
		if (FChildren* Children = Widget->GetChildren())
		{
			for (int i = 0; i < Children->Num(); i++)
			{
				TSharedPtr<SWidget> ReturnWidget = GetChildWidgetByTypes(Children->GetChildAt(i), WidgetClassNames);
				if (ReturnWidget.IsValid())
				{
					return ReturnWidget;
				}
			}
		}
	}

	return nullptr;
}

TSharedPtr<SWidget> FBAUtils::GetChildWidget(TSharedPtr<SWidget> Widget, TFunctionRef<bool(TSharedPtr<SWidget>)> Pred, TSet<TSharedPtr<SWidget>>& Visited, bool bVisibleOnly)
{
	if (Widget.IsValid() && !Visited.Contains(Widget))
	{
		Visited.Add(Widget);

		if (Pred(Widget))
		{
			if (!bVisibleOnly || (Widget->GetVisibility() != EVisibility::Collapsed && Widget->GetVisibility() != EVisibility::Hidden))
			{
				return Widget;
			}
		}

		// iterate through children
		if (FChildren* Children = Widget->GetChildren())
		{
			for (int i = 0; i < Children->Num(); i++)
			{
				TSharedPtr<SWidget> ReturnWidget = GetChildWidget(Children->GetChildAt(i), Pred, Visited);
				if (ReturnWidget.IsValid() && Pred(ReturnWidget))
				{
					return ReturnWidget;
				}
			}
		}
	}

	return nullptr;
}

void FBAUtils::GetChildWidgets(
	TSharedPtr<SWidget> Widget,
	const FName& WidgetClassName,
	TArray<TSharedPtr<SWidget>>& OutWidgets)
{
	if (Widget.IsValid())
	{
		if (IsWidgetOfTypeFast(Widget, WidgetClassName))
		{
			OutWidgets.Add(Widget);
		}

		// iterate through children
		if (FChildren* Children = Widget->GetChildren())
		{
			for (int i = 0; i < Children->Num(); i++)
			{
				GetChildWidgets(Children->GetChildAt(i), WidgetClassName, OutWidgets);
			}
		}
	}
}

void FBAUtils::GetChildWidgetsByTypes(TSharedPtr<SWidget> Widget,
	const TArray<FName>& WidgetClassNames, TSet<TSharedPtr<SWidget>>& OutWidgets)
{
	if (Widget.IsValid())
	{
		if (IsWidgetOfAnyType(Widget, WidgetClassNames))
		{
			OutWidgets.Add(Widget);
		}

		// iterate through children
		if (FChildren* Children = Widget->GetChildren())
		{
			for (int i = 0; i < Children->Num(); i++)
			{
				GetChildWidgetsByTypes(Children->GetChildAt(i), WidgetClassNames, OutWidgets);
			}
		}
	}
}

TSharedPtr<SWindow> FBAUtils::GetParentWindow(TSharedPtr<SWidget> Widget)
{
	if (!Widget.IsValid())
	{
		return nullptr;
	}

	TSharedPtr<SWidget> CurrentWidget = Widget;
	while (CurrentWidget->IsParentValid())
	{
		CurrentWidget = CurrentWidget->GetParentWidget();
	}

	return StaticCastSharedPtr<SWindow>(CurrentWidget);
}

TSharedPtr<SWindow> FBAUtils::GetTopMostWindow(TSharedPtr<SWindow> Window)
{
	if (Window->IsTopmostWindow())
	{
		return Window;
	}

	TSharedPtr<SWindow> ParentWindow = Window->GetParentWindow();
	if (!ParentWindow.IsValid())
	{
		return Window;
	}

	return ParentWindow;
}

TSharedPtr<SWidget> FBAUtils::GetParentWidgetOfType(
	TSharedPtr<SWidget> Widget,
	const FString& ParentType,
	bool bCheckContains)
{
	if (!Widget.IsValid())
	{
		return nullptr;
	}

	if (IsWidgetOfType(Widget, ParentType, bCheckContains))
	{
		return Widget;
	}

	if (!Widget->IsParentValid())
	{
		return nullptr;
	}

	check(Widget->GetParentWidget() != Widget)

	TSharedPtr<SWidget> ReturnWidget = GetParentWidgetOfType(Widget->GetParentWidget(), ParentType, bCheckContains);
	if (ReturnWidget.IsValid())
	{
		return ReturnWidget;
	}

	return nullptr;
}

TSharedPtr<SWidget> FBAUtils::ScanParentContainersForTypes(TSharedPtr<SWidget> Widget, const TArray<FName>& Types, const FName& StopAtParent)
{
	struct FLocal
	{
		static TSharedPtr<SWidget> Impl(TSharedPtr<SWidget> Widget, const TArray<FName>& Types, const FName& StopAtParent, TSet<TSharedPtr<SWidget>>& Visited)
		{
			if (Visited.Contains(Widget))
			{
				return nullptr;
			}

			if (!Widget)
			{
				return nullptr;
			}

			const auto IsWidgetValid = [&Types](TSharedPtr<SWidget> Widget)
			{
				return IsWidgetOfAnyType(Widget, Types) && Widget->GetVisibility() != EVisibility::Collapsed && Widget->GetVisibility() != EVisibility::Hidden;
			};

			if (TSharedPtr<SWidget> FoundChild = GetChildWidget(Widget, IsWidgetValid, Visited))
			{
				return FoundChild;
			}

			Visited.Add(Widget);

			if (IsWidgetOfTypeFast(Widget, StopAtParent))
			{
				return nullptr;
			}

			return Impl(Widget->GetParentWidget(), Types, StopAtParent, Visited);
		}
	};

	TSet<TSharedPtr<SWidget>> Visited;
	return FLocal::Impl(Widget, Types, StopAtParent, Visited);
}

TSharedPtr<SGraphNode> FBAUtils::GetGraphNode(
	TSharedPtr<SGraphPanel> GraphPanel,
	UEdGraphNode* Node)
{
	if (Node == nullptr || !GraphPanel.IsValid())
	{
		return nullptr;
	}

	if (IsNodeDeleted(Node))
	{
		return nullptr;
	}

	TSharedPtr<SGraphNode> GraphNode_GUID = GraphPanel->GetNodeWidgetFromGuid(Node->NodeGuid);
	if (GraphNode_GUID.IsValid())
	{
		return GraphNode_GUID;
	}

	if (FChildren* Children = GraphPanel->GetChildren())
	{
		for (int i = 0; i < Children->Num(); i++)
		{
			TSharedPtr<SGraphNode> GraphNode = StaticCastSharedRef<SGraphNode>(Children->GetChildAt(i));

			if (GraphNode.IsValid())
			{
				if (GraphNode->GetNodeObj() == Node)
				{
					return GraphNode;
				}
			}
		}
	}

	return nullptr;
}

TSharedPtr<SCommentBubble> FBAUtils::GetCommentBubble(TSharedPtr<SGraphNode> GraphNode)
{
	if (SNodePanel::SNode::FNodeSlot* CommentSlot = GraphNode->GetSlot(ENodeZone::TopCenter))
	{
		return StaticCastSharedRef<SCommentBubble>(CommentSlot->GetWidget());
	}

	return nullptr;
}

TSharedPtr<SGraphPin> FBAUtils::GetGraphPin(TSharedPtr<SGraphPanel> GraphPanel, UEdGraphPin* Pin)
{
	if (!IsValidPin(Pin) || !GraphPanel.IsValid())
	{
		return nullptr;
	}

	TSharedPtr<SGraphNode> GraphNode = GetGraphNode(GraphPanel, Pin->GetOwningNode());
	return GraphNode.IsValid() ? GraphNode->FindWidgetForPin(Pin) : nullptr;
}

TSharedPtr<SGraphPin> FBAUtils::GetGraphPin(TSharedPtr<SGraphNode> GraphNode, UEdGraphPin* Pin)
{
	if (!GraphNode.IsValid() || !IsValidPin(Pin))
	{
		return nullptr;
	}

	return GraphNode.IsValid() ? GraphNode->FindWidgetForPin(Pin) : nullptr;
}

TSharedPtr<SGraphPanel> FBAUtils::GetHoveredGraphPanel()
{
	FSlateApplication& SlateApp = FSlateApplication::Get();
	FWidgetPath Path = SlateApp.LocateWindowUnderMouse(SlateApp.GetCursorPos(), SlateApp.GetInteractiveTopLevelWindows());
	if (Path.IsValid())
	{
		for (int32 PathIndex = Path.Widgets.Num() - 1; PathIndex >= 0; PathIndex--)
		{
			TSharedRef<SWidget> Widget = Path.Widgets[PathIndex].Widget;
			TSharedPtr<SWidget> GraphPanelAsWidget = GetChildWidget(Widget, "SGraphPanel");
			if (GraphPanelAsWidget.IsValid())
			{
				auto GraphPanel = StaticCastSharedPtr<SGraphPanel>(GraphPanelAsWidget);
				if (GraphPanel.IsValid())
				{
					return GraphPanel;
				}
			}
		}
	}

	return nullptr;
}

TSharedPtr<SGraphPin> FBAUtils::GetHoveredGraphPin(TSharedPtr<SGraphPanel> GraphPanel)
{
	TSharedPtr<SGraphNode> HoveredNode = GetHoveredGraphNode(GraphPanel);
	if (!HoveredNode.IsValid())
	{
		return nullptr;
	}

	TSet<TSharedRef<SWidget>> HoveredNodePins;
	HoveredNode->GetPins(HoveredNodePins);

	FSlateApplication& SlateApp = FSlateApplication::Get();
	FWidgetPath WidgetPath = SlateApp.LocateWindowUnderMouse(SlateApp.GetCursorPos(), SlateApp.GetInteractiveTopLevelWindows());

	for (int i = WidgetPath.Widgets.Num() - 1; i >= 0; --i)
	{
		const TSharedRef<SWidget>& Widget = WidgetPath.Widgets[i].Widget;

		// don't bother checking past the graph panel
		if (Widget == GraphPanel)
		{
			return nullptr;
		}

		if (HoveredNodePins.Contains(Widget))
		{
			return StaticCastSharedRef<SGraphPin>(Widget);
		}
	}

	return nullptr;
}

FPinLink FBAUtils::GetHoveredPinLink(TSharedPtr<SGraphPanel> GraphPanel)
{
	if (!GraphPanel.IsValid())
	{
		return FPinLink();
	}

	const FGraphSplineOverlapResult& SplineOverlap = GraphPanel->GetPreviousFrameSplineOverlap();

	UEdGraphPin* PinA = nullptr;
	UEdGraphPin* PinB = nullptr;

	// returns true if both pins are not null
	if (SplineOverlap.GetPins(*GraphPanel.Get(), PinA, PinB)) 
	{
		return FPinLink(PinA, PinB);
	}

	return FPinLink();
}

UEdGraphPin* FBAUtils::GetHoveredPin(TSharedPtr<SGraphPanel> GraphPanel)
{
	if (TSharedPtr<SGraphPin> GraphPin = GetHoveredGraphPin(GraphPanel))
	{
		return GraphPin->GetPinObj();
	}

	return nullptr;
}

TSharedPtr<SGraphNode> FBAUtils::GetHoveredGraphNode(TSharedPtr<SGraphPanel> GraphPanel)
{
	if (!GraphPanel.IsValid())
	{
		return nullptr;
	}

	UEdGraph* Graph = GraphPanel->GetGraphObj();
	if (Graph == nullptr)
	{
		return nullptr;
	}

	// NOTE: Ruling this optimization causing a crash. This could also issues when relying on engine's SNode OnMouseEnter / OnMouseLeave
	// Perhaps a better optimization is to check the cursor widget path?
#if 0 // BA_UE_VERSION_OR_LATER(5, 6)
	if (UBASettings_Advanced::Get().bUsePanelHoveredNode)
	{
		if (UEdGraphNode* HoveredNodeObj = Cast<UEdGraphNode>(GraphPanel->GetCurrentHoveredNode()))
		{
			return GetGraphNode(GraphPanel, HoveredNodeObj);
		}

		return nullptr;
	}
#endif

	const FVector2D CursorInPanel = FBAUtils::ScreenSpaceToPanelCoord(GraphPanel, FSlateApplication::Get().GetCursorPos());

	TArray<UEdGraphNode*> CommentNodes;
	TArray<UEdGraphNode*> RegularNodes;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		if (Node->IsA(UEdGraphNode_Comment::StaticClass()))
		{
			CommentNodes.Add(Node);
		}
		else
		{
			RegularNodes.Add(Node);
		}
	}

	const auto NodeArrays = { &RegularNodes, &CommentNodes };
	for (const auto& NodeArray : NodeArrays )
	{
		for (UEdGraphNode* Node : *NodeArray )
		{
			TSharedPtr<SGraphNode> GraphNode = FBAUtils::GetGraphNode(GraphPanel, Node);
			if (!GraphNode)
			{
				continue;
			}

			const FSlateRect Bounds = FBAUtils::GetNodeBounds(GraphNode);
			if (Bounds.ContainsPoint(CursorInPanel))
			{
				return GraphNode;
			}
		}
	}

	return nullptr;
}

UEdGraphNode* FBAUtils::GetHoveredNode(TSharedPtr<SGraphPanel> GraphPanel)
{
	if (TSharedPtr<SGraphNode> GraphNode = GetHoveredGraphNode(GraphPanel))
	{
		return GraphNode->GetNodeObj();
	}

	return nullptr;
}


TSharedPtr<SWidget> FBAUtils::GetLastHoveredWidget()
{
	FSlateApplication& SlateApp = FSlateApplication::Get();
	FWidgetPath WidgetPath = SlateApp.LocateWindowUnderMouse(SlateApp.GetCursorPos(), SlateApp.GetInteractiveTopLevelWindows());
	return WidgetPath.IsValid() ? WidgetPath.Widgets.Last().Widget : TSharedPtr<SWidget>();
}

bool FBAUtils::ContainsWidgetInFront(TSharedPtr<SWidget> Widget, const TSet<FName>& WidgetTypesInFront)
{
	if (!Widget.IsValid() || !WidgetTypesInFront.Num())
	{
		return false;
	}

	FSlateApplication& SlateApp = FSlateApplication::Get();
	FWidgetPath Path = SlateApp.LocateWindowUnderMouse(SlateApp.GetCursorPos(), SlateApp.GetInteractiveTopLevelWindows());

	for (int i = Path.Widgets.Num() - 1; i >= 0; --i)
	{
		const FArrangedWidget& ArrangedWidget = Path.Widgets[i];
		for (const FName& WidgetType : WidgetTypesInFront)
		{
			if (IsWidgetOfTypeFast(ArrangedWidget.Widget, WidgetType))
			{
				// UE_LOG(LogTemp, Warning, TEXT("Found widget %s in %s"), *WidgetType.ToString(), *ArrangedWidget.Widget->ToString());
				return true;
			}
		}

		if (ArrangedWidget.Widget == Widget)
		{
			return false;
		}
	}

	return false;
}

void FBAUtils::PrintChildWidgets(TSharedPtr<SWidget> Widget)
{
	if (Widget.IsValid())
	{
		UE_LOG(LogBlueprintAssist, Warning, TEXT("ChildWidget: %s | %s"),
			*Widget->GetTypeAsString(),
			*Widget->ToString());

		if (FChildren* Children = Widget->GetChildren())
		{
			for (int i = 0; i < Children->Num(); i++)
			{
				PrintChildWidgets(Children->GetChildAt(i));
			}
		}
	}
}

float FBAUtils::SnapToGrid(const float& InFloat, EBARoundingMethod RoundingMethod, int NumGridTiles)
{
	if (InFloat != 0)
	{
		const float SnapSize = SNodePanel::GetSnapGridSize() * NumGridTiles;

		switch (RoundingMethod)
		{
			case EBARoundingMethod::Round:
				return SnapSize * FMath::RoundToFloat(InFloat / SnapSize);
			case EBARoundingMethod::Ceil:
				return SnapSize * FMath::CeilToFloat(InFloat / SnapSize);
			case EBARoundingMethod::Floor:
				return SnapSize * FMath::FloorToFloat(InFloat / SnapSize);
		}
	}

	return 0;
}

FVector2D FBAUtils::SnapToGrid(const FVector2D& Position)
{
	const float SnapSize = SNodePanel::GetSnapGridSize();
	return FVector2D(
		SnapSize * FMath::RoundToFloat(Position.X / SnapSize),
		SnapSize * FMath::RoundToFloat(Position.Y / SnapSize)
	);
}

float FBAUtils::AlignTo8x8Grid(const float& InFloat, EBARoundingMethod RoundingMethod)
{
	if (UBASettings::Get().bAlignExecNodesTo8x8Grid)
	{
		return SnapToGrid(InFloat, RoundingMethod, 8);
	}

	return InFloat;
}

bool FBAUtils::IsUserInputWidget(TSharedPtr<SWidget> Widget)
{
	const static TArray<FString> UserInputTypes
	{
		"SEditableText", 
		"SMultiLineEditableTextBox", 
		"SMultiLineEditableText", 
		"SSearchBox",
	};

	const FString Type = Widget->GetTypeAsString();
	if (UserInputTypes.Contains(Type))
	{
		return true;
	}

	const bool Contains = Type.Contains("SSpinBox");
	return Contains;
}

bool FBAUtils::IsClickableWidget(TSharedPtr<SWidget> Widget)
{
	if (!Widget.IsValid())
	{
		return false;
	}

	if (IsComboWidget(Widget))
	{
		return true;
	}

	const static TSet<FName> Types = { "SCheckBox", "SColorBlock" };
	if (IsWidgetOfAnyType(Widget, Types))
	{
		return true;
	}

	return Widget->GetTypeAsString().Contains("Button");
}

bool FBAUtils::IsComboWidget(TSharedPtr<SWidget> Widget)
{
	if (!Widget.IsValid())
	{
		return false;
	}

	const static TSet<FName> ComboTypes = {
		"SComboBox", "SComboButton", "SComponentClassCombo", "SPropertyComboBox", "STextComboBox",
		"SSearchableComboBox", "SIpAddressComboBox", "SNameComboBox", "SStateTreeEditorColorComboBox",
		"SComboPanelIconTile", "SComboRow", "SDataTableStructComboBox" "SEnumComboBox",
	};

	if (IsWidgetOfAnyType(Widget, ComboTypes))
	{
		return true;
	}

	return false;
}

FVector2D FBAUtils::GraphCoordToPanelCoord(
	TSharedPtr<SGraphPanel> GraphPanel,
	const FVector2D& GraphSpaceCoordinate)
{
	return (GraphSpaceCoordinate - GraphPanel->GetViewOffset()) * GraphPanel->GetZoomAmount();
}

FVector2D FBAUtils::PanelCoordToGraphCoord(TSharedPtr<SGraphPanel> GraphPanel, const FVector2D& PanelSpaceCoordinate)
{
	return (PanelSpaceCoordinate / GraphPanel->GetZoomAmount()) + GraphPanel->GetViewOffset();  
}

FVector2D FBAUtils::ScreenSpaceToPanelCoord(TSharedPtr<SGraphPanel> GraphPanel, const FVector2D& ScreenSpace)
{
	return GraphPanel->PanelCoordToGraphCoord(GraphPanel->GetCachedGeometry().AbsoluteToLocal(ScreenSpace));
}

FName FBAUtils::GetObjectClassName(const UObject* Obj)
{
	return Obj ? Obj->GetClass()->GetFName() : FName(); 
}

const UEdGraph* FBAUtils::GetGraph(UEdGraphPin* Pin)
{
	if (Pin)
	{
		if (const UEdGraphNode* Node = Pin->GetOwningNodeUnchecked())
		{
			return Node->GetGraph();
		}
	}

	return nullptr;
}

const UEdGraphSchema* FBAUtils::GetSchema(UEdGraphPin* Pin)
{
	if (Pin)
	{
		return GetSchema(Pin->GetOwningNodeUnchecked());
	}

	return nullptr;
}

const UEdGraphSchema* FBAUtils::GetSchema(UEdGraphNode* Node)
{
	if (Node)
	{
		if (UEdGraph* Graph = Node->GetGraph())
		{
			return Graph->GetSchema();
		}
	}

	return nullptr;
}

void FBAUtils::SchemaBreakPinLinks(FBANodePinHandle& Pin, bool bSendsNotification, bool bModify)
{
	if (const UEdGraphSchema* Schema = GetSchema(Pin.GetPin()))
	{
		if (bModify)
		{
			Pin->Modify();
		}

		Schema->BreakPinLinks(*Pin.GetPin(), bSendsNotification);
	}
}

void FBAUtils::SchemaBreakSinglePinLink(FBANodePinHandle& A, FBANodePinHandle& B, bool bModify)
{
	if (A.IsValid() && B.IsValid())
	{
		if (const UEdGraphSchema* Schema = GetSchema(A.GetPin()))
		{
			if (bModify)
			{
				A->Modify();
				B->Modify();
			}

			Schema->BreakSinglePinLink(A.GetPin(), B.GetPin());

			if (const USoundCueGraph* SoundCueGraph = Cast<USoundCueGraph>(GetGraph(A.GetPin())))
			{
				CastChecked<USoundCueGraph>(SoundCueGraph)->GetSoundCue()->CompileSoundNodesFromGraphNodes();
			}
		}
	}
}

void FBAUtils::SchemaBreakSinglePinLink(FPinLink& PinLink, bool bModify)
{
	if (!PinLink.HasBothPins())
	{
		return;
	}

	static const FName PCGGraphName("PCGEditorGraph");
	const FName GraphName = GetObjectClassName(GetGraph(PinLink.GetFromPin()));

	// some issues in PCG graph causing SchemaBreakSinglePinLink to not work
	// maybe due to FPCGDeferNodeReconstructScope not being called in the schema?
	if (GraphName == PCGGraphName)
	{
		SchemaBreakPinLinks(PinLink.ToNodePinHandle, true, bModify);
		SchemaBreakPinLinks(PinLink.FromNodePinHandle, true, bModify);
	}
	else
	{
		SchemaBreakSinglePinLink(PinLink.FromNodePinHandle, PinLink.ToNodePinHandle, bModify);
	}
}

void FBAUtils::SchemaSetNodePos(TSharedPtr<SGraphPanel> GraphPanel, UEdGraphNode* Node, const FBAVector2& NewPos)
{
	if (!GraphPanel.IsValid() || !Node)
	{
		return;
	}

	Node->Modify();

	// GraphNode::MoveTo is the more important function to call
	// only a few graph types implement the Schema::SetNodePosition and many instead rely on MoveTo
	// see: SGraphNode_BehaviorTree, SGraphNodeMaterialBase  
	if (TSharedPtr<SGraphNode> GraphNode = GetGraphNode(GraphPanel, Node))
	{
		TSet<TWeakPtr<SNodePanel::SNode>> NodeSet;
		GraphNode->MoveTo(NewPos, NodeSet);
	}

#if BA_UE_VERSION_OR_LATER(5, 0)
	Node->GetSchema()->SetNodePosition(Node, NewPos);
#else
	Node->NodePosX = NewPos.X;
	Node->NodePosY = NewPos.Y;
#endif
}

bool FBAUtils::SchemaTryCreateConnection_Internal(FBANodePinHandle& A, FBANodePinHandle& B, bool bModify)
{
	if (A.IsValid() && B.IsValid())
	{
		if (const UEdGraphSchema* Schema = GetSchema(A.GetPin()))
		{
			if (bModify)
			{
				A->Modify();
				B->Modify();
			}

			return Schema->TryCreateConnection(A.GetPin(), B.GetPin());
		}
	}

	return false;
}

bool FBAUtils::CanConnectPins(
	UEdGraphPin* PinA,
	UEdGraphPin* PinB,
	bool bOverrideLinks,
	bool bAcceptConversions,
	bool bAcceptHiddenPins)
{
	if (PinA == nullptr || PinB == nullptr)
	{
		return false;
	}

	const UEdGraphSchema* Schema = PinA->GetOwningNodeUnchecked()->GetGraph()->GetSchema();
	ECanCreateConnectionResponse Response = Schema->CanCreateConnection(PinA, PinB).Response;

	if (!bAcceptHiddenPins && (PinA->bHidden || PinB->bHidden))
	{
		return false;
	}

	switch (Response)
	{
		case CONNECT_RESPONSE_MAKE:
			return true;
		case CONNECT_RESPONSE_DISALLOW:
			return false;
		case CONNECT_RESPONSE_BREAK_OTHERS_A:
			return bOverrideLinks;
		case CONNECT_RESPONSE_BREAK_OTHERS_B:
			return bOverrideLinks;
		case CONNECT_RESPONSE_BREAK_OTHERS_AB:
			return bOverrideLinks;
		case CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE:
			return bAcceptConversions;
		case CONNECT_RESPONSE_MAX:
			return false;
		default:
			return false;
	}
}

void FBAUtils::InteractWithWidget(TSharedPtr<SWidget> Widget)
{
	if (!Widget.IsValid())
	{
		return;
	}

	if (FBAUtils::IsClickableWidget(Widget))
	{
		// Combo buttons should be clicked not keyboard pressed, e.g. SelectObject in Construct Object or AddComponent btn to actor
		const bool bComboButton = FBAUtils::IsComboWidget(Widget);
		if (bComboButton)
		{
			TSharedPtr<SWidget> ChildButton = GetChildWidget(Widget, "SButton");
			if (ChildButton.IsValid())
			{
				Widget = ChildButton;
			}
		}

		// Not sure why check box click is not working, but send a key event instead
		const bool bNeedsKeyboardEvent = FBAUtils::IsWidgetOfType(Widget,  "SCheckBox") || FBAUtils::IsWidgetOfType(Widget, "Button", true);
		if (!bComboButton && bNeedsKeyboardEvent)
		{
			FKeyEvent KeyEvent(
				EKeys::Virtual_Accept,
				FModifierKeysState(),
				FSlateApplication::Get().GetUserIndexForKeyboard(),
				false,
				-1,
				-1);

			Widget->OnKeyDown(Widget->GetTickSpaceGeometry(), KeyEvent);
			Widget->OnKeyUp(Widget->GetTickSpaceGeometry(), KeyEvent);
			return;
		}

		TSet<FKey> PressedKeys;
		const FGeometry CachedGeometry = Widget->GetTickSpaceGeometry();
		const FVector2D GeometryCenter = CachedGeometry.GetAbsolutePosition() + CachedGeometry.
			GetAbsoluteSize() * 0.5f;

		FPointerEvent PointerEvent(
			FSlateApplication::Get().CursorPointerIndex,
			GeometryCenter,
			GeometryCenter,
			PressedKeys,
			EKeys::LeftMouseButton,
			0.f,
			FModifierKeysState());

		FReply TheReply = Widget->OnMouseButtonDown(Widget->GetTickSpaceGeometry(), PointerEvent);

		// Maybe we should use FSlateApplication::ProcessReply, but for now this works
		if (TheReply.ShouldSetUserFocus() || TheReply.GetUserFocusRecepient().IsValid())
		{
			FSlateApplication::Get().SetUserFocus(0, TheReply.GetUserFocusRecepient());
			FSlateApplication::Get().SetKeyboardFocus(TheReply.GetUserFocusRecepient());
		}

		Widget->OnMouseButtonUp(Widget->GetTickSpaceGeometry(), PointerEvent);
	}
	else if (FBAUtils::IsUserInputWidget(Widget))
	{
		FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::Navigation);
	}
}

void FBAUtils::TryClickOnWidget(TSharedPtr<SWidget> Widget)
{
	TSet<FKey> PressedKeys;
	const FGeometry CachedGeometry = Widget->GetTickSpaceGeometry();
	const FVector2D GeometryCenter = CachedGeometry.GetAbsolutePosition() + CachedGeometry.
		GetAbsoluteSize() * 0.5f;

	FPointerEvent PointerEvent(
		FSlateApplication::Get().CursorPointerIndex,
		GeometryCenter,
		GeometryCenter,
		PressedKeys,
		EKeys::LeftMouseButton,
		0.f,
		FModifierKeysState());

	FReply TheReply = Widget->OnMouseButtonDown(Widget->GetTickSpaceGeometry(), PointerEvent);

	// Maybe we should use FSlateApplication::ProcessReply, but for now this works
	if (TheReply.ShouldSetUserFocus() || TheReply.GetUserFocusRecepient().IsValid())
	{
		FSlateApplication::Get().SetUserFocus(0, TheReply.GetUserFocusRecepient());
		FSlateApplication::Get().SetKeyboardFocus(TheReply.GetUserFocusRecepient());
	}

	Widget->OnMouseButtonUp(Widget->GetTickSpaceGeometry(), PointerEvent);
}

TSharedPtr<SWidget> FBAUtils::GetInteractableChildWidget(TSharedPtr<SWidget> Widget)
{
	struct FLocal
	{
		static TSharedPtr<SWidget> GetInteractableWidget(TSharedPtr<SWidget> CurrentWidget)
		{
			if (FBAUtils::IsUserInputWidget(CurrentWidget) || FBAUtils::IsClickableWidget(
				CurrentWidget))
			{
				return CurrentWidget;
			}

			if (FChildren* Children = CurrentWidget->GetChildren())
			{
				for (int i = 0; i < Children->Num(); i++)
				{
					TSharedPtr<SWidget> OutWidget = GetInteractableWidget(Children->GetChildAt(i));
					if (OutWidget.IsValid())
					{
						return OutWidget;
					}
				}
			}

			return nullptr;
		}
	};

	return FLocal::GetInteractableWidget(Widget);
}

bool FBAUtils::DoesPinTypeCategoryObjectMatchName(const UEdGraphPin* Pin, const FName& StructName)
{
	// Do we need to test if it is a struct?
	// Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct
	if (Pin->PinType.PinSubCategoryObject.IsValid() && (Pin->PinType.PinSubCategoryObject->GetFName() == StructName))
	{
		return true;
	}

	if (Pin->PinType.PinCategory == StructName)
	{
		return true;
	}

	return false;
}

void FBAUtils::GetInteractableChildWidgets(
	TSharedPtr<SWidget> Widget,
	TArray<TSharedPtr<SWidget>>& OutWidgets,
	bool bCheckEditable,
	bool bCheckClickable)
{
	struct FLocal
	{
		static void GetInteractableWidget(
			TSharedPtr<SWidget> CurrentWidget,
			TArray<TSharedPtr<SWidget>>& OutWidgets,
			bool bCheckEditable,
			bool bCheckClickable)
		{
			if ((bCheckEditable && FBAUtils::IsUserInputWidget(CurrentWidget)) ||
				(bCheckClickable && FBAUtils::IsClickableWidget(CurrentWidget)))
			{
				OutWidgets.Add(CurrentWidget);
			}

			if (FChildren* Children = CurrentWidget->GetChildren())
			{
				for (int i = 0; i < Children->Num(); i++)
				{
					GetInteractableWidget(Children->GetChildAt(i), OutWidgets, bCheckEditable, bCheckClickable);
				}
			}
		}
	};

	FLocal::GetInteractableWidget(Widget, OutWidgets, bCheckEditable, bCheckClickable);
}

UEdGraphNode* FBAUtils::GetTopMost(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
	const auto Pred = [](UEdGraphNode*) { return true; };
	return GetTopMostWithFilter(Node, Direction, Pred);
}

UEdGraphNode* FBAUtils::GetTopMostWithFilter(
	UEdGraphNode* Node,
	EEdGraphPinDirection Direction,
	TFunctionRef<bool (UEdGraphNode*)> Pred)
{
	struct FLocalInfo
	{
		UEdGraphNode* Node;
		int Top;
		int Right;

		FLocalInfo(UEdGraphNode* InNode, const int InTop, const int InRight)
			: Node(InNode)
			, Top(InTop)
			, Right(InRight) { }
	};

	struct FLocal
	{
		TArray<UEdGraphNode*> VisitedNodes;

		FLocalInfo Recursive_TopMost(
			FLocalInfo& Info,
			UEdGraphPin* InPin,
			const EEdGraphPinDirection InDirection,
			TFunctionRef<bool (UEdGraphNode*)> Pred)
		{
			TArray<EEdGraphPinDirection> InputOutput
				= InDirection == EGPD_Output
				? TArray<EEdGraphPinDirection>{ EGPD_Input, EGPD_Output }
				: TArray<EEdGraphPinDirection>{ EGPD_Output, EGPD_Input };

			const int32 PinIndex = InPin == nullptr
				? 0
				: FBAUtils::GetLinkedPins(Info.Node, InPin->Direction).IndexOfByKey(InPin);

			Info.Top -= PinIndex;

			TArray<FLocalInfo> Children;
			for (EEdGraphPinDirection Direction : InputOutput)
			{
				const int NewRight = Info.Right + (Direction == InDirection ? 1 : -1);
				int ChildIndexOffset = 0;
				for (UEdGraphPin* ParentPin : FBAUtils::GetLinkedPins(Info.Node, Direction))
				{
					for (UEdGraphPin* LinkedPin : ParentPin->LinkedTo)
					{
						UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
						if (!Pred(LinkedNode))
						{
							continue;
						}

						const int NewTop = Info.Top + ChildIndexOffset;

						ChildIndexOffset += 1;

						if (VisitedNodes.Contains(LinkedNode))
						{
							continue;
						}

						VisitedNodes.Add(LinkedNode);

						FLocalInfo LinkedInfo(LinkedNode, NewTop, NewRight);
						Children.Add(
							Recursive_TopMost(LinkedInfo, LinkedPin, InDirection, Pred));
					}
				}
			}
			Children.Add(Info);

			const auto& TopRightMost = [](const FLocalInfo& A, const FLocalInfo& B)
			{
				if (A.Top != B.Top)
				{
					return A.Top < B.Top;
				}

				return A.Right > B.Right;
			};
			Children.Sort(TopRightMost);

			return Children[0];
		}
	};

	FLocal Local;
	FLocalInfo RootInfo(Node, 0, 0);
	return Local.Recursive_TopMost(RootInfo, nullptr, Direction, Pred).Node;
}

void FBAUtils::SafeDelete(TSharedPtr<FBAGraphHandler> GraphHandler, UEdGraphNode* Node)
{
	if (!Node)
	{
		return;
	}

	UEdGraph* Graph = Node->GetGraph();
	if (!Graph)
	{
		return;
	}

	// from FBlueprintEditor::DeleteSelectedNodes, close related tabs for the node
	if (FBlueprintEditor* BlueprintEditor = FBAUtils::GetBlueprintEditorForGraph(Node->GetGraph()))
	{
		if (Node->CanUserDeleteNode())
		{
			auto CloseAllDocumentsTab = [BlueprintEditor](const UEdGraphNode* InNode)
			{
				TArray<UObject*> NodesToClose;
				GetObjectsWithOuter(InNode, NodesToClose);
				for (UObject* Node : NodesToClose)
				{
					if (UEdGraph* NodeGraph = Cast<UEdGraph>(Node))
					{
						BlueprintEditor->CloseDocumentTab(NodeGraph);
					}
				}
			};

#if BA_UE_VERSION_OR_LATER(5, 0)
			if (Node->GetSubGraphs().Num() > 0)
			{
				CloseAllDocumentsTab(Node);
			}
#else
			// check by name so we don't have to include AnimGraph module
			FName NodeClassName = GetObjectClassName(Node);
			if (NodeClassName == "AnimStateNode" ||
				NodeClassName == "AnimStateConduitNode" ||
				NodeClassName == "AnimStateTransitionNode" ||
				NodeClassName == "AnimGraphNode_StateMachine" ||
				Node->IsA<UK2Node_Composite>())
			{
				CloseAllDocumentsTab(Node);
			}
#endif
		}
	}

	// deselect the node
	if (TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel())
	{
		GraphPanel->SelectionManager.SetNodeSelection(Node, false);
	}

	// try to call the safe delete function
	if (const UEdGraphSchema* Schema = Graph->GetSchema())
	{
		if (Schema->SafeDeleteNodeFromGraph(Graph, Node))
		{
			// post cleanup tabs (we shouldn't need to do this later cause blueprint editor should have this function defined)
			if (FBlueprintEditor* BlueprintEditor = FBAUtils::GetBlueprintEditorForGraph(Node->GetGraph()))
			{
				if (TSharedPtr<FDocumentTracker> DocumentManager = BlueprintEditor->DocumentManager)
				{
#if BA_UE_VERSION_OR_LATER(5, 0)
					if (Node->GetSubGraphs().Num() > 0)
					{
						DocumentManager->CleanInvalidTabs();
					}
#else
					if (UK2Node_Composite* SelectedNode = Cast<UK2Node_Composite>(Node))
					{
						// close the tab for the composite if it was open
						if (SelectedNode->BoundGraph)
						{
							DocumentManager->CleanInvalidTabs();
						}
					}
#endif
					else if (UK2Node_Timeline* TimelineNode = Cast<UK2Node_Timeline>(Node))
					{
						DocumentManager->CleanInvalidTabs();
					}
				}
			}

			return;
		}
	}

	// schema hasn't defined delete, remove the node using custom logic
	Graph->Modify();

	bool bNeedToModifyStructurally = false;

	if (Node != nullptr)
	{
		if (Node->CanUserDeleteNode())
		{
			UK2Node* K2Node = Cast<UK2Node>(Node);
			if (K2Node != nullptr && K2Node->NodeCausesStructuralBlueprintChange())
			{
				bNeedToModifyStructurally = true;
			}

			UBlueprint* Blueprint = GraphHandler->GetBlueprint();
			if (Blueprint != nullptr)
			{
				FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);

				if (bNeedToModifyStructurally)
				{
					FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
				}
				else
				{
					FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
				}
				
				return;
			}

			Node->Modify();
			Node->DestroyNode();
		}
	}
}

bool FBAUtils::IsPinLinkedToNode(UEdGraphPin* SourcePin, UEdGraphNode* NodeToCheck, const TSet<UEdGraphNode*>& IgnoredNodes)
{
	TSet<UEdGraphNode*> VisitedNodes;
	TArray<UEdGraphNode*> PendingNodes;

	VisitedNodes.Add(SourcePin->GetOwningNode());

	for (UEdGraphPin* Pin : SourcePin->LinkedTo)
	{
		if (FBAUtils::IsExecPin(Pin))
		{
			PendingNodes.Add(Pin->GetOwningNode());
			VisitedNodes.Add(Pin->GetOwningNode());
		}
	}

	while (PendingNodes.Num() > 0)
	{
		UEdGraphNode* NextNode = PendingNodes.Pop();

		if (NextNode == NodeToCheck)
		{
			return true;
		}

		if (IgnoredNodes.Contains(NextNode))
		{
			continue;
		}

		TArray<UEdGraphPin*> LinkedExecPins = FBAUtils::GetLinkedToPins(NextNode).FilterByPredicate(FBAUtils::IsExecPin);
		for (UEdGraphPin* Pin : LinkedExecPins)
		{
			if (VisitedNodes.Contains(Pin->GetOwningNode()))
			{
				continue;
			}

			VisitedNodes.Add(Pin->GetOwningNode());
			PendingNodes.Add(Pin->GetOwningNode());
		}
	}

	return false;
}

TArray<FPinLink> FBAUtils::GetPinLinks(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
	TArray<FPinLink> OutLinks;
	for (UEdGraphPin* Pin : GetLinkedPins(Node, Direction))
	{
		for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			OutLinks.Add(FPinLink(Pin, LinkedPin));
		}
	}

	return OutLinks;
}

TArray<FPinLink> FBAUtils::GetPinLinksIgnoringKnots(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
	TArray<FPinLink> OutLinks;
	for (UEdGraphPin* Pin : GetLinkedPins(Node, Direction))
	{
		for (UEdGraphPin* LinkedPin : FBAUtils::GetPinLinkedToIgnoringKnots(Pin))
		{
			OutLinks.Add(FPinLink(Pin, LinkedPin));
		}
	}

	return OutLinks;
}

UEdGraphPin* FBAUtils::GetPinFromGraph(const FGraphPinHandle& Handle, UEdGraph* Graph)
{
	if (!Graph)
	{
		return nullptr;
	}

	for (auto Node : Graph->Nodes)
	{
		if (Node->NodeGuid == Handle.NodeGuid)
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->PinId == Handle.PinId)
				{
					return Pin;
				}
			}
		}
	}

	return nullptr;
}

UEdGraphNode* FBAUtils::GetNodeFromGraph(const UEdGraph* Graph, const FGuid& NodeGuid)
{
	if (!Graph)
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node->NodeGuid == NodeGuid)
		{
			return Node;
		}
	}

	return nullptr;
}

bool FBAUtils::IsExtraRootNode(UEdGraphNode* Node)
{
	if (FBAMetaData* MetaData = GetNodeMetaData(Node))
	{
		if (MetaData->HasValue(Node, FNodeMetadata::DefaultGraphNode))
		{
			return true;
		}
	}
	
	if (FBAFormatterSettings* FormatterSettings = UBASettings::FindFormatterSettings(Node->GetGraph()))
	{
		return FormatterSettings->RootNodes.Contains(GetObjectClassName(Node));
	}

	return false;
}

void FBAUtils::SwapNodes(UEdGraphNode* NodeA, UEdGraphNode* NodeB)
{
	if (NodeA == nullptr || NodeB == nullptr)
	{
		return;
	}

	TArray<UEdGraphPin*> NodeAPins = NodeA->Pins;
	TArray<UEdGraphPin*> NodeBPins = NodeB->Pins;

	TArray<FPinLink> PinsToLink;
	TSet<UEdGraphPin*> PinsConnected;

	// loop through our pins and check which pins can be connected to the new node
	for (int i = 0; i < 2; ++i)
	{
		for (UEdGraphPin* PinA : NodeAPins)
		{
			if (PinA->LinkedTo.Num() == 0)
			{
				continue;
			}

			if (PinsConnected.Contains(PinA))
			{
				continue;
			}

			for (UEdGraphPin* PinB : NodeBPins)
			{
				if (PinsConnected.Contains(PinB))
				{
					continue;
				}

				// on the first run (i = 0), we only use pins which have the same name
				if (FBAUtils::GetPinName(PinA) == FBAUtils::GetPinName(PinB) || i > 0)
				{
					TArray<UEdGraphPin*> LinkedToA = PinA->LinkedTo;
					TArray<UEdGraphPin*> LinkedToB = PinB->LinkedTo;

					auto LinkedToBoth = { LinkedToA, LinkedToB };

					bool bConnected = false;

					const auto AddLinks = [](const TArray<UEdGraphPin*>& First, const TArray<UEdGraphPin*> Second) { };

					for (UEdGraphPin* LinkedPin : LinkedToA)
					{
						if (FBAUtils::CanConnectPins(LinkedPin, PinB, true, false))
						{
							PinsToLink.Add(FPinLink(LinkedPin, PinB));
							PinsConnected.Add(PinA);
							PinsConnected.Add(PinB);
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

		// link the pins marked in the last two loops
		for (FPinLink& PinToLink : PinsToLink)
		{
			for (UEdGraphPin* Pin : NodeB->Pins)
			{
				if (Pin->PinId == PinToLink.To->PinId)
				{
					// Can use unsafe cos we are breaking here!
					FBAUtils::TryCreateConnectionUnsafe(PinToLink.GetFromPin(), Pin, EBABreakMethod::Always);
					break;
					//UE_LOG(LogBlueprintAssist, Warning, TEXT("\tConnected"));
				}
			}
		}
	}
}

TArray<UEdGraphNode*> FBAUtils::GetNodesUnderComment(UEdGraphNode_Comment* CommentNode)
{
	TArray<UEdGraphNode*> OutNodes;
	for (UObject* Obj : CommentNode->GetNodesUnderComment())
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(Obj))
		{
			OutNodes.Add(Node);
		}
	}

	return OutNodes;
}

TArray<UEdGraphNode_Comment*> FBAUtils::GetCommentNodesFromGraph(UEdGraph* Graph)
{
	TArray<UEdGraphNode_Comment*> Comments;
	for (auto Node : Graph->Nodes)
	{
		if (auto Comment = Cast<UEdGraphNode_Comment>(Node))
		{
			Comments.Add(Comment);
		}
	}

	return Comments;
}

TArray<UEdGraphNode_Comment*> FBAUtils::GetContainingCommentNodes(const TArray<UEdGraphNode_Comment*>& Comments,
																UEdGraphNode* Node)
{
	TArray<UEdGraphNode_Comment*> ContainingComments;
	for (auto Comment : Comments)
	{
		auto NodesUnderComments = FBAUtils::GetNodesUnderComment(Comment);
		if (NodesUnderComments.Contains(Node))
		{
			ContainingComments.Add(Comment);
		}
	}

	return ContainingComments;
}

void FBAUtils::MoveComment(UEdGraphNode_Comment* Comment, FVector2D Delta)
{
	for (UEdGraphNode* Node : GetNodesUnderComment(Comment))
	{
		Node->NodePosX += Delta.X;
		Node->NodePosY += Delta.Y;
	}

	Comment->NodePosX += Delta.X;
	Comment->NodePosY += Delta.Y;
}

FSlateRect FBAUtils::GetCommentBounds(FCommentHandler* CommentHandler, UEdGraphNode_Comment* CommentNode, UEdGraphNode* NodeAsking)
{
	return CommentHandler->GetCommentBounds(CommentNode, NodeAsking);
}

FSlateRect FBAUtils::GetCachedNodeArrayBoundsWithComments(TSharedPtr<FBAGraphHandler> GraphHandler, FCommentHandler* CommentHandler, const TArray<UEdGraphNode*>& Nodes, UEdGraphNode* NodeAsking)
{
	if (!GraphHandler.IsValid() || !CommentHandler || !UBASettings::Get().bApplyCommentPadding)
	{
		return GetCachedNodeArrayBounds(GraphHandler, Nodes);
	}

	TOptional<FSlateRect> Bounds;

	TSet<UEdGraphNode_Comment*> Comments;
	for (UEdGraphNode* Node : Nodes)
	{
		if (!Node)
			continue;

		const FSlateRect NodeBounds = GetCachedNodeBounds(GraphHandler, Node);
		Bounds = !Bounds.IsSet() ? NodeBounds : Bounds.GetValue().Expand(NodeBounds);

		Comments.Append(CommentHandler->GetParentComments(Node));
	}

	for (UEdGraphNode_Comment* Comment : Comments)
	{
		if (!Comment)
			continue;

		const FSlateRect CommentBounds = GetCommentBounds(CommentHandler, Comment, NodeAsking);
		Bounds = !Bounds.IsSet() ? CommentBounds : Bounds.GetValue().Expand(CommentBounds);
	}

	return Bounds.Get(FSlateRect());
}

UEdGraphNode* FBAUtils::GetFirstLinkedNodePreferringInput(UEdGraphNode* Node)
{
	const auto LinkedInput = GetLinkedNodes(Node, EGPD_Input);
	if (LinkedInput.Num() > 0)
	{
		return LinkedInput[0];
	}

	const auto LinkedOutput = GetLinkedNodes(Node, EGPD_Output);
	if (LinkedOutput.Num() > 0)
	{
		return LinkedOutput[0];
	}

	return nullptr;
}

void FBAUtils::RemoveNodeFromComment(UEdGraphNode_Comment* Comment, UEdGraphNode* NodeToRemove)
{
	const FCommentNodeSet NodesUnderComment = Comment->GetNodesUnderComment();

	// Clear all nodes under comment
	Comment->ClearNodesUnderComment();

	// Add back the nodes under comment while filtering out any which are to be removed
	for (UObject* NodeUnderComment : NodesUnderComment)
	{
		if (NodeUnderComment != NodeToRemove)
		{
			Comment->AddNodeUnderComment(NodeUnderComment);
		}
	}
}

TSharedPtr<FBAGraphHandler> FBAUtils::GetCurrentGraphHandler()
{
	return FBATabHandler::Get().GetActiveGraphHandler();
}

float FBAUtils::GetCenterYOfPins(TSharedPtr<FBAGraphHandler> GraphHandler, TArray<UEdGraphPin*>& Pins)
{
	float PinMin = MAX_flt;
	float PinMax = -MAX_flt;
	for (auto Pin : Pins)
	{
		PinMin = FMath::Min(PinMin, FBAUtils::GetPinPos(GraphHandler, Pin).Y);
		PinMax = FMath::Max(PinMax, FBAUtils::GetPinPos(GraphHandler, Pin).Y);
	}

	return (PinMin + PinMax) / 2;
}

bool FBAUtils::IsCompilingCode()
{
#if WITH_LIVE_CODING
	// Note: Seems ILiveCodingModule::IsCompiling does return the correctly  
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (LiveCoding != nullptr && (LiveCoding->IsEnabledForSession() || LiveCoding->IsEnabledByDefault()))
	{
		return LiveCoding->IsCompiling();
	}
#endif

	// We're not able to recompile if a compile is already in progress!
	IHotReloadInterface& HotReloadSupport = FModuleManager::LoadModuleChecked<IHotReloadInterface>("HotReload");
	return HotReloadSupport.IsCurrentlyCompiling();
}

TSharedPtr<IMenu> FBAUtils::OpenPopupMenu(TSharedRef<SWidget> WindowContents, const FVector2D& PopupDesiredSize, const FVector2D& Offset, const FVector2D& MenuAnchor)
{
	FSlateApplication::Get().DismissAllMenus();

	// Determine where the pop-up should open
	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (!ParentWindow.IsValid())
	{
		TSharedPtr<SDockTab> LevelEditorTab = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>("LevelEditor").GetLevelEditorTab();
		ParentWindow = LevelEditorTab->GetParentWindow();
		check(ParentWindow.IsValid());
	}

	if (ParentWindow.IsValid())
	{
		const FSlateRect ParentMonitorRect = ParentWindow->GetFullScreenInfo();
		const FVector2D MonitorCenter((ParentMonitorRect.Right + ParentMonitorRect.Left) * (0.5f + Offset.X), (ParentMonitorRect.Top + ParentMonitorRect.Bottom) * (0.5f + Offset.Y));
		const FVector2D PopupAnchor = PopupDesiredSize * MenuAnchor;
		const FVector2D WindowPosition = MonitorCenter - PopupAnchor;

		// Open the pop-up
		const FPopupTransitionEffect TransitionEffect(FPopupTransitionEffect::None);
		return FSlateApplication::Get().PushMenu(ParentWindow.ToSharedRef(), FWidgetPath(), WindowContents, WindowPosition, TransitionEffect, true);
	}

	return TSharedPtr<IMenu>();
}

TSharedPtr<SGraphActionMenu> FBAUtils::GetGraphActionMenu()
{
	TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow();

	TSharedPtr<SGraphActionMenu> ActionMenu = GetChildWidgetCasted<SGraphActionMenu>(Window, "SGraphActionMenu");

	return ActionMenu;
}

bool FBAUtils::HasPropertyFlags(const FBPVariableDescription& Variable, uint64 FlagsToCheck)
{
	return (Variable.PropertyFlags & FlagsToCheck) != 0 || FlagsToCheck == CPF_AllFlags;
}

bool FBAUtils::HasMetaDataChanged(const FBPVariableDescription& OldVariable, const FBPVariableDescription& NewVariable, const FName Key)
{
	if (OldVariable.HasMetaData(Key) && NewVariable.HasMetaData(Key))
	{
		return !OldVariable.GetMetaData(Key).Compare(NewVariable.GetMetaData(Key));
	}

	return OldVariable.HasMetaData(Key) != NewVariable.HasMetaData(Key);
}

IAssetEditorInstance* FBAUtils::GetEditorFromActiveTab()
{
	if (UBARootObject* RootObject = FBlueprintAssistModule::Get().GetRootObject())
	{
		return RootObject->GetAssetHandler()->GetEditorFromTab(FBATabHandler::Get().GetLastMajorTab());
	}

	return nullptr;
}

TSharedPtr<SDockTab> FBAUtils::FindParentTabForWidget(TSharedPtr<SWidget> Widget)
{
	if (Widget)
	{
		if (TSharedPtr<SWidget> TabStack = GetParentWidgetOfType(Widget, "SDockingTabStack"))
		{
			if (TSharedPtr<SWidget> TabWell = GetChildWidgetFast(TabStack, "SDockingTabWell"))
			{
				TArray<TSharedPtr<SDockTab>> Tabs;
				GetChildWidgetsCasted<SDockTab>(TabWell, "SDockTab", Tabs);

				for (TSharedPtr<SDockTab> Tab : Tabs)
				{
					if (Tab->IsForeground())
					{
						return Tab;
					}
				}
			}
		}
	}

	return nullptr;
}

UBlueprint* FBAUtils::GetBlueprintFromGraph(const UEdGraph* Graph)
{
	if (Graph)
	{
		UObject* Outer = Graph->GetOuter();
		while (Outer)
		{
			if (UBlueprint* Blueprint = Cast<UBlueprint>(Outer))
			{
				return Blueprint;
			}

			Outer = Outer->GetOuter();
		}
	}

	return nullptr;
}

FBlueprintEditor* FBAUtils::GetBlueprintEditorForGraph(const UEdGraph* Graph)
{
	if (UBlueprint* Blueprint = GetBlueprintFromGraph(Graph))
	{
		if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			if (IAssetEditorInstance* AssetEditor = AssetEditorSubsystem->FindEditorForAsset(Blueprint, false))
			{
				if (UBASettings::Get().SupportedAssetEditors.Contains(AssetEditor->GetEditorName()))
				{
					FAssetEditorToolkit* AssetEditorToolkit = static_cast<FAssetEditorToolkit*>(AssetEditor);
					if (AssetEditorToolkit->IsBlueprintEditor())
					{
						return static_cast<FBlueprintEditor*>(AssetEditorToolkit);
					}
				}
			}
		}
	}

	return nullptr;
}

FString FBAUtils::GetVariableName(const FString& Name, const FName& PinCategory, EPinContainerType ContainerType)
{
	if ((PinCategory == UEdGraphSchema_K2::PC_Boolean) && (ContainerType == EPinContainerType::None))
	{
		if (Name.Len() >= 2)
		{
			const TCHAR First = Name[0];
			const TCHAR Second = Name[1];
			if (FChar::IsLower(First) && (First == TEXT('b')) && FChar::IsUpper(Second))
			{
				FString OutName = Name;
				OutName.RemoveAt(0);
				return OutName;
			}
		}
	}

	return Name;
}


FBAMetaData* FBAUtils::GetPackageMetaData(UPackage* Package)
{
#if BA_UE_VERSION_OR_LATER(5, 6)
	return &Package->GetMetaData();
#else
	return Package->GetMetaData();
#endif
}

FBAMetaData* FBAUtils::GetNodeMetaData(UEdGraphNode* Node)
{
	if (UPackage* Package = Node->GetOutermost())
	{
		return FBAUtils::GetPackageMetaData(Package);
	}
	return nullptr;
}

UEdGraphPin* FBAUtils::GetKnotPinByDirection(UK2Node_Knot* KnotNode, EEdGraphPinDirection Direction)
{
	return Direction == EGPD_Input ? KnotNode->GetInputPin() : KnotNode->GetOutputPin();
}

bool FBAUtils::IsGamePlayingAndHasFocus()
{
	if (!GEditor->PlayWorld)
	{
		return false;
	}

	if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
	{
		for (const FWorldContext& WorldContext : EditorEngine->GetWorldContexts())
		{
			if ((WorldContext.WorldType == EWorldType::PIE) && !WorldContext.RunAsDedicated && WorldContext.GameViewport)
			{
				if (TSharedPtr<SViewport> GameViewport = WorldContext.GameViewport->GetGameViewportWidget())
				{
					if (GameViewport->HasAnyUserFocus().IsSet())
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

FGuid FBAUtils::GetNodeGuid(UEdGraphNode* Node)
{
	if (Node)
	{
		// see InitExpressionNewNode
		if (UMaterialGraphNode* MaterialNode = Cast<UMaterialGraphNode>(Node))
		{
			return MaterialNode->MaterialExpression->GetMaterialExpressionId();
		}
		else
		{
			return Node->NodeGuid;
		}
	}

	return FGuid();
}

const TArray<FName>& FBAUtils::GetSearchBoxNames()
{
	static TArray<FName> SearchBoxNames = {
		"SSearchBox",
		// "SAssetSearchBox", - we can skip the asset search box cause it contains a filter search box
		"SFilterSearchBox"
	};
	return SearchBoxNames;
}

FGuid FBAUtils::GetGraphGuid(UEdGraph* Graph)
{
	if (Graph)
	{
		// material graph is recreated in the editor, so the graph guid cannot be used
		// see FMaterialEditor::InitMaterialEditor
		if (Cast<UMaterialGraph>(Graph))
		{
			if (UPackage* Package = Graph->GetPackage())
			{
				return Package->GetPersistentGuid();
			}
		}
		else
		{
			return Graph->GraphGuid;
		}
	}

	return FGuid();
}

bool FBAUtils::IsNodeBeingRenamed(TSharedPtr<SGraphNode> GraphNode)
{
	if (GraphNode)
	{
		if (GraphNode->IsRenamePending())
		{
			return true;
		}

		// if an editable text box has keyboard focus
		if (TSharedPtr<SWidget> KeyboardFocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget())
		{
			if (FBAUtils::IsWidgetOfTypeFast(KeyboardFocusedWidget, "SEditableText"))
			{
				return true;
			}
		}

		if (GraphNode->GetNodeObj()->GetCanRenameNode())
		{
			if (TSharedPtr<SInlineEditableTextBlock> TitleText = FIND_CHILD_WIDGET(GraphNode, SInlineEditableTextBlock))
			{
				return TitleText->IsInEditMode();
			}
		}
	}

	return false;
}

EEdGraphPinDirection FBAUtils::GetOppositeDirection(EEdGraphPinDirection Direction)
{
	return UEdGraphPin::GetComplementaryDirection(Direction);
}

bool FBAUtils::IsObjectPinType(const FName& PinCategory)
{
	static const TSet<FName> ObjTypes = { UEdGraphSchema_K2::PC_Class, UEdGraphSchema_K2::PC_Object }; 
	return ObjTypes.Contains(PinCategory);
}

FString FBAUtils::GetDefaultPinValue(UEdGraphPin* Pin)
{
	if (Pin)
	{
		FString DefaultValue = !Pin->DefaultValue.IsEmpty() ? Pin->DefaultValue : Pin->AutogeneratedDefaultValue;

		const auto& PC = Pin->PinType.PinCategory;

		if (IsObjectPinType(PC))
		{
			return GetPathNameSafe(Pin->DefaultObject);
		}

		if (PC == UEdGraphSchema_K2::PC_Text)
		{
			FString TextAsString;
			FTextStringHelper::WriteToBuffer(TextAsString, Pin->DefaultTextValue);
			return TextAsString;
		}

		if (PC == UEdGraphSchema_K2::PC_SoftObject)
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
#if BA_UE_VERSION_OR_LATER(5, 0)
			FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(Pin->DefaultValue));
#else
			FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FName(Pin->DefaultValue));
#endif
			return AssetData.GetExportTextName();
		}

		if (PC == UEdGraphSchema_K2::PC_Struct)
		{
			if (const UScriptStruct* StructType = Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject))
			{
				// if the struct has it's own import text, it *should* already be in the correct format
				if (StructType->StructFlags & STRUCT_ImportTextItemNative)
				{
					return DefaultValue;
				}

				// see UEdGraphSchema_K2::SplitPin / UEdGraphSchema_K2::RecombinePin
				// often the pins have no default value at all, so it's easier to init the data type
				// then convert it back to a string to get an accurate default value
				if (StructType == TBaseStructure<FVector>::Get())
				{
					FVector V3;
					V3.InitFromString(AttachPropertyNamesToValue(DefaultValue, StructType));
					DefaultValue = V3.ToString();
					DefaultValue.ReplaceInline(TEXT(" "), TEXT(","));
				}
				else if (StructType == TBaseStructure<FVector2D>::Get())
				{
					FVector2D V2D;
					V2D.InitFromString(AttachPropertyNamesToValue(DefaultValue, StructType));
					DefaultValue = V2D.ToString();
					DefaultValue.ReplaceInline(TEXT(" "), TEXT(","));
				}
				else if (StructType == TBaseStructure<FRotator>::Get())
				{
					FRotator Rot;
					DefaultValue = AttachPropertyNamesToValue(DefaultValue, StructType);
					DefaultValue.ReplaceInline(TEXT("Pitch="), TEXT("P="));
					DefaultValue.ReplaceInline(TEXT("Yaw="), TEXT("Y="));
					DefaultValue.ReplaceInline(TEXT("Roll="), TEXT("R="));
					Rot.InitFromString(DefaultValue);
					DefaultValue = Rot.ToString();
					DefaultValue.ReplaceInline(TEXT(" "), TEXT(","));
					DefaultValue.ReplaceInline(TEXT("P="), TEXT("Pitch="));
					DefaultValue.ReplaceInline(TEXT("Y="), TEXT("Yaw="));
					DefaultValue.ReplaceInline(TEXT("R="), TEXT("Roll="));
				}
				else if (StructType == TBaseStructure<FLinearColor>::Get())
				{
					FLinearColor LC;
					LC.InitFromString(DefaultValue);
					return LC.ToString();
				}
				else
				{
					// for which structs does this actually happen?
					DefaultValue = AttachPropertyNamesToValue(DefaultValue, StructType);
				}

				return FString::Printf(TEXT("(%s)"), *DefaultValue);
			}
		}
		else
		{
			static TSet<FName> NumTypes = {
				UEdGraphSchema_K2::PC_Int,
				UEdGraphSchema_K2::PC_Int64,
				UEdGraphSchema_K2::PC_Float,
#if BA_UE_VERSION_OR_LATER(5, 0)
				UEdGraphSchema_K2::PC_Double,
				UEdGraphSchema_K2::PC_Real,
#endif
				UEdGraphSchema_K2::PC_Name
			};

			if (NumTypes.Contains(PC))
			{
				if (DefaultValue.IsEmpty())
				{
					return "0";
				}
			}

			return DefaultValue;
		}
	}

	return FString();
}

FString FBAUtils::AttachPropertyNamesToValue(const FString& DefaultValue, const UScriptStruct* StructType)
{
	// default value is expected to be in the form p1,p2,p3,...

	if (DefaultValue.IsEmpty())
	{
		return DefaultValue;
	}

	TArray<FString> PropertyNames;
	if (StructType)
	{
		for (TFieldIterator<BA_PROPERTY> PropertyIter(StructType); PropertyIter; ++PropertyIter)
		{
			// skip transient properties
			if (PropertyIter->HasAnyPropertyFlags(CPF_Transient))
			{
				continue;
			}

			PropertyNames.Add(PropertyIter->GetName());
		}
	}

	TArray<FString> ValueArray;
	DefaultValue.ParseIntoArray(ValueArray, TEXT(","));

	for (int i = 0; i < ValueArray.Num(); ++i)
	{
		if (!PropertyNames.IsValidIndex(i))
		{
			return DefaultValue;
		}

		ValueArray[i] = PropertyNames[i] + '=' + ValueArray[i];
	}

	FString ParsedStructValue;
	for (FString& Value : ValueArray)
	{
		ParsedStructValue += Value + ",";
	}

	ParsedStructValue.RemoveFromEnd(",");
	return ParsedStructValue;
}

bool FBAUtils::CanSetDefaultPinValue(UEdGraphPin* Pin, const FString& NewDefaultValue, UObject* NewDefaultObject, const FText& NewDefaultTextValue, FString* OutError)
{
	if (!Pin)
	{
		return false;
	}

	if (UMaterialGraphNode* MaterialNode = Cast<UMaterialGraphNode>(Pin->GetOwningNode()))
	{
		if (!CanSetDefaultMaterialPinValue(MaterialNode->MaterialExpression, Pin, NewDefaultValue))
		{
			return false;
		}
	}

	return true;
}

bool FBAUtils::CanSetDefaultMaterialPinValue(UMaterialExpression* MaterialExpression, UEdGraphPin* Pin, const FString& DefaultValue)
{
	// So that UMaterialExpression::PinDefaultValueChanged doesn't hit asserts when trying to set default values
	int PinIndex = Pin->SourceIndex;

	TArray<FProperty*> Properties = MaterialExpression->GetInputPinProperty(PinIndex);
	if (Properties.IsEmpty())
	{
		return false;
	}

	TArray<FString> PropertyValues;
	if (Properties.Num() == 1)
	{
		PropertyValues.Add(DefaultValue);
	}
	else if (Properties.Num() == 2)
	{
		FVector2D Value;
		if (!Value.InitFromString(DefaultValue))
		{
			return false;
		}
	}
	else
	{
		// Vector 3/4 are formatted as numbers separated by commas
		DefaultValue.ParseIntoArray(PropertyValues, TEXT(","), true);
		if (PropertyValues.Num() != Properties.Num())
		{
			return false;
		}
	}

	for (int32 i = 0; i < Properties.Num(); ++i)
	{
		const FProperty* Property = Properties[i];
		const FString& PropertyValue = PropertyValues[i];
		const FFieldClass* PropertyClass = Property->GetClass();

		if (PropertyClass == FStructProperty::StaticClass())
		{
			UScriptStruct* Struct = ((FStructProperty*)Property)->Struct;
			if (Struct == TBaseStructure<FLinearColor>::Get())
			{
				if (Property->HasMetaData(TEXT("HideAlphaChannel")))
				{
					// This is a 3 element vector
					TArray<FString> Elements;
					PropertyValue.ParseIntoArray(Elements, TEXT(","), true);
					if (Elements.Num() != 3)
					{
						return false;
					}
				}
			}
			else if (Struct == TVariantStructure<FVector4>::Get() || Struct == TVariantStructure<FVector4d>::Get())
			{
				TArray<FString> Elements;
				PropertyValue.ParseIntoArray(Elements, TEXT(","), true);
				if (Elements.Num() != 4)
				{
					return false;
				}
			}
			else if (Struct == TBaseStructure<FVector>::Get() || Struct == TVariantStructure<FVector3f>::Get())
			{
				TArray<FString> Elements;
				PropertyValue.ParseIntoArray(Elements, TEXT(","), true);
				if (Elements.Num() != 3)
				{
					return false;
				}
			}
		}
	}

	return true;
}

bool FBAUtils::TrySetDefaultPinValues(UEdGraphPin* Pin, const FString& NewDefaultValue, UObject* NewDefaultObject, const FText& NewDefaultTextValue, FString* OutError)
{
	if (Pin)
	{
		if (auto Node = Pin->GetOwningNode())
		{
			if (auto Graph = Node->GetGraph())
			{
				if (auto Schema = Graph->GetSchema())
				{
					if (const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(Schema))
					{
						if (!K2Schema->DefaultValueSimpleValidation(Pin->PinType, Pin->PinName, NewDefaultValue, NewDefaultObject, NewDefaultTextValue, OutError))
						{
							return false;
						}
					}

					if (!CanSetDefaultPinValue(Pin, NewDefaultValue, NewDefaultObject, NewDefaultTextValue))
					{
						return false;
					}

					const auto& PC = Pin->PinType.PinCategory;

					if (IsObjectPinType(PC))
					{
						Schema->TrySetDefaultObject(*Pin, NewDefaultObject);
					}
					else if (PC == UEdGraphSchema_K2::PC_Text)
					{
						Schema->TrySetDefaultText(*Pin, NewDefaultTextValue);
					}
					else
					{
						Schema->TrySetDefaultValue(*Pin, NewDefaultValue);
					}

					return true;
				}
			}
		}
	}

	return false;
}

bool FBAUtils::TrySetDefaultPinValuesFromString(UEdGraphPin* Pin, const FString& NewDefault, FString* OutError)
{
	if (!Pin)
	{
		if (OutError)
		{
			*OutError = "Null pin";
		}

		return false;
	}

	FString NewDefaultValue;
	UObject* NewDefaultObject = nullptr;
	FText NewDefaultTextValue;

	const auto& PC = Pin->PinType.PinCategory;
	if (IsObjectPinType(PC))
	{
		// Avoid hitting assert if you try to load object from a very long string
		if (NewDefault.Len() >= NAME_SIZE)
		{
			return false;
		}

		NewDefaultObject = NewDefault.IsEmpty() ? nullptr : LoadObject<UObject>(nullptr, *NewDefault, nullptr, 0, nullptr);
	}
	else if (PC == UEdGraphSchema_K2::PC_Text)
	{
		FText DefaultAsText;
		FTextStringHelper::ReadFromBuffer(*NewDefault, DefaultAsText);
		NewDefaultTextValue = DefaultAsText;
	}
	else if (PC == UEdGraphSchema_K2::PC_SoftObject)
	{
		// SPropertyEditorAsset::OnCopy
		NewDefaultValue = FPackageName::ExportTextPathToObjectPath(NewDefault);
	}
	else if (PC == UEdGraphSchema_K2::PC_Struct)
	{
		if (const UScriptStruct* Struct = Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject))
		{
			// if the struct has it's own import text, it *should* already be in the correct format
			if (Struct->StructFlags & STRUCT_ImportTextItemNative)
			{
				NewDefaultValue = NewDefault;
			}
			else if (Struct == TBaseStructure<FLinearColor>::Get())
			{
				NewDefaultValue = NewDefault;
			}
			else
			{
				// currently format: "(P1=V1,P2=V2,P3=V3)"
				// convert it to format: "V1,V2,V3"
				FString NoBrackets = NewDefault;
				if (NoBrackets.RemoveFromStart("(") && NoBrackets.RemoveFromEnd(")"))
				{
					TArray<FString> Properties;
					if (Struct)
					{
						for (TFieldIterator<BA_PROPERTY> PropertyIter(Struct); PropertyIter; ++PropertyIter)
						{
							// skip transient properties
							if (PropertyIter->HasAnyPropertyFlags(CPF_Transient))
							{
								continue;
							}

								Properties.Add(PropertyIter->GetName());
							}
						}

						TArray<FString> InProperties;
						NoBrackets.ParseIntoArray(InProperties, TEXT(","));

						FString ParsedStructValue;
						for (int i = 0; i < InProperties.Num(); ++i)
						{
							FString& Str = InProperties[i];
							FString StructName;
							FString StructValue;
							Str.Split(TEXT("="), &StructName, &StructValue);

							if (!Properties.IsValidIndex(i))
							{
								return false;
							}

							if (!Properties.IsValidIndex(i) || Properties[i] != StructName)
							{
								return false;
							}

							ParsedStructValue.Append(StructValue + ",");
						}

					ParsedStructValue.RemoveFromEnd(",");
					NewDefaultValue = ParsedStructValue;
				}
				else
				{
					return false;
				}
			}
		}
	}
	else
	{
		NewDefaultValue = NewDefault;
	}

	return TrySetDefaultPinValues(Pin, NewDefaultValue, NewDefaultObject, NewDefaultTextValue, OutError);
}

UEdGraphPin* FBAUtils::FindSelfPin(UEdGraphNode* Node)
{
	if (UK2Node_CallFunction* CallFunction = Cast<UK2Node_CallFunction>(Node))
	{
		// Connect a self reference pin if there is a TScriptInterface default to self
		if (const UFunction* TargetFunc = CallFunction->GetTargetFunction())
		{
			const FString& MetaData = TargetFunc->GetMetaData(FBlueprintMetadata::MD_DefaultToSelf);
			if (!MetaData.IsEmpty())
			{
				// Find the default to self value pin
				if (UEdGraphPin* DefaultToSelfPin = CallFunction->FindPinChecked(MetaData, EGPD_Input))
				{
					return DefaultToSelfPin;
				}
			}
		}
	}

	if (Node)
	{
		if (auto Schema = Node->GetSchema())
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Schema->IsSelfPin(*Pin))
				{
					return Pin;
				}
			}
		}
	}

	return nullptr;
}

UEdGraphPin* FBAUtils::GetFirstValuePinOnNode(TSharedPtr<FBAGraphHandler> GraphHandler, UEdGraphNode* NewNode)
{
	if (!NewNode || !GraphHandler.IsValid())
	{
		return nullptr;
	}

	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();
	if (!GraphPanel)
	{
		return nullptr;
	}

	// only have value pins in the input direction
	TArray<UEdGraphPin*> Pins = FBAUtils::GetParameterPins(NewNode, EGPD_Input);
	UEdGraphPin* SelfPin = FBAUtils::FindSelfPin(NewNode);
	Pins.RemoveAll([GraphPanel, SelfPin](UEdGraphPin* Pin)
	{
		const TSharedPtr<SGraphPin> GraphPin = FBAUtils::GetGraphPin(GraphPanel, Pin);
		if (GraphPin.IsValid())
		{
			if (GraphPin->GetPinObj() == SelfPin)
			{
				return true;
			}

			return GraphPin->IsPinVisibleAsAdvanced() != EVisibility::Visible;
		}

		return false;
	});

	if (Pins.Num() > 0)
	{
		Pins.StableSort(FBAUtils::GetHighestPinPredicate(GraphPanel));
		return Pins[0];
	}

	return nullptr;
}

bool FBAUtils::GetPinOffset(TSharedPtr<SGraphPanel> GraphPanel, UEdGraphPin* Pin, FVector2D& OutPinOffset)
{
	if (GraphPanel.IsValid() && Pin)
	{
		if (TSharedPtr<SGraphPin> GraphPin = FBAUtils::GetGraphPin(GraphPanel, Pin))
		{
			OutPinOffset = GraphPin->GetNodeOffset();
			return true;
		}
	}

	return false;
}

TFunction<bool(UEdGraphPin&, UEdGraphPin&)> FBAUtils::GetHighestPinPredicate(TSharedPtr<SGraphPanel> GraphPanel)
{
	return [GraphPanel](UEdGraphPin& PinA, UEdGraphPin& PinB)
	{
		FVector2D OffsetA(0, FLT_MAX);
		FVector2D OffsetB(0, FLT_MAX);
		GetPinOffset(GraphPanel, &PinA, OffsetA);
		GetPinOffset(GraphPanel, &PinB, OffsetB);
		return OffsetA.Y < OffsetB.Y;
	};
}

bool FBAUtils::IsPinVisible(UEdGraphPin* Pin)
{
	if (!Pin || !Pin->GetOwningNode())
	{
		return false;
	}

	// check if pin is hidden due to advanced pin display being not on 
	if (Pin->bAdvancedView && Pin->GetOwningNode()->AdvancedPinDisplay != ENodeAdvancedPins::Type::Shown)
	{
		return false;
	}

	return true;
}

FText FBAUtils::GetNodeTitle(UEdGraphNode* Node)
{
	if (!Node)
	{
		return FText::GetEmpty();
	}

	if (const UK2Node* K2Node = Cast<UK2Node>(Node))
	{
		if (K2Node->ShouldDrawCompact())
		{
			return K2Node->GetCompactNodeTitle();
		}
	}

	return Node->GetNodeTitle(ENodeTitleType::FullTitle);
}

UPackage* FBAUtils::GetPackage(UObject* Obj)
{
	if (!Obj)
	{
		return nullptr;
	}

	if (UEdGraph* Graph = Cast<UEdGraph>(Obj))
	{
		if (UMaterialGraph* MatGraph = Cast<UMaterialGraph>(Graph))
		{
			UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
			TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();

			for (UObject* Asset : EditedAssets)
			{
				if (UMaterial* EditedMat = Cast<UMaterial>(Asset))
				{
					if (MatGraph->OriginalMaterialFullName == EditedMat->GetName())
					{
						return EditedMat->GetPackage();
					}
				}
			}
		}
	}

	return Obj->GetPackage();
}

FBAVector2 FBAUtils::GetGraphNodePos(TSharedPtr<SGraphNode> Node)
{
#if BA_UE_VERSION_OR_LATER(5, 6)
	return Node->GetPosition2f();
#else
	return Node->GetPosition();
#endif
}

FBAVector2 FBAUtils::GetGraphNodeMarqueeSize(TSharedPtr<SGraphNode> Node)
{
#if BA_UE_VERSION_OR_LATER(5, 6)
	return Node->GetDesiredSizeForMarquee2f();
#else
	return Node->GetDesiredSizeForMarquee();
#endif
}

FBAVector2 FBAUtils::GetGraphEditorPasteLocation(TSharedPtr<SGraphEditor> Editor)
{
#if BA_UE_VERSION_OR_LATER(5, 6)
	return Editor->GetPasteLocation2f();
#else
	return Editor->GetPasteLocation();
#endif
}

FString FBAUtils::ExportNodesToText(const TSet<UEdGraphNode*>& Nodes)
{
	FString Out;

	TSet<UObject*> Objs;
	for (UEdGraphNode* Node : Nodes)
	{
		if (Node)
		{
			Node->PrepareForCopying();
			Objs.Add(Node);
		}
	}

	FEdGraphUtilities::ExportNodesToText(Objs, Out);
	return Out;
}

