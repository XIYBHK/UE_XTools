/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "PointSamplingTypes.h"
#include "Core/SamplingCache.h"

// ============================================================================
// 日志类别定义
// ============================================================================

/** PointSampling模块统一日志类别 */
DEFINE_LOG_CATEGORY(LogPointSampling);

// ============================================================================
// 模块实现
// ============================================================================

/**
 * PointSampling模块类
 *
 * 负责模块的生命周期管理和初始化清理工作
 */
class FPointSamplingModule : public IModuleInterface
{
public:
	/** 模块启动 */
	virtual void StartupModule() override
	{
		UE_LOG(LogPointSampling, Log, TEXT("PointSampling module started"));
	}

	/** 模块关闭 */
	virtual void ShutdownModule() override
	{
		// 清理缓存，释放资源
		FSamplingCache::Get().ClearCache();
		UE_LOG(LogPointSampling, Log, TEXT("PointSampling module shutdown, cache cleared"));
	}
};

// ============================================================================
// 模块注册
// ============================================================================

IMPLEMENT_MODULE(FPointSamplingModule, PointSampling)
