// Copyright Epic Games, Inc. All Rights Reserved.


#include "BlueprintScreenshotToolModule.h"
#include "BlueprintScreenshotToolCommandManager.h"
#include "BlueprintScreenshotToolHandler.h"
#include "BlueprintScreenshotToolSettings.h"
#include "BlueprintScreenshotToolStyle.h"
#include "ISettingsModule.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "FBlueprintScreenshotToolModule"

void FBlueprintScreenshotToolModule::StartupModule()
{
	RegisterSettings();

	// Check if plugin is enabled
	const UBlueprintScreenshotToolSettings* Settings = GetDefault<UBlueprintScreenshotToolSettings>();
	if (!Settings || !Settings->bEnablePlugin)
	{
		UE_LOG(LogTemp, Log, TEXT("BlueprintScreenshotTool: Plugin disabled (setting bEnablePlugin), not initializing"));
		return;
	}

	RegisterStyle();
	RegisterCommands();
	InitializeAsyncScreenshot();
	bIsPluginInitialized = true;
}

void FBlueprintScreenshotToolModule::ShutdownModule()
{
	// Only shutdown if plugin was initialized
	if (bIsPluginInitialized)
	{
		ShutdownAsyncScreenshot();
		UnregisterStyle();
		UnregisterCommands();
		bIsPluginInitialized = false;
	}
	UnregisterSettings();
}

void FBlueprintScreenshotToolModule::RegisterStyle()
{
	FBlueprintScreenshotToolStyle::Initialize();
	FBlueprintScreenshotToolStyle::ReloadTextures();
}

void FBlueprintScreenshotToolModule::RegisterCommands()
{
	CommandManager = MakeShareable(new FBlueprintScreenshotToolCommandManager());
	CommandManager->RegisterCommands();
}

void FBlueprintScreenshotToolModule::RegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings(
			"Editor", "Plugins", "Blueprint Screenshot Tool",
			LOCTEXT("BlueprintScreenshotTool_Label", "Blueprint Screenshot Tool"),
			LOCTEXT("BlueprintScreenshotTool_Description", "配置蓝图截图工具插件的行为和快捷键"),
			GetMutableDefault<UBlueprintScreenshotToolSettings>()
		);
	}
}

void FBlueprintScreenshotToolModule::UnregisterStyle()
{
	FBlueprintScreenshotToolStyle::Shutdown();
}

void FBlueprintScreenshotToolModule::UnregisterCommands()
{
	CommandManager->UnregisterCommands();
	CommandManager.Reset();
}

void FBlueprintScreenshotToolModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Editor", "Plugins", "Blueprint Screenshot Tool");
	}
}

void FBlueprintScreenshotToolModule::InitializeAsyncScreenshot()
{
#if WITH_EDITOR
	if (GIsEditor && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnPostTick().AddRaw(this, &FBlueprintScreenshotToolModule::OnPostTick);
	}
#endif
}

void FBlueprintScreenshotToolModule::ShutdownAsyncScreenshot()
{
#if WITH_EDITOR
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnPostTick().RemoveAll(this);
	}
#endif
}

void FBlueprintScreenshotToolModule::OnPostTick(float DeltaTime)
{
	// 直接调用静态方法处理两阶段截图
	UBlueprintScreenshotToolHandler::OnPostTick(DeltaTime);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlueprintScreenshotToolModule, XTools_BlueprintScreenshotTool)
