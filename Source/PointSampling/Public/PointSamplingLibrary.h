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
};
