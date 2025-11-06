#include "BlueprintAssistActions/BlueprintAssistTabActions.h"

#include "BlueprintAssistGraphHandler.h"
#include "SGraphPanel.h"
#include "EdGraph/EdGraph.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SCheckBox.h"

bool FBATabActionsBase::HasOpenTab() const
{
	if (CanExecuteActions())
	{
		if (TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler())
		{
			TSharedPtr<SDockTab> Tab = GraphHandler->GetTab();
			return Tab.IsValid() && Tab->IsForeground();
		}
	}

	return false;
}

bool FBATabActionsBase::HasOpenActionMenu() const
{
	if (!HasOpenTab())
	{
		return false;
	}

	return FBAUtils::GetGraphActionMenu().IsValid();
}

void FBATabActions::Init()
{
	TabCommands = MakeShareable(new FUICommandList());
	ActionMenuCommands = MakeShareable(new FUICommandList());

	TabCommands->MapAction(
		FBACommands::Get().SelectNodeUp,
		FExecuteAction::CreateRaw(this, &FBATabActions::SelectAnyNodeInDirection, 0, -1),
		FCanExecuteAction::CreateRaw(this, &FBATabActions::HasOpenTab)
	);

	TabCommands->MapAction(
		FBACommands::Get().SelectNodeDown,
		FExecuteAction::CreateRaw(this, &FBATabActions::SelectAnyNodeInDirection, 0, 1),
		FCanExecuteAction::CreateRaw(this, &FBATabActions::HasOpenTab)
	);

	TabCommands->MapAction(
		FBACommands::Get().SelectNodeLeft,
		FExecuteAction::CreateRaw(this, &FBATabActions::SelectAnyNodeInDirection, -1, 0),
		FCanExecuteAction::CreateRaw(this, &FBATabActions::HasOpenTab)
	);

	TabCommands->MapAction(
		FBACommands::Get().SelectNodeRight,
		FExecuteAction::CreateRaw(this, &FBATabActions::SelectAnyNodeInDirection, 1, 0),
		FCanExecuteAction::CreateRaw(this, &FBATabActions::HasOpenTab)
	);

	TabCommands->MapAction(
		FBACommands::Get().ShiftCameraUp,
		FExecuteAction::CreateRaw(this, &FBATabActions::ShiftCameraInDirection, 0, -1),
		FCanExecuteAction::CreateRaw(this, &FBATabActions::HasOpenTab)
	);

	TabCommands->MapAction(
		FBACommands::Get().ShiftCameraDown,
		FExecuteAction::CreateRaw(this, &FBATabActions::ShiftCameraInDirection, 0, 1),
		FCanExecuteAction::CreateRaw(this, &FBATabActions::HasOpenTab)
	);

	TabCommands->MapAction(
		FBACommands::Get().ShiftCameraLeft,
		FExecuteAction::CreateRaw(this, &FBATabActions::ShiftCameraInDirection, -1, 0),
		FCanExecuteAction::CreateRaw(this, &FBATabActions::HasOpenTab)
	);

	TabCommands->MapAction(
		FBACommands::Get().ShiftCameraRight,
		FExecuteAction::CreateRaw(this, &FBATabActions::ShiftCameraInDirection, 1, 0),
		FCanExecuteAction::CreateRaw(this, &FBATabActions::HasOpenTab)
	);
}

void FBATabActions::SelectNodeInDirection(const TArray<UEdGraphNode*>& Nodes, int X, int Y, float DistLimit) const
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	if (Nodes.Num() == 0)
	{
		return;
	}

	TSharedPtr<SGraphPanel> Panel = GraphHandler->GetGraphPanel();
	if (!Panel.IsValid())
	{
		return;
	}

	UEdGraph* Graph = GraphHandler->GetFocusedEdGraph();
	if (!Graph)
	{
		return;
	}

	// if selected node is null, then use the cursor location
	UEdGraphNode* SelectedNode = GraphHandler->GetSelectedNode();
	const FVector2D StartPosition
		= SelectedNode != nullptr
		? FVector2D(SelectedNode->NodePosX, SelectedNode->NodePosY)
		: Panel->GetPastePosition();

	// filter all nodes on the graph towards our direction
	TArray<UEdGraphNode*> FilteredNodes;
	bool bIsXDirection = X != 0;
	for (UEdGraphNode* Other : Nodes)
	{
		// skip the currently selected
		if (Other == SelectedNode)
		{
			continue;
		}

		// skip comment nodes and knot nodes
		if (!FBAUtils::IsGraphNode(Other) || FBAUtils::IsCommentNode(Other) || FBAUtils::IsKnotNode(Other))
		{
			continue;
		}

		const float DeltaX = Other->NodePosX - StartPosition.X;
		const float DeltaY = Other->NodePosY - StartPosition.Y;

		if (bIsXDirection)
		{
			if (FMath::Sign(DeltaX) == X)
			{
				if (DistLimit <= 0 || (FMath::Abs(DeltaX) < DistLimit && FMath::Abs(DeltaY) < DistLimit * 0.5f))
				{
					FilteredNodes.Add(Other);
				}
			}
		}
		else // y direction
		{
			if (FMath::Sign(DeltaY) == Y)
			{
				if (DistLimit <= 0 || (FMath::Abs(DeltaY) < DistLimit && FMath::Abs(DeltaX) < DistLimit * 0.5f))
				{
					FilteredNodes.Add(Other);
				}
			}
		}
	}

	// no nodes found stop
	if (FilteredNodes.Num() == 0)
	{
		return;
	}

	// sort nodes by distance
	const auto& Sorter = [StartPosition, bIsXDirection](UEdGraphNode& A, UEdGraphNode& B)-> bool
	{
		const float XWeight = bIsXDirection ? 1 : 5;
		const float YWeight = bIsXDirection ? 5 : 1;

		float DeltaX = A.NodePosX - StartPosition.X;
		float DeltaY = A.NodePosY - StartPosition.Y;
		const float DistA = XWeight * DeltaX * DeltaX + YWeight * DeltaY * DeltaY;

		DeltaX = B.NodePosX - StartPosition.X;
		DeltaY = B.NodePosY - StartPosition.Y;
		const float DistB = XWeight * DeltaX * DeltaX + YWeight * DeltaY * DeltaY;

		return DistA < DistB;
	};
	FilteredNodes.Sort(Sorter);

	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();

	// now that we have sorted the nodes we get the closest node and select it
	UEdGraphNode* NodeToSelect = FilteredNodes[0];
	GraphHandler->SelectNode(NodeToSelect);
}

void FBATabActions::SelectAnyNodeInDirection(const int X, const int Y) const
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler.IsValid())
	{
		return;
	}

	UEdGraph* Graph = GraphHandler->GetFocusedEdGraph();
	if (Graph == nullptr)
	{
		return;
	}

	SelectNodeInDirection(Graph->Nodes, X, Y, 5000);
}

void FBATabActions::ShiftCameraInDirection(int X, int Y) const
{
	if (TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler())
	{
		/** get the current view location */
		float Zoom;
		FVector2D CurrentViewLocation;
		GraphHandler->GetGraphEditor()->GetViewLocation(CurrentViewLocation, Zoom);

		/** Shift the current view location */
		const FVector2D Offset = FVector2D(X, Y) * UBASettings::Get().ShiftCameraDistance / Zoom;

		const FVector2D CurrentLocation = GraphHandler->IsLerpingViewport() ? GraphHandler->GetTargetLerpLocation() : CurrentViewLocation;

		GraphHandler->BeginLerpViewport(CurrentLocation + Offset, false);
	}
}