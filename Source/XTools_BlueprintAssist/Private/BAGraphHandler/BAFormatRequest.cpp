// Copyright fpwong. All Rights Reserved.


#include "BAGraphHandler/BAFormatRequest.h"
#include "BlueprintAssistGlobals.h"

#include "BlueprintAssistCache.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistUtils.h"
#include "BlueprintEditor.h"
#include "FileHelpers.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Knot.h"
#include "K2Node_Tunnel.h"
#include "SGraphPanel.h"
#include "BAGraphHandler/BAGraphTasks.h"
#include "BlueprintAssistFormatters/BAFormatterUtils.h"
#include "BlueprintAssistFormatters/BehaviorTreeGraphFormatter.h"
#include "BlueprintAssistFormatters/EdGraphFormatter.h"
#include "BlueprintAssistFormatters/FormatterInterface.h"
#include "BlueprintAssistFormatters/SimpleFormatter.h"
#include "BlueprintAssistMisc/BACrashReporter.h"
#include "BlueprintAssistMisc/BAGraphSchema.h"
#include "BlueprintAssistMisc/BAMiscUtils.h"
#include "BlueprintAssistWidgets/BlueprintAssistGraphOverlay.h"
#include "EdGraph/EdGraph.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/SavePackage.h"

FBAFormatRequest::FBAFormatRequest(FBAGraphHandler& InGH)
	: GH(InGH)
{

}

void FBAFormatRequest::RequestFormatAll()
{
	UEdGraph* EdGraph = GH.GetFocusedEdGraph();
	if (EdGraph == nullptr)
	{
		return;
	}

	FormatterParameters.Reset();
	FormatterParameters.InitIgnoredPins(GH.AsShared());

	const FBAFormatterSettings& FormatterSettings = UBASettings::GetFormatterSettings(EdGraph);
	bool bIsBlueprintFormatting = FormatterSettings.FormatterType == EBAFormatterType::Blueprint;

	const EBAFormatAllStyle FormatAllStyle = UBASettings::Get().FormatAllStyle;

	TArray<TWeakObjectPtr<UEdGraphNode>> ExtraNodes;
	TArray<TWeakObjectPtr<UEdGraphNode>> CustomEvents;
	TArray<TWeakObjectPtr<UEdGraphNode>> InputEvents;
	TArray<TWeakObjectPtr<UEdGraphNode>> ActorEvents;
	TArray<TWeakObjectPtr<UEdGraphNode>> ComponentEvents;
	TArray<TWeakObjectPtr<UEdGraphNode>> OtherEvents;

	for (UEdGraphNode* Node : EdGraph->Nodes)
	{
		if (GH.GetNodeData(Node).bLocked)
		{
			continue;
		}

		if (UBASettings::Get().FormatAllStyle == EBAFormatAllStyle::NodeType)
		{
			if (FBAUtils::IsExtraRootNode(Node))
			{
				ExtraNodes.Add(Node);
			}
			else if (Node->IsA(UK2Node_CustomEvent::StaticClass()))
			{
				CustomEvents.Add(Node);
			}
			else if (FBAUtils::IsInputNode(Node))
			{
				InputEvents.Add(Node);
			}
			else if (Node->IsA(UK2Node_ComponentBoundEvent::StaticClass()))
			{
				ComponentEvents.Add(Node);
			}
			else if (Node->IsA(UK2Node_Event::StaticClass()))
			{
				ActorEvents.Add(Node);
			}
			else if (FBAUtils::IsEventNode(Node) || (bIsBlueprintFormatting && IsBlueprintRootNode(Node, false)))
			{
				OtherEvents.Add(Node);
			}
		}
		else
		{
			if (FBAUtils::IsEventNode(Node) || FBAUtils::IsExtraRootNode(Node) || (bIsBlueprintFormatting && IsBlueprintRootNode(Node, false)))
			{
				OtherEvents.Add(Node);
			}
		}
	}

	if (FormatAllStyle == EBAFormatAllStyle::NodeType)
	{
		// TODO: Add setting to allow for user-defined columns
		FormatAllColumns = {
			ExtraNodes,
			ActorEvents,
			CustomEvents,
			InputEvents,
			ComponentEvents,
			OtherEvents
		};
	}
	else
	{
		FormatAllColumns = { OtherEvents };
	}

	const auto ExtraRootNodeSorter = [](TWeakObjectPtr<UEdGraphNode> NodeA, TWeakObjectPtr<UEdGraphNode> NodeB)
	{
		return FBAUtils::GetPinsByDirection(NodeA.Get(), EGPD_Input).Num() < FBAUtils::GetPinsByDirection(NodeB.Get(), EGPD_Input).Num();
	};

	const auto TopMostSorter = [](const TWeakObjectPtr<UEdGraphNode>& NodeA, const TWeakObjectPtr<UEdGraphNode>& NodeB)
	{
		if (!NodeA.IsValid())
		{
			return false;
		}

		if (!NodeB.IsValid())
		{
			return true;
		}

		return NodeA.Get()->NodePosY < NodeB.Get()->NodePosY;
	};

	bool bHasNodeToFormat = false;

	for (int i = 0; i < FormatAllColumns.Num(); ++i)
	{
		TArray<TWeakObjectPtr<UEdGraphNode>>& Column = FormatAllColumns[i];
		Column.RemoveAll([](TWeakObjectPtr<UEdGraphNode>& Node)
		{
			return !Node.IsValid();
		});

		for (TWeakObjectPtr<UEdGraphNode> WeakPtr : Column)
		{
			if (WeakPtr.IsValid())
			{
				UEdGraphNode* Node = WeakPtr.Get();

				TSet<UEdGraphNode*> NodeTree = FBAUtils::GetNodeTree(Node);
				GH.UpdateNodeSizesChanges(NodeTree.Array());
			}
		}

		if (!bHasNodeToFormat && Column.Num() > 0)
		{
			bHasNodeToFormat = true;
		}

		// TODO: Handle extra root nodes properly
		if ((i == 0) && (FormatAllStyle == EBAFormatAllStyle::NodeType))
		{
			ExtraNodes.StableSort(ExtraRootNodeSorter);
		}
		else
		{
			Column.Sort(TopMostSorter);
		}
	}

	if (bHasNodeToFormat)
	{
		FormatAllTransaction = MakeShared<FBAScopedGraphAction>(GH.GetFocusedEdGraph(), "Format All Nodes");
	}
}

