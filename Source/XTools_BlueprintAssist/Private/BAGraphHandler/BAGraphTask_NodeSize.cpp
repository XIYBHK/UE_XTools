// Copyright fpwong. All Rights Reserved.


#include "BAGraphHandler/BAGraphTask_NodeSize.h"

#include "BlueprintAssistCache.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistUtils.h"
#include "SCommentBubble.h"
#include "SGraphPanel.h"
#include "BlueprintAssistWidgets/BlueprintAssistGraphOverlay.h"
#include "BlueprintAssistWidgets/SBASizeProgress.h"
#include "EdGraph/EdGraph.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"


FBAGraphTask_CacheNodeSizes::FBAGraphTask_CacheNodeSizes(FBAGraphHandler& InGraphHandler)
	: FBAGraphTaskBase(InGraphHandler)
{
	DelayedCacheSizeTimeout.SetOnDelayEnded(FBAOnDelayEnded::CreateRaw(this, &FBAGraphTask_CacheNodeSizes::ShowSizeTimeoutNotification));
}

bool FBAGraphTask_CacheNodeSizes::IsRunning()
{
	return PendingSize.Num() > 0;
}

bool FBAGraphTask_CacheNodeSizes::TickTask(float DeltaTime)
{
	UE_LOG(LogBAGraphTask, Log, TEXT("[%hs] Pending Size %d"), __FUNCTION__, PendingSize.Num());
	DelayedCacheSizeTimeout.Tick();

	if (PendingSize.Num() == 0)
	{
		return false;
	}

	if (!GraphHandler.HasInitialZoomFinished())
	{
		return true;
	}

	TSharedPtr<SGraphEditor> GraphEditor = GraphHandler.GetGraphEditor();
	if (!GraphEditor.IsValid())
	{
		return true;
	}

	UEdGraph* Graph = GraphHandler.GetFocusedEdGraph();
	if (Graph == nullptr)
	{
		return true;
	}

	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler.GetGraphPanel();
	if (!GraphPanel)
	{
		return true;
	}

	bool bIsPanelValid = false;
	for (auto Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		if (GraphPanel->GetNodeWidgetFromGuid(Node->NodeGuid).IsValid())
		{
			bIsPanelValid = true;
		}
	}

	if (!bIsPanelValid)
	{
		return true;
	}

	PendingSize.RemoveAll([](TWeakObjectPtr<UEdGraphNode> Node)
	{
		if (Node.IsValid() && !FBAUtils::IsNodeDeleted(Node.Get()))
		{
			return false;
		}

		return true;
	});

	if (PendingSize.Num() > 0)
	{
		UEdGraphNode* FirstNode = PendingSize[0].Get();
		if (FocusedNode != FirstNode)
		{
			DelayedCacheSizeTimeout.StartDelay(16);
			DelayedViewportZoomIn.StartDelay(2);
			FocusedNode = FirstNode;

			// Zoom fully in, to cache the node size
			GraphHandler.SetViewLocation(FVector2D(FocusedNode->NodePosX, FocusedNode->NodePosY), 1.f);
		}
		else
		{
			GraphHandler.SetViewLocation(FVector2D(FocusedNode->NodePosX, FocusedNode->NodePosY), 1.f);

			DelayedCacheSizeTimeout.Tick();
			if (DelayedCacheSizeTimeout.IsComplete())
			{
				NodeSizeTimeout -= DeltaTime;

				if (NodeSizeTimeout <= 0)
				{
					NodeSizeTimeout = 0;

					if (SizeTimeoutNotification.IsValid())
					{
						CancelSizeTimeoutNotification(true);
					}
				}
			}
		}
	}

	// delay for two ticks to make sure the size is accurate
	DelayedViewportZoomIn.Tick();
	if (DelayedViewportZoomIn.IsActive())
	{
		return true;
	}

	// cache node sizes
	for (auto It = PendingSize.CreateIterator(); It; ++It)
	{
		auto WeakPtr = *It;
		if (!WeakPtr.IsValid())
		{
			continue;
		}

		UEdGraphNode* Node = WeakPtr.Get();
		const bool bIsCommentNode = FBAUtils::IsCommentNode(Node);

		const bool bIsFocusedNode = Node == FocusedNode;

		if (!bIsFocusedNode)
		{
			// only cache the focused node resulting in more accurate node caching
			if (UBASettings_Advanced::Get().bSlowButAccurateSizeCaching)
			{
				continue;
			}

			// comment nodes should only cache size if they are the focused node
			if (bIsCommentNode)
			{
				continue;
			}
		}

		if (FBAUtils::IsNodeDeleted(Node))
		{
			It.RemoveCurrent();
			continue;
		}

		TSharedPtr<SGraphNode> GraphNode = GraphHandler.GetGraphNode(Node);
		if (!GraphNode.IsValid())
		{
			continue;
		}

		// to calculate the node size, the node should be at least visible on screen
		// do not apply this restriction for focused nodes, since they are already in the optimal location in the viewport
		if (!bIsFocusedNode && !FBAUtils::IsNodeVisible(GraphPanel, Node))
		{
			continue;
		}

		FVector2D Size = GraphNode->GetDesiredSize();

		// for comment nodes we only want to cache the title bar height
		if (FBAUtils::IsCommentNode(Node))
		{
			Size.Y = FBAUtils::GetGraphNodeMarqueeSize(GraphNode).Y;
		}

		// the size can be zero when a node is initially created, do not use this value
		if (Size.SizeSquared() <= 0)
		{
			continue;
		}

		const bool bSuccessfullyCachedNodeSize = CacheNodeSize(Node);
		if (bSuccessfullyCachedNodeSize)
		{
			It.RemoveCurrent();

			// Complete the size timeout notification
			if (SizeTimeoutNotification.IsValid())
			{
				SizeTimeoutNotification.Pin()->SetText(FText::FromString("Successfully calculated size"));
				SizeTimeoutNotification.Pin()->ExpireAndFadeout();
				SizeTimeoutNotification.Pin()->SetCompletionState(SNotificationItem::CS_Success);
			}
		}
	}

	return IsRunning();
}

