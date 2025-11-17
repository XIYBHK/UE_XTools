// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistModule.h"

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
#include "BlueprintAssistWidgets/BAWelcomeScreen.h"
#include "Developer/Settings/Public/ISettingsModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"
#include "Modules/ModuleManager.h"

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
	if (!FSlateApplication::IsInitialized())
	{
		UE_LOG(LogBlueprintAssist, Log, TEXT("FBlueprintAssistModule: Slate App is not initialized, not loading the plugin"));
		return;
	}

	RegisterSettings();

	// Check if plugin is enabled in settings
	if (UBASettings_Advanced::Get().bDisableBlueprintAssistPlugin)
	{
		UE_LOG(LogBlueprintAssist, Log, TEXT("FBlueprintAssistModule: Blueprint Assist plugin disabled (setting DisableBlueprintAssistPlugin), not initializing"));
		return;
	}

	// Check if external BlueprintAssist plugin exists
	if (FModuleManager::Get().IsModuleLoaded("BlueprintAssist"))
	{
		UE_LOG(LogBlueprintAssist, Warning, TEXT("FBlueprintAssistModule: External BlueprintAssist plugin detected, XTools integrated version will remain inactive"));
		return;
	}

	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FBlueprintAssistModule::OnPostEngineInit);

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
		.SetDisplayName(INVTEXT("BA Welcome Screen"))
		.SetIcon(FSlateIcon("EditorStyle", "Icons.Help"))
		.SetTooltipText(INVTEXT("Opens the Blueprint Assist Welcome Screen"));

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SBASettingsChangeWindow::GetTabId(), FOnSpawnTab::CreateStatic(&SBASettingsChangeWindow::CreateTab))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetDisplayName(INVTEXT("BA Settings Changes"))
		.SetIcon(FSlateIcon("EditorStyle", "Icons.Help"))
		.SetTooltipText(INVTEXT("Opens a window where you can see the changes for Blueprint Assist settings"));

	UE_LOG(LogBlueprintAssist, Log, TEXT("Finished loaded BlueprintAssist Module"));
}

void FBlueprintAssistModule::OnMainFrameCreationFinished(TSharedPtr<SWindow> InRootWindow, bool bIsRunningStartupDialog)
{
	FBACrashReporter::Get().Init();

	if (UBASettings_EditorFeatures::Get().bShowWelcomeScreenOnLaunch)
	{
		FGlobalTabmanager::Get()->TryInvokeTab(SBAWelcomeScreen::GetTabId());
	}
}

void FBlueprintAssistModule::ShutdownModule()
{
#if BA_ENABLED
	if (!bWasModuleInitialized)
	{
		return;
	}

	FBATabHandler::Get().Cleanup();

	FBAInputProcessor::Get().Cleanup();

	FBAToolbar::Get().Cleanup();

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

	FBACommands::Unregister();
	FBAToolbarCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SBAWelcomeScreen::GetTabId());

	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
	IMainFrameModule::Get().OnMainFrameCreationFinished().RemoveAll(this);

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
		LiveCoding->GetOnPatchCompleteDelegate().Remove(LiveCodingDelegate);
		LiveCodingDelegate.Reset();
		UE_LOG(LogBlueprintAssist, Log, TEXT("Unbound sound from live coding complete"));
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
		INVTEXT("Blueprint Assist Formatting"),
		INVTEXT("Configure the Blueprint Assist formatting settings"),
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
		INVTEXT("Blueprint Assist Editor Features"),
		INVTEXT("Configure the Blueprint Assist editor features settings"),
		GetMutableDefault<UBASettings_EditorFeatures>()
	);

	// Register UBASettings_Advanced to appear in the editor settings
	SettingsModule.RegisterSettings(
		"Editor",
		"Plugins",
		"XTools_BlueprintAssist_Advanced",
		INVTEXT("Blueprint Assist Advanced"),
		INVTEXT("Configure the Blueprint Assist advanced settings"),
		GetMutableDefault<UBASettings_Advanced>()
	);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlueprintAssistModule, XTools_BlueprintAssist)
