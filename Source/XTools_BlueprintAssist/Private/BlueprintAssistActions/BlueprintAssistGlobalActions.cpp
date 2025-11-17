#include "BlueprintAssistActions/BlueprintAssistGlobalActions.h"

#include "BlueprintAssistGlobals.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistUtils.h"
#include "IAssetFamily.h"
#include "IAssetSearchModule.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_VariableGet.h"
#include "SGraphPanel.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditor.h"
#include "BlueprintAssistActions/BlueprintAssistNodeActions.h"
#include "BlueprintAssistMisc/BAMiscUtils.h"
#include "BlueprintAssistWidgets/BAEditDetailsMenu.h"
#include "BlueprintAssistWidgets/BAFocusSearchBoxMenu.h"
#include "BlueprintAssistWidgets/BAOpenWindowMenu.h"
#include "BlueprintAssistWidgets/BASearchMenu.h"
#include "BlueprintAssistWidgets/BlueprintAssistCreateAssetMenu.h"
#include "BlueprintAssistWidgets/BlueprintAssistHotkeyMenu.h"
#include "BlueprintAssistWidgets/BlueprintAssistTabSwitcher.h"
#include "BlueprintAssistWidgets/BlueprintAssistWorkflowModeMenu.h"
#include "Components/Widget.h"
#include "EdGraph/EdGraph.h"
#include "Editor/ContentBrowser/Private/SContentBrowser.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"

#include "BlueprintAssistMisc/BAPrivate.h"

#if BA_UE_VERSION_OR_LATER(5, 6)
BA_DEFINE_PRIVATE_MEMBER_PTR(void(EGetNodeVariation), GTogglePurity, UK2Node_VariableGet, TogglePurity);
#endif

bool FBAGlobalActionsBase::CanOpenEditDetailsMenu() const
{
	return CanExecuteActions() && SBAEditDetailsMenu::CanOpenMenu();
}

bool FBAGlobalActionsBase::HasWorkflowModes() const
{
	const TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (!ActiveWindow)
	{
		return false;
	}

	TArray<TSharedPtr<SWidget>> ModeWidgets;
	FBAUtils::GetChildWidgets(ActiveWindow, "SModeWidget", ModeWidgets);

	TArray<TSharedPtr<SWidget>> AssetShortcutWidgets;
	FBAUtils::GetChildWidgets(ActiveWindow, "SAssetShortcut", AssetShortcutWidgets);

	return ModeWidgets.Num() > 0 || AssetShortcutWidgets.Num() > 0;
}

