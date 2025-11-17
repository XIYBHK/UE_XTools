// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistToolbar.h"

#include "BlueprintAssistCache.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistInputProcessor.h"
#include "BlueprintAssistSettings.h"
#include "BlueprintAssistSettings_EditorFeatures.h"
#include "BlueprintAssistStyle.h"
#include "BlueprintAssistTabHandler.h"
#include "BlueprintAssistUtils.h"
#include "BlueprintEditorModule.h"
#include "ISettingsModule.h"
#include "LevelEditorActions.h"
#include "BlueprintAssistActions/BlueprintAssistGlobalActions.h"
#include "BlueprintAssistMisc/BlueprintAssistToolbar_BlueprintImpl.h"
#include "BlueprintAssistWidgets/BASettingsChangeWindow.h"
#include "BlueprintAssistWidgets/BAWelcomeScreen.h"
#include "EdGraph/EdGraph.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/LazySingleton.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "BlueprintAssist"

FBAToolbarCommandsImpl::FBAToolbarCommandsImpl() : TCommands<FBAToolbarCommandsImpl>(
	TEXT("BlueprintAssistToolbarCommands"),
	NSLOCTEXT("Contexts", "BlueprintAssistToolbarCommands", "Blueprint Assist 工具栏命令"),
	NAME_None,
	BA_GET_STYLE_SET_NAME()) { }

void FBAToolbarCommandsImpl::RegisterCommands()
{
	UI_COMMAND(
		AutoFormatting_Never,
		"从不自动格式化",
		"创建新节点时从不自动格式化",
		EUserInterfaceActionType::RadioButton,
		FInputChord());

	UI_COMMAND(
		AutoFormatting_FormatAll,
		"总是格式化所有连接的节点",
		"创建新节点时总是格式化所有连接的节点",
		EUserInterfaceActionType::RadioButton,
		FInputChord());

	UI_COMMAND(
		AutoFormatting_FormatNewlyCreated,
		"仅格式化新创建的节点",
		"创建新节点时仅格式化新创建的节点",
		EUserInterfaceActionType::RadioButton,
		FInputChord());

	UI_COMMAND(
		FormattingStyle_Compact,
		"紧凑样式",
		"将格式化样式设置为紧凑",
		EUserInterfaceActionType::RadioButton,
		FInputChord());

	UI_COMMAND(
		FormattingStyle_Expanded,
		"展开样式",
		"将格式化样式设置为展开",
		EUserInterfaceActionType::RadioButton,
		FInputChord());

	UI_COMMAND(
		ParameterStyle_LeftHandSide,
		"左侧样式",
		"格式化时参数将被定位在左侧",
		EUserInterfaceActionType::RadioButton,
		FInputChord());

	UI_COMMAND(
		ParameterStyle_Helixing,
		"螺旋样式",
		"格式化时参数节点将被定位在下方",
		EUserInterfaceActionType::RadioButton,
		FInputChord());

	UI_COMMAND(
		FormatAllStyle_Simple,
		"简单样式",
		"将根节点定位为单列",
		EUserInterfaceActionType::RadioButton,
		FInputChord());

	UI_COMMAND(
		FormatAllStyle_Smart,
		"智能样式",
		"根据节点位置将根节点定位为多列",
		EUserInterfaceActionType::RadioButton,
		FInputChord());

	UI_COMMAND(
		FormatAllStyle_NodeType,
		"节点类型样式",
		"根据根节点类型将节点定位为列",
		EUserInterfaceActionType::RadioButton,
		FInputChord());

	UI_COMMAND(
		BlueprintAssistSettings,
		"BlueprintAssist设置",
		"打开BlueprintAssist设置",
		EUserInterfaceActionType::Button,
		FInputChord());

	UI_COMMAND(
		DetectUnusedNodes,
		"检测未使用节点",
		"检测当前图表上的未使用节点并在消息日志中显示它们",
		EUserInterfaceActionType::Button,
		FInputChord());
}

