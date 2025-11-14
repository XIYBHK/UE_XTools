/* Copyright (C) 2024 Hugo ATTAL - All Rights Reserved
* This plugin is downloadable from the Unreal Engine Marketplace
*/

#include "ElectronicNodes.h"
#include "ENConnectionDrawingPolicy.h"
#include "ENCommands.h"
#include "NodeFactory.h"
#include "Interfaces/IPluginManager.h"
#include "Lib/HotPatch.h"
#include "Interfaces/IMainFrameModule.h"
#include "Patch/NodeFactoryPatch.h"
#include "Popup/ENUpdatePopup.h"
#include "ISettingsEditorModule.h"

#define LOCTEXT_NAMESPACE "FElectronicNodesModule"

void FElectronicNodesModule::StartupModule()
{
	// 如果项目中已启用 Marketplace 版本的 ElectronicNodes 插件，则集成版保持空载，避免重复初始化和连接工厂冲突
	if (const TSharedPtr<IPlugin> ExternalENPlugin = IPluginManager::Get().FindPlugin(TEXT("ElectronicNodes")))
	{
		if (ExternalENPlugin->IsEnabled())
		{
			UE_LOG(LogTemp, Warning, TEXT("XTools_ElectronicNodes: Detected external ElectronicNodes plugin enabled, integrated version will stay idle."));
			return;
		}
	}

	const TSharedPtr<FENConnectionDrawingPolicyFactory> ENConnectionFactory = MakeShareable(
		new FENConnectionDrawingPolicyFactory);
	FEdGraphUtilities::RegisterVisualPinConnectionFactory(ENConnectionFactory);

	auto const CommandBindings = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame").
		GetMainFrameCommandBindings();
	ENCommands::Register();

	CommandBindings->MapAction(
		ENCommands::Get().ToggleMasterActivation,
		FExecuteAction::CreateRaw(this, &FElectronicNodesModule::ToggleMasterActivation)
	);

	FString GlobalSettingsPath = IPluginManager::Get().FindPlugin(TEXT("XTools"))->GetBaseDir();
	GlobalSettingsPath /= "Settings.ini";
	GlobalSettingsFile = FConfigCacheIni::NormalizeConfigIniPath(GlobalSettingsPath);

	ElectronicNodesSettings = GetMutableDefault<UElectronicNodesSettings>();
	ElectronicNodesSettings->OnSettingChanged().AddRaw(this, &FElectronicNodesModule::ReloadConfiguration);

	if (ElectronicNodesSettings->UseGlobalSettings)
	{
		if (FPaths::FileExists(GlobalSettingsFile))
		{
			ElectronicNodesSettings->LoadConfig(nullptr, *GlobalSettingsFile);
		}
	}

	if (ElectronicNodesSettings->UseHotPatch && ElectronicNodesSettings->MasterActivate)
	{
#if PLATFORM_WINDOWS && !UE_BUILD_SHIPPING
		FHotPatch::Hook(&FNodeFactory::CreateConnectionPolicy, &FNodeFactoryPatch::CreateConnectionPolicy_Hook);
#endif
	}

	if (ElectronicNodesSettings->ActivatePopupOnUpdate)
	{
		ENUpdatePopup::Register();
	}
}


void FElectronicNodesModule::ReloadConfiguration(UObject* Object, struct FPropertyChangedEvent& Property)
{
	const FName PropertyName = Property.GetPropertyName();

	if (PropertyName == "UseGlobalSettings")
	{
		if (ElectronicNodesSettings->UseGlobalSettings)
		{
			if (FPaths::FileExists(GlobalSettingsFile))
			{
				ElectronicNodesSettings->LoadConfig(nullptr, *GlobalSettingsFile);
			}
			else
			{
				ElectronicNodesSettings->SaveConfig(CPF_Config, *GlobalSettingsFile);
			}
		}
	}

	if (PropertyName == "UseHotPatch")
	{
		ISettingsEditorModule* SettingsEditorModule = FModuleManager::GetModulePtr<ISettingsEditorModule>(
			"SettingsEditor");
		if (SettingsEditorModule)
		{
			SettingsEditorModule->OnApplicationRestartRequired();
		}
	}

	if (ElectronicNodesSettings->LoadGlobalSettings)
	{
		if (FPaths::FileExists(GlobalSettingsFile))
		{
			ElectronicNodesSettings->LoadConfig(nullptr, *GlobalSettingsFile);
		}
		ElectronicNodesSettings->LoadGlobalSettings = false;
	}

	ElectronicNodesSettings->SaveConfig();

	if (ElectronicNodesSettings->UseGlobalSettings)
	{
		ElectronicNodesSettings->SaveConfig(CPF_Config, *GlobalSettingsFile);
	}
}

void FElectronicNodesModule::ShutdownModule()
{
}

void FElectronicNodesModule::ToggleMasterActivation() const
{
	ElectronicNodesSettings->ToggleMasterActivation();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FElectronicNodesModule, XTools_ElectronicNodes)
