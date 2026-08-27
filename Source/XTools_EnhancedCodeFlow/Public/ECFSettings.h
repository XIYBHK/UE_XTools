/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ECFSettings.generated.h"

/**
 * XTools 增强代码流运行时开发者设置
 *
 * 运行时可用（编辑器与打包后均生效），替代原 Editor-only 的
 * /Script/X_AssetEditor.X_AssetEditorSettings 字符串反射读取方案，
 * 确保同一项目在编辑器与打包成品中的增强代码流开关行为一致。
 *
 * 配置入口：项目设置 -> 游戏 -> XTools 增强代码流设置
 */
UCLASS(config = Game, DefaultConfig, meta = (DisplayName = "XTools 增强代码流设置"))
class XTOOLS_ENHANCEDCODEFLOW_API UECFSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * 启用增强代码流子系统
	 *
	 * 功能：提供高级异步执行、协程、延迟任务等代码流控制
	 * 性能影响：内存占用极小（<10KB），提供强大的异步编程能力
	 * 关闭后子系统不会创建，FFlow 静态接口调用将静默无效
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "增强代码流", meta = (
		DisplayName = "启用增强代码流子系统",
		ToolTip = "提供异步执行、协程等高级代码流功能\n轻量级子系统，建议保持启用\n编辑器与打包成品行为一致"))
	bool bEnableEnhancedCodeFlowSubsystem = true;
};
