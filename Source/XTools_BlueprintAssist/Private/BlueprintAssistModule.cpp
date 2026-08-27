// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistModule.h"

#include "BASettings_Meta.h"
#include "BlueprintAssistCache.h"
#include "BlueprintAssistCommands.h"
#include "BlueprintAssistGlobals.h"
#include "BlueprintAssistGraphCommands.h"
#include "BlueprintAssistGraphExtender.h"
#include "BlueprintAssistGraphPanelNodeFactory.h"
#include "BlueprintAssistInputProcessor.h"
#include "BlueprintAssistSettings.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistSettings_EditorFeatures.h"
#include "BlueprintAssistStyle.h"
#include "BlueprintAssistTabHandler.h"
#include "BlueprintAssistToolbar.h"
#include "BlueprintEditorModule.h"
#include "PropertyEditorModule.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "BlueprintAssistMisc/BACrashReporter.h"
#include "BlueprintAssistObjects/BARootObject.h"
#include "BlueprintAssistWidgets/BADebugMenu.h"
#include "BlueprintAssistWidgets/BASettingsChangeWindow.h"
#include "BlueprintAssistWidgets/BAConfigViewer.h"
#include "BlueprintAssistWidgets/BAWelcomeScreen.h"
#include "Developer/Settings/Public/ISettingsModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/AppStyle.h"

#if WITH_EDITOR
#include "MessageLogInitializationOptions.h"
#include "MessageLogModule.h"
#endif

#if WITH_LIVE_CODING
#include "ILiveCodingModule.h"
#endif

#define LOCTEXT_NAMESPACE "BlueprintAssist"

#define BA_ENABLED (!IS_MONOLITHIC && !UE_BUILD_SHIPPING && !UE_BUILD_TEST && !UE_GAME && !UE_SERVER && WITH_EDITOR)

void FBlueprintAssistModule::StartupModule()
{
#if BA_ENABLED
	// Keep the integrated module idle when the Marketplace plugin is enabled.
	if (const TSharedPtr<IPlugin> ExternalBAPlugin = IPluginManager::Get().FindPlugin(TEXT("BlueprintAssist")))
	{
		if (ExternalBAPlugin->IsEnabled())
		{
			UE_LOG(LogBlueprintAssist, Warning, TEXT("XTools_BlueprintAssist: external BlueprintAssist is enabled; integrated module will remain idle."));
			return;
		}
	}

	if (!FSlateApplication::IsInitialized())
	{
		UE_LOG(LogBlueprintAssist, Log, TEXT("FBlueprintAssistModule: Slate App is not initialized, not loading the plugin"));
		return;
	}

	RegisterSettings();
	bWereSettingsRegistered = true;

	if (!UBASettings::Get().bEnablePlugin || UBASettings_Advanced::Get().bDisableBlueprintAssistPlugin)
	{
		UE_LOG(LogBlueprintAssist, Log, TEXT("FBlueprintAssistModule: Blueprint Assist plugin disabled by settings, not initializing"));
		return;
	}

#if BA_UE_VERSION_OR_LATER(5, 8)
	FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FBlueprintAssistModule::OnPostEngineInit);
#else
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FBlueprintAssistModule::OnPostEngineInit);
#endif

	IMainFrameModule::Get().OnMainFrameCreationFinished().AddRaw(this, &FBlueprintAssistModule::OnMainFrameCreationFinished);
#endif
}

