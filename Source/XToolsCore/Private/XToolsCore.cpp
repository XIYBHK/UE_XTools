/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "XToolsCore.h"

#define LOCTEXT_NAMESPACE "FXToolsCoreModule"

DEFINE_LOG_CATEGORY(LogXToolsCore);

const TArray<FName>& FXToolsLogCategories::Get()
{
	// 与各插件模块中的日志类别保持同步（避免在多个地方硬编码）
	static const TArray<FName> Categories = {
		// 核心
		TEXT("LogXToolsCore"),
		TEXT("LogXTools"),
		// 运行时模块
		TEXT("LogSort"),
		TEXT("LogRandomShuffle"),
		TEXT("LogEnhancedCodeFlow"),
		TEXT("LogPointSampling"),
		TEXT("LogFormationSystem"),
		TEXT("LogComponentTimelineRuntime"),
		TEXT("LogBlueprintExtensions"),
		TEXT("LogBlueprintExtensionsRuntime"),
		TEXT("LogObjectPool"),
		TEXT("LogActorPool"),
		TEXT("LogObjectPoolSubsystem"),
		TEXT("LogObjectPoolManager"),
		TEXT("LogObjectPoolConfigManager"),
		TEXT("LogObjectPoolUtils"),
		TEXT("LogActorPoolMemoryOptimizer"),
		TEXT("LogFieldSystemExtensions"),
		TEXT("LogGeometryTool"),
		// 编辑器模块
		TEXT("LogX_AssetEditor"),
		TEXT("LogX_AssetNaming"),
		TEXT("LogX_AssetNamingDelegates"),
		TEXT("LogX_PivotTools"),
		TEXT("LogX_CollisionManager"),
		TEXT("LogObjectPoolEditor"),
		TEXT("LogSortEditor"),
		TEXT("LogAutoSizeComments"),
		TEXT("LogBlueprintAssist"),
		TEXT("LogBlueprintScreenshotTool"),
	};

	return Categories;
}

void FXToolsCoreModule::StartupModule()
{
	UE_LOG(LogXToolsCore, Log, TEXT("XToolsCore module started"));
}

void FXToolsCoreModule::ShutdownModule()
{
	UE_LOG(LogXToolsCore, Log, TEXT("XToolsCore module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FXToolsCoreModule, XToolsCore)
