/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBlueprintExtensions, Log, All);

/**
 * BlueprintExtensions 模块
 *
 * 提供强大的蓝图扩展功能，包括：
 * - 增强的循环节点（ForLoop、ForEach系列）
 * - Map/Set/Array容器扩展操作
 * - 变量反射和动态访问
 * - 数学和转换工具
 * - 游戏功能特性（炮塔、车辆等）
 */
class FBlueprintExtensionsModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

