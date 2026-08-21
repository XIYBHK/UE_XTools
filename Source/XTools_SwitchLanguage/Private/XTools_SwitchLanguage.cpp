/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "XTools_SwitchLanguage.h"
#include "XTools_SwitchLanguageStyle.h"
#include "XTools_SwitchLanguageCommands.h"
#include "ToolMenus.h"
#include "Interfaces/IMainFrameModule.h"
#include "Interfaces/IPluginManager.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Editor/InternationalizationSettings/Classes/InternationalizationSettingsModel.h"
#include "Engine/Blueprint.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "FXTools_SwitchLanguageModule"

namespace
{
FString GetNextEditorCulture()
{
	const FString CurrentLanguage = FInternationalization::Get().GetCurrentLanguage()->GetName();
	if (CurrentLanguage == TEXT("en"))
	{
		return TEXT("zh-Hans");
	}

	if (CurrentLanguage.StartsWith(TEXT("zh")))
	{
		return TEXT("en");
	}

	return TEXT("en");
}

void RefreshGraphSchemas()
{
	static TArray<TWeakObjectPtr<UEdGraphSchema>> CachedSchemas;
	const bool bNeedsRefresh = CachedSchemas.Num() == 0 || CachedSchemas.ContainsByPredicate(
		[](const TWeakObjectPtr<UEdGraphSchema>& Schema) { return !Schema.IsValid(); });

	if (bNeedsRefresh)
	{
		CachedSchemas.Reset();
		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			if (UEdGraphSchema* Schema = Cast<UEdGraphSchema>(ClassIt->GetDefaultObject()))
			{
				CachedSchemas.Add(Schema);
			}
		}
	}

	for (const TWeakObjectPtr<UEdGraphSchema>& Schema : CachedSchemas)
	{
		if (Schema.IsValid())
		{
			Schema->ForceVisualizationCacheClear();
		}
	}
}
}

void FXTools_SwitchLanguageModule::StartupModule()
{
	// 如果项目中已启用 Marketplace 版本的 SwitchLanguage 插件，则集成版保持空载，避免重复添加工具栏按钮
	if (const TSharedPtr<IPlugin> ExternalSLPlugin = IPluginManager::Get().FindPlugin(TEXT("SwitchLanguage")))
	{
		if (ExternalSLPlugin->IsEnabled())
		{
			UE_LOG(LogTemp, Warning, TEXT("XTools_SwitchLanguage: Detected external SwitchLanguage plugin enabled, integrated version will stay idle."));
			return;
		}
	}

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
	TSharedPtr<FUICommandList> MainFrameCommandsLocal = MainFrame.GetMainFrameCommandBindings();
	MainFrameCommandsLocal->Append(PluginCommands.ToSharedRef());
	MainFrameCommands = MainFrameCommandsLocal;

	// 注册工具栏菜单
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FXTools_SwitchLanguageModule::RegisterMenus));

	bInitialized = true;
}

void FXTools_SwitchLanguageModule::ShutdownModule()
{
	if (!bInitialized)
	{
		return;
	}

	if (const TSharedPtr<FUICommandList> MainFrameCommandList = MainFrameCommands.Pin())
	{
		MainFrameCommandList->UnmapAction(FXTools_SwitchLanguageCommands::Get().PluginAction);
	}

	MainFrameCommands.Reset();
	PluginCommands.Reset();

	// 模块关闭
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FXTools_SwitchLanguageStyle::Shutdown();
	FXTools_SwitchLanguageCommands::Unregister();

	bInitialized = false;
}

void FXTools_SwitchLanguageModule::PluginButtonClicked()
{
	const FString TargetCulture = GetNextEditorCulture();
	const TArray<FString> LocalizedCultureNames = FTextLocalizationManager::Get().GetLocalizedCultureNames(ELocalizationLoadFlags::Editor);
	if (!LocalizedCultureNames.Contains(TargetCulture))
	{
		UE_LOG(LogTemp, Warning, TEXT("XTools_SwitchLanguage: Editor language '%s' is not available."), *TargetCulture);
		return;
	}

	UInternationalizationSettingsModel* SettingsModel = GetMutableDefault<UInternationalizationSettingsModel>();
	if (!SettingsModel)
	{
		UE_LOG(LogTemp, Warning, TEXT("XTools_SwitchLanguage: Failed to access editor internationalization settings."));
		return;
	}

	FInternationalization& I18N = FInternationalization::Get();
	if (!I18N.SetCurrentLanguage(TargetCulture))
	{
		UE_LOG(LogTemp, Warning, TEXT("XTools_SwitchLanguage: Failed to switch editor language to '%s'."), *TargetCulture);
		return;
	}

	SettingsModel->SetEditorLanguage(TargetCulture);
	FTextLocalizationManager::Get().RefreshResources();
	RefreshGraphSchemas();
	RefreshBlueprints();
}

void FXTools_SwitchLanguageModule::RefreshBlueprints()
{
	if (!GEditor)
	{
		return;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		return;
	}

	TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();

	if (EditedAssets.Num() > 0)
	{
		for (UObject* Data : EditedAssets)
		{
			TWeakObjectPtr<UBlueprint> Blueprint = Cast<UBlueprint>(Data);
			if (Blueprint.IsValid())
			{
				TArray<UEdGraph*> Graphs;
				Blueprint->GetAllGraphs(Graphs);
				for (UEdGraph* Graph : Graphs)
				{
					if (Graph)
					{
						Graph->NotifyGraphChanged();
					}
				}
				Blueprint->BroadcastChanged();
			}
		}
	}
}

void FXTools_SwitchLanguageModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	// 关卡编辑器的用户扩展工具栏。
	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("XTools_SwitchLanguage");
		FToolMenuEntry& Entry = Section.AddEntry(
			FToolMenuEntry::InitToolBarButton(FXTools_SwitchLanguageCommands::Get().PluginAction));
		Entry.SetCommandList(PluginCommands);
		Entry.Name = "XTools_SwitchLanguageButton";
		Entry.Label = FText::FromString(TEXT("SwitchLanguage"));
		Entry.ToolTip = FText::FromString(TEXT("切换编辑器语言 (英文/中文)"));
	}

	// 资产编辑器的公共操作工具栏。不要扩展 AssetEditor.DefaultToolBar：它是所有资产编辑器的父菜单。
	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("AssetEditorToolbar.CommonActions");
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("XTools_SwitchLanguage");
		FToolMenuEntry& Entry = Section.AddEntry(
			FToolMenuEntry::InitToolBarButton(FXTools_SwitchLanguageCommands::Get().PluginAction));
		Entry.SetCommandList(PluginCommands);
		Entry.Name = "XTools_SwitchLanguageButton";
		Entry.Label = FText::FromString(TEXT("SwitchLanguage"));
		Entry.ToolTip = FText::FromString(TEXT("切换编辑器语言 (英文/中文)"));
	}

}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FXTools_SwitchLanguageModule, XTools_SwitchLanguage)