void FBAToolbarCommands::Register()
{
	FBAToolbarCommandsImpl::Register();
}

const FBAToolbarCommandsImpl& FBAToolbarCommands::Get()
{
	return FBAToolbarCommandsImpl::Get();
}

void FBAToolbarCommands::Unregister()
{
	return FBAToolbarCommandsImpl::Unregister();
}

FBAToolbar& FBAToolbar::Get()
{
	return TLazySingleton<FBAToolbar>::Get();
}

void FBAToolbar::TearDown()
{
	TLazySingleton<FBAToolbar>::TearDown();
}

void FBAToolbar::Init()
{
	FBAToolbarCommands::Register();
	BindToolbarCommands();
}

void FBAToolbar::Cleanup()
{
	ToolbarExtenderMap.Empty();
}

void FBAToolbar::OnAssetOpenedInEditor(UObject* Asset, IAssetEditorInstance* AssetEditor)
{
	if (!UBASettings_EditorFeatures::Get().bAddToolbarWidget || !Asset || !AssetEditor)
	{
		return;
	}

	if (!UBASettings::Get().SupportedAssetEditors.Contains(AssetEditor->GetEditorName()))
	{
		return;
	}

	FAssetEditorToolkit* AssetEditorToolkit = static_cast<FAssetEditorToolkit*>(AssetEditor);
	if (AssetEditorToolkit)
	{
		TWeakPtr<FAssetEditorToolkit> WeakToolkit = AssetEditorToolkit->AsShared();
		TSharedRef<FUICommandList> ToolkitCommands = AssetEditorToolkit->GetToolkitCommands();

		TSharedPtr<FExtender> Extender = ToolbarExtenderMap.FindRef(WeakToolkit);
		if (Extender.IsValid())
		{
			AssetEditorToolkit->RemoveToolbarExtender(Extender);
		}

		TSharedRef<FExtender> ToolbarExtender = MakeShareable(new FExtender);

		TSharedRef<const FExtensionBase> Extension = ToolbarExtender->AddToolBarExtension(
			"Asset",
			EExtensionHook::After,
			ToolkitCommands,
			FToolBarExtensionDelegate::CreateRaw(this, &FBAToolbar::ExtendToolbar));

		ToolbarExtenderMap.Add(WeakToolkit, ToolbarExtender);
		AssetEditorToolkit->AddToolbarExtender(ToolbarExtender);
	}
}

void FBAToolbar::SetAutoFormattingStyle(EBAAutoFormatting FormattingStyle)
{
	if (FBAFormatterSettings* FormatterSettings = GetCurrentFormatterSettings())
	{
		UBASettings& BASettings = UBASettings::GetMutable();
		FormatterSettings->AutoFormatting = FormattingStyle;
		BASettings.PostEditChange();
		BASettings.SaveConfig();
	}
}

bool FBAToolbar::IsAutoFormattingStyleChecked(EBAAutoFormatting FormattingStyle)
{
	if (FBAFormatterSettings* FormatterSettings = GetCurrentFormatterSettings())
	{
		return FormatterSettings->AutoFormatting == FormattingStyle;
	}

	return false;
}

void FBAToolbar::SetParameterStyle(EBAParameterFormattingStyle Style)
{
	UBASettings& BASettings = UBASettings::GetMutable();
	BASettings.ParameterStyle = Style;
	BASettings.PostEditChange();
	BASettings.SaveConfig();
}

bool FBAToolbar::IsParameterStyleChecked(EBAParameterFormattingStyle Style)
{
	return UBASettings::Get().ParameterStyle == Style;
}

void FBAToolbar::SetNodeFormattingStyle(EBANodeFormattingStyle Style)
{
	UBASettings& BASettings = UBASettings::GetMutable();
	BASettings.FormattingStyle = Style;
	BASettings.PostEditChange();
	BASettings.SaveConfig();
}

bool FBAToolbar::IsNodeFormattingStyleChecked(EBANodeFormattingStyle Style)
{
	return UBASettings::Get().FormattingStyle == Style;
}

void FBAToolbar::SetFormatAllStyle(EBAFormatAllStyle Style)
{
	UBASettings& BASettings = UBASettings::GetMutable();
	BASettings.FormatAllStyle = Style;
	BASettings.PostEditChange();
	BASettings.SaveConfig();
}

bool FBAToolbar::IsFormatAllStyleChecked(EBAFormatAllStyle Style)
{
	return UBASettings::Get().FormatAllStyle == Style;
}

void FBAToolbar::SetUseCommentBoxPadding(ECheckBoxState NewCheckedState)
{
	UBASettings& BASettings = UBASettings::GetMutable();
	BASettings.bApplyCommentPadding = (NewCheckedState == ECheckBoxState::Checked) ? true : false;
	BASettings.PostEditChange();
	BASettings.SaveConfig();
}

void FBAToolbar::SetGraphReadOnly(ECheckBoxState NewCheckedState)
{
	TSharedPtr<FBAGraphHandler> GraphHandler = FBAUtils::GetCurrentGraphHandler();
	if (GraphHandler.IsValid())
	{
		GraphHandler->GetFocusedEdGraph()->bEditable = (NewCheckedState == ECheckBoxState::Checked) ? false : true;
	}
}

void FBAToolbar::OpenBlueprintAssistSettings()
{
	FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer("Editor", "Plugins", "XTools_BlueprintAssist");
}

void FBAToolbar::BindToolbarCommands()
{
	const FBAToolbarCommandsImpl& Commands = FBAToolbarCommands::Get();
	BlueprintAssistToolbarActions = MakeShareable(new FUICommandList);
	FUICommandList& ActionList = *BlueprintAssistToolbarActions;

	ActionList.MapAction(
		Commands.AutoFormatting_Never,
		FExecuteAction::CreateStatic(&FBAToolbar::SetAutoFormattingStyle, EBAAutoFormatting::Never),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FBAToolbar::IsAutoFormattingStyleChecked, EBAAutoFormatting::Never)
	);

	ActionList.MapAction(
		Commands.AutoFormatting_FormatNewlyCreated,
		FExecuteAction::CreateStatic(&FBAToolbar::SetAutoFormattingStyle, EBAAutoFormatting::FormatSingleConnected),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FBAToolbar::IsAutoFormattingStyleChecked, EBAAutoFormatting::FormatSingleConnected)
	);

	ActionList.MapAction(
		Commands.AutoFormatting_FormatAll,
		FExecuteAction::CreateStatic(&FBAToolbar::SetAutoFormattingStyle, EBAAutoFormatting::FormatAllConnected),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FBAToolbar::IsAutoFormattingStyleChecked, EBAAutoFormatting::FormatAllConnected)
	);

	ActionList.MapAction(
		Commands.FormattingStyle_Compact,
		FExecuteAction::CreateStatic(&FBAToolbar::SetNodeFormattingStyle, EBANodeFormattingStyle::Compact),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FBAToolbar::IsNodeFormattingStyleChecked, EBANodeFormattingStyle::Compact)
	);

	ActionList.MapAction(
		Commands.FormattingStyle_Expanded,
		FExecuteAction::CreateStatic(&FBAToolbar::SetNodeFormattingStyle, EBANodeFormattingStyle::Expanded),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FBAToolbar::IsNodeFormattingStyleChecked, EBANodeFormattingStyle::Expanded)
	);

	ActionList.MapAction(
		Commands.ParameterStyle_LeftHandSide,
		FExecuteAction::CreateStatic(&FBAToolbar::SetParameterStyle, EBAParameterFormattingStyle::LeftSide),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FBAToolbar::IsParameterStyleChecked, EBAParameterFormattingStyle::LeftSide)
	);

	ActionList.MapAction(
		Commands.ParameterStyle_Helixing,
		FExecuteAction::CreateStatic(&FBAToolbar::SetParameterStyle, EBAParameterFormattingStyle::Helixing),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FBAToolbar::IsParameterStyleChecked, EBAParameterFormattingStyle::Helixing)
	);

	ActionList.MapAction(
		Commands.FormatAllStyle_Simple,
		FExecuteAction::CreateStatic(&FBAToolbar::SetFormatAllStyle, EBAFormatAllStyle::Simple),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FBAToolbar::IsFormatAllStyleChecked, EBAFormatAllStyle::Simple)
	);

	ActionList.MapAction(
		Commands.FormatAllStyle_Smart,
		FExecuteAction::CreateStatic(&FBAToolbar::SetFormatAllStyle, EBAFormatAllStyle::Smart),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FBAToolbar::IsFormatAllStyleChecked, EBAFormatAllStyle::Smart)
	);

	ActionList.MapAction(
		Commands.FormatAllStyle_NodeType,
		FExecuteAction::CreateStatic(&FBAToolbar::SetFormatAllStyle, EBAFormatAllStyle::NodeType),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FBAToolbar::IsFormatAllStyleChecked, EBAFormatAllStyle::NodeType)
	);

	ActionList.MapAction(
		Commands.BlueprintAssistSettings,
		FExecuteAction::CreateStatic(&FBAToolbar::OpenBlueprintAssistSettings)
	);

	ActionList.MapAction(
		Commands.DetectUnusedNodes,
		FExecuteAction::CreateStatic(&FBAToolbar_BlueprintImpl::DetectUnusedNodes)
	);
}

TSharedRef<SWidget> FBAToolbar::CreateToolbarWidget()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;

	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, BlueprintAssistToolbarActions);

	FText GlobalSettingsDescription = FText::FromString(FString::Printf(TEXT("Other")));

	TSharedPtr<FBAGraphHandler> GraphHandler = FBAUtils::GetCurrentGraphHandler();
	if (GraphHandler.IsValid())
	{
		UEdGraph* Graph = GraphHandler->GetFocusedEdGraph();
		FString GraphClassName = Graph ? Graph->GetClass()->GetName() : FString("Null");
		const FText SectionName = FText::FromString(FString::Printf(TEXT("%s settings"), *GraphClassName));
		MenuBuilder.BeginSection("FormattingSettings", SectionName);
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("AutoFormattingSubMenu", "自动格式化行为"),
				LOCTEXT("AutoFormattingSubMenu_Tooltip", "允许你设置向图表添加新节点时的自动格式化行为"),
				FNewMenuDelegate::CreateRaw(this, &FBAToolbar::MakeAutoFormattingSubMenu));

			MenuBuilder.AddSubMenu(
				LOCTEXT("FormattingStyleSubMenu", "格式化样式"),
				LOCTEXT("FormattingStyleSubMenu_Tooltip", "设置格式化样式"),
				FNewMenuDelegate::CreateRaw(this, &FBAToolbar::MakeFormattingStyleSubMenu));

			MenuBuilder.AddSubMenu(
				LOCTEXT("ParameterStyleSubMenu", "参数样式"),
				LOCTEXT("ParameterStyleSubMenu_Tooltip", "设置格式化时参数的样式"),
				FNewMenuDelegate::CreateRaw(this, &FBAToolbar::MakeParameterStyleSubMenu));

			MenuBuilder.AddSubMenu(
				LOCTEXT("FormatAllInsertStyleSubMenu", "全部格式化样式"),
				LOCTEXT("FormatAllInsertStyle_Tooltip", "设置全部格式化样式"),
				FNewMenuDelegate::CreateRaw(this, &FBAToolbar::MakeFormatAllStyleSubMenu));

			TSharedRef<SWidget> ApplyCommentPaddingCheckbox = SNew(SBox)
			[
				SNew(SCheckBox)
				.IsChecked(UBASettings::Get().bApplyCommentPadding ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged(FOnCheckStateChanged::CreateRaw(this, &FBAToolbar::SetUseCommentBoxPadding))
				.Style(BA_STYLE_CLASS::Get(), "Menu.CheckBox")
				.ToolTipText(LOCTEXT("ApplyCommentPaddingToolTip", "切换是否在格式化时应用注释内边距"))
				.Content()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(2.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ApplyCommentPadding", "应用注释内边距"))
					]
				]
			];

			MenuBuilder.AddMenuEntry(FUIAction(), ApplyCommentPaddingCheckbox);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("MiscSettings");
		{
			TSharedRef<SWidget> GraphReadOnlyCheckbox = SNew(SBox)
			[
				SNew(SCheckBox)
				.IsChecked(GraphHandler->IsGraphReadOnly() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged(FOnCheckStateChanged::CreateRaw(this, &FBAToolbar::SetGraphReadOnly))
				.Style(BA_STYLE_CLASS::Get(), "Menu.CheckBox")
				.ToolTipText(LOCTEXT("GraphReadOnlyToolTip", "设置图表只读状态(没有BA插件无法撤销!)"))
				.Content()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(2.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("GraphReadOnly", "图表只读"))
					]
				]
			];

			MenuBuilder.AddMenuEntry(FUIAction(), GraphReadOnlyCheckbox);
		}
		MenuBuilder.EndSection();
	}
	else
	{
		GlobalSettingsDescription = FText::FromString(FString::Printf(TEXT("Settings hidden: Graph is not focused")));
	}

	// global settings
	MenuBuilder.BeginSection("GlobalSettings", GlobalSettingsDescription);
	{
		MenuBuilder.AddSubMenu(
			INVTEXT("工具"),
			INVTEXT("Blueprint Assist工具集合"),
			FNewMenuDelegate::CreateRaw(this, &FBAToolbar::MakeToolsSubMenu));

		MenuBuilder.AddSubMenu(
			INVTEXT("窗口"),
			INVTEXT("Blueprint Assist窗口集合"),
			FNewMenuDelegate::CreateRaw(this, &FBAToolbar::MakeWindowsSubMenu));

		// open blueprint settings
		MenuBuilder.AddMenuEntry(FBAToolbarCommands::Get().BlueprintAssistSettings);
	}
	MenuBuilder.EndSection();

	TSharedRef<SWidget> MenuBuilderWidget = MenuBuilder.MakeWidget();

	return MenuBuilderWidget;
}

void FBAToolbar::MakeAutoFormattingSubMenu(FMenuBuilder& InMenuBuilder)
{
	InMenuBuilder.BeginSection("AutoFormattingStyle", LOCTEXT("AutoFormattingStyle", "自动格式化样式"));
	{
		InMenuBuilder.AddMenuEntry(FBAToolbarCommands::Get().AutoFormatting_Never);
		InMenuBuilder.AddMenuEntry(FBAToolbarCommands::Get().AutoFormatting_FormatNewlyCreated);
		InMenuBuilder.AddMenuEntry(FBAToolbarCommands::Get().AutoFormatting_FormatAll);
	}
	InMenuBuilder.EndSection();
}

void FBAToolbar::MakeParameterStyleSubMenu(FMenuBuilder& InMenuBuilder)
{
	InMenuBuilder.BeginSection("ParameterStyle", LOCTEXT("ParameterStyle", "参数样式"));
	{
		InMenuBuilder.AddMenuEntry(FBAToolbarCommands::Get().ParameterStyle_Helixing);
		InMenuBuilder.AddMenuEntry(FBAToolbarCommands::Get().ParameterStyle_LeftHandSide);
	}
	InMenuBuilder.EndSection();
}

void FBAToolbar::MakeFormattingStyleSubMenu(FMenuBuilder& InMenuBuilder)
{
	InMenuBuilder.BeginSection("FormattingStyle", LOCTEXT("FormattingStyle", "格式化样式"));
	{
		InMenuBuilder.AddMenuEntry(FBAToolbarCommands::Get().FormattingStyle_Compact);
		InMenuBuilder.AddMenuEntry(FBAToolbarCommands::Get().FormattingStyle_Expanded);
	}
	InMenuBuilder.EndSection();
}

