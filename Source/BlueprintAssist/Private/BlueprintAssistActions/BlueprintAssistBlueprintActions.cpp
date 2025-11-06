#include "BlueprintAssistActions/BlueprintAssistBlueprintActions.h"

#include "BlueprintAssistCommands.h"
#include "BlueprintAssistUtils.h"
#include "BlueprintEditor.h"
#include "BlueprintAssistMisc/BAMiscUtils.h"
#include "BlueprintAssistWidgets/AddSymbolMenu.h"
#include "BlueprintAssistWidgets/GoToSymbolMenu.h"
#include "BlueprintAssistWidgets/VariableSelectorMenu.h"
#include "Framework/Commands/UICommandList.h"

bool FBABlueprintActionsBase::HasOpenBlueprintEditor() const
{
	FBlueprintEditor* BPEditor = FBAMiscUtils::GetEditorFromActiveTabCasted<UBlueprint, FBlueprintEditor>();
	return BPEditor != nullptr && BPEditor->IsBlueprintEditor() && BPEditor->InEditingMode();
}

void FBABlueprintActions::Init()
{
	BlueprintCommands = MakeShareable(new FUICommandList());

	BlueprintCommands->MapAction(
		FBACommands::Get().VariableSelectorMenu,
		FExecuteAction::CreateRaw(this, &FBABlueprintActions::OpenVariableSelectorMenu),
		FCanExecuteAction::CreateRaw(this, &FBABlueprintActions::HasOpenBlueprintEditor)
	);

	BlueprintCommands->MapAction(
		FBACommands::Get().AddSymbolMenu,
		FExecuteAction::CreateRaw(this, &FBABlueprintActions::OpenCreateSymbolMenu),
		FCanExecuteAction::CreateRaw(this, &FBABlueprintActions::HasOpenBlueprintEditor)
	);

	BlueprintCommands->MapAction(
		FBACommands::Get().GoToInGraph,
		FExecuteAction::CreateRaw(this, &FBABlueprintActions::OpenGoToSymbolMenu),
		FCanExecuteAction::CreateRaw(this, &FBABlueprintActions::HasOpenBlueprintEditor)
	);

	BlueprintCommands->MapAction(
		FBACommands::Get().GoForwardInTabHistory,
		FExecuteAction::CreateRaw(this, &FBABlueprintActions::GoForwardInTabHistory),
		FCanExecuteAction::CreateRaw(this, &FBABlueprintActions::HasOpenBlueprintEditor)
	);

	BlueprintCommands->MapAction(
		FBACommands::Get().GoBackInTabHistory,
		FExecuteAction::CreateRaw(this, &FBABlueprintActions::GoBackInTabHistory),
		FCanExecuteAction::CreateRaw(this, &FBABlueprintActions::HasOpenBlueprintEditor)
	);
}

void FBABlueprintActions::OpenVariableSelectorMenu()
{
	TSharedRef<SVariableSelectorMenu> Widget = SNew(SVariableSelectorMenu);
	FBAUtils::OpenPopupMenu(Widget, Widget->GetWidgetSize());
}

void FBABlueprintActions::OpenCreateSymbolMenu()
{
	TSharedRef<SAddSymbolMenu> Widget = SNew(SAddSymbolMenu);
	FBAUtils::OpenPopupMenu(Widget, Widget->GetWidgetSize());
}

void FBABlueprintActions::OpenGoToSymbolMenu()
{
	TSharedRef<SGoToSymbolMenu> Widget = SNew(SGoToSymbolMenu);
	FBAUtils::OpenPopupMenu(Widget, Widget->GetWidgetSize());
}

void FBABlueprintActions::GoBackInTabHistory()
{
	if (FBlueprintEditor* BPEditor = FBAMiscUtils::GetEditorFromActiveTabCasted<UBlueprint, FBlueprintEditor>())
	{
		BPEditor->OpenDocument(nullptr, FDocumentTracker::NavigateBackwards);
	}
}

void FBABlueprintActions::GoForwardInTabHistory()
{
	if (FBlueprintEditor* BPEditor = FBAMiscUtils::GetEditorFromActiveTabCasted<UBlueprint, FBlueprintEditor>())
	{
		BPEditor->OpenDocument(nullptr, FDocumentTracker::NavigateForwards);
	}
}
