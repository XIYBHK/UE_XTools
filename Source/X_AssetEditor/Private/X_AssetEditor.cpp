// Copyright Epic Games, Inc. All Rights Reserved.

#include "X_AssetEditor.h"

// 管理器头文件
#include "AssetNaming/X_AssetNamingManager.h"
#include "MenuExtensions/X_MenuExtensionManager.h"
#include "Core/X_ModuleRegistrationManager.h"
#include "MaterialTools/X_MaterialFunctionOperation.h"
#include "CollisionTools/X_CollisionManager.h"

// UE核心头文件
#include "ToolMenus.h"

DEFINE_LOG_CATEGORY(LogX_AssetEditor);

IMPLEMENT_MODULE(FX_AssetEditorModule, X_AssetEditor);

#define LOCTEXT_NAMESPACE "X_AssetEditor"

void FX_AssetEditorModule::StartupModule()
{
    UE_LOG(LogX_AssetEditor, Log, TEXT("X_AssetEditor 模块启动中..."));

    // 确保在编辑器中运行
    if (IsRunningCommandlet())
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("在命令行模式下运行，跳过编辑器功能初始化"));
        return;
    }

    // 初始化所有管理器
    InitializeManagers();

    UE_LOG(LogX_AssetEditor, Log, TEXT("X_AssetEditor 模块已启动"));
}

void FX_AssetEditorModule::ShutdownModule()
{
    UE_LOG(LogX_AssetEditor, Log, TEXT("X_AssetEditor 模块关闭中..."));

    // 清理所有管理器
    CleanupManagers();

    UE_LOG(LogX_AssetEditor, Log, TEXT("X_AssetEditor 模块已关闭"));
}

void FX_AssetEditorModule::InitializeManagers()
{
    // 初始化模块注册管理器
    FX_ModuleRegistrationManager::Get().RegisterAll();

    // 初始化资产命名管理器
    FX_AssetNamingManager::Get().Initialize();

    // 初始化菜单扩展管理器
    FX_MenuExtensionManager::Get().RegisterMenuExtensions();

    // 等待工具菜单系统初始化后注册菜单
    if (UToolMenus::IsToolMenuUIEnabled())
    {
        FX_MenuExtensionManager::Get().RegisterMenus();
    }
    else
    {
        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
            {
                FX_MenuExtensionManager::Get().RegisterMenus();
            })
        );
    }

    UE_LOG(LogX_AssetEditor, Log, TEXT("所有管理器初始化完成"));
}

void FX_AssetEditorModule::CleanupManagers()
{
    // 清理菜单扩展管理器
    FX_MenuExtensionManager::Get().UnregisterMenuExtensions();

    // 清理模块注册管理器
    FX_ModuleRegistrationManager::Get().UnregisterAll();

    // 清理工具菜单
    if (UToolMenus::IsToolMenuUIEnabled())
    {
        UToolMenus* ToolMenus = UToolMenus::Get();
        if (ToolMenus)
        {
            ToolMenus->UnregisterOwner(this);
        }
    }

    UE_LOG(LogX_AssetEditor, Log, TEXT("所有管理器清理完成"));
}

#undef LOCTEXT_NAMESPACE
