/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogXToolsCore, Log, All);

/**
 * 获取 XTools 插件相关的日志分类列表（用于统一设置日志级别等场景）
 *
 * 注意：该列表是只读的共享静态数组，请勿在外部修改返回的数组内容。
 */
class XTOOLSCORE_API FXToolsLogCategories
{
public:
	/** 返回所有 XTools 插件日志分类名称的只读数组 */
	static const TArray<FName>& Get();
};

/**
 * XToolsCore 模块
 * 
 * 提供跨版本兼容性和核心工具函数
 * 这是一个 Runtime 模块，可在打包游戏中使用
 */
class FXToolsCoreModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
