/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

#include "CoreMinimal.h"
#include "PointSamplingTypes.h"

class UBoxComponent;

/**
 * 泊松圆盘采样算法
 * 
 * 提供2D和3D空间的泊松圆盘采样实现，用于生成均匀分布且满足最小距离约束的点集。
 * 
 *  完全匹配原XToolsLibrary实现
 */
class POINTSAMPLING_API FPoissonDiskSampling
{
public:
	// ============================================================================
	// 基础2D/3D采样
	// ============================================================================

	static TArray<FVector2D> GeneratePoisson2D(
		float Width,
		float Height,
		float Radius,
		int32 MaxAttempts = 30
	);

	static TArray<FVector> GeneratePoisson3D(
		float Width,
		float Height,
		float Depth,
		float Radius,
		int32 MaxAttempts = 30
	);

	// ============================================================================
	// Box组件采样（签名完全匹配原XToolsLibrary）
	// ============================================================================

	/**
	 * Box组件内泊松采样
	 *  原签名：UXToolsLibrary::GeneratePoissonPointsInBox
	 */
	static TArray<FVector> GeneratePoissonInBox(
		const UBoxComponent* BoxComponent,
		float Radius = 50.0f,
		int32 MaxAttempts = 30,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		int32 TargetPointCount = 0,
		float JitterStrength = 0.0f,
		bool bUseCache = true
	);

	/**
	 * 通过Box参数采样
	 *  原签名：UXToolsLibrary::GeneratePoissonPointsInBoxByVector
	 *  Transform是值传递（保持与原实现一致）
	 */
	static TArray<FVector> GeneratePoissonInBoxByVector(
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
	// FromStream版本（确定性随机）
	// ============================================================================

	/**
	 * Box组件内泊松采样（确定性随机）
	 *  原签名：UXToolsLibrary::GeneratePoissonPointsInBoxFromStream
	 */
	static TArray<FVector> GeneratePoissonInBoxFromStream(
		const FRandomStream& RandomStream,
		const UBoxComponent* BoxComponent,
		float Radius = 50.0f,
		int32 MaxAttempts = 30,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		int32 TargetPointCount = 0,
		float JitterStrength = 0.0f
	);

	/**
	 * 通过Box参数采样（确定性随机）
	 *  原签名：UXToolsLibrary::GeneratePoissonPointsInBoxByVectorFromStream
	 *  注意：原实现没有bUseCache参数
	 */
	static TArray<FVector> GeneratePoissonInBoxByVectorFromStream(
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

	static void ClearCache();
	static void GetCacheStats(int32& OutHits, int32& OutMisses);
};
