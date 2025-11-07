// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistGlobals.h"
#include "Framework/Commands/Commands.h"

// UE 5.0+ EditorStyleSet.h 已废弃，使用 BlueprintAssistGlobals.h 中的宏适配
#if BA_UE_VERSION_OR_LATER(5, 0)
#include "Styling/AppStyle.h"
#else
#include "EditorStyleSet.h"
#endif

/**
 * 
 */
class FBACommandsImpl : public TCommands<FBACommandsImpl>
{
public:
	FBACommandsImpl()
		: TCommands<FBACommandsImpl>(
			TEXT("BlueprintAssistCommands"),
			NSLOCTEXT("Contexts", "BlueprintAssistCommands", "Blueprint Assist Commands"),
			NAME_None,
			BA_GET_STYLE_SET_NAME()) { }

	virtual ~FBACommandsImpl() override { }

	virtual void RegisterCommands() override;

	/** Opens the blueprint context menu */
	TSharedPtr<FUICommandInfo> OpenContextMenu;

	/** Replace the selected node */
	TSharedPtr<FUICommandInfo> ReplaceNodeWith;

	/** Rename the selected node */
	TSharedPtr<FUICommandInfo> RenameSelectedNode;

	TSharedPtr<FUICommandInfo> EditNodeComment;

	/** Formats the position of a chain of nodes */
	TSharedPtr<FUICommandInfo> FormatNodes;

	/** Formats the position for only the selected nodes */
	TSharedPtr<FUICommandInfo> FormatNodes_Selectively;

	/** Formats the position of a chain of nodes */
	TSharedPtr<FUICommandInfo> FormatNodes_Helixing;

	/** Formats the position of a chain of nodes */
	TSharedPtr<FUICommandInfo> FormatNodes_LHS;

	/** Formats the position of all custom events nodes on the graph */
	TSharedPtr<FUICommandInfo> FormatAllEvents;

	/** Deletes a node B in chain A-B-C and links A-B */
	TSharedPtr<FUICommandInfo> DeleteAndLink;
	
	/** Cuts a node B in chain A-B-C and links A-B */
	TSharedPtr<FUICommandInfo> CutAndLink;

	/** Connects free pins of a node to nearby pins */
	TSharedPtr<FUICommandInfo> ConnectUnlinkedPins;

	TSharedPtr<FUICommandInfo> LinkNodesBetweenWires;

	/**
	 * Toggle current context, current handles:
	 * - BP Action Menu - Toggle Context Sensitive
	 * - Widget BP - Toggle selected widget IsVariable
	 * - BP Selected Node - Toggle between a pure get node and a validated get node
	 */
	TSharedPtr<FUICommandInfo> ToggleContext;

	/** Selects the closest node in a direction */
	TSharedPtr<FUICommandInfo> SelectNodeUp;
	TSharedPtr<FUICommandInfo> SelectNodeDown;
	TSharedPtr<FUICommandInfo> SelectNodeLeft;
	TSharedPtr<FUICommandInfo> SelectNodeRight;

	TSharedPtr<FUICommandInfo> ExpandNodeSelection;
	TSharedPtr<FUICommandInfo> ExpandSelectionLeft;
	TSharedPtr<FUICommandInfo> ExpandSelectionRight;

	/** Selects the pin in a direction */
	TSharedPtr<FUICommandInfo> SelectPinUp;
	TSharedPtr<FUICommandInfo> SelectPinDown;
	TSharedPtr<FUICommandInfo> SelectPinLeft;
	TSharedPtr<FUICommandInfo> SelectPinRight;

	/** Shifts the camera in a direction */
	TSharedPtr<FUICommandInfo> ShiftCameraUp;
	TSharedPtr<FUICommandInfo> ShiftCameraLeft;
	TSharedPtr<FUICommandInfo> ShiftCameraRight;
	TSharedPtr<FUICommandInfo> ShiftCameraDown;

	/** Selects all nodes down the chain in a direction */
	TSharedPtr<FUICommandInfo> SelectChainLeft;
	TSharedPtr<FUICommandInfo> SelectChainRight;

	/** Swap a node in a direction */
	TSharedPtr<FUICommandInfo> SwapNodeLeft;
	TSharedPtr<FUICommandInfo> SwapNodeRight;

	/** Swaps the selected pin connection with the next valid pin */
	TSharedPtr<FUICommandInfo> SwapConnectionUp;
	TSharedPtr<FUICommandInfo> SwapConnectionDown;

	/** Link the selected pin to the hovered pin */
	TSharedPtr<FUICommandInfo> LinkToHoveredPin;

	/** Split the selected or hovered pin */
	TSharedPtr<FUICommandInfo> SplitPin;

	/** Recombine the selected or hovered pin */
	TSharedPtr<FUICommandInfo> RecombinePin;

	/** Go to command */
	TSharedPtr<FUICommandInfo> GoToInGraph;

	/** Open window menu */
	TSharedPtr<FUICommandInfo> OpenWindow;

	/** Duplicate node for each link */
	TSharedPtr<FUICommandInfo> DuplicateNodeForEachLink;

	/** Merge selected nodes */
	TSharedPtr<FUICommandInfo> MergeSelectedNodes;

	/** Refresh node sizes */
	TSharedPtr<FUICommandInfo> RefreshNodeSizes;

	TSharedPtr<FUICommandInfo> EditSelectedPinValue;
	TSharedPtr<FUICommandInfo> DisconnectPinLink;
	TSharedPtr<FUICommandInfo> DisconnectNodeExecution;
	TSharedPtr<FUICommandInfo> DisconnectAllNodeLinks;

	TSharedPtr<FUICommandInfo> ZoomToNodeTree;

	TSharedPtr<FUICommandInfo> GetContextMenuForPin;
	TSharedPtr<FUICommandInfo> GetContextMenuForNode;

	TSharedPtr<FUICommandInfo> FocusSearchBoxMenu;
	TSharedPtr<FUICommandInfo> VariableSelectorMenu;
	TSharedPtr<FUICommandInfo> AddSymbolMenu;
	TSharedPtr<FUICommandInfo> EditDetailsMenu;
	TSharedPtr<FUICommandInfo> LinkPinMenu;
	TSharedPtr<FUICommandInfo> TabSwitcherMenu;

	TSharedPtr<FUICommandInfo> ToggleNode;

	TSharedPtr<FUICommandInfo> CreateRerouteNode;

	TSharedPtr<FUICommandInfo> OpenBlueprintAssistHotkeySheet;

	TSharedPtr<FUICommandInfo> ToggleFullscreen;

	TSharedPtr<FUICommandInfo> SwitchWorkflowMode;

	TSharedPtr<FUICommandInfo> OpenAssetCreationMenu;

	TSharedPtr<FUICommandInfo> FocusGraphPanel;

	TSharedPtr<FUICommandInfo> OpenBlueprintAssistDebugMenu;

	TSharedPtr<FUICommandInfo> FocusSearchBox;

	TSharedPtr<FUICommandInfo> GoToParentClassDefinition;

	TSharedPtr<FUICommandInfo> ToggleLockNode;
	TSharedPtr<FUICommandInfo> GroupNodes;
	TSharedPtr<FUICommandInfo> UngroupNodes;

	TSharedPtr<FUICommandInfo> ToggleNodeAdvancedDisplay;

	TSharedPtr<FUICommandInfo> GoForwardInTabHistory;
	TSharedPtr<FUICommandInfo> GoBackInTabHistory;
};

class XTOOLS_BLUEPRINTASSIST_API FBACommands
{
public:
	static void Register();

	static const FBACommandsImpl& Get();

	static void Unregister();
};