void FBAGlobalActions::Init()
{
	GlobalCommands = MakeShareable(new FUICommandList());

	GlobalCommands->MapAction(
		FBACommands::Get().OpenBlueprintAssistDebugMenu,
		FExecuteAction::CreateRaw(this, &FBAGlobalActions::OpenBlueprintAssistDebugMenu)
	);

	GlobalCommands->MapAction(
		FBACommands::Get().FocusSearchBoxMenu,
		FExecuteAction::CreateRaw(this, &FBAGlobalActions::OpenFocusSearchBoxMenu),
		FCanExecuteAction::CreateRaw(this, &FBAGlobalActions::CanExecuteActions)
	);

	GlobalCommands->MapAction(
		FBACommands::Get().EditDetailsMenu,
		FExecuteAction::CreateRaw(this, &FBAGlobalActions::OpenEditDetailsMenu),
		FCanExecuteAction::CreateRaw(this, &FBAGlobalActions::CanOpenEditDetailsMenu)
	);

	GlobalCommands->MapAction(
		FBACommands::Get().OpenWindow,
		FExecuteAction::CreateRaw(this, &FBAGlobalActions::OpenWindowMenu),
		FCanExecuteAction::CreateRaw(this, &FBAGlobalActions::CanExecuteActions)
	);

	GlobalCommands->MapAction(
		FBACommands::Get().OpenBlueprintAssistHotkeySheet,
		FExecuteAction::CreateStatic(&FBAGlobalActions::OpenBlueprintAssistHotkeyMenu),
		FCanExecuteAction::CreateRaw(this, &FBAGlobalActions::CanExecuteActions)
	);

	GlobalCommands->MapAction(
		FBACommands::Get().TabSwitcherMenu,
		FExecuteAction::CreateRaw(this, &FBAGlobalActions::OpenTabSwitcherMenu),
		FCanExecuteAction::CreateRaw(this, &FBAGlobalActions::CanExecuteActions)
	);

	GlobalCommands->MapAction(
		FBACommands::Get().ToggleFullscreen,
		FExecuteAction::CreateRaw(this, &FBAGlobalActions::ToggleFullscreen),
		FCanExecuteAction::CreateRaw(this, &FBAGlobalActions::CanExecuteActions)
	);

#if BA_UE_VERSION_OR_LATER(5, 4)
	GlobalCommands->MapAction(
		FBACommands::Get().FindInFilesMenu,
		FExecuteAction::CreateRaw(this, &FBAGlobalActions::OpenFindInFilesMenu),
		FCanExecuteAction::CreateRaw(this, &FBAGlobalActions::CanExecuteActions)
	);

	GlobalCommands->MapAction(
		FBACommands::Get().OpenFileMenu,
		FExecuteAction::CreateRaw(this, &FBAGlobalActions::OpenFileMenu),
		FCanExecuteAction::CreateRaw(this, &FBAGlobalActions::CanExecuteActions)
	);
#endif

	GlobalCommands->MapAction(
		FBACommands::Get().SwitchWorkflowMode,
		FExecuteAction::CreateRaw(this, &FBAGlobalActions::SwitchWorkflowMode),
		FCanExecuteAction::CreateRaw(this, &FBAGlobalActions::HasWorkflowModes)
	);

	GlobalCommands->MapAction(
		FBACommands::Get().OpenAssetCreationMenu,
		FExecuteAction::CreateRaw(this, &FBAGlobalActions::OpenAssetCreationMenu),
		FCanExecuteAction::CreateRaw(this, &FBAGlobalActions::CanExecuteActions)
	);

	GlobalCommands->MapAction(
		FBACommands::Get().FocusSearchBox,
		FExecuteAction::CreateRaw(this, &FBAGlobalActions::FocusSearchBox),
		FCanExecuteAction::CreateRaw(this, &FBAGlobalActions::CanFocusSearchBox)
	);

	GlobalCommands->MapAction(
		FBACommands::Get().ToggleContext,
		FExecuteAction::CreateRaw(this, &FBAGlobalActions::ToggleContext),
		FCanExecuteAction::CreateRaw(this, &FBAGlobalActions::CanToggleContext)
	);
}

void FBAGlobalActions::OpenBlueprintAssistDebugMenu() const
{
	FGlobalTabmanager::Get()->TryInvokeTab(FName("BADebugMenu"));
}

void FBAGlobalActions::OpenFocusSearchBoxMenu()
{
	TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (!Window.IsValid())
	{
		return;
	}

	TSharedRef<SBAFocusSearchBoxMenu> Widget = SNew(SBAFocusSearchBoxMenu);

	FBAUtils::OpenPopupMenu(Widget, Widget->GetWidgetSize());
}

void FBAGlobalActions::OpenEditDetailsMenu()
{
	TSharedRef<SBAEditDetailsMenu> Widget = SNew(SBAEditDetailsMenu);
	FBAUtils::OpenPopupMenu(Widget, Widget->GetWidgetSize());
}

void FBAGlobalActions::OpenWindowMenu()
{
	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();

	TSharedRef<SBAOpenWindowMenu> Widget = SNew(SBAOpenWindowMenu);

	FBAUtils::OpenPopupMenu(Widget, Widget->GetWidgetSize());
}

void FBAGlobalActions::OpenBlueprintAssistHotkeyMenu()
{
	TSharedRef<SBAHotkeyMenu> Widget = SNew(SBAHotkeyMenu);
	FBAUtils::OpenPopupMenu(Widget, Widget->GetWidgetSize());
}

