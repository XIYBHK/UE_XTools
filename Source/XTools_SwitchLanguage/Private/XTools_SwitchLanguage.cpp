/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "XTools_SwitchLanguage.h"
#include "XTools_SwitchLanguageStyle.h"
#include "XTools_SwitchLanguageCommands.h"
#include "ToolMenus.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "Interfaces/IMainFrameModule.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "FXTools_SwitchLanguageModule"

void FXTools_SwitchLanguageModule::StartupModule()
{
	// 模块启动
	FXTools_SwitchLanguageStyle::Initialize();
	FXTools_SwitchLanguageStyle::ReloadTextures();
	FXTools_SwitchLanguageCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FXTools_SwitchLanguageCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FXTools_SwitchLanguageModule::PluginButtonClicked),
		FCanExecuteAction());

	// 绑定到主窗口
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
}

void FXTools_SwitchLanguageModule::ShutdownModule()
{
	// 模块关闭
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FXTools_SwitchLanguageStyle::Shutdown();
	FXTools_SwitchLanguageCommands::Unregister();
}

void FXTools_SwitchLanguageModule::PluginButtonClicked()
{
	// 简单切换：英文 <-> 中文
	FString CurrentLanguage = UKismetInternationalizationLibrary::GetCurrentLanguage();

	if (CurrentLanguage == "en")
	{
		UKismetInternationalizationLibrary::SetCurrentLanguage("zh-Hans");
	}
	else if (CurrentLanguage.StartsWith("zh"))
	{
		UKismetInternationalizationLibrary::SetCurrentLanguage("en");
	}
	else
	{
		UKismetInternationalizationLibrary::SetCurrentLanguage("en");
	}

	// 刷新蓝图
	RefreshBlueprints();
}

void FXTools_SwitchLanguageModule::RefreshBlueprints()
{
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();

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
	FToolMenuOwnerScoped OwnerScoped(this);

	// 关卡编辑器工具栏
	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
		FToolMenuEntry& Entry = Section.AddEntry(
			FToolMenuEntry::InitToolBarButton(FXTools_SwitchLanguageCommands::Get().PluginAction));
		Entry.SetCommandList(PluginCommands);
		Entry.Name = "XTools_SwitchLanguageButton";
		Entry.Label = FText::FromString(TEXT("SwitchLanguage"));
		Entry.ToolTip = FText::FromString(TEXT("切换编辑器语言 (英文/中文)"));
	}

	// 资产编辑器工具栏
	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("AssetEditor.DefaultToolBar");
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
		FToolMenuEntry& Entry = Section.AddEntry(
			FToolMenuEntry::InitToolBarButton(FXTools_SwitchLanguageCommands::Get().PluginAction));
		Entry.SetCommandList(PluginCommands);
		Entry.Name = "XTools_SwitchLanguageButton";
		Entry.Label = FText::FromString(TEXT(""));
		Entry.ToolTip = FText::FromString(TEXT("切换编辑器语言 (英文/中文)"));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FXTools_SwitchLanguageModule, XTools_SwitchLanguage)
