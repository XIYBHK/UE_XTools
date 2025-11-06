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
#include "BlueprintAssistObjects/BARootObject.h"
#include "BlueprintAssistWidgets/BADebugMenu.h"
#include "Developer/Settings/Public/ISettingsModule.h"
#include "Framework/Application/SlateApplication.h"
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

	if (UBASettings::Get().bDisableBlueprintAssistPlugin)
	{
		UE_LOG(LogBlueprintAssist, Log, TEXT("FBlueprintAssistModule: Blueprint Assist plugin disabled (setting DisableBlueprintAssistPlugin), not initializing"));
		return;
	}

	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FBlueprintAssistModule::OnPostEngineInit);
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

	UE_LOG(LogBlueprintAssist, Log, TEXT("Finished loaded BlueprintAssist Module"));
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
		PropertyEditorModule->UnregisterCustomClassLayout(BASettingsClassName);
	}

	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Editor", "Plugins", "BlueprintAssist");
		SettingsModule->UnregisterSettings("Editor", "Plugins", "BlueprintAssist_EditorFeatures");
		SettingsModule->UnregisterSettings("Editor", "Plugins", "BlueprintAssist_Advanced");
	}

	FBACommands::Unregister();
	FBAToolbarCommands::Unregister();

	FCoreDelegates::OnPostEngineInit.RemoveAll(this);

	FBAStyle::Shutdown();

	UE_LOG(LogBlueprintAssist, Log, TEXT("Shutdown BlueprintAssist Module"));
#endif
}

void FBlueprintAssistModule::BindLiveCodingSound()
{
#if WITH_LIVE_CODING
	if (ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME))
	{
		if (LiveCoding->IsEnabledByDefault() || LiveCoding->IsEnabledForSession())
		{
			auto PlaySound = []()
			{
				if (UBASettings::Get().bPlayLiveCompileSound)
				{
					GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileSuccess_Cue.CompileSuccess_Cue"));
				}
			};

			LiveCoding->GetOnPatchCompleteDelegate().AddLambda(PlaySound);
			UE_LOG(LogBlueprintAssist, Log, TEXT("Bound to live coding patch complete"));
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
		"BlueprintAssist",
		LOCTEXT("BlueprintAssistSettingsName", "Blueprint Assist"),
		LOCTEXT("BlueprintAssistSettingsNameDesc", "配置 Blueprint Assist 蓝图编辑增强插件"),
		&UBASettings::GetMutable()
	);

	BASettingsClassName = UBASettings::StaticClass()->GetFName();
	PropertyModule.RegisterCustomClassLayout(BASettingsClassName, FOnGetDetailCustomizationInstance::CreateStatic(&FBASettingsDetails::MakeInstance));

	// Register UBASettings_EditorFeatures to appear in the editor settings
	SettingsModule.RegisterSettings(
		"Editor",
		"Plugins",
		"BlueprintAssist_EditorFeatures",
		LOCTEXT("BlueprintAssistEditorFeaturesName", "Blueprint Assist 编辑器功能"),
		LOCTEXT("BlueprintAssistEditorFeaturesDesc", "配置 Blueprint Assist 编辑器增强功能和特性"),
		GetMutableDefault<UBASettings_EditorFeatures>()
	);

	// Register UBASettings_Advanced to appear in the editor settings
	SettingsModule.RegisterSettings(
		"Editor",
		"Plugins",
		"BlueprintAssist_Advanced",
		LOCTEXT("BlueprintAssistAdvancedName", "Blueprint Assist 高级选项"),
		LOCTEXT("BlueprintAssistAdvancedDesc", "配置 Blueprint Assist 高级选项和实验性功能"),
		GetMutableDefault<UBASettings_Advanced>()
	);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlueprintAssistModule, BlueprintAssist)
