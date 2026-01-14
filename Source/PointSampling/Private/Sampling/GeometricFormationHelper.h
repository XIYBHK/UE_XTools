/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h"

/**
 * 几何阵型采样辅助类
 *
 * 实现各种数学几何形状的阵型：
 * - 蜂巢阵型 (六边形网格)
 * - 星形阵型 (多角星)
 * - 螺旋阵型 (阿基米德螺旋、对数螺旋)
 * - 心脏阵型 (心形曲线)
 * - 花瓣阵型 (花朵形状)
 */
class FGeometricFormationHelper
{
public:
	/**
	 * 生成蜂巢阵型 (六边形网格)
	 * 最紧凑的2D填充模式
	 */
	static TArray<FVector> GenerateHexagonalGrid(
		int32 PointCount,
		float Spacing,
		int32 Rings
	);

	/**
	 * 生成星形阵型
	 * @param PointsCount 星角数量
	 */
	static TArray<FVector> GenerateStarFormation(
		int32 PointCount,
		float OuterRadius,
		float InnerRadius,
		int32 PointsCount
	);

	/**
	 * 生成阿基米德螺旋阵型
	 * 等距螺旋线
	 */
	static TArray<FVector> GenerateArchimedeanSpiral(
		int32 PointCount,
		float Spacing,
		float Turns
	);

	/**
	 * 生成对数螺旋阵型 (黄金螺旋)
	 * 斐波那契螺旋
	 */
	static TArray<FVector> GenerateLogarithmicSpiral(
		int32 PointCount,
		float GrowthFactor,
		float AngleStep
	);

	/**
	 * 生成心脏形阵型
	 * 心形曲线
	 */
	static TArray<FVector> GenerateHeartFormation(
		int32 PointCount,
		float Size
	);

	/**
	 * 生成花瓣阵型
	 * @param PetalCount 花瓣数量
	 */
	static TArray<FVector> GenerateFlowerFormation(
		int32 PointCount,
		float OuterRadius,
		float InnerRadius,
		int32 PetalCount
	);

private:
	/**
	 * 计算心形曲线上的一点
	 * @param t 参数 (0-2π)
	 */
	static FVector HeartCurvePoint(float t, float Size);

	/**
	 * 计算花瓣曲线上的一点
	 * @param t 参数 (0-2π)
	 * @param PetalIndex 花瓣索引
	 * @param PetalCount 总花瓣数
	 */
	static FVector FlowerPetalPoint(float t, int32 PetalIndex, int32 PetalCount, float OuterRadius, float InnerRadius);
};