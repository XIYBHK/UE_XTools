/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "XTools_SwitchLanguage.h"
#include "XTools_SwitchLanguageLibrary.h"
#include "XTools_SwitchLanguageStyle.h"
#include "XTools_SwitchLanguageCommands.h"
#include "XToolsCore.h"
#include "ToolMenus.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "Interfaces/IMainFrameModule.h"
#include "Kismet2/BlueprintEditorUtils.h"

// 定义模块专用日志分类
DEFINE_LOG_CATEGORY(LogXTools_SwitchLanguage);

#define LOCTEXT_NAMESPACE "FXTools_SwitchLanguageModule"

void FXTools_SwitchLanguageModule::StartupModule()
{
	// 模块启动逻辑
	UE_LOG(LogXTools_SwitchLanguage, Log, TEXT("XTools_SwitchLanguage 模块已启动"));

	// 初始化样式和命令
	FXTools_SwitchLanguageStyle::Initialize();
	FXTools_SwitchLanguageStyle::ReloadTextures();
	FXTools_SwitchLanguageCommands::Register();

	// 创建命令列表
	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FXTools_SwitchLanguageCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FXTools_SwitchLanguageModule::PluginButtonClicked),
		FCanExecuteAction());

	// 绑定到主窗口命令
	IMainFrameModule& MainFrame = FModuleManager::GetModuleChecked<IMainFrameModule>("MainFrame");
#if ENGINE_MAJOR_VERSION >= 5
	TSharedPtr<FUICommandList> MainFrameCommands = MainFrame.GetMainFrameCommandBindings();
#else
	TSharedRef<FUICommandList> MainFrameCommands = MainFrame.GetMainFrameCommands();
#endif
	MainFrameCommands->Append(PluginCommands.ToSharedRef());

	// 注册工具栏菜单
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FXTools_SwitchLanguageModule::RegisterMenus));

	bIsInitialized = true;
}

void FXTools_SwitchLanguageModule::ShutdownModule()
{
	// 模块关闭逻辑
	if (bIsInitialized)
	{
		// 注销工具栏
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);

		// 关闭样式和命令
		FXTools_SwitchLanguageStyle::Shutdown();
		FXTools_SwitchLanguageCommands::Unregister();

		UE_LOG(LogXTools_SwitchLanguage, Log, TEXT("XTools_SwitchLanguage 模块已关闭"));
		bIsInitialized = false;
	}
}

void FXTools_SwitchLanguageModule::PluginButtonClicked()
{
	// 切换语言：英文 <-> 中文
	FString CurrentLanguage = UKismetInternationalizationLibrary::GetCurrentLanguage();

	if (CurrentLanguage == "en")
	{
		UKismetInternationalizationLibrary::SetCurrentLanguage("zh-Hans");
		UE_LOG(LogXTools_SwitchLanguage, Log, TEXT("切换到中文"));
	}
	else if (CurrentLanguage.StartsWith("zh"))
	{
		UKismetInternationalizationLibrary::SetCurrentLanguage("en");
		UE_LOG(LogXTools_SwitchLanguage, Log, TEXT("切换到英文"));
	}
	else
	{
		// 默认切换到英文
		UKismetInternationalizationLibrary::SetCurrentLanguage("en");
		UE_LOG(LogXTools_SwitchLanguage, Log, TEXT("切换到英文"));
	}

	// 刷新蓝图
	RefreshBlueprints();
}

void FXTools_SwitchLanguageModule::RefreshBlueprints()
{
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();

	if (EditedAssets.Num() > 0)
	{
		for (UObject* Data : EditedAssets)
		{
			TWeakObjectPtr<UBlueprint> Blueprint = Cast<UBlueprint>(Data);
			if (Blueprint.IsValid())
			{
				FBlueprintEditorUtils::RefreshAllNodes(Blueprint.Get());
			}
		}
	}
}

void FXTools_SwitchLanguageModule::RegisterMenus()
{
	// 注册工具栏按钮
	FToolMenuOwnerScoped OwnerScoped(this);

	// 添加到关卡编辑器工具栏
	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
		FToolMenuEntry& Entry = Section.AddEntry(
			FToolMenuEntry::InitToolBarButton(FXTools_SwitchLanguageCommands::Get().PluginAction));
		Entry.SetCommandList(PluginCommands);
		Entry.Name = "XTools_SwitchLanguageButton";
		Entry.Label = FText::FromString(TEXT("SwitchLanguage"));
		Entry.ToolTip = FText::FromString(TEXT("切换编辑器界面语言 (英文/中文)"));
	}

	// 添加到资产编辑器工具栏
	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("AssetEditor.DefaultToolBar");
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
		FToolMenuEntry& Entry = Section.AddEntry(
			FToolMenuEntry::InitToolBarButton(FXTools_SwitchLanguageCommands::Get().PluginAction));
		Entry.SetCommandList(PluginCommands);
		Entry.Name = "XTools_SwitchLanguageButton";
		Entry.Label = FText::FromString(TEXT(""));
		Entry.ToolTip = FText::FromString(TEXT("切换编辑器界面语言 (英文/中文)"));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FXTools_SwitchLanguageModule, XTools_SwitchLanguage)
