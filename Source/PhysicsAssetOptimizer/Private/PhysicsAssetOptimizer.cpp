/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "PhysicsAssetOptimizer.h"
#include "PhysicsAssetEditorExtension.h"

#define LOCTEXT_NAMESPACE "FPhysicsAssetOptimizerModule"

void FPhysicsAssetOptimizerModule::StartupModule()
{
	// 初始化编辑器扩展
	FPhysicsAssetEditorExtension::Initialize();

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 模块已加载"));
}

void FPhysicsAssetOptimizerModule::ShutdownModule()
{
	// 清理编辑器扩展
	FPhysicsAssetEditorExtension::Shutdown();

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 模块已卸载"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPhysicsAssetOptimizerModule, PhysicsAssetOptimizer)
