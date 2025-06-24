// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/X_ModuleRegistrationManager.h"
#include "MaterialTools/X_MaterialToolsSettings.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ISettingsModule.h"
#include "ThumbnailRendering/ThumbnailManager.h"

TUniquePtr<FX_ModuleRegistrationManager> FX_ModuleRegistrationManager::Instance = nullptr;

FX_ModuleRegistrationManager& FX_ModuleRegistrationManager::Get()
{
    if (!Instance.IsValid())
    {
        Instance = TUniquePtr<FX_ModuleRegistrationManager>(new FX_ModuleRegistrationManager());
    }
    return *Instance;
}

void FX_ModuleRegistrationManager::RegisterAll()
{
    RegisterAssetTools();
    RegisterFolderActions();
    RegisterMeshActions();
    RegisterMeshComponentActions();
    RegisterAssetEditorActions();
    RegisterThumbnailRenderer();
    RegisterSettings();
}

void FX_ModuleRegistrationManager::UnregisterAll()
{
    UnregisterSettings();
    // 其他注销操作在模块关闭时自动处理
}

void FX_ModuleRegistrationManager::RegisterAssetTools()
{
    // 注册自定义资产工具
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    
    // 这里可以注册自定义的资产操作
    // 例如：AssetTools.RegisterAssetTypeActions(MakeShareable(new FCustomAssetTypeActions()));
}

void FX_ModuleRegistrationManager::RegisterFolderActions()
{
    // 注册文件夹相关操作
    // 这里可以添加自定义的文件夹操作
}

void FX_ModuleRegistrationManager::RegisterMeshActions()
{
    // 注册网格体相关操作
    // 这里可以添加自定义的网格体操作
}

void FX_ModuleRegistrationManager::RegisterMeshComponentActions()
{
    // 注册网格体组件相关操作
    // 这里可以添加自定义的组件操作
}

void FX_ModuleRegistrationManager::RegisterAssetEditorActions()
{
    // 注册资产编辑器相关操作
    // 这里可以添加自定义的编辑器操作
}

void FX_ModuleRegistrationManager::RegisterThumbnailRenderer()
{
    // 注册缩略图渲染器
    // 目前暂时不注册自定义渲染器，如需要可以在此添加
    // UThumbnailManager::Get().RegisterCustomRenderer(
    //     UCustomAssetClass::StaticClass(),
    //     UCustomThumbnailRenderer::StaticClass()
    // );
}

void FX_ModuleRegistrationManager::RegisterSettings()
{
    RegisterMaterialToolsSettings();
}

void FX_ModuleRegistrationManager::UnregisterSettings()
{
    UnregisterMaterialToolsSettings();
}

void FX_ModuleRegistrationManager::RegisterMaterialToolsSettings()
{
    if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        SettingsModule->RegisterSettings(
            "Project",
            "Plugins",
            "X_MaterialTools",
            NSLOCTEXT("X_MaterialTools", "MaterialToolsSettingsName", "XTools 材质工具"),
            NSLOCTEXT("X_MaterialTools", "MaterialToolsSettingsDescription", "配置XTools材质工具的设置"),
            GetMutableDefault<UX_MaterialToolsSettings>()
        );
    }
}

void FX_ModuleRegistrationManager::UnregisterMaterialToolsSettings()
{
    if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        SettingsModule->UnregisterSettings("Project", "Plugins", "X_MaterialTools");
    }
}