void FBAFormatRequest::RequestFormatNode(UEdGraphNode* Node, TSharedPtr<FBAScopedGraphAction> InPendingTransaction, FEdGraphFormatterParameters InFormatterParameters)
{
	if (FBAUtils::IsCommentNode(Node) || FBAUtils::IsKnotNode(Node))
	{
		return;
	}

	if (!FBAUtils::IsGraphNode(Node))
	{
		return;
	}

	OngoingFormattingAction = InPendingTransaction;
	FormatterParameters = InFormatterParameters;
	PendingFormatting.Add(Node);

	TSet<UEdGraphNode*> NodeTree = FBAUtils::GetNodeTree(Node);
	GH.UpdateNodeSizesChanges(NodeTree.Array());
}

void FBAFormatRequest::UpdateNodesRequiringFormatting()
{
	if (GH.GetGraphTasks().HasTaskRunning())
	{
		return;
	}

	RunFormatting();
}

bool FBAFormatRequest::IsBlueprintRootNode(UEdGraphNode* Node, bool bOnlyOutputRoots)
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

void FBAFormatRequest::Reset()
{
	PendingFormatting.Reset();
	FormatAllColumns.Reset();
	FormatterParameters.Reset();

	bSaveAfterFormatting = false;

	ResetTransactions();
}

UEdGraphNode* FBAFormatRequest::FindRootNode(UEdGraphNode* InitialNode)
{
	TSharedPtr<FFormatterInterface> Formatter = MakeFormatter();
	if (!Formatter.IsValid())
	{
		return nullptr;
	}

	FBAFormatterSettings FormatterSettings = Formatter->GetFormatterSettings();
	EEdGraphPinDirection FormatterDirection = FormatterSettings.FormatterDirection;

	const auto OppositeDirection = UEdGraphPin::GetComplementaryDirection(FormatterDirection);

	const auto NodeTreeFilter = [&](const FPinLink& Link)
	{
		return FormatterParameters.IsValidNode(Link.GetToNodeUnsafe())
			&& FBAFormatterUtils::FilterDelegateLink(Link)
			&& FormatterParameters.IsValidLink(Link);
	};

	TSet<UEdGraphNode*> NodeTree = FBAUtils::GetNodeTreeWithFilter(InitialNode, NodeTreeFilter);

	const bool bIsParameterTree = !NodeTree.Array().ContainsByPredicate(FBAUtils::IsNodeImpure);
	if (bIsParameterTree)
	{
		const auto Filter = [&](const FPinLink& Link)
		{
			UEdGraphNode* Node = Link.GetToNodeUnsafe();
			return FBAUtils::IsNodePure(Node)
				&& !FBAUtils::IsKnotNode(Node)
				&& FormatterParameters.IsValidNode(Node)
				&& FormatterParameters.IsValidLink(Link);
		};

		// get the right-most pure node
		return FBAUtils::GetTopMostWithFilter(InitialNode, EGPD_Output, Filter);
	}

	/* TODO consider if BlueprintInputRoots are needed, it seems to cause issues to select an
	  * Input root node for BP (which is not a parameter tree) */
	// TArray<UEdGraphNode*> BlueprintInputRoots;

	TArray<UEdGraphNode*> InputTunnel;
	TArray<UEdGraphNode*> BlueprintOutputRoots;
	TArray<UEdGraphNode*> EventNodes;
	TArray<UEdGraphNode*> UnlinkedNodes;
	TArray<UEdGraphNode*> RootNodes;
	TArray<UEdGraphNode*> ImpureNodes;

	for (UEdGraphNode* Node : NodeTree)
	{
		// UE_LOG(LogTemp, Warning, TEXT("Checking Node %s"), *FBAUtils::GetNodeName(Node));
		if (FBAUtils::IsKnotNode(Node))
		{
			continue;
		}

		if (FormatterSettings.FormatterType == EBAFormatterType::Blueprint)
		{
			if (IsBlueprintRootNode(Node, true))
			{
				// prioritize input tunnel nodes (for macros / collapsed graphs)
				EGraphType GraphType = FBAUtils::GetGraphType(Node->GetGraph());
				if (GraphType == GT_Ubergraph || GraphType == GT_Macro)
				{
					if (Node->GetClass() == UK2Node_Tunnel::StaticClass())
					{
						return Node;
					}
				}

				BlueprintOutputRoots.Add(Node);
			}
		}

		if (FBAUtils::IsExtraRootNode(Node) && FBAUtils::DoesNodeHaveExecutionTo(InitialNode, Node))
		{
			// UE_LOG(LogTemp, Warning, TEXT("\tRoot node EXTRA %s"), *FBAUtils::GetNodeName(Node));
			RootNodes.Add(Node);
			continue;
		}

		if (FBAUtils::IsNodeImpure(Node))
		{
			ImpureNodes.Add(Node);

			if (FBAUtils::IsEventNode(Node, FormatterDirection) && FBAUtils::DoesNodeHaveExecutionTo(InitialNode, Node))
			{
				// UE_LOG(LogTemp, Warning, TEXT("\tRoot node EVENT %s"), *FBAUtils::GetNodeName(Node));
				EventNodes.Add(Node);
				continue;
			}

			TArray<UEdGraphPin*> LinkedInputPins = FBAUtils::GetLinkedPins(Node, OppositeDirection).FilterByPredicate(FBAUtils::IsExecPin);

			if ((LinkedInputPins.Num() == 0) && FBAUtils::DoesNodeHaveExecutionTo(InitialNode, Node))
			{
				// UE_LOG(LogTemp, Warning, TEXT("\tRoot node UNLINKED %s"), *FBAUtils::GetNodeName(Node));
				UnlinkedNodes.Emplace(Node);
			}
		}
	}

	const auto& IsNotLinkedToStart = [&InitialNode](UEdGraphNode* A)
	{
		return !FBAUtils::DoesNodeHaveExecutionTo(InitialNode, A);
	};

	BlueprintOutputRoots.RemoveAll(IsNotLinkedToStart);

	// UE_LOG(LogBlueprintAssist, Log, TEXT("BP Out %d Events %d Unlinked %d Root %d"), BlueprintOutputRoots.Num(), EventNodes.Num(), UnlinkedNodes.Num(), RootNodes.Num());
	if (EventNodes.IsEmpty() && UnlinkedNodes.IsEmpty() && RootNodes.IsEmpty() && BlueprintOutputRoots.IsEmpty())
	{
		// prefer executable / impure nodes
		UEdGraphNode* StartNode = InitialNode;
		if (ImpureNodes.Num() > 0)
		{
			StartNode = ImpureNodes[0];
		}

		const auto Filter = [&](UEdGraphNode* Node)
		{
			return FormatterParameters.IsValidNode(Node) && FBAUtils::IsNodeImpure(Node);
		};

		UEdGraphNode* NodeInDirection = FBAUtils::GetTopMostWithFilter(StartNode, OppositeDirection, Filter);

		// UE_LOG(LogBlueprintAssist, Warning, TEXT("Node in dir %s (Orig %s) (Dir %d)"), *FBAUtils::GetNodeName(NodeInDirection), *FBAUtils::GetNodeName(StartNode), OppositeDirection);

		const TArray<UEdGraphNode*> Visited = { NodeInDirection };
		while (UK2Node_Knot* Knot = Cast<UK2Node_Knot>(NodeInDirection))
		{
			const auto& LinkedOut = Knot->GetOutputPin()->LinkedTo;
			if (LinkedOut.Num() > 0)
			{
				auto NextNode = LinkedOut[0]->GetOwningNode();
				if (Visited.Contains(NextNode))
				{
					break;
				}

				NodeInDirection = NextNode;
			}
		}

		return NodeInDirection;
	}

	const auto& SortByDirection = [&FormatterDirection](const UEdGraphNode& A, const UEdGraphNode& B)
	{
		if (FormatterDirection == EGPD_Output) // sort left to right
		{
			if (A.NodePosX != B.NodePosX)
			{
				return A.NodePosX < B.NodePosX;
			}
		}
		else // sort right to left
		{
			if (A.NodePosX != B.NodePosX)
			{
				return A.NodePosX > B.NodePosX;
			}
		}

		// sort top to bottom
		return A.NodePosY < B.NodePosY;
	};

	if (BlueprintOutputRoots.Num() > 0) // use the top left most blueprint root node
	{
		FormatterDirection = EGPD_Output;
		BlueprintOutputRoots.Sort(SortByDirection);
		return BlueprintOutputRoots[0];
	}

	if (RootNodes.Num() > 0)
	{
		RootNodes.StableSort(SortByDirection);
		return RootNodes[0];
	}

	if (EventNodes.Num() > 0) // use the top left most event node
	{
		EventNodes.Sort(SortByDirection);
		return EventNodes[0];
	}

	if (UnlinkedNodes.Contains(InitialNode))
	{
		return InitialNode;
	}

	if (UnlinkedNodes.Num() > 0)
	{
		// use the top left most unlinked node
		UnlinkedNodes.Sort(SortByDirection);
		return UnlinkedNodes[0];
	}

	UE_LOG(LogBlueprintAssist, Error, TEXT("[%hs] No root node found"), __FUNCTION__);
	return nullptr;
}