void FBAGraphTask_CacheNodeSizes::InitTask()
{
	UE_LOG(LogBAGraphTask, Log, TEXT("[%hs] Saving view location"), __FUNCTION__)

	GraphHandler.DelayedCacheSizeFinished.Cancel();

	// Save the currently viewport to restore once we are done
	if (PendingSize.Num() > 0)
	{
		InitialPendingSize = PendingSize.Num();

		// sort top left most node
		PendingSize.Sort([](const TWeakObjectPtr<UEdGraphNode> NodeA, const TWeakObjectPtr<UEdGraphNode> NodeB)
		{
			if (NodeA->NodePosX == NodeB->NodePosX)
			{
				return NodeA->NodePosY <= NodeB->NodePosY;
			}

			return NodeA->NodePosX <= NodeB->NodePosX;
		});
	}

	if (TSharedPtr<SBlueprintAssistGraphOverlay> GraphOverlay = GraphHandler.GetGraphOverlay())
	{
		GraphOverlay->SizeProgressWidget->ShowOverlay();
	}

	GraphHandler.GetViewLocation(ViewCache, ZoomCache);
}

void FBAGraphTask_CacheNodeSizes::ResetTask(bool bActive, bool bCancelled)
{
	PendingSize.Empty();

	InitialPendingSize = 0;

	GraphHandler.DelayedCacheSizeFinished.StartDelay(2);

	CancelSizeTimeoutNotification(false);

	if (bActive)
	{
		UE_LOG(LogBAGraphTask, Log, TEXT("[%hs] Resetting view location"), __FUNCTION__);
		GraphHandler.SetViewLocation(ViewCache, ZoomCache);
	}
}

float FBAGraphTask_CacheNodeSizes::GetProgress() const
{
	if (InitialPendingSize > 0)
	{
		return 1.0f - (static_cast<float>(PendingSize.Num()) / static_cast<float>(InitialPendingSize));
	}

	return 0.0f;
}

void FBAGraphTask_CacheNodeSizes::ShowSizeTimeoutNotification()
{
	UE_LOG(LogBlueprintAssist, VeryVerbose, TEXT("[%hs]"), __FUNCTION__)
	if (SizeTimeoutNotification.IsValid())
	{
		return;
	}

	if (!FocusedNode.IsValid())
	{
		return;
	}

	NodeSizeTimeout = 10.0f;

	FNotificationInfo Info(FText::GetEmpty());

	Info.ExpireDuration = 0.5f;
	Info.FadeInDuration = 0.1f;
	Info.FadeOutDuration = 0.5f;
	Info.bUseSuccessFailIcons = true;
	Info.bUseThrobber = true;
	Info.bFireAndForget = false;
#if ENGINE_MAJOR_VERSION >= 5
	Info.ForWindow = GraphHandler.GetWindow();
#endif
	Info.ButtonDetails.Add(FNotificationButtonInfo(
		FText::FromString(TEXT("Use inaccurate node size")),
		FText(),
		FSimpleDelegate::CreateRaw(this, &FBAGraphTask_CacheNodeSizes::CancelSizeTimeoutNotification, true),
		SNotificationItem::CS_Pending
	));

	SizeTimeoutNotification = FSlateNotificationManager::Get().AddNotification(Info);
	SizeTimeoutNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);

	SizeTimeoutNotification.Pin()->SetText(
		TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateRaw(this, &FBAGraphTask_CacheNodeSizes::GetSizeTimeoutMessage))
	);
}

