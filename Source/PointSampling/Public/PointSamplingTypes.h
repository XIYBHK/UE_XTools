/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

#include "CoreMinimal.h"
#include "PointSamplingTypes.generated.h"

/**
 * 泊松采样坐标空间类型
 */
UENUM(BlueprintType)
enum class EPoissonCoordinateSpace : uint8
{
	/** 世界空间 - 返回世界绝对坐标（应用位置+旋转）
	 * 适用场景：AddInstance的World Space = true，或手动放置Actor */
	World		UMETA(DisplayName = "世界空间"),
	
	/** 局部空间 - 返回相对于Box中心的坐标（不应用旋转，由父组件处理）
	 * 适用场景：AddInstance的World Space = false（最常用） */
	Local		UMETA(DisplayName = "局部空间"),
	
	/** 原始空间 - 返回算法原始输出（未旋转，相对于Box中心）
	 * 适用场景：需要自行处理坐标变换的情况 */
	Raw			UMETA(DisplayName = "原始空间")
};

/**
 * 泊松采样配置结构体
 */
USTRUCT(BlueprintType)
struct POINTSAMPLING_API FPoissonSamplingConfig
{
	GENERATED_BODY()

	/** 点之间的最小距离（<=0时根据目标点数自动计算） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poisson",
		meta = (DisplayName = "最小距离", ToolTip = "点之间的最小距离，<=0时根据目标点数计算"))
	float MinDistance = 50.0f;

	/** 目标点数量（<=0时忽略，由MinDistance控制；>0时自动计算MinDistance并严格裁剪到目标数量） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poisson",
		meta = (DisplayName = "目标点数", ToolTip = "期望生成的点数量，0表示由MinDistance控制"))
	int32 TargetPointCount = 0;

	/** 最大尝试次数（在标记一个点为不活跃前的最大尝试次数） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poisson|Advanced",
		meta = (DisplayName = "最大尝试次数", ToolTip = "构造函数建议5-10，运行时建议15-30"))
	int32 MaxAttempts = 30;

	/** 坐标空间类型 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poisson",
		meta = (DisplayName = "坐标空间"))
	EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local;

	/** 扰动强度 0-1（0=无扰动，1=最大扰动） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poisson|Advanced",
		meta = (ClampMin = "0", ClampMax = "1", DisplayName = "扰动强度"))
	float JitterStrength = 0.0f;

	/** 是否使用结果缓存 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poisson|Performance",
		meta = (DisplayName = "启用缓存", ToolTip = "构造函数建议开启，运行时可选"))
	bool bUseCache = true;

	/** 构造函数 - 提供合理的默认值 */
	FPoissonSamplingConfig()
		: MinDistance(50.0f)
		, TargetPointCount(0)
		, MaxAttempts(30)
		, CoordinateSpace(EPoissonCoordinateSpace::Local)
		, JitterStrength(0.0f)
		, bUseCache(true)
	{
	}
};

// ============================================================================
// 日志类别声明
// ============================================================================

/** PointSampling模块统一日志类别 */
POINTSAMPLING_API DECLARE_LOG_CATEGORY_EXTERN(LogPointSampling, Log, All);

