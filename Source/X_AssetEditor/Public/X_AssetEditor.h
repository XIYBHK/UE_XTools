/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogX_AssetEditor, Log, All);

/**
 * X_AssetEditor 模块 - 重构版本
 * 精简的主模块类，将具体功能委托给专门的管理器
 */
class FX_AssetEditorModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /**
     * 获取模块实例
     */
    static FX_AssetEditorModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FX_AssetEditorModule>("X_AssetEditor");
    }

    /**
     * 检查模块是否已加载
     */
    static bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("X_AssetEditor");
    }

private:
    /**
     * 初始化所有管理器
     */
    void InitializeManagers();

	/**
	 * 清理所有管理器
	 */
	void CleanupManagers();

	/**
	 * 注册设置页面自定义
	 */
	void RegisterSettingsCustomization();

	/**
	 * 注销设置页面自定义
	 */
	void UnregisterSettingsCustomization();

	/**
	 * 延迟注册菜单 - 等待工具菜单系统就绪
	 */
	void RegisterMenusWhenReady();

    /**
     * 验证模块状态 - 用于测试和调试
     */
    bool ValidateModuleState() const;

private:
    //  性能优化 - 缓存模块状态，避免重复检查
    mutable bool bIsInitialized = false;
    mutable bool bIsShuttingDown = false;
};
