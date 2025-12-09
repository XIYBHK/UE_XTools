/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

#include "CoreMinimal.h"
#include "PointSamplingTypes.h"

/**
 * 泊松采样辅助函数命名空间
 * 
 * 包含所有泊松采样算法的内部辅助函数，不对外暴露。
 */
namespace PoissonSamplingHelpers
{
	// ============================================================================
	// 半径和距离计算
	// ============================================================================

	/** 根据目标点数计算合适的Radius */
	float CalculateRadiusFromTargetCount(int32 TargetPointCount, float Width, float Height, float Depth, bool bIs2DPlane);

	/** 找到点的最近邻距离（平方） */
	float FindNearestDistanceSquared(const FVector& Point, const TArray<FVector>& Points, int32 ExcludeIndex = -1);

	// ============================================================================
	// 点数调整
	// ============================================================================

	/** 智能裁剪：移除最拥挤的点，保持最优分布 */
	void TrimToOptimalDistribution(TArray<FVector>& Points, int32 TargetCount);

	/** 分层网格填充：用均匀分层采样补充不足的点 */
	void FillWithStratifiedSampling(
		TArray<FVector>& Points,
		int32 TargetCount,
		FVector BoxSize,
		float MinDist,
		bool bIs2D,
		const FRandomStream* Stream = nullptr);

	/** 调整点数到目标数量（裁剪或补充） */
	void AdjustToTargetCount(
		TArray<FVector>& Points,
		int32 TargetCount,
		FVector BoxSize,
		float Radius,
		bool bIs2D,
		const FRandomStream* Stream = nullptr);

	// ============================================================================
	// 扰动和变换
	// ============================================================================

	/** 应用扰动噪波 */
	void ApplyJitter(TArray<FVector>& Points, float Radius, float JitterStrength, bool bIs2D = false, const FRandomStream* Stream = nullptr);

	/** 应用坐标变换（根据坐标空间类型） */
	void ApplyTransform(TArray<FVector>& Points, const FTransform& Transform, EPoissonCoordinateSpace CoordinateSpace, const FVector& ScaleCompensation = FVector::OneVector);

	// ============================================================================
	// 2D采样辅助函数
	// ============================================================================

	/** 在2D圆环中生成随机点 */
	FVector2D GenerateRandomPointAround2D(const FVector2D& Point, float MinDist, float MaxDist, const FRandomStream* Stream = nullptr);

	/** 检查2D点是否有效 */
	bool IsValidPoint2D(
		const FVector2D& Point,
		float Radius,
		float Width,
		float Height,
		const TArray<FVector2D>& Grid,
		int32 GridWidth,
		int32 GridHeight,
		float CellSize);

	// ============================================================================
	// 3D采样辅助函数
	// ============================================================================

	/** 在3D球壳中生成随机点 */
	FVector GenerateRandomPointAround3D(const FVector& Point, float MinDist, float MaxDist, const FRandomStream* Stream = nullptr);

	/** 检查3D点是否有效 */
	bool IsValidPoint3D(
		const FVector& Point,
		float Radius,
		float Width,
		float Height,
		float Depth,
		const TArray<FVector>& Grid,
		int32 GridWidth,
		int32 GridHeight,
		int32 GridDepth,
		float CellSize);

	// ============================================================================
	// 核心采样函数（统一随机源）
	// ============================================================================

	/** 
	 * 2D泊松采样核心实现
	 * @param Stream 可选随机流，nullptr时使用全局随机
	 */
	TArray<FVector2D> GeneratePoisson2DInternal(
		float Width,
		float Height,
		float Radius,
		int32 MaxAttempts,
		const FRandomStream* Stream = nullptr);

	/** 
	 * 3D泊松采样核心实现
	 * @param Stream 可选随机流，nullptr时使用全局随机
	 */
	TArray<FVector> GeneratePoisson3DInternal(
		float Width,
		float Height,
		float Depth,
		float Radius,
		int32 MaxAttempts,
		const FRandomStream* Stream = nullptr);
}

