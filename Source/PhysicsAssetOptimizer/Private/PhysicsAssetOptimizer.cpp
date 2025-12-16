/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "PhysicsAssetOptimizer.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FPhysicsAssetOptimizerModule"

void FPhysicsAssetOptimizerModule::StartupModule()
{
	// 模块启动时扩展物理资产编辑器工具栏
	ExtendPhysicsAssetEditorToolbar();

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 模块已加载"));
}

void FPhysicsAssetOptimizerModule::ShutdownModule()
{
	// 清理工具栏扩展
	RemoveToolbarExtension();

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 模块已卸载"));
}

void FPhysicsAssetOptimizerModule::ExtendPhysicsAssetEditorToolbar()
{
	// TODO: 实现工具栏扩展
	// 在物理资产编辑器中添加"自动优化"按钮
	// 使用 ToolMenus 系统注册菜单项

	UE_LOG(LogTemp, Warning, TEXT("[物理资产优化器] 工具栏扩展尚未实现"));
}

void FPhysicsAssetOptimizerModule::RemoveToolbarExtension()
{
	// TODO: 移除工具栏扩展
	if (ToolbarExtender.IsValid())
	{
		ToolbarExtender.Reset();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPhysicsAssetOptimizerModule, PhysicsAssetOptimizer)
