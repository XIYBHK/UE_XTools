/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;

// 声明模块专用日志分类
DECLARE_LOG_CATEGORY_EXTERN(LogXTools_SwitchLanguage, Log, All);

/**
 * XTools_SwitchLanguage 模块
 * 
 * 提供语言切换功能，支持运行时动态切换本地化语言
 */
class FXTools_SwitchLanguageModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * 获取模块实例
	 * @return 模块单例引用
	 */
	static FXTools_SwitchLanguageModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FXTools_SwitchLanguageModule>("XTools_SwitchLanguage");
	}

	/**
	 * 检查模块是否已加载
	 * @return 模块是否可用
	 */
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("XTools_SwitchLanguage");
	}

	/** 工具栏按钮点击回调 */
	void PluginButtonClicked();

	/** 刷新所有打开的蓝图 */
	void RefreshBlueprints();

private:
	/** 注册工具栏菜单 */
	void RegisterMenus();

private:
	/** 模块初始化状态 */
	bool bIsInitialized = false;

	/** 插件命令列表 */
	TSharedPtr<class FUICommandList> PluginCommands;

	/** 当前编辑的资产列表 */
	TArray<UObject*> EditedAssets;
};