void FBAGraphTask_CacheNodeSizes::CancelSizeTimeoutNotification(bool bSaveFocusedNodeSize)
{
	UE_LOG(LogBlueprintAssist, VeryVerbose, TEXT("[%hs]"), __FUNCTION__)
	if (SizeTimeoutNotification.IsValid())
	{
		const FString NotificationMsg = FString::Printf(
			TEXT("Using inaccurate node size for \"%s\""),
			*FBAUtils::GetNodeName(FocusedNode.Get()));

		SizeTimeoutNotification.Pin()->SetExpireDuration(0.5f);
		SizeTimeoutNotification.Pin()->SetFadeOutDuration(0.5f);
		SizeTimeoutNotification.Pin()->SetText(FText::FromString(NotificationMsg));
		SizeTimeoutNotification.Pin()->SetCompletionState(SNotificationItem::CS_Fail);
		SizeTimeoutNotification.Pin()->ExpireAndFadeout();
		SizeTimeoutNotification.Reset();
	}

	if (FocusedNode.IsValid())
	{
		if (bSaveFocusedNodeSize)
		{
			// try cache node size but it will be inaccurate
			PendingSize.RemoveSwap(FocusedNode);
			CacheNodeSize(FocusedNode.Get());
		}

		FocusedNode = nullptr;
	}
}

FText FBAGraphTask_CacheNodeSizes::GetSizeTimeoutMessage() const
{
	return FText::FromString(FString::Printf(
		TEXT("\"%s\" is not fully visible on screen. Please resize the window to fit the node. Timeout in %.0f..."),
		*FBAUtils::GetNodeName(FocusedNode.Get()),
		NodeSizeTimeout
	));
}

void FBAGraphTask_CacheNodeSizes::OnDelayedCacheSizeFinished()
{
	UE_LOG(LogBlueprintAssist, VeryVerbose, TEXT("[%hs]"), __FUNCTION__)
	if (auto Overlay = GraphHandler.GetGraphOverlay())
	{
		Overlay->SizeProgressWidget->HideOverlay();
	}
}

bool FBAGraphTask_CacheNodeSizes::CacheNodeSize(UEdGraphNode* Node)
{
	TSharedPtr<SGraphNode> GraphNode = GraphHandler.GetGraphNode(Node);
	if (!GraphNode)
	{
		return false;
	}

	FVector2D Size = GraphNode->GetDesiredSize();

	// for comment nodes we only want to cache the title bar height
	if (FBAUtils::IsCommentNode(Node))
	{
		Size.Y = FBAUtils::GetGraphNodeMarqueeSize(GraphNode).Y;
	}

	// cache pin offset
	TArray<TSharedRef<SWidget>> PinsAsWidgets;
	GraphNode->GetPins(PinsAsWidgets);
	bool bAllPinsCached = true;

	FBANodeData& NodeData = GraphHandler.GetNodeData(Node);
	NodeData.ResetSize();

	for (const TSharedRef<SWidget>& Widget : PinsAsWidgets)
	{
		TSharedPtr<SGraphPin> GraphPin = StaticCastSharedRef<SGraphPin>(Widget);
		if (GraphPin.IsValid())
		{
			if (UEdGraphPin* Pin = GraphPin->GetPinObj())
			{
				NodeData.CachedPins.Add(Pin->PinId, GraphPin->GetNodeOffset().Y);
			}
		}
		else
		{
			UE_LOG(LogBlueprintAssist, Error,
					TEXT("BlueprintAssistGraphHandler::UpdateCachedNodeSize: GraphPin is invalid for node %s"),
					*FBAUtils::GetNodeName(Node));

			bAllPinsCached = false;
			break;
		}
	}

	if (bAllPinsCached)
	{
		if (!Node->IsAutomaticallyPlacedGhostNode() && Node->bCommentBubbleVisible)
		{
			SNodePanel::SNode::FNodeSlot* CommentSlot = GraphNode->GetSlot(ENodeZone::TopCenter);
			if (CommentSlot != nullptr)
			{
				TSharedPtr<SCommentBubble> CommentBubble = StaticCastSharedRef<SCommentBubble>(CommentSlot->GetWidget());
				if (CommentBubble.IsValid() && CommentBubble->IsBubbleVisible())
				{
					NodeData.SetCommentBubbleSize(CommentBubble->GetDesiredSize());
				}
			}
		}

		NodeData.SetSize(Size);
		UE_LOG(LogBAGraphTask, Log, TEXT("[%hs] Saved node size for %s"), __FUNCTION__, *FBAUtils::GetNodeName(Node));

		// resizable nodes need to use NodeWidth & NodeHeight, don't override this
		if (!Node->bCanResizeNode && UBASettings_Advanced::Get().bWriteSizeToEdGraphNode)
		{
			Node->NodeWidth = Size.X;
			Node->NodeHeight = Size.Y;
		}

		return true;
	}

	return false;
}