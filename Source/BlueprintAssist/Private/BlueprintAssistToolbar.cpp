// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistToolbar.h"

#include "BlueprintAssistCache.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistSettings.h"
#include "BlueprintAssistStyle.h"
#include "BlueprintAssistTabHandler.h"
#include "BlueprintAssistUtils.h"
#include "BlueprintEditorModule.h"
#include "ISettingsModule.h"
#include "LevelEditorActions.h"
#include "BlueprintAssistMisc/BlueprintAssistToolbar_BlueprintImpl.h"
#include "EdGraph/EdGraph.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/LazySingleton.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "BlueprintAssist"

FBAToolbarCommandsImpl::FBAToolbarCommandsImpl() : TCommands<FBAToolbarCommandsImpl>(
	TEXT("BlueprintAssistToolbarCommands"),
	NSLOCTEXT("Contexts", "BlueprintAssistToolbarCommands", "Blueprint Assist Toolbar Commands"),
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
		"始终格式化所有连接的节点",
		"创建新节点时始终格式化所有连接的节点",
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
		"紧凑式格式化",
		"将格式化样式设置为紧凑式",
		EUserInterfaceActionType::RadioButton,
		FInputChord());

	UI_COMMAND(
		FormattingStyle_Expanded,
		"展开式格式化",
		"将格式化样式设置为展开式",
		EUserInterfaceActionType::RadioButton,
		FInputChord());

	UI_COMMAND(
		ParameterStyle_LeftHandSide,
		"左侧参数样式",
		"格式化时参数节点将定位在左侧",
		EUserInterfaceActionType::RadioButton,
		FInputChord());

	UI_COMMAND(
		ParameterStyle_Helixing,
		"螺旋参数样式",
		"格式化时参数节点将定位在下方",
		EUserInterfaceActionType::RadioButton,
		FInputChord());

	UI_COMMAND(
		FormatAllStyle_Simple,
		"全部格式化样式：简单",
		"将根节点定位到单列",
		EUserInterfaceActionType::RadioButton,
		FInputChord());

	UI_COMMAND(
		FormatAllStyle_Smart,
		"全部格式化样式：智能",
		"根据节点位置将根节点定位到多列",
		EUserInterfaceActionType::RadioButton,
		FInputChord());

	UI_COMMAND(
		FormatAllStyle_NodeType,
		"全部格式化样式：节点类型",
		"根据根节点类型将节点定位到列",
		EUserInterfaceActionType::RadioButton,
		FInputChord());

	UI_COMMAND(
		BlueprintAssistSettings,
		"Blueprint Assist 设置",
		"打开 Blueprint Assist 设置",
		EUserInterfaceActionType::Button,
		FInputChord());

	UI_COMMAND(
		DetectUnusedNodes,
		"检测未使用的节点",
		"检测当前图表上未使用的节点并在消息日志中显示",
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
	if (!UBASettings::Get().bAddToolbarWidget || !Asset || !AssetEditor)
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

		if (AssetEditor)
		{
			TSharedPtr<FTabManager> TabManager = AssetEditor->GetAssociatedTabManager();
			if (TabManager.IsValid())
			{
				ToolbarExtender->AddToolBarExtension(
					"Asset",
					EExtensionHook::After,
					ToolkitCommands,
					FToolBarExtensionDelegate::CreateRaw(this, &FBAToolbar::ExtendToolbarAndProcessTab, TWeakPtr<SDockTab>(TabManager->GetOwnerTab())));
			}
		}
		else
		{
			TSharedRef<const FExtensionBase> Extension = ToolbarExtender->AddToolBarExtension(
				"Asset",
				EExtensionHook::After,
				ToolkitCommands,
				FToolBarExtensionDelegate::CreateRaw(this, &FBAToolbar::ExtendToolbar));
		}

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
	FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer("Editor", "Plugins", "BlueprintAssist");
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

	FText GlobalSettingsDescription = LOCTEXT("OtherSettings", "其他");

	TSharedPtr<FBAGraphHandler> GraphHandler = FBAUtils::GetCurrentGraphHandler();
	if (GraphHandler.IsValid())
	{
		UEdGraph* Graph = GraphHandler->GetFocusedEdGraph();
		FString GraphClassName = Graph ? Graph->GetClass()->GetName() : FString("Null");
		const FText SectionName = FText::Format(LOCTEXT("GraphSettingsFormat", "{0} 设置"), FText::FromString(GraphClassName));
		MenuBuilder.BeginSection("FormattingSettings", SectionName);
		{
			MenuBuilder.AddSubMenu(
			LOCTEXT("AutoFormattingSubMenu", "自动格式化行为"),
			LOCTEXT("AutoFormattingSubMenu_Tooltip", "设置向图表添加新节点时的自动格式化行为"),
				FNewMenuDelegate::CreateRaw(this, &FBAToolbar::MakeAutoFormattingSubMenu));

			MenuBuilder.AddSubMenu(
			LOCTEXT("FormattingStyleSubMenu", "格式化样式"),
			LOCTEXT("FormattingStyleSubMenu_Tooltip", "设置格式化样式"),
				FNewMenuDelegate::CreateRaw(this, &FBAToolbar::MakeFormattingStyleSubMenu));

			MenuBuilder.AddSubMenu(
			LOCTEXT("ParameterStyleSubMenu", "参数样式"),
			LOCTEXT("ParameterStyleSubMenu_Tooltip", "设置格式化时的参数样式"),
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
				.ToolTipText(LOCTEXT("ApplyCommentPaddingToolTip", "切换格式化时是否应用注释内边距"))
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
				.ToolTipText(LOCTEXT("GraphReadOnlyToolTip", "设置图表只读状态（没有 BA 插件无法撤销！）"))
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
		GlobalSettingsDescription = LOCTEXT("SettingsHiddenGraphNotFocused", "设置已隐藏：图表未聚焦");
	}

	// global settings
	MenuBuilder.BeginSection("GlobalSettings", GlobalSettingsDescription);
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("ToolsSubMenu", "工具"),
			LOCTEXT("ToolsSubMenu_Tooltip", "Blueprint Assist 工具集合"),
			FNewMenuDelegate::CreateRaw(this, &FBAToolbar::MakeToolsSubMenu));

		const FText OpenDebugMenuLabel = LOCTEXT("OpenDebugMenu", "打开调试菜单");
		const FText OpenDebugMenuTooltip = LOCTEXT("OpenDebugMenu_Tooltip", "打开调试菜单");
		FUIAction OpenDebugMenuAction(FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FName("BADebugMenu"));
		}));

		MenuBuilder.AddMenuEntry(OpenDebugMenuLabel, OpenDebugMenuTooltip, FSlateIcon(), OpenDebugMenuAction);

		// open blueprint settings
		{
			MenuBuilder.AddMenuEntry(FBAToolbarCommands::Get().BlueprintAssistSettings);
		}
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
	InMenuBuilder.BeginSection("BlueprintAssistTools", LOCTEXT("ToolsSection", "工具"));
	{
		if (auto GH = FBAUtils::GetCurrentGraphHandler())
		{
			if (FBAUtils::IsBlueprintGraph(GH->GetFocusedEdGraph()))
			{
				InMenuBuilder.AddMenuEntry(FBAToolbarCommands::Get().DetectUnusedNodes);
			}
		}
	}
	InMenuBuilder.EndSection();
}

void FBAToolbar::ExtendToolbarAndProcessTab(FToolBarBuilder& ToolbarBuilder, TWeakPtr<SDockTab> Tab)
{
	if (!Tab.IsValid())
	{
		return;
	}

	FBATabHandler::Get().ProcessTab(Tab.Pin());
	ExtendToolbar(ToolbarBuilder);
}

void FBAToolbar::ExtendToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateRaw(this, &FBAToolbar::CreateToolbarWidget),
		LOCTEXT("BlueprintAssist", "BP Assist"),
		LOCTEXT("BlueprintAssist_Tooltip", "Blueprint Assist 设置"),
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