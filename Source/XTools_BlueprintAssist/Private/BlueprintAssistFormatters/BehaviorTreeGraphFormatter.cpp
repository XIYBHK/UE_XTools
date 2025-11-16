// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistFormatters/BehaviorTreeGraphFormatter.h"

#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistUtils.h"
#include "BlueprintAssistSettings.h"
#include "Containers/Map.h"
#include "Algo/Transform.h"

FBehaviorTreeGraphFormatter::FBehaviorTreeGraphFormatter(
	TSharedPtr<FBAGraphHandler> InGraphHandler,
	const FEdGraphFormatterParameters& InFormatterParameters)
	: GraphHandler(InGraphHandler)
	, FormatterParameters(InFormatterParameters)
	, RootNode(nullptr)
{
	FormatterParameters.Init();
	FormatterSettings = UBASettings::GetFormatterSettings(InGraphHandler->GetFocusedEdGraph());
}

FBAFormatterSettings FBehaviorTreeGraphFormatter::GetFormatterSettings()
{
	return UBASettings::GetFormatterSettings(GraphHandler->GetFocusedEdGraph());
}

void FBehaviorTreeGraphFormatter::FormatNode(UEdGraphNode* InNode)
{
	RootNode = InNode;
	if (!RootNode)
	{
		return;
	}

	while (true)
	{
		TArray<UEdGraphNode*> LinkedInputNodes = FBAUtils::GetLinkedNodes(RootNode, EGPD_Input);
		if (!ensureMsgf(LinkedInputNodes.Num() <= 1, TEXT("Behavior tree has more than one input node? Early exit formatting. %s"), *FBAUtils::GetNodeName(RootNode)))
		{
			return;
		}

		if (LinkedInputNodes.Num() == 0)
		{
			break;
		}

		RootNode = LinkedInputNodes[0];
	}

	FBATidyTree TidyTree(GraphHandler, FormatterSettings.Padding);
	TidyTree.FormatTidyTree(RootNode);

	// Get the formatted nodes from the tidy tree
	FormattedNodes.Empty();
	Algo::Transform(TidyTree.AllNodes, FormattedNodes, &FBATidyNode::GraphNode);
}

void FBATidyTree::FormatTidyTree(UEdGraphNode* RootNode)
{
	TMap<UEdGraphNode*, FTidyNodePtr> Visited;
	FTidyNodePtr TidyRoot = BuildTidyTree(RootNode, 0, Visited);

	// Set the tree layer positions
	for (int i = 0; i < Layers.Num(); ++i)
	{
		FBALayer& Layer = Layers[i];
		if (i > 0)
		{
			FBALayer& PrevLayer = Layers[i - 1];
			Layer.FinalPos = PrevLayer.FinalPos + PrevLayer.Height + Padding.Y;
		}
		else
		{
			Layer.FinalPos = RootNode->NodePosY;
		}
	}

	// Calculate positions and modifiers
	FirstPass_Setup(TidyRoot);

	// Get and apply the final position
	const float OldNodeX = TidyRoot->GraphNode->NodePosX;
	SecondPass_ApplyMods(TidyRoot, 0.0f);

	// Make it so the root node doesn't move
	const float DeltaX = TidyRoot->GraphNode->NodePosX - OldNodeX;
	for (FTidyNodePtr TidyNode : AllNodes)
	{
		TidyNode->GraphNode->NodePosX -= DeltaX;
	}
}

TSharedPtr<FBATidyNode> FBATidyTree::BuildTidyTree(UEdGraphNode* CurrGraphNode, int32 Depth, TMap<UEdGraphNode*, FTidyNodePtr>& VisitedNodes)
{
	if (FTidyNodePtr FoundNode = VisitedNodes.FindRef(CurrGraphNode))
	{
		return FoundNode;
	}

	FTidyNodePtr NewTidyNode = MakeShared<FBATidyNode>();
	NewTidyNode->GraphNode = CurrGraphNode;
	NewTidyNode->Depth = Depth;
	NewTidyNode->Size = FBAUtils::GetCachedNodeBounds(GraphHandler, CurrGraphNode).GetSize();

	AllNodes.Add(NewTidyNode);

	// Set the layer size
	if (!Layers.IsValidIndex(Depth))
	{
		FBALayer NewLayer;
		NewLayer.Height = NewTidyNode->Size.Y;
		Layers.Add(NewLayer);
	}
	else
	{
		FBALayer& CurrLayer = Layers[Depth];
		CurrLayer.Height = FMath::Max(CurrLayer.Height, NewTidyNode->Size.Y);
	}

	VisitedNodes.Add(CurrGraphNode, NewTidyNode);

	// Nodes must be sorted left-right for behavior tree
	TArray<UEdGraphNode*> ChildGraphNodes = FBAUtils::GetLinkedNodes(CurrGraphNode, EGPD_Output);
	ChildGraphNodes.Sort([](const UEdGraphNode& A, const UEdGraphNode& B)
	{
		return A.NodePosX < B.NodePosX;
	});

	// Recursively build the tree
	for (UEdGraphNode* ChildGraphNode : ChildGraphNodes)
	{
		FTidyNodePtr ChildTidyNode = BuildTidyTree(ChildGraphNode, Depth + 1, VisitedNodes);

		// Setup child node connections
		ChildTidyNode->Parent = NewTidyNode;
		NewTidyNode->Children.Add(ChildTidyNode);
	}

	return NewTidyNode;
}

