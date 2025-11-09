// Copyright 2023 Tomasz Klin. All Rights Reserved.

#pragma once

/**
 * 组件时间轴运行时模块的核心头文件
 * 该模块提供了组件时间轴功能的运行时支持
 */

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

/**
 * 组件时间轴运行时模块类
 * 负责模块的初始化和清理工作
 */
class FComponentTimelineRuntimeModule : public IModuleInterface
{
public:
	/** 模块接口实现 */
	/** 模块启动时调用，用于初始化模块资源 */
	virtual void StartupModule() override;
	/** 模块关闭时调用，用于清理模块资源 */
	virtual void ShutdownModule() override;
};
