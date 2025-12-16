/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * 物理资产优化器模块
 *
 * 提供一键优化物理资产的功能，通过12条硬规则自动优化：
 * - 移除小骨骼
 * - 优化碰撞形状（Capsule/Sphere/Convex）
 * - 配置质量和阻尼
 * - 设置约束角度限制
 * - 禁用不必要的碰撞
 * - 生成 LSV（Level Set Volume）
 * - 配置 Sleep 设置
 *
 * 目标：在5秒内将���认物理资产优化为影视级布娃娃
 */
class FPhysicsAssetOptimizerModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** 扩展物理资产编辑器工具栏 */
	void ExtendPhysicsAssetEditorToolbar();

	/** 移除工具栏扩展 */
	void RemoveToolbarExtension();

	/** 工具栏扩展管理器 */
	TSharedPtr<class FExtender> ToolbarExtender;
};