void FBAToolbar::MakeFormatAllStyleSubMenu(FMenuBuilder& InMenuBuilder)
{
	InMenuBuilder.BeginSection("FormatAllStyle", LOCTEXT("FormatAllStyle", "全部格式化样式"));
	{
		InMenuBuilder.AddMenuEntry(FBAToolbarCommands::Get().FormatAllStyle_Simple);
		InMenuBuilder.AddMenuEntry(FBAToolbarCommands::Get().FormatAllStyle_Smart);
		InMenuBuilder.AddMenuEntry(FBAToolbarCommands::Get().FormatAllStyle_NodeType);
	}
	InMenuBuilder.EndSection();
}

void FBAToolbar::MakeToolsSubMenu(FMenuBuilder& InMenuBuilder)
{
	InMenuBuilder.BeginSection("BlueprintAssistTools", INVTEXT("工具"));
	{
		if (auto GH = FBAUtils::GetCurrentGraphHandler())
		{
			if (FBAUtils::IsBlueprintGraph(GH->GetFocusedEdGraph()))
			{
				InMenuBuilder.AddMenuEntry(FBAToolbarCommands::Get().DetectUnusedNodes);
			}
		}

		// debug menu
		InMenuBuilder.AddMenuEntry(INVTEXT("打开调试菜单"), INVTEXT("打开调试菜单以查看当前图表的信息"), FSlateIcon(), FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FName("BADebugMenu"));
		}));
	}
	InMenuBuilder.EndSection();
}

void FBAToolbar::MakeWindowsSubMenu(FMenuBuilder& InMenuBuilder)
{
	InMenuBuilder.BeginSection("BlueprintAssistWindows", INVTEXT("窗口"));
	{
		// welcome screen
		InMenuBuilder.AddMenuEntry(INVTEXT("打开欢迎屏幕"), INVTEXT("打开Blueprint Assist欢迎屏幕"), FSlateIcon(), FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(SBAWelcomeScreen::GetTabId());
		}));

		// hotkey list
		InMenuBuilder.PushCommandList(FBAInputProcessor::Get().GlobalActions.GlobalCommands.ToSharedRef());
		InMenuBuilder.AddMenuEntry(FBACommands::Get().OpenBlueprintAssistHotkeySheet);
		InMenuBuilder.PopCommandList();

		// setting changes window
		InMenuBuilder.AddMenuEntry(INVTEXT("设置更改"), INVTEXT("打开一个窗口显示Blueprint Assist设置的本地更改"), FSlateIcon(), FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(SBASettingsChangeWindow::GetTabId());
		}));
	}

	InMenuBuilder.EndSection();
}

void FBAToolbar::ExtendToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateRaw(this, &FBAToolbar::CreateToolbarWidget),
		LOCTEXT("BlueprintAssist", "BlueprintAssist"),
		FText::FromString("Blueprint Assist Settings"),
		FSlateIcon(BA_GET_STYLE_SET_NAME(), "LevelEditor.GameSettings")
	);
}

FBAFormatterSettings* FBAToolbar::GetCurrentFormatterSettings()
{
	auto GraphHandler = FBAUtils::GetCurrentGraphHandler();
	if (!GraphHandler.IsValid())
	{
		return nullptr;
	}

	return UBASettings::FindFormatterSettings(GraphHandler->GetFocusedEdGraph());
}

#undef LOCTEXT_NAMESPACE