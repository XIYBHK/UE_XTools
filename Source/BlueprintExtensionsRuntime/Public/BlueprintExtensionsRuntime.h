/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBlueprintExtensionsRuntime, Log, All);

/**
 * BlueprintExtensionsRuntime 模块
 *
 * 运行时蓝图功能库，包括：
 * - Map/Set/Array容器扩展操作
 * - 变量反射和动态访问
 * - 数学和转换工具
 * - 游戏功能特性（炮塔、车辆、支撑系统等）
 * 
 * 注意：此模块为Runtime类型，可在打包游戏中使用
 */
class FBlueprintExtensionsRuntimeModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

