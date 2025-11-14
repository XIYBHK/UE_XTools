// Copyright fpwong. All Rights Reserved.

#include "AutoSizeCommentsModule.h"

#include "AutoSizeCommentsCacheFile.h"
#include "AutoSizeCommentsCommands.h"
#include "AutoSizeCommentsGraphHandler.h"
#include "AutoSizeCommentsGraphPanelNodeFactory.h"
#include "AutoSizeCommentsInputProcessor.h"
#include "AutoSizeCommentsNotifications.h"
#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsStyle.h"
#include "ISettingsModule.h"
#include "PropertyEditorModule.h"
#include "Misc/CoreDelegates.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FAutoSizeCommentsModule"
#define ASC_ENABLED (!IS_MONOLITHIC && !UE_BUILD_SHIPPING && !UE_BUILD_TEST && !UE_GAME && !UE_SERVER)

DEFINE_LOG_CATEGORY(LogAutoSizeComments)

void FAutoSizeCommentsModule::StartupModule()
{
#if ASC_ENABLED
	// 如果项目中已启用 Marketplace 版本的 AutoSizeComments 插件，则集成版保持空载，避免重复初始化和节点工厂冲突
	if (const TSharedPtr<IPlugin> ExternalASCPlugin = IPluginManager::Get().FindPlugin(TEXT("AutoSizeComments")))
	{
		if (ExternalASCPlugin->IsEnabled())
		{
			UE_LOG(LogAutoSizeComments, Warning, TEXT("XTools_AutoSizeComments: Detected external AutoSizeComments plugin enabled, integrated version will stay idle."));
			return;
		}
	}

	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FAutoSizeCommentsModule::OnPostEngineInit);
#endif
}

void FAutoSizeCommentsModule::OnPostEngineInit()
{
	UE_LOG(LogAutoSizeComments, Log, TEXT("Startup AutoSizeComments"));

	FAutoSizeCommentsCacheFile::Get().Init();

	// Register the graph node factory
	ASCNodeFactory = MakeShareable(new FAutoSizeCommentsGraphPanelNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(ASCNodeFactory);

	// Register custom settings to appear in the editor preferences
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings(
			"Editor", "Plugins", "AutoSizeComments",
			LOCTEXT("AutoSizeCommentsName", "Auto Size Comments"),
			LOCTEXT("AutoSizeCommentsNameDesc", "配置自动调整注释框插件的行为和外观"),
			GetMutableDefault<UAutoSizeCommentsSettings>()
		);

		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(UAutoSizeCommentsSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FASCSettingsDetails::MakeInstance));
	}

	FASCCommands::Register();

	FAutoSizeCommentGraphHandler::Get().BindDelegates();

	FAutoSizeCommentsInputProcessor::Create();

	FAutoSizeCommentsNotifications::Get().Initialize();

	FASCStyle::Initialize();
}

void FAutoSizeCommentsModule::ShutdownModule()
{
#if ASC_ENABLED
	UE_LOG(LogAutoSizeComments, Log, TEXT("Shutdown AutoSizeComments"));

	FCoreDelegates::OnPostEngineInit.RemoveAll(this);

	// Remove custom settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Editor", "Plugins", "AutoSizeComments");

		if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
		{
			PropertyModule->UnregisterCustomClassLayout(UAutoSizeCommentsSettings::StaticClass()->GetFName());
		}
	}

	// Unregister the graph node factory
	if (ASCNodeFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(ASCNodeFactory);
		ASCNodeFactory.Reset();
	}

	FCoreDelegates::OnPostEngineInit.RemoveAll(this);

	FAutoSizeCommentGraphHandler::Get().UnbindDelegates();

	FAutoSizeCommentsInputProcessor::Cleanup();

	FAutoSizeCommentsNotifications::Get().Shutdown();

	FAutoSizeCommentsCacheFile::Get().Cleanup();

	FASCStyle::Shutdown();

	FASCCommands::Unregister();
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAutoSizeCommentsModule, XTools_AutoSizeComments)
