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
		TEXT("LogXTools"),
		TEXT("LogX_AssetEditor"),
		TEXT("LogX_AssetNaming"),
		TEXT("LogX_AssetNamingDelegates"),
		TEXT("LogSort"),
		TEXT("LogRandomShuffles"),
		TEXT("LogEnhancedCodeFlow"),
		TEXT("LogPointSampling"),
		TEXT("LogFormationSystem"),
		TEXT("LogComponentTimeline"),
		TEXT("LogBlueprintExtensions"),
		TEXT("LogObjectPool"),
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