void FBlueprintAssistModule::OnPostEngineInit()
{
	if (!FSlateApplication::IsInitialized())
	{
		UE_LOG(LogBlueprintAssist, Log, TEXT("FBlueprintAssistModule: Slate App is not initialized, not loading the plugin"));
		return;
	}

	bWasModuleInitialized = true;

	FBACommands::Register();
	FBAGraphCommands::Register();

	FBAGraphExtender::ApplyExtender();

	// Init singletons
	FBACache::Get().Init();
	FBATabHandler::Get().Init();
	FBAInputProcessor::Create();

#if WITH_EDITOR
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions InitOptions;
	InitOptions.bShowFilters = false;
	InitOptions.bDiscardDuplicates = true;
	MessageLogModule.RegisterLogListing("BlueprintAssist", FText::FromString("Blueprint Assist"), InitOptions);
#endif

	FBAToolbar::Get().Init();

	FBAStyle::Initialize();

	// Register the graph node factory
	BANodeFactory = MakeShareable(new FBlueprintAssistGraphPanelNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(BANodeFactory);

	BindLiveCodingSound();

	SBADebugMenu::RegisterNomadTab();

	RootObject = NewObject<UBARootObject>();
	RootObject->AddToRoot();
	RootObject->Init();

	// display welcome screen
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SBAWelcomeScreen::GetTabId(), FOnSpawnTab::CreateStatic(&SBAWelcomeScreen::CreateWelcomeScreenTab))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetDisplayName(INVTEXT("BA 欢迎界面"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Help"))
		.SetTooltipText(INVTEXT("打开 Blueprint Assist 欢迎界面"));

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SBASettingsChangeWindow::GetTabId(), FOnSpawnTab::CreateStatic(&SBASettingsChangeWindow::CreateTab))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetDisplayName(INVTEXT("BA 设置更改"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Help"))
		.SetTooltipText(INVTEXT("查看 Blueprint Assist 设置相对默认值的更改"));

#if BA_UE_VERSION_OR_LATER(5, 4)
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SBAConfigViewer::GetTabId(), FOnSpawnTab::CreateStatic(&SBAConfigViewer::CreateTab))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetDisplayName(INVTEXT("BA 配置查看器"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Help"))
		.SetTooltipText(INVTEXT("查看配置设置及其所在的 ini 文件"));
#endif

	UE_LOG(LogBlueprintAssist, Log, TEXT("Finished loaded BlueprintAssist Module"));
}

void FBlueprintAssistModule::OnMainFrameCreationFinished(TSharedPtr<SWindow> InRootWindow, bool bIsRunningStartupDialog)
{
	// Crash upload is intentionally disabled for the integrated XTools build.
	// FBACrashReporter::Get().Init();

	if (UBASettings_EditorFeatures::Get().bShowWelcomeScreenOnLaunch)
	{
		FGlobalTabmanager::Get()->TryInvokeTab(SBAWelcomeScreen::GetTabId());
	}
}

void FBlueprintAssistModule::ShutdownModule()
{
#if BA_ENABLED
#if BA_UE_VERSION_OR_LATER(5, 8)
	FCoreDelegates::GetOnPostEngineInit().RemoveAll(this);
#else
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
#endif

	if (IMainFrameModule* MainFrameModule = FModuleManager::GetModulePtr<IMainFrameModule>("MainFrame"))
	{
		MainFrameModule->OnMainFrameCreationFinished().RemoveAll(this);
	}

	if (bWereSettingsRegistered)
	{
		if (FPropertyEditorModule* PropertyEditorModule = FModuleManager::Get().GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
		{
			PropertyEditorModule->UnregisterCustomClassLayout(UBASettings::StaticClass()->GetFName());
			PropertyEditorModule->UnregisterCustomClassLayout(UBASettings_Advanced::StaticClass()->GetFName());
			PropertyEditorModule->UnregisterCustomClassLayout(UBASettings_EditorFeatures::StaticClass()->GetFName());
		}

		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->UnregisterSettings("Editor", "Plugins", "XTools_BlueprintAssist");
			SettingsModule->UnregisterSettings("Editor", "Plugins", "XTools_BlueprintAssist_EditorFeatures");
			SettingsModule->UnregisterSettings("Editor", "Plugins", "XTools_BlueprintAssist_Advanced");
		}

		bWereSettingsRegistered = false;
	}

	if (!bWasModuleInitialized)
	{
		return;
	}

	FBATabHandler::Get().Cleanup();
	FBATabHandler::TearDown();

	FBAInputProcessor::Get().Cleanup();

	FBAToolbar::Get().Cleanup();
	FBAToolbar::TearDown();

	FBACache::Get().Cleanup();
	FBACache::TearDown();

	UnbindLiveCodingSound();

	if (RootObject.IsValid())
	{
		UE_LOG(LogBlueprintAssist, Log, TEXT("Remove BlueprintAssist Root Object"));
		RootObject->Cleanup();
		RootObject->RemoveFromRoot();
	}

#if WITH_EDITOR
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.UnregisterLogListing("BlueprintAssist");
#endif

	// Unregister the graph node factory
	if (BANodeFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(BANodeFactory);
		BANodeFactory.Reset();
	}

	FBACommands::Unregister();
	FBAToolbarCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SBAWelcomeScreen::GetTabId());
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SBASettingsChangeWindow::GetTabId());
#if BA_UE_VERSION_OR_LATER(5, 4)
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SBAConfigViewer::GetTabId());
#endif
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(FName("BADebugMenu"));

	FBAStyle::Shutdown();

	UE_LOG(LogBlueprintAssist, Log, TEXT("Shutdown BlueprintAssist Module"));
#endif
}

void FBlueprintAssistModule::BindLiveCodingSound()
{
#if WITH_LIVE_CODING
	if (!LiveCodingDelegate.IsValid())
	{
		if (ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME))
		{
			if (LiveCoding->IsEnabledByDefault() || LiveCoding->IsEnabledForSession())
			{
				auto PlaySound = []()
				{
					if (UBASettings_EditorFeatures::Get().bPlayLiveCompileSound)
					{
						GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileSuccess_Cue.CompileSuccess_Cue"));
					}
				};

				LiveCodingDelegate = LiveCoding->GetOnPatchCompleteDelegate().AddLambda(PlaySound);
				UE_LOG(LogBlueprintAssist, Log, TEXT("Bound sound to live coding complete"));
			}
		}
	}
#endif
}