TSharedPtr<FFormatterInterface> FBAFormatRequest::MakeFormatter()
{
	UEdGraph* EdGraph = GH.GetFocusedEdGraph();
	if (!EdGraph)
	{
		return nullptr;
	}

	TSharedRef<FBAGraphHandler> GraphHandlerPtr = GH.AsShared();

	if (FBAFormatterSettings* FormatterSettings = UBASettings::FindFormatterSettings(EdGraph))
	{
		switch (FormatterSettings->FormatterType)
		{
		case EBAFormatterType::Blueprint:
			return MakeShared<FEdGraphFormatter>(GraphHandlerPtr, FormatterParameters);
		case EBAFormatterType::BehaviorTree:
			return MakeShared<FBehaviorTreeGraphFormatter>(GraphHandlerPtr, FormatterParameters);
		case EBAFormatterType::Simple:
			return MakeShared<FSimpleFormatter>(GraphHandlerPtr, FormatterParameters);
		default: ;
		}
	}

	if (FBAUtils::IsBlueprintGraph(EdGraph))
	{
		return MakeShared<FEdGraphFormatter>(GraphHandlerPtr, FormatterParameters);
	}

	return nullptr;
}

bool FBAFormatRequest::HasActiveTransaction() const
{
	const bool bHasPendingTransaction = OngoingFormattingAction.IsValid() && OngoingFormattingAction->IsOutstanding();
	const bool bHasFormatAllTransaction = FormatAllTransaction.IsValid() && FormatAllTransaction->IsOutstanding();
	return bHasPendingTransaction || bHasFormatAllTransaction;
}

