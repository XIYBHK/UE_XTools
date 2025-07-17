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
    UE_LOG(LogX_AssetEditor, Log, TEXT("🚀 X_AssetEditor 模块启动中..."));

    // ✅ 性能优化 - 快速检查避免重复初始化
    if (bIsInitialized)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("⚠️ X_AssetEditor 模块已经初始化，跳过重复启动"));
        return;
    }

    // 确保在编辑器中运行
    if (IsRunningCommandlet())
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("⚠️ 在命令行模式下运行，跳过编辑器功能初始化"));
        return;
    }

    // 初始化所有管理器
    InitializeManagers();

    // ✅ 标记初始化完成
    bIsInitialized = true;

    UE_LOG(LogX_AssetEditor, Log, TEXT("✅ X_AssetEditor 模块启动完成"));
}

void FX_AssetEditorModule::ShutdownModule()
{
    UE_LOG(LogX_AssetEditor, Log, TEXT("🔄 X_AssetEditor 模块关闭中..."));

    // ✅ 性能优化 - 避免重复关闭
    if (bIsShuttingDown || !bIsInitialized)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("⚠️ X_AssetEditor 模块未初始化或正在关闭，跳过重复关闭"));
        return;
    }

    // ✅ 标记正在关闭
    bIsShuttingDown = true;

    // 清理所有管理器
    CleanupManagers();

    // ✅ 重置状态
    bIsInitialized = false;
    bIsShuttingDown = false;

    UE_LOG(LogX_AssetEditor, Log, TEXT("✅ X_AssetEditor 模块关闭完成"));
}

void FX_AssetEditorModule::InitializeManagers()
{
    // ✅ 简化的管理器初始化流程
    try
    {
        // 1. 注册核心模块功能
        FX_ModuleRegistrationManager::Get().RegisterAll();

        // 2. 初始化资产管理功能
        FX_AssetNamingManager::Get().Initialize();

        // 3. 注册菜单扩展
        FX_MenuExtensionManager::Get().RegisterMenuExtensions();

        // 4. 延迟注册菜单 - 简化逻辑
        RegisterMenusWhenReady();

        UE_LOG(LogX_AssetEditor, Log, TEXT("✅ X_AssetEditor 管理器初始化完成"));
    }
    catch (...)
    {
        UE_LOG(LogX_AssetEditor, Error, TEXT("❌ X_AssetEditor 管理器初始化失败"));
    }
}

void FX_AssetEditorModule::RegisterMenusWhenReady()
{
    // ✅ 简化的菜单注册逻辑
    if (UToolMenus::IsToolMenuUIEnabled())
    {
        FX_MenuExtensionManager::Get().RegisterMenus();
    }
    else
    {
        // 使用简化的回调注册
        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateStatic([]()
            {
                if (FX_AssetEditorModule::IsAvailable())
                {
                    FX_MenuExtensionManager::Get().RegisterMenus();
                }
            })
        );
    }
}

void FX_AssetEditorModule::CleanupManagers()
{
    // ✅ 简化的管理器清理流程
    try
    {
        // 1. 清理菜单扩展
        FX_MenuExtensionManager::Get().UnregisterMenuExtensions();

        // 2. 清理模块注册
        FX_ModuleRegistrationManager::Get().UnregisterAll();

        // 3. 清理工具菜单所有者
        if (UToolMenus* ToolMenus = UToolMenus::Get())
        {
            ToolMenus->UnregisterOwner(this);
        }

        UE_LOG(LogX_AssetEditor, Log, TEXT("✅ X_AssetEditor 管理器清理完成"));
    }
    catch (...)
    {
        UE_LOG(LogX_AssetEditor, Error, TEXT("❌ X_AssetEditor 管理器清理失败"));
    }
}

bool FX_AssetEditorModule::ValidateModuleState() const
{
    // ✅ 模块状态验证 - 用于测试和调试
    if (!bIsInitialized)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("⚠️ X_AssetEditor 模块未初始化"));
        return false;
    }

    if (bIsShuttingDown)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("⚠️ X_AssetEditor 模块正在关闭"));
        return false;
    }

    // 验证关键管理器是否可用
    bool bManagersValid = true;

    // 这里可以添加更多的验证逻辑
    // 例如检查管理器的状态等

    if (bManagersValid)
    {
        UE_LOG(LogX_AssetEditor, VeryVerbose, TEXT("✅ X_AssetEditor 模块状态验证通过"));
    }
    else
    {
        UE_LOG(LogX_AssetEditor, Error, TEXT("❌ X_AssetEditor 模块状态验证失败"));
    }

    return bManagersValid;
}

#undef LOCTEXT_NAMESPACE