void FBAGlobalActions::OpenTabSwitcherMenu()
{
	TSharedRef<SBATabSwitcher> Widget = SNew(SBATabSwitcher);
	FBAUtils::OpenPopupMenu(Widget, Widget->GetWidgetSize());
}

void FBAGlobalActions::ToggleFullscreen()
{
	TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (Window.IsValid())
	{
		if (Window->IsWindowMaximized())
		{
			Window->Restore();
		}
		else
		{
			Window->Maximize();
		}
	}
}

void FBAGlobalActions::SwitchWorkflowMode()
{
	if (UObject* CurrentAsset = FBAMiscUtils::GetAssetForActiveTab<UObject>())
	{
		if (FWorkflowCentricApplication* App = static_cast<FWorkflowCentricApplication*>(FBAUtils::GetEditorFromActiveTab()))
		{
			const FString AssetClassName = CurrentAsset->GetClass()->GetName();
			if (AssetClassName == "WidgetBlueprint")
			{
				static const FName GraphMode(TEXT("GraphName"));
				static const FName DesignerMode(TEXT("DesignerName"));

				const FName& NewMode = App->IsModeCurrent(DesignerMode) ? GraphMode : DesignerMode;
				App->SetCurrentMode(NewMode);
				return;
			}
			else if (AssetClassName == "BehaviorTree")
			{
				static const FName BehaviorTreeMode(TEXT("BehaviorTree"));
				static const FName BlackboardMode(TEXT("Blackboard"));

				const FName& NewMode = App->IsModeCurrent(BehaviorTreeMode) ? BlackboardMode : BehaviorTreeMode;
				App->SetCurrentMode(NewMode);
				return;
			}
			else
			{
				if (UObject* ActiveObject = FBAMiscUtils::GetAssetForActiveTab<UObject>())
				{
					if (TSharedPtr<IAssetFamily> AssetFamily = FBAMiscUtils::GetAssetFamilyForObject(ActiveObject))
					{
						TArray<UClass*> AssetTypes;
						AssetFamily->GetAssetTypes(AssetTypes);

						if (AssetTypes.Num())
						{
							TSharedRef<SBAWorkflowModeMenu> Widget = SNew(SBAWorkflowModeMenu);
							FBAUtils::OpenPopupMenu(Widget, Widget->GetWidgetSize());
						}
					}
				}
			}
		}
	}
}

void FBAGlobalActions::OpenAssetCreationMenu()
{
	TSharedRef<SBACreateAssetMenu> Widget = SNew(SBACreateAssetMenu);
	FBAUtils::OpenPopupMenu(Widget, Widget->GetWidgetSize());
}

void FBAGlobalActions::FocusSearchBox()
{
	if (auto SearchBox = FindSearchBox())
	{
		FSlateApplication::Get().SetKeyboardFocus(SearchBox, EFocusCause::SetDirectly);
	}
}

void FBAGlobalActions::OpenFindInFilesMenu()
{
#if BA_UE_VERSION_OR_LATER(5, 4)
	if (IAssetSearchModule::IsAvailable())
	{
		FVector2D MenuSize = FVector2D(1440, 810);
		TSharedRef<SBASearchMenu> Widget = SNew(SBASearchMenu, MenuSize, EBASearchMenuType::Property).AllowAssets(false).AllowObjects(false);
		FBAUtils::OpenPopupMenu(Widget, MenuSize);
		return;
	}
	else
	{
		FNotificationInfo Notification(INVTEXT("FindInFiles requires the 'AssetSearch' plugin to be enabled"));
		Notification.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Notification);
	}
#endif
}

void FBAGlobalActions::OpenFileMenu()
{
#if BA_UE_VERSION_OR_LATER(5, 4)
	if (IAssetSearchModule::IsAvailable())
	{
		FVector2D MenuSize = FVector2D(1440, 810);
		TSharedRef<SBASearchMenu> Widget = SNew(SBASearchMenu, MenuSize, EBASearchMenuType::File).SearchSpecifier("name");
		FBAUtils::OpenPopupMenu(Widget, MenuSize);
		return;
	}
	else
	{
		FNotificationInfo Notification(INVTEXT("OpenFileMenu requires the 'AssetSearch' plugin to be enabled!"));
		Notification.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Notification);
	}