TSharedPtr<FFormatterInterface> FBAFormatRequest::FormatNodes(UEdGraphNode* Node, bool bUsingFormatAll)
{
	if (!GH.GetGraphPanel().IsValid())
	{
		return nullptr;
	}

	if (!FBAUtils::IsGraphNode(Node))
	{
		return nullptr;
	}

	UEdGraph* EdGraph = GH.GetFocusedEdGraph();
	if (EdGraph == nullptr)
	{
		return nullptr;
	}

	if (FBlueprintEditorUtils::IsGraphReadOnly(EdGraph))
	{
		return nullptr;
	}

	TSharedPtr<FFormatterInterface> Formatter;

	for (UEdGraphNode* EdNode : GH.GetFocusedEdGraph()->Nodes)
	{
		if (GH.GetNodeData(EdNode).bLocked)
		{
			FormatterParameters.IgnoredNodes.Add(EdNode);
		}
	}

	FormatterParameters.InitIgnoredPins(GH.AsShared());

	UEdGraphNode* NodeToFormat = FindRootNode(Node);
	if (!NodeToFormat)
	{
		FBAMiscUtils::ShowSimpleSlateNotification(INVTEXT("Unable to format, no root node found"), SNotificationItem::CS_Fail);
		return nullptr;
	}

	if (!FormatterParameters.MasterContainsGraph)
	{
		FormatterParameters.MasterContainsGraph = MakeShared<FBACommentContainsGraph>();
		FormatterParameters.MasterContainsGraph->Init(GH.AsShared());
		FormatterParameters.MasterContainsGraph->BuildCommentTree();
	}

	// UE_LOG(LogTemp, Warning, TEXT("Using root node %s"), *FBAUtils::GetNodeName(NodeToFormat));

	if (FBAUtils::IsBlueprintGraph(EdGraph))
	{
		Formatter = MakeShared<FEdGraphFormatter>(GH.AsShared(), FormatterParameters);
	}
	else
	{
		Formatter = MakeFormatter();
	}

	if (Formatter.IsValid())
	{
		if (!bUsingFormatAll)
		{
			PreFormatting();
		}

		const FString GraphHint = FString::Printf(TEXT("%s (%s)"),
												*FBAUtils::GetObjectClassName(EdGraph).ToString(),
												*FBAUtils::GraphTypeToString(FBAUtils::GetGraphType(EdGraph))
		);

		FGenericCrashContext::SetEngineData("BAGraphHint", GraphHint);
		FGenericCrashContext::SetEngineData("BAAssetHint", FBAUtils::GetObjectClassName(EdGraph->GetOutermostObject()).ToString());

#if WITH_ADDITIONAL_CRASH_CONTEXTS
		if (UBASettings_Advanced::Get().bDumpFormattingCrashNodes && (UBASettings_Advanced::Get().CrashReportingMethod != EBACrashReportingMethod::Never))
		{
			static TCHAR TextBuffer[48 * 1024];
			static TCHAR PathBuffer[1024];

			FString NodeTreeStr;

			// ExportNodesToText seems to have some odd side-effects on non-bp graphs, so skip it for other graphs
			if (FBAUtils::IsBlueprintGraph(GH.GetFocusedEdGraph()))
			{
				TSet<UEdGraphNode*> RelatedNodes = FBAUtils::GetNodeTree(NodeToFormat);
				if (TSharedPtr<FBACommentContainsGraph> CommentGraph = FormatterParameters.MasterContainsGraph)
				{
					TSet<UEdGraphNode*> NodesCopy = RelatedNodes;
					for (UEdGraphNode* N : NodesCopy)
					{
						for (UEdGraphNode_Comment* C : CommentGraph->GetContainingCommentsForNode(N))
						{
							RelatedNodes.Add(C);
						}
					}
				}

				if (RelatedNodes.Num())
				{
					NodeTreeStr = FBAMiscUtils::CompressString(FBAUtils::ExportNodesToText(RelatedNodes));

					// if our graph is too big then just clear it
					if (NodeTreeStr.Len() >= UE_ARRAY_COUNT(TextBuffer))
					{
						// UE_LOG(LogTemp, Warning, TEXT("Node Tree won't be saved %d %d"), NodeTreeStr.Len(), (int) UE_ARRAY_COUNT(TextBuffer));
						NodeTreeStr.Empty();
					}
				}
			}

			const FString CrashDirPath = FPaths::ConvertRelativePathToFull(FBAPaths::BACrashDir());
#if BA_UE_VERSION_OR_LATER(5, 4)
			const FString CrashPathName = FGenericCrashContext::GetCachedSessionContext().CrashGUIDRoot;
#else
			const FString CrashPathName = TEXT("BlueprintAssist");
#endif

			// Ignore the default writer since we want to save the log to a different path so it doesn't get sent to Epic
			UE_ADD_CRASH_CONTEXT_SCOPE([&](FCrashContextExtendedWriter& Writer)
				{
				// the _0000 is meant to come from FGenericCrashContext::StaticCrashContextIndex but it is private so assume it is 0
				if (!NodeTreeStr.IsEmpty())
				{
				FCString::Snprintf(PathBuffer, UE_ARRAY_COUNT(PathBuffer), TEXT("%s/%s_0000"), *CrashDirPath, *CrashPathName);
				IPlatformFile::GetPlatformPhysical().CreateDirectoryTree(PathBuffer);

				FCString::Strcat(PathBuffer, UE_ARRAY_COUNT(PathBuffer), TEXT("/Nodes.txt"));

				FCString::Snprintf(TextBuffer, UE_ARRAY_COUNT(TextBuffer), TEXT("%s"), *NodeTreeStr);
				FBAMiscUtils::WriteTextToFile(PathBuffer, TextBuffer);
				}
				});

			if (ensureMsgf(IsValid(NodeToFormat), TEXT("NodeToFormat become invalid before formatting. Unable to perform formatting.")))
			{
				Formatter->PreFormatting();
				Formatter->FormatNode(NodeToFormat);
				Formatter->PostFormatting();

				if (!bUsingFormatAll)
				{
					PostFormatting({ Formatter });
				}
			}
		}
		else
#endif
		{
			Formatter->PreFormatting();
			Formatter->FormatNode(NodeToFormat);
			Formatter->PostFormatting();

			if (!bUsingFormatAll)
			{
				PostFormatting({ Formatter });
			}
		}
	}

	return Formatter;
}

void FBAFormatRequest::PreFormatting()
{
	if (auto GraphOverlay = GH.GetGraphOverlay())
	{
		GraphOverlay->ClearBounds();
		GraphOverlay->ClearNodesInQueue();
	}

	if (UBASettings::Get().bSaveAllBeforeFormatting)
	{
		constexpr bool bPromptUserToSave = false;
		constexpr bool bSaveMapPackages = true;
		constexpr bool bSaveContentPackages = true;
		FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages);
	}

	if (auto Graph = GH.GetFocusedEdGraph())
	{
		for (auto Node : Graph->Nodes)
		{
			PreFormatNodePositions.Add(Node->NodeGuid, FIntPoint(Node->NodePosX, Node->NodePosY));
		}
	}
}

