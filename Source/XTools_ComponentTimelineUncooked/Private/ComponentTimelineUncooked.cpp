// Copyright 2023 Tomasz Klin. All Rights Reserved.

/**
 * 组件时间轴未烘焙模块实现文件
 * 负责在编辑器中处理组件时间轴的初始化和清理工作
 */

#include "ComponentTimelineUncooked.h"

#define LOCTEXT_NAMESPACE "FComponentTimelineUncookedModule"

/**
 * 模块启动函数
 * 在模块被加载到内存后执行
 * 具体的执行时机在模块的.uplugin文件中指定
 */
void FComponentTimelineUncookedModule::StartupModule()
{
	// 模块加载时的初始化代码
}

/**
 * 模块关闭函数
 * 在模块关闭时执行清理工作
 * 对于支持动态重载的模块，该函数会在模块卸载前调用
 */
void FComponentTimelineUncookedModule::ShutdownModule()
{
	// 模块关闭时的清理代码
}

#undef LOCTEXT_NAMESPACE
	
// 实现模块接口
IMPLEMENT_MODULE(FComponentTimelineUncookedModule, XTools_ComponentTimelineUncooked)