void FBlueprintAssistModule::UnbindLiveCodingSound()
{
#if WITH_LIVE_CODING
	if (ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME))
	{
		if (LiveCodingDelegate.IsValid())
		{
			LiveCoding->GetOnPatchCompleteDelegate().Remove(LiveCodingDelegate);
			LiveCodingDelegate.Reset();
			UE_LOG(LogBlueprintAssist, Log, TEXT("Unbound sound from live coding complete"));
		}
	}
#endif
}

void FBlueprintAssistModule::RegisterSettings()
{
	// Register UBASettings to appear in the editor settings
	ISettingsModule& SettingsModule = FModuleManager::GetModuleChecked<ISettingsModule>("Settings");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	SettingsModule.RegisterSettings(
		"Editor",
		"Plugins",
		"XTools_BlueprintAssist",
		INVTEXT("Blueprint Assist 格式化"),
		INVTEXT("配置 Blueprint Assist 格式化设置"),
		&UBASettings::GetMutable()
	);

	PropertyModule.RegisterCustomClassLayout(UBASettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FBASettingsDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(UBASettings_Advanced::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FBASettingsDetails_Advanced::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(UBASettings_EditorFeatures::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FBASettingsDetails_EditorFeatures::MakeInstance));

	// Register UBASettings_EditorFeatures to appear in the editor settings
	SettingsModule.RegisterSettings(
		"Editor",
		"Plugins",
		"XTools_BlueprintAssist_EditorFeatures",
		INVTEXT("Blueprint Assist 编辑器功能"),
		INVTEXT("配置 Blueprint Assist 编辑器功能设置"),
		GetMutableDefault<UBASettings_EditorFeatures>()
	);

	// Register UBASettings_Advanced to appear in the editor settings
	SettingsModule.RegisterSettings(
		"Editor",
		"Plugins",
		"XTools_BlueprintAssist_Advanced",
		INVTEXT("Blueprint Assist 高级设置"),
		INVTEXT("配置 Blueprint Assist 高级设置"),
		GetMutableDefault<UBASettings_Advanced>()
	);

	const FString& Path = FConfigCacheIni::NormalizeConfigIniPath(UBASettings_Meta::Get().CustomSettingsIniPath.FilePath);
	if (FPaths::FileExists(Path))
	{
		UBASettings::GetMutable().LoadConfig(nullptr, *Path);
		UBASettings_EditorFeatures::GetMutable().LoadConfig(nullptr, *Path);
		UBASettings_Advanced::GetMutable().LoadConfig(nullptr, *Path);
		UE_LOG(LogBlueprintAssist, Log, TEXT("Loaded custom settings from file: %s"), *Path)
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlueprintAssistModule, XTools_BlueprintAssist)