void FBAFormatRequest::PostFormatting(const TArray<TSharedPtr<FFormatterInterface>>& Formatters)
{
	if (ZoomToTargetPostFormatting.IsValid())
	{
		GH.AutoLerpToNewlyCreatedNode(ZoomToTargetPostFormatting.Get());
		ZoomToTargetPostFormatting = nullptr;
	}

	PostFormatComments(Formatters);

	TSet<UEdGraphNode*> FormattedNodeSet;

	// update change data
	for (TSharedPtr<FFormatterInterface> FormatterInterface : Formatters)
	{
		for (UEdGraphNode* FormattedNode : FormatterInterface->GetFormattedNodes())
		{
			if (FormattedNode)
			{
				// update gh change data
				GH.UpdateNodeChangeData(FormattedNode);

				// update cache last formatted location
				auto& NodeData = GH.GetNodeData(FormattedNode);
				NodeData.SetLastFormatted(FormattedNode);

				FormattedNodeSet.Add(FormattedNode);

				if (UEdGraphNode* Root = FormatterInterface->GetRootNode())
				{
					NodeData.LastRoot = Root->NodeGuid;
				}
			}
		}
	}

	for (UEdGraphNode* FormattedNode : FormattedNodeSet)
	{
		// the Metasound Graph requires you to move nodes via GraphNode::MoveTo, so it's easier to do it once here
		// certain graph types require you to update node positions via their schema or GraphNode::MoveTo
		if (FIntPoint* PrePosition = PreFormatNodePositions.Find(FormattedNode->NodeGuid))
		{
			int32 NewX = FormattedNode->NodePosX;
			int32 NewY = FormattedNode->NodePosY;

			if (NewX != PrePosition->X || NewY != PrePosition->Y)
			{
				// for undo on control rig to work correctly, reset the position back to what it was pre-formatting then move it again
				FormattedNode->NodePosX = PrePosition->X;
				FormattedNode->NodePosY = PrePosition->Y;

				GH.GetSchema().SetNodePosition(FormattedNode, NewX, NewY, GH.GetGraphPanel());
			}
		}
		else
		{
			GH.GetSchema().SetNodePosition(FormattedNode, FormattedNode->NodePosX, FormattedNode->NodePosY, GH.GetGraphPanel());
		}
	}

	PreFormatNodePositions.Empty();
	FormatterParameters.Reset();
	GH.ResetTransactions();

	if (bSaveAfterFormatting)
	{
		RunSavePostFormatting();
		bSaveAfterFormatting = false;
	}

	OnPostFormatting.Broadcast();

#if 0
	for (TSharedPtr<FFormatterInterface> Formatter : Formatters)
	{
		TSet<UEdGraphNode*> Nodes = Formatter->GetFormattedNodes();
		if (Nodes.Num())
		{
			GraphOverlay->DrawBounds(FBAUtils::GetNodeArrayBounds(Nodes.Array()));
		}
	}
#endif
}

void FBAFormatRequest::PostFormatComments(const TArray<TSharedPtr<FFormatterInterface>>& Formatters)
{
	if (!FormatterParameters.MasterContainsGraph)
	{
		return;
	}

	TSet<UEdGraphNode_Comment*> AllRelatedComments;
	TSet<UEdGraphNode_Comment*> RelatedComments;
	for (TSharedPtr<FFormatterInterface> FormatterInterface : Formatters)
	{
		if (FCommentHandler* MainCH = FormatterInterface->GetCommentHandler())
		{
			if (MainCH->IsValid())
			{
				AllRelatedComments.Append(MainCH->IgnoredRelatedComments);
				AllRelatedComments.Append(MainCH->GetComments());

				RelatedComments.Append(MainCH->IgnoredRelatedComments);
			}
		}

		for (TSharedPtr<FFormatterInterface> ChildFormatter : FormatterInterface->GetChildFormatters())
		{
			if (FCommentHandler* ChildCH = FormatterInterface->GetCommentHandler())
			{
				if (ChildCH->IsValid())
				{
					AllRelatedComments.Append(ChildCH->IgnoredRelatedComments);
					AllRelatedComments.Append(ChildCH->GetComments());

					RelatedComments.Append(ChildCH->IgnoredRelatedComments);
					for (UEdGraphNode_Comment* Comment : ChildCH->GetComments())
					{
						RelatedComments.Remove(Comment);
					}
				}
			}
		}
	}

	bool bGraphPanelNeedsRefresh = false;

	for (UEdGraphNode_Comment* Comment : AllRelatedComments)
	{
		TSet<UEdGraphNode*> Visited;
		TSet<UEdGraphNode_Comment*> Ignored;
		if (TOptional<FSlateRect> Bounds = FormatterParameters.MasterContainsGraph->GetCommentBounds(Comment, Ignored, nullptr, Visited))
		{
			Comment->Modify();
			Comment->SetBounds(*Bounds);
			bGraphPanelNeedsRefresh = true;
		}
	}

	for (UEdGraphNode_Comment* Comment : RelatedComments)
	{
		TSet<UEdGraphNode*> Visited;
		TSet<UEdGraphNode_Comment*> Ignored;
		if (TOptional<FSlateRect> Bounds = FormatterParameters.MasterContainsGraph->GetCommentBounds(Comment, Ignored, nullptr, Visited))
		{
			Comment->Modify();
			Comment->SetBounds(*Bounds);
			bGraphPanelNeedsRefresh = true;

			if (UBASettings_Advanced::Get().bHighlightBadComments)
			{
				GH.GetGraphOverlay()->DrawBounds(*Bounds, FLinearColor::Red, 0.5f);
			}
		}
	}

	if (UBASettings_Advanced::Get().bForceRefreshGraphAfterFormatting && bGraphPanelNeedsRefresh)
	{
		if (TSharedPtr<SGraphPanel> GraphPanel = GH.GetGraphPanel())
		{
			GraphPanel->PurgeVisualRepresentation();

			const auto UpdateGraphPanel = [](TWeakPtr<SGraphPanel> LocalPanel)
			{
				if (LocalPanel.IsValid())
				{
					LocalPanel.Pin()->Update();
				}
			};

			GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateLambda(UpdateGraphPanel, GraphPanel));
		}
	}
}

void FBAFormatRequest::RunFormatting()
{
	FormatPendingNodes();
	FormatPendingColumns();
}

