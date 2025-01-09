// Copyright 2023 Tomasz Klin. All Rights Reserved.

#pragma once

/**
 * 组件时间轴未烘焙模块的核心头文件
 * 该模块提供了组件时间轴功能的编辑器支持
 */

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * 组件时间轴未烘焙模块类
 * 负责在编辑器中处理组件时间轴的初始化和清理工作
 */
class FComponentTimelineUncookedModule : public IModuleInterface
{
public:
	/** 模块接口实现 */
	/** 模块启动时调用，用于初始化编辑器相关资源 */
	virtual void StartupModule() override;
	/** 模块关闭时调用，用于清理编辑器相关资源 */
	virtual void ShutdownModule() override;
};
