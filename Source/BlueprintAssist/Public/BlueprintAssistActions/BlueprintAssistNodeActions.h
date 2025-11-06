#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistGraphActions.h"
#include "EdGraph/EdGraphNode.h"

class FUICommandList;

class BLUEPRINTASSIST_API FBANodeActionsBase : public FBAGraphActionsBase
{
public:
	bool HasSingleNodeSelected() const;
	bool HasMultipleNodesSelected() const;
	bool HasMultipleNodesSelectedInclComments() const;
	bool HasHoveredNode() const;
	bool HasHoveredOrSelectedNodes() const;
	bool HasHoveredOrSingleSelectedNode() const;
};

class BLUEPRINTASSIST_API FBANodeActions final : public FBANodeActionsBase
{
public:
	virtual void Init() override;

	static void SmartWireNode(UEdGraphNode* Node);
	static void DisconnectExecutionOfNodes(TArray<UEdGraphNode*> Nodes);
	static UEdGraphNode* GetSingleHoveredOrSelectedNode();

	TSharedPtr<FUICommandList> SingleNodeCommands;
	void OnSmartWireSelectedNode();
	void ZoomToNodeTree();
	void DisconnectAllNodeLinks();
	bool CanSelectPinInDirection();
	void SelectPinInDirection(int X, int Y) const;
	static void OnGetContextMenuActions(const bool bUsePin = true);
	void ReplaceNodeWith();
	void OnReplaceNodeMenuClosed(const TSharedRef<class SWindow>& Window);
	bool CanRenameSelectedNode();
	void RenameSelectedNode();
	void ToggleNodeAdvancedDisplay() const;
	bool CanToggleNodeAdvancedDisplay() const;
	void RenameCommentBubble();

	TSharedPtr<FUICommandList> MultipleNodeCommands;
	void FormatNodes();
	void FormatNodesSelectively();
	void FormatNodesWithHelixing();
	void FormatNodesWithLHS();
	void LinkNodesBetweenWires();
	void DisconnectExecutionOfSelectedNode();
	void SwapNodeInDirection(EEdGraphPinDirection Direction);
	void DeleteAndLink();
	void CutAndLink();
	bool CanToggleNodes();
	void ToggleNodes();
	void ToggleLockNodes();
	void GroupNodes();
	void UngroupNodes();

	void MergeNodes();
	bool CanMergeNodes() const;

	TSharedPtr<FUICommandList> MultipleNodeCommandsIncludingComments;
	void RefreshNodeSizes();

	TSharedPtr<FUICommandList> MiscNodeCommands;
	void ExpandSelection();
	void ExpandNodeTreeInDirection(EEdGraphPinDirection Direction);
};