void FBAFormatRequest::FormatPendingNodes()
{
	if (PendingFormatting.Num())
	{
		// remove invalid / deleted nodes
		for (auto It = PendingFormatting.CreateIterator(); It; ++It)
		{
			TWeakObjectPtr<UEdGraphNode> Node = *It;
			if (!Node.IsValid() || FBAUtils::IsNodeDeleted(Node.Get()))
			{
				It.RemoveCurrent();
			}
		}
	}

	if (PendingFormatting.Num() == 0)
	{
		return;
	}

	TArray<TWeakObjectPtr<UEdGraphNode>> PendingFormattingArr = PendingFormatting.Array();

	TArray<TWeakObjectPtr<UEdGraphNode>> NodesWithoutSize = PendingFormattingArr.FilterByPredicate([&](TWeakObjectPtr<UEdGraphNode> Node)
	{
		return Node.IsValid() && !GH.GetNodeData(Node.Get()).HasSize();
	});

	// if we have nodes requiring size, then we should wait for them to process and early exit here
	if (NodesWithoutSize.Num() > 0)
	{
		bool bPendingSize = false;
		for (TWeakObjectPtr<UEdGraphNode> Pending : PendingFormatting)
		{
			if (Pending.IsValid())
			{
				TSet<UEdGraphNode*> NodeTree = FBAUtils::GetNodeTree(Pending.Get());
				bPendingSize |= GH.UpdateNodeSizesChanges(NodeTree.Array());
			}
		}

		if (bPendingSize)
		{
			return;
		}
	}

	// format dirty nodes
	TArray<TWeakObjectPtr<UEdGraphNode>> NodesToFormatCopy = PendingFormattingArr.FilterByPredicate([&](TWeakObjectPtr<UEdGraphNode> Node)
	{
		return Node.IsValid() ? GH.GetNodeData(Node.Get()).HasSize() : false;
	});

	int CountError = NodesToFormatCopy.Num();

	while (NodesToFormatCopy.Num() > 0)
	{
		CountError -= 1;
		if (CountError < 0)
		{
			FBAMiscUtils::ShowSimpleSlateNotification(INVTEXT("Failed to format all nodes"), SNotificationItem::CS_Fail);

			NodesToFormatCopy.Empty();
			PendingFormatting.Empty();
			break;
		}

		TWeakObjectPtr<UEdGraphNode> NodeToFormat = NodesToFormatCopy.Pop();
		if (!NodeToFormat.IsValid())
		{
			continue;
		}

		// UE_LOG(LogBlueprintAssist, Warning, TEXT("Formatting %s"), *FBAUtils::GetNodeName(NodeToFormat));

		TSharedPtr<FFormatterInterface> Formatter = FormatNodes(NodeToFormat.Get());
		PendingFormatting.Remove(NodeToFormat);
		NodesToFormatCopy.Remove(NodeToFormat);

		if (Formatter.IsValid())
		{
			for (UEdGraphNode* Node : Formatter->GetFormattedNodes())
			{
				PendingFormatting.Remove(Node);
				NodesToFormatCopy.Remove(Node);
			}
		}

		if (GH.ReplaceNewNodeTransaction.IsValid())
		{
			GH.ReplaceNewNodeTransaction.Reset();
		}
	}
}

void FBAFormatRequest::FormatPendingColumns()
{
	if (FormatAllColumns.Num() == 0)
	{
		return;
	}

	// handle format all nodes
	FormatterParameters.MasterContainsGraph = MakeShared<FBACommentContainsGraph>();
	FormatterParameters.MasterContainsGraph->Init(GH.AsShared());
	FormatterParameters.MasterContainsGraph->BuildCommentTree();

	PreFormatting();

	if (UBASettings::Get().FormatAllStyle == EBAFormatAllStyle::Smart)
	{
		SmartFormatAll();
	}
	else
	{
		// this also handles EBAFormatAllStyle::NodeType, should separate into another function
		SimpleFormatAll();
	}

	FormatAllColumns.Reset();
	FormatterParameters.Reset();
}

void FBAFormatRequest::SimpleFormatAll()
{
	TSharedPtr<FBACommentContainsGraph> MasterContainsGraph = MakeShared<FBACommentContainsGraph>();
	MasterContainsGraph->Init(GH.AsShared());
	MasterContainsGraph->BuildCommentTree();

	TArray<TSharedPtr<FFormatterInterface>> AllFormatterSaved;
	TArray<TSharedPtr<FFormatterInterface>> AllFormatters;

	// format all the nodes
	TSet<UEdGraphNode*> PreviouslyFormattedNodes;

	for (TWeakObjectPtr<UEdGraphNode> WeakPtr : FormatAllColumns[0])
	{
		UEdGraphNode* Node = WeakPtr.Get();
		if (PreviouslyFormattedNodes.Contains(Node))
		{
			continue;
		}

		Node->Modify();

		TSharedPtr<FFormatterInterface> Formatter = FormatNodes(Node, true);
		AllFormatterSaved.Add(Formatter);

		PreviouslyFormattedNodes.Append(Formatter->GetFormattedNodes());
	}

	AllFormatters = AllFormatterSaved;

	int NumColumns = 0;
	float ColumnX = 0;

	while (AllFormatters.Num() > 0)
	{
		TArray<TSharedPtr<FFormatterInterface>> AllFormattersCopy = AllFormatters;

		// sort formatted nodes by left most
		AllFormattersCopy.Sort([](TSharedPtr<FFormatterInterface> FormatterA, TSharedPtr<FFormatterInterface> FormatterB)
		{
			UEdGraphNode* RootA = FormatterA->GetRootNode();
			UEdGraphNode* RootB = FormatterB->GetRootNode();
			if (RootA->NodePosX != RootB->NodePosX)
			{
				return RootA->NodePosX < RootB->NodePosX;
			}

			return RootA->NodePosY < RootB->NodePosY;
		});

		TOptional<float> RightMost;
		TArray<TSharedPtr<FFormatterInterface>> CurrentColumn;

		float CommentOffset = 0;

		// create columns by checking for overlapping formatted node-trees
		for (TSharedPtr<FFormatterInterface> Formatter : AllFormattersCopy)
		{
			TSet<UEdGraphNode*> FormatterNodes = Formatter->GetFormattedNodes();
			FSlateRect CommentBounds = FBAUtils::GetCachedNodeArrayBoundsWithComments(GH.AsShared(), Formatter->GetCommentHandler(), Formatter->GetFormattedNodes().Array());
			FSlateRect NodeBounds = FBAUtils::GetCachedNodeArrayBounds(GH.AsShared(), Formatter->GetFormattedNodes().Array());
			FSlateRect Bounds = UBASettings::Get().bApplyCommentPadding ? CommentBounds : NodeBounds;

			if (!RightMost.IsSet())
			{
				RightMost = Bounds.Right;
			}
			else if (Bounds.Left < RightMost.GetValue())
			{
				RightMost = FMath::Max(RightMost.GetValue(), Bounds.Right);
			}
			else
			{
				// this node is not in this column, skip it
				continue;
			}

			if (NumColumns > 0)
			{
				CommentOffset = FMath::Max(CommentOffset, NodeBounds.Left - CommentBounds.Left);
			}

			CurrentColumn.Add(Formatter);
			AllFormatters.Remove(Formatter);
		}

		if (TSharedPtr<SBlueprintAssistGraphOverlay> Overlay = GH.GetGraphOverlay())
		{
			Overlay->DrawBounds(FBAFormatterUtils::GetFormatterArrayBounds(CurrentColumn, GH.AsShared(), true));
		}

		ColumnX += CommentOffset;

		FormatColumn(CurrentColumn, ColumnX);

		FSlateRect ColumnBounds = FBAFormatterUtils::GetFormatterArrayBounds(CurrentColumn, GH.AsShared(), UBASettings::Get().bApplyCommentPadding);
		ColumnX = ColumnBounds.Right + UBASettings::Get().FormatAllPadding.X;
		ColumnX = FBAUtils::AlignTo8x8Grid(ColumnX, EBARoundingMethod::Ceil);
		NumColumns += 1;
	}

	FormatAllColumns.Empty();
	PostFormatting(AllFormatters);
}

