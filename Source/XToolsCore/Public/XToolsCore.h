/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogXToolsCore, Log, All);

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
