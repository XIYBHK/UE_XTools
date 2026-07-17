// Copyright fpwong. All Rights Reserved.


#include "BAGraphHandler/BAGraphTask_WaitForRename.h"

#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistUtils.h"
#include "Framework/Application/SlateApplication.h"

bool FBAGraphTask_WaitForRename::IsRunning()
{
	return !Nodes.IsEmpty();
}

bool FBAGraphTask_WaitForRename::TickTask(float DeltaTime)
{
	for (auto It = Nodes.CreateIterator(); It; ++It)
	{
		TWeakObjectPtr<UEdGraphNode> Node = *It;
		if (Node.IsValid() && !IsNodeBeingRenamed(Node.Get()))
		{
			UE_LOG(LogBAGraphTask, Log, TEXT("[%hs] Removed %s"), __FUNCTION__, *FBAUtils::GetNodeName(Node.Get()))
			It.RemoveCurrentSwap();
		}
	}

	return IsRunning();
}

void FBAGraphTask_WaitForRename::ResetTask(bool bActive, bool bCancelled)
{
	// ensure that the keyboard focus is cleared after renaming has finished
	// to fix the edge case where the user promotes to var and commits a default name via Enter
	if (bActive && !bCancelled)
	{
		FSlateApplication::Get().ClearKeyboardFocus();
	}

	Nodes.Empty();
}

void FBAGraphTask_WaitForRename::AddNodes(const TArray<UEdGraphNode*>& NodesToAdd)
{
	for (UEdGraphNode* Node : NodesToAdd)
	{
		if (IsNodeBeingRenamed(Node))
		{
			Nodes.Add(Node);
		}
	}
}

bool FBAGraphTask_WaitForRename::IsNodeBeingRenamed(UEdGraphNode* Node)
{
	if (TSharedPtr<SGraphNode> GraphNode = FBAUtils::GetGraphNode(GraphHandler.GetGraphPanel(), Node))
	{
		if (FBAUtils::IsNodeBeingRenamed(GraphNode))
		{
			return true;
		}
	}

	return false;
}
