// Copyright 2023 Tomasz Klin. All Rights Reserved.

/**
 * 组件时间轴运行时模块实现文件
 * 负责模块的初始化和清理工作
 */

#include "ComponentTimelineRuntime.h"

#define LOCTEXT_NAMESPACE "FComponentTimelineRuntimeModule"

/**
 * 模块启动函数
 * 在模块被加载到内存后执行
 * 具体的执行时机在模块的.uplugin文件中指定
 */
void FComponentTimelineRuntimeModule::StartupModule()
{
	// 模块加载时的初始化代码
}

/**
 * 模块关闭函数
 * 在模块关闭时执行清理工作
 * 对于支持动态重载的模块，该函数会在模块卸载前调用
 */
void FComponentTimelineRuntimeModule::ShutdownModule()
{
	// 模块关闭时的清理代码
}

#undef LOCTEXT_NAMESPACE
	
// 实现模块接口
IMPLEMENT_MODULE(FComponentTimelineRuntimeModule, XTools_ComponentTimelineRuntime)