void FBAFormatRequest::SmartFormatAll()
{
	TSharedPtr<FBACommentContainsGraph> MasterContainsGraph = MakeShared<FBACommentContainsGraph>();
	MasterContainsGraph->Init(GH.AsShared());
	MasterContainsGraph->BuildCommentTree();

	TArray<TSharedPtr<FFormatterInterface>> AllFormatterSaved;
	TArray<TSharedPtr<FFormatterInterface>> AllFormatters;

	// format all the nodes
	TSet<UEdGraphNode*> PreviouslyFormattedNodes;

	for (TWeakObjectPtr<UEdGraphNode> WeakPtr : FormatAllColumns[0])
	{
		UEdGraphNode* Node = WeakPtr.Get();
		if (PreviouslyFormattedNodes.Contains(Node))
		{
			continue;
		}

		Node->Modify();

		TSharedPtr<FFormatterInterface> Formatter = FormatNodes(Node, true);
		AllFormatterSaved.Add(Formatter);

		PreviouslyFormattedNodes.Append(Formatter->GetFormattedNodes());
	}

	AllFormatters = AllFormatterSaved;

	int NumColumns = 0;
	float ColumnX = 0;

	while (AllFormatters.Num() > 0)
	{
		TArray<TSharedPtr<FFormatterInterface>> AllFormattersCopy = AllFormatters;

		// sort formatted nodes by left most
		AllFormattersCopy.Sort([](TSharedPtr<FFormatterInterface> FormatterA, TSharedPtr<FFormatterInterface> FormatterB)
		{
			UEdGraphNode* RootA = FormatterA->GetRootNode();
			UEdGraphNode* RootB = FormatterB->GetRootNode();
			if (RootA->NodePosX != RootB->NodePosX)
			{
				return RootA->NodePosX < RootB->NodePosX;
			}

			return RootA->NodePosY < RootB->NodePosY;
		});

		TOptional<float> RightMost;
		TArray<TSharedPtr<FFormatterInterface>> CurrentColumn;

		float CommentOffset = 0;

		// create columns by checking for overlapping formatted node-trees
		for (TSharedPtr<FFormatterInterface> Formatter : AllFormattersCopy)
		{
			TSet<UEdGraphNode*> FormatterNodes = Formatter->GetFormattedNodes();
			FSlateRect CommentBounds = FBAUtils::GetCachedNodeArrayBoundsWithComments(GH.AsShared(), Formatter->GetCommentHandler(), Formatter->GetFormattedNodes().Array());
			FSlateRect NodeBounds = FBAUtils::GetCachedNodeArrayBounds(GH.AsShared(), Formatter->GetFormattedNodes().Array());
			FSlateRect Bounds = UBASettings::Get().bApplyCommentPadding ? CommentBounds : NodeBounds;

			if (!RightMost.IsSet())
			{
				RightMost = Bounds.Right;
			}
			else if (Bounds.Left < RightMost.GetValue())
			{
				RightMost = FMath::Max(RightMost.GetValue(), Bounds.Right);
			}
			else
			{
				// this node is not in this column, skip it
				continue;
			}

			if (NumColumns > 0)
			{
				CommentOffset = FMath::Max(CommentOffset, NodeBounds.Left - CommentBounds.Left);
			}

			CurrentColumn.Add(Formatter);
			AllFormatters.Remove(Formatter);
		}

		GH.GetGraphOverlay()->DrawBounds(FBAFormatterUtils::GetFormatterArrayBounds(CurrentColumn, GH.AsShared(), true));

		ColumnX += CommentOffset;

		FormatColumn(CurrentColumn, ColumnX);

		FSlateRect ColumnBounds = FBAFormatterUtils::GetFormatterArrayBounds(CurrentColumn, GH.AsShared(), UBASettings::Get().bApplyCommentPadding);
		ColumnX = ColumnBounds.Right + UBASettings::Get().FormatAllPadding.X;
		ColumnX = FBAUtils::AlignTo8x8Grid(ColumnX, EBARoundingMethod::Ceil);
		NumColumns += 1;
	}

	FormatAllColumns.Empty();
	PostFormatting(AllFormatters);
}

