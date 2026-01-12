/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PointSamplingTypes.h"
#include "PointSamplingLibrary.generated.h"

class UBoxComponent;

/**
 * 点采样蓝图函数库
 * 
 *  完全匹配原XToolsLibrary的Blueprint接口
 */
UCLASS()
class POINTSAMPLING_API UPointSamplingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================================================
	// 基础2D/3D采样
	// ============================================================================

	UFUNCTION(BlueprintCallable, Category = "Point Sampling|Poisson",
		meta = (DisplayName = "泊松采样 2D"))
	static TArray<FVector2D> GeneratePoissonPoints2D(
		float Width,
		float Height,
		float Radius,
		int32 MaxAttempts = 30
	);

	UFUNCTION(BlueprintCallable, Category = "Point Sampling|Poisson",
		meta = (DisplayName = "泊松采样 3D"))
	static TArray<FVector> GeneratePoissonPoints3D(
		float Width,
		float Height,
		float Depth,
		float Radius,
		int32 MaxAttempts = 30
	);

	// ============================================================================
	// Box组件采样（完全匹配原XToolsLibrary签名）
	// ============================================================================

	UFUNCTION(BlueprintCallable, Category = "Point Sampling|Poisson",
		meta = (DisplayName = "泊松采样（Box组件）",
			AdvancedDisplay = "TargetPointCount,JitterStrength,bUseCache"))
	static TArray<FVector> GeneratePoissonPointsInBox(
		UPARAM(ref) UBoxComponent* BoxComponent,
		float Radius = 50.0f,
		int32 MaxAttempts = 30,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		int32 TargetPointCount = 0,
		float JitterStrength = 0.0f,
		bool bUseCache = true
	);

	UFUNCTION(BlueprintCallable, Category = "Point Sampling|Poisson",
		meta = (DisplayName = "泊松采样（Box参数）",
			ScriptMethod = "GeneratePoissonPointsInBox",
			AdvancedDisplay = "TargetPointCount,JitterStrength,bUseCache"))
	static TArray<FVector> GeneratePoissonPointsInBoxByVector(
		FVector BoxExtent,
		FTransform Transform,
		float Radius = 50.0f,
		int32 MaxAttempts = 30,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		int32 TargetPointCount = 0,
		float JitterStrength = 0.0f,
		bool bUseCache = true
	);

	// ============================================================================
	// FromStream版本（流送）
	// ============================================================================

	UFUNCTION(BlueprintCallable, Category = "Point Sampling|Poisson|Stream",
		meta = (DisplayName = "泊松采样（流送-Box组件）",
			AdvancedDisplay = "TargetPointCount,JitterStrength"))
	static TArray<FVector> GeneratePoissonPointsInBoxFromStream(
		const FRandomStream& RandomStream,
		UPARAM(ref) UBoxComponent* BoxComponent,
		float Radius = 50.0f,
		int32 MaxAttempts = 30,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		int32 TargetPointCount = 0,
		float JitterStrength = 0.0f
	);

	UFUNCTION(BlueprintCallable, Category = "Point Sampling|Poisson|Stream",
		meta = (DisplayName = "泊松采样（流送-Box参数）",
			ScriptMethod = "GeneratePoissonPointsInBoxByVector",
			AdvancedDisplay = "TargetPointCount,JitterStrength"))
	static TArray<FVector> GeneratePoissonPointsInBoxByVectorFromStream(
		const FRandomStream& RandomStream,
		FVector BoxExtent,
		FTransform Transform,
		float Radius = 50.0f,
		int32 MaxAttempts = 30,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		int32 TargetPointCount = 0,
		float JitterStrength = 0.0f
	);

	// ============================================================================
	// 缓存管理
	// ============================================================================

	UFUNCTION(BlueprintCallable, Category = "Point Sampling|Cache",
		meta = (DisplayName = "清空泊松采样缓存"))
	static void ClearPoissonSamplingCache();

	UFUNCTION(BlueprintCallable, Category = "Point Sampling|Cache",
		meta = (DisplayName = "获取泊松缓存统计"))
	static void GetPoissonSamplingCacheStats(int32& OutHits, int32& OutMisses);

	// ============================================================================
	// 采样质量验证
	// ============================================================================

	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|分析",
		meta = (DisplayName = "分析采样点统计",
			ToolTip = "计算点集的距离统计信息，包括最小距离、最大距离、平均距离和质心位置。\n\n参数:\nPoints - 要分析的点集\nOutMinDistance - 输出最小点间距\nOutMaxDistance - 输出最大点间距\nOutAvgDistance - 输出平均点间距\nOutCentroid - 输出点集质心位置",
			Keywords = "统计,分析,距离,质心"))
	static void AnalyzeSamplingStats(
		const TArray<FVector>& Points,
		float& OutMinDistance,
		float& OutMaxDistance,
		float& OutAvgDistance,
		FVector& OutCentroid
	);

	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|分析",
		meta = (DisplayName = "验证泊松采样质量",
			ToolTip = "验证泊松采样结果是否满足最小距离约束。\n\n参数:\nPoints - 要验证的点集\nExpectedMinDistance - 期望的最小距离\nTolerance - 容差范围 (0.0-1.0)\n\n返回值:\n如果所有点对距离都大于等于 (ExpectedMinDistance * (1-Tolerance)) 则返回true",
			Keywords = "验证,质量,泊松,约束"))
	static bool ValidatePoissonSampling(
		const TArray<FVector>& Points,
		float ExpectedMinDistance,
		float Tolerance = 0.1f
	);

	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|分析",
		meta = (DisplayName = "计算点集分布均匀性"))
	static float CalculateDistributionUniformity(const TArray<FVector>& Points);

	// ============================================================================
	// 通用阵型生成器 (基于模式选择)
	// ============================================================================

	/**
	 * 通用阵型生成器 - 根据模式自动选择合适的阵型算法
	 * 这是一个统一的接口，支持所有阵型模式的生成
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|通用阵型",
		meta = (DisplayName = "生成阵型（通用）",
			ToolTip = "根据指定的阵型模式生成点集。这是所有阵型生成器的统一接口。\n\n参数:\nMode - 阵型模式（选择具体的阵型类型）\nPointCount - 点数量\nCenterLocation - 中心位置\nRotation - 旋转角度\nCoordinateSpace - 坐标空间\n其他参数根据模式自动选择相关参数",
			Keywords = "阵型,formation,pattern,通用"))
	static TArray<FVector> GenerateFormation(
		EPointSamplingMode Mode,
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation = FRotator::ZeroRotator,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float Spacing = 100.0f,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0,
		// 阵型特定参数
		float Param1 = 0.0f, // 根据模式不同含义不同
		float Param2 = 0.0f, // 根据模式不同含义不同
		int32 Param3 = 0     // 根据模式不同含义不同
	);
};