#endif
}

bool FBAGlobalActions::CanFocusSearchBox() const
{
	return FindSearchBox().IsValid();
}

TSharedPtr<SWidget> FBAGlobalActions::FindSearchBox() const
{
	if (TSharedPtr<SWidget> HoveredWidget = FBAUtils::GetLastHoveredWidget())
	{
		if (TSharedPtr<SWidget> AssetSearchBox = FBAUtils::ScanParentContainersForTypes(HoveredWidget, FBAUtils::GetSearchBoxNames(), "SDockingTabStack"))
		{
			return AssetSearchBox;
		}
	}

	if (TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetUserFocusedWidget(0))
	{
		if (TSharedPtr<SWidget> AssetSearchBox = FBAUtils::ScanParentContainersForTypes(FocusedWidget, FBAUtils::GetSearchBoxNames(), "SDockingTabStack"))
		{
			return AssetSearchBox;
		}
	}

	return nullptr;
}

void FBAGlobalActions::ToggleContext() const
{
	if (ShouldToggleActionMenuContextSensitive())
	{
		ToggleActionMenuContextSensitive();
		return;
	}

	if (CanToggleNodePurity())
	{
		ToggleNodePurity();
		return;
	}

	if (ShouldToggleWidgetIsVariable())
	{
		ToggleWidgetIsVariable();
		return;
	}
}

bool FBAGlobalActions::CanToggleContext() const
{
	if (ShouldToggleWidgetIsVariable())
	{
		return true;
	}

	if (ShouldToggleActionMenuContextSensitive())
	{
		return true;
	}

	if (CanToggleNodePurity())
	{
		return true;
	}

	return false;
}

#define LOCTEXT_NAMESPACE "SBlueprintGraphContextMenu"
void FBAGlobalActions::ToggleActionMenuContextSensitive() const
{
	TSharedPtr<SWindow> Menu = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (!Menu.IsValid())
	{
		return;
	}

	TSharedPtr<SWidget> BlueprintActionMenu = FBAUtils::GetChildWidget(Menu, "SBlueprintActionMenu");
	if (!BlueprintActionMenu.IsValid())
	{
		return;
	}

	TArray<TSharedPtr<SWidget>> CheckBoxes;
	FBAUtils::GetChildWidgets(BlueprintActionMenu, "SCheckBox", CheckBoxes);

	static FText ContextSensitiveText = LOCTEXT("BlueprintActionMenuContextToggle", "Context Sensitive");

	// look through all the checkboxes within the blueprint action menu
	for (TSharedPtr<SWidget> CheckBox : CheckBoxes)
	{
		if (TSharedPtr<STextBlock> ChildText = FIND_CHILD_WIDGET(CheckBox, STextBlock))
		{
			if (ChildText->GetText().EqualTo(ContextSensitiveText))
			{
				TSharedPtr<SCheckBox> ToggleSensitiveCheckBox = StaticCastSharedPtr<SCheckBox>(CheckBox);
				ToggleSensitiveCheckBox->ToggleCheckedState();
				return;
			}
		}
	}
}
#undef LOCTEXT_NAMESPACE

bool FBAGlobalActions::ShouldToggleActionMenuContextSensitive() const
{
	TSharedPtr<SWindow> Menu = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (!Menu.IsValid())
	{
		return false;
	}

	TSharedPtr<SWidget> BlueprintActionMenu = FBAUtils::GetChildWidget(Menu, "SBlueprintActionMenu");
	if (!BlueprintActionMenu.IsValid())
	{
		return false;
	}

	return true;
}

void FBAGlobalActions::ToggleWidgetIsVariable() const
{
	FWidgetBlueprintEditor* BPEditor = FBAMiscUtils::GetEditorFromActiveTabCasted<UWidgetBlueprint, FWidgetBlueprintEditor>();
	if (!BPEditor)
	{
		return;
	}

	UWidgetBlueprint* Blueprint = BPEditor->GetWidgetBlueprintObj();
	if (!Blueprint)
	{
		return;
	}

	// logic based off SWidgetDetailsView::HandleIsVariableChanged
	TSet<FWidgetReference> SelectedWidgets = BPEditor->GetSelectedWidgets();
	TArray<UWidget*, TInlineAllocator<16>> WidgetToModify;

	for (auto& WidgetRef : SelectedWidgets)
	{
		if (WidgetRef.IsValid())
		{
			WidgetToModify.Add(WidgetRef.GetTemplate());
			WidgetToModify.Add(WidgetRef.GetPreview());
		}
	}

	if (WidgetToModify.Num() > 0)
	{
		// if any selected widgets are false, set the variable state true
		bool bNewVarState = false;
		for (UWidget* Widget : WidgetToModify)
		{
			if (!Widget->bIsVariable)
			{
				bNewVarState = true;
			}
		}

		const FScopedTransaction Transaction(INVTEXT("Toggle Widget IsVariable"));
		for (UWidget* Widget : WidgetToModify)
		{
			Widget->Modify();
			Widget->bIsVariable = bNewVarState;
		}

		// Refresh references and flush editors
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
}

bool FBAGlobalActions::ShouldToggleWidgetIsVariable() const
{
	FWidgetBlueprintEditor* BPEditor = FBAMiscUtils::GetEditorFromActiveTabCasted<UWidgetBlueprint, FWidgetBlueprintEditor>();
	if (!BPEditor)
	{
		return false;
	}

	// only toggle if we are in designer mode
	static const FName DesignerMode(TEXT("DesignerName"));
	if (!BPEditor->IsModeCurrent(DesignerMode))
	{
		return false;
	}

	UWidgetBlueprint* Blueprint = BPEditor->GetWidgetBlueprintObj();
	if (!Blueprint)
	{
		return false;
	}

	// logic based off SWidgetDetailsView::HandleIsVariableChanged
	TSet<FWidgetReference> SelectedWidgets = BPEditor->GetSelectedWidgets();
	return SelectedWidgets.Num() > 0;
}

void FBAGlobalActions::ToggleNodePurity() const
{
	TSharedPtr<FBAGraphHandler> GraphHandler = GetGraphHandler();
	if (!GraphHandler)
	{
		return;
	}

	{
		if (UK2Node_VariableGet* SelectedGetNode = Cast<UK2Node_VariableGet>(GraphHandler->GetSelectedNode()))
		{
			const FScopedTransaction Transaction(INVTEXT("Toggle Node Purity"));
			SelectedGetNode->Modify();
			const bool bIsPureNode = FBAUtils::IsNodePure(SelectedGetNode);
#if BA_UE_VERSION_OR_LATER(5, 6)
			auto NewPurity = bIsPureNode ? EGetNodeVariation::ValidatedObject : EGetNodeVariation::Pure;
			(SelectedGetNode->*GTogglePurity)(NewPurity);
#else
			SelectedGetNode->SetPurity(!bIsPureNode);
#endif
			return;
		}
	}

	{
		if (UK2Node_DynamicCast* DynamicCast = Cast<UK2Node_DynamicCast>(GraphHandler->GetSelectedNode()))
		{
			const FScopedTransaction Transaction(INVTEXT("Toggle Node Purity"));
			DynamicCast->Modify();
			const bool bIsPureNode = FBAUtils::IsNodePure(DynamicCast);
			DynamicCast->SetPurity(!bIsPureNode);
			return;
		}
	}
}

bool FBAGlobalActions::CanToggleNodePurity() const
{
	if (TSharedPtr<FBAGraphHandler> GH = GetGraphHandler())
	{
		if (UEdGraphNode* N = GH->GetSelectedNode())
		{
			return N->IsA(UK2Node_VariableGet::StaticClass()) || N->IsA(UK2Node_DynamicCast::StaticClass());
		}
	}

	return false;
}