void FBAFormatRequest::FormatColumn(TArray<TSharedPtr<FFormatterInterface>>& CurrentColumn, float ColumnX)
{
	ColumnX = FBAUtils::SnapToGrid(ColumnX);
	ColumnX = FBAUtils::AlignTo8x8Grid(ColumnX);


	// UE_LOG(LogTemp, Warning, TEXT("Column X %f"), ColumnX);

	// Sort the column by height
	CurrentColumn.Sort([](TSharedPtr<FFormatterInterface> FormatterA, TSharedPtr<FFormatterInterface> FormatterB)
	{
		UEdGraphNode* RootA = FormatterA->GetRootNode();
		UEdGraphNode* RootB = FormatterB->GetRootNode();
		if (RootA->NodePosY != RootB->NodePosY)
		{
			return RootA->NodePosY < RootB->NodePosY;
		}

		return RootA->NodePosX < RootB->NodePosX;
	});

	TOptional<FSlateRect> FormattedBounds;
	TSharedPtr<FFormatterInterface> PrevFormatter = nullptr;

	// position the node-trees into columns
	for (TSharedPtr<FFormatterInterface> Formatter : CurrentColumn)
	{
		TArray<UEdGraphNode*> FormattedNodes = Formatter->GetFormattedNodes().Array();

		FSlateRect CommentBounds = FBAUtils::GetCachedNodeArrayBoundsWithComments(GH.AsShared(), Formatter->GetCommentHandler(), FormattedNodes);
		FSlateRect NodeBounds = FBAUtils::GetCachedNodeArrayBounds(GH.AsShared(), FormattedNodes);

		FSlateRect CurrentBounds = UBASettings::Get().bApplyCommentPadding ? CommentBounds : NodeBounds;

		// align the position of the formatted nodes to the column
		float Left = 0;
		float Top = 0;

		switch (UBASettings::Get().FormatAllHorizontalAlignment)
		{
		case EBAFormatAllHorizontalAlignment::RootNode:
			{
				Left = NodeBounds.Left;
				Top = NodeBounds.Top;
				break;
			}
		case EBAFormatAllHorizontalAlignment::Comment:
			{
				Left = CurrentBounds.Left;
				Top = CurrentBounds.Top;
			}
			break;
		default: ;
		}

		int32 DeltaX = ColumnX - Left;
		// DeltaX = FBAUtils::SnapToGrid(Left + DeltaX) - Left;

		// offset the first formatted node's Y position to zero
		const int32 DeltaY = !FormattedBounds.IsSet() ? 0 - Top : 0;

		for (UEdGraphNode* FormattedNode : FormattedNodes)
		{
			FormattedNode->NodePosX += DeltaX;
			FormattedNode->NodePosY += DeltaY;
		}

		CurrentBounds = UBASettings::Get().bApplyCommentPadding
			? FBAUtils::GetCachedNodeArrayBoundsWithComments(GH.AsShared(), Formatter->GetCommentHandler(), FormattedNodes)
			: FBAUtils::GetCachedNodeArrayBounds(GH.AsShared(), FormattedNodes);

		NodeBounds = FBAUtils::GetCachedNodeArrayBounds(GH.AsShared(), FormattedNodes);

		if (!FormattedBounds.IsSet())
		{
			FormattedBounds = CurrentBounds;
		}
		else
		{
			float VerticalPadding = UBASettings::Get().FormatAllPadding.Y;

			if (UBASettings::Get().bUseFormatAllPaddingInComment)
			{
				if (PrevFormatter)
				{
					if (TSharedPtr<FBACommentContainsGraph> CommentGraph = FormatterParameters.MasterContainsGraph)
					{
						const TSet<UEdGraphNode_Comment*> CommentsA = CommentGraph->GetContainingCommentsForNode(Formatter->GetRootNode());
						const TSet<UEdGraphNode_Comment*> CommentsB = CommentGraph->GetContainingCommentsForNode(PrevFormatter->GetRootNode());
						if (CommentsA.Intersect(CommentsB).Num() > 0)
						{
							VerticalPadding = UBASettings::Get().FormatAllPaddingInComment;
						}
					}
				}
			}

			float Bottom = FormattedBounds->Bottom + VerticalPadding;
			Bottom = FBAUtils::AlignTo8x8Grid(Bottom, EBARoundingMethod::Ceil);

			float Delta = Bottom - CurrentBounds.Top;

			float OldRootPos = Formatter->GetRootNode()->NodePosY;
			const float RootNewPos = FBAUtils::AlignTo8x8Grid(OldRootPos + Delta, EBARoundingMethod::Ceil);
			Delta = RootNewPos - OldRootPos;

			for (UEdGraphNode* FormattedNode : FormattedNodes)
			{
				FormattedNode->NodePosY += Delta;
			}

			CurrentBounds = UBASettings::Get().bApplyCommentPadding
				? FBAUtils::GetCachedNodeArrayBoundsWithComments(GH.AsShared(), Formatter->GetCommentHandler(), FormattedNodes)
				: FBAUtils::GetCachedNodeArrayBounds(GH.AsShared(), FormattedNodes);

			FormattedBounds = FormattedBounds->Expand(CurrentBounds);
		}

		PrevFormatter = Formatter;

		// UE_LOG(LogBlueprintAssist, Warning, TEXT("\t%s"), *FBAUtils::GetNodeName(Formatter->GetRootNode()));
	}
}

void FBAFormatRequest::ResetTransactions()
{
	OngoingFormattingAction.Reset();
	FormatAllTransaction.Reset();
}

void FBAFormatRequest::RunSavePostFormatting()
{
#if BA_UE_VERSION_OR_LATER(5, 0)
	// only support blueprint graph types for now
	if (UBlueprint* BP = GH.GetBlueprint())
	{
		if (UPackage* const Package = BP->GetPackage())
		{
			FString const PackageName = Package->GetName();
			FString const PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Standalone;
			Args.SaveFlags = SAVE_FromAutosave;
			if (FBlueprintEditor* BPEditor = FBAUtils::GetBlueprintEditorForGraph(GH.GetFocusedEdGraph()))
			{
				BPEditor->Compile();
			}

			UPackage::SavePackage(Package, nullptr, *PackageFileName, Args);
		}
	}
#endif
}

bool FBAFormatRequest::RequestFormatAllAndSave()
{
	// don't format twice
	if (bSaveAfterFormatting)
	{
		return false;
	}

	bSaveAfterFormatting = true;
	RequestFormatAll();
	return true;
}
