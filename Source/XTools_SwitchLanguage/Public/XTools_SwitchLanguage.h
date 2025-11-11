/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;

/**
 * XTools_SwitchLanguage 模块
 * 
 * 提供编辑器语言切换功能
 */
class FXTools_SwitchLanguageModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** 工具栏按钮点击回调 */
	void PluginButtonClicked();

private:
	/** 注册工具栏菜单 */
	void RegisterMenus();

	/** 刷新所有打开的蓝图 */
	void RefreshBlueprints();

private:
	/** 插件命令列表 */
	TSharedPtr<class FUICommandList> PluginCommands;
};