void FBATidyTree::FirstPass_Setup(FTidyNodePtr TidyNode) const
{
	// Recurse to the bottom of the tree first (post-order traversal)
	for (const auto& Child : TidyNode->Children)
	{
		FirstPass_Setup(Child);
	}

	if (TidyNode->IsLeaf())
	{
		// A leaf's position is relative to its left sibling if it has one
		if (FTidyNodePtr LeftSibling = TidyNode->GetLeftSibling())
		{
			TidyNode->RelativeX = LeftSibling->RelativeX + LeftSibling->Size.X + Padding.X;
		}
		else
		{
			// Otherwise, it's the first child, position it at 0
			TidyNode->RelativeX = 0.f;
		}
	}
	else // has children
	{
		// center the parent over the gap between the left and right most children 
		float Midpoint = (TidyNode->GetLeftMostChild()->RelativeX + TidyNode->GetLeftMostChild()->Size.X + TidyNode->GetRightMostChild()->RelativeX) / 2.f;

		if (FTidyNodePtr LeftSibling = TidyNode->GetLeftSibling())
		{
			// place ourselves right of our sibling
			TidyNode->RelativeX = LeftSibling->RelativeX + LeftSibling->Size.X + Padding.X;

			// calculate how much our children need to be shifted so that we are still centered
			TidyNode->Mod = TidyNode->RelativeX - (Midpoint - (TidyNode->Size.X / 2.f));
		}
		else
		{
			// have no left sibling, just center directly
			TidyNode->RelativeX = Midpoint - (TidyNode->Size.X / 2.f);
		}

		// collision check if this branch overlaps with any branches to its left and push it right
		if (TidyNode->Children.Num() > 0 && TidyNode->GetLeftSibling())
		{
			PushRight(TidyNode);
		}
	}
}

void FBATidyTree::PushRight(FTidyNodePtr TidyNode) const
{
	float ShiftAmount = 0.f;

	FTidyNodePtr CurrentSibling = TidyNode;

	while (FTidyNodePtr LeftSibling = CurrentSibling->GetLeftSibling())
	{
		// Get the right contour of the left sibling's subtree
		FContour RightContourOfLeft;
		GetRightContour(LeftSibling, 0.f, RightContourOfLeft);

		// Get the left contour of the current node's subtree
		FContour LeftContourOfRight;
		GetLeftContour(TidyNode, 0.f, LeftContourOfRight);

		float MinDistance = Padding.X;
		if (!LeftSibling->IsLeaf())
		{
			MinDistance += UBASettings::Get().BehaviorTreeBranchExtraPadding;
		}

		// check for overlaps and calculate the necessary shift
		for (auto const& [Depth, LeftPos] : LeftContourOfRight)
		{
			if (RightContourOfLeft.Contains(Depth))
			{
				float RightPos = RightContourOfLeft[Depth];
				float Distance = LeftPos - RightPos;
				if (Distance < MinDistance)
				{
					ShiftAmount = FMath::Max(ShiftAmount, MinDistance - Distance);
				}
			}
		}

		CurrentSibling = LeftSibling;
	}

	if (ShiftAmount > 0)
	{
		TidyNode->RelativeX += ShiftAmount;
		TidyNode->Mod += ShiftAmount;
	}
}

void FBATidyTree::GetLeftContour(FTidyNodePtr TidyNode, float ModSum, FContour& Contour) const
{
	if (!TidyNode)
	{
		return;
	}

	float CurrentX = TidyNode->RelativeX + ModSum;
	if (!Contour.Contains(TidyNode->Depth) || CurrentX < Contour[TidyNode->Depth])
	{
		Contour.Add(TidyNode->Depth, CurrentX);
	}

	for (const auto& Child : TidyNode->Children)
	{
		GetLeftContour(Child, ModSum + TidyNode->Mod, Contour);
	}
}

void FBATidyTree::GetRightContour(FTidyNodePtr TidyNode, float ModSum, FContour& Contour) const
{
	if (!TidyNode)
	{
		return;
	}

	float CurrentX = TidyNode->RelativeX + ModSum;
	if (!Contour.Contains(TidyNode->Depth) || CurrentX + TidyNode->Size.X > Contour[TidyNode->Depth])
	{
		Contour.Add(TidyNode->Depth, CurrentX + TidyNode->Size.X);
	}

	for (const auto& Child : TidyNode->Children)
	{
		GetRightContour(Child, ModSum + TidyNode->Mod, Contour);
	}
}

void FBATidyTree::SecondPass_ApplyMods(FTidyNodePtr TidyNode, float ModSum) const
{
	if (!TidyNode)
	{
		return;
	}

	// final x position is the RelativeX plus the sum of all its ancestors' modifiers
	TidyNode->GraphNode->Modify();
	TidyNode->GraphNode->NodePosX = TidyNode->RelativeX + ModSum;
	TidyNode->GraphNode->NodePosY = Layers[TidyNode->Depth].FinalPos;

	// Pass the accumulated modifier sum down to the children
	for (const auto& Child : TidyNode->Children)
	{
		SecondPass_ApplyMods(Child, ModSum + TidyNode->Mod);
	}
}

TSharedPtr<FBATidyNode> FBATidyNode::GetLeftSibling() const
{
	if (!Parent.IsValid() || Parent->GetLeftMostChild() == SharedThis(this))
	{
		return nullptr;
	}

	const int32 MyIndex = Parent->Children.IndexOfByKey(SharedThis(this));
	return Parent->Children[MyIndex - 1];
}