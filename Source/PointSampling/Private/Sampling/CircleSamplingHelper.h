/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "PointSamplingTypes.h"

/**
 * 圆形和雪花类采样算法辅助类
 *
 * 职责：实现圆形相关的点阵生成算法
 * - 圆形（单层）- 支持2D/3D、均匀/斐波那契/泊松分布
 * - 雪花形（多层同心圆）
 * - 雪花弧形（多层同心圆弧）
 */
class FCircleSamplingHelper
{
public:
	/**
	 * 生成圆形/球体点阵（局部坐标）
	 * @param PointCount 总点数
	 * @param Radius 圆形/球体半径
	 * @param bIs3D 是否为3D球体
	 * @param DistributionMode 分布模式
	 * @param MinDistance 泊松分布的最小距离
	 * @param StartAngle 起始角度（度，仅Uniform模式2D有效）
	 * @param bClockwise 是否顺时针排列（仅Uniform模式2D有效）
	 * @param JitterStrength 扰动强度(0-1)
	 * @param RandomStream 随机流
	 * @return 相对于中心的点位数组
	 */
	static TArray<FVector> GenerateCircle(
		int32 PointCount,
		float Radius,
		bool bIs3D,
		ECircleDistributionMode DistributionMode,
		float MinDistance,
		float StartAngle,
		bool bClockwise,
		float JitterStrength,
		FRandomStream& RandomStream
	);

	/**
	 * 生成雪花形点阵（多层同心圆）
	 * @param PointCount 总点数
	 * @param Radius 最外层半径
	 * @param SnowflakeLayers 圆环层数
	 * @param Spacing 环与环之间的间距
	 * @param JitterStrength 扰动强度(0-1)
	 * @param RandomStream 随机流
	 * @return 相对于中心的点位数组
	 */
	static TArray<FVector> GenerateSnowflake(
		int32 PointCount,
		float Radius,
		int32 SnowflakeLayers,
		float Spacing,
		float JitterStrength,
		FRandomStream& RandomStream
	);

	/**
	 * 生成雪花弧形点阵（部分圆弧的多层）
	 * @param PointCount 总点数
	 * @param Radius 最外层半径
	 * @param SnowflakeLayers 圆环层数
	 * @param Spacing 环与环之间的间距
	 * @param ArcAngle 弧度范围（度）
	 * @param StartAngle 起始角度（度）
	 * @param JitterStrength 扰动强度(0-1)
	 * @param RandomStream 随机流
	 * @return 相对于中心的点位数组
	 */
	static TArray<FVector> GenerateSnowflakeArc(
		int32 PointCount,
		float Radius,
		int32 SnowflakeLayers,
		float Spacing,
		float ArcAngle,
		float StartAngle,
		float JitterStrength,
		FRandomStream& RandomStream
	);

private:
	/**
	 * 生成均匀分布的圆形/球体点位
	 * @param PointCount 点数
	 * @param Radius 半径
	 * @param bIs3D 是否为3D球体
	 * @param StartAngle 起始角度（仅2D有效）
	 * @param bClockwise 是否顺时针（仅2D有效）
	 * @return 点位数组
	 */
	static TArray<FVector> GenerateUniform(
		int32 PointCount,
		float Radius,
		bool bIs3D,
		float StartAngle,
		bool bClockwise
	);

	/**
	 * 生成斐波那契分布的圆形/球体点位
	 * @param PointCount 点数
	 * @param Radius 半径
	 * @param bIs3D 是否为3D球体
	 * @return 点位数组
	 */
	static TArray<FVector> GenerateFibonacci(
		int32 PointCount,
		float Radius,
		bool bIs3D
	);

	/**
	 * 生成泊松分布的圆形/球体点位
	 * @param PointCount 目标点数
	 * @param Radius 半径
	 * @param bIs3D 是否为3D球体
	 * @param MinDistance 最小距离
	 * @param RandomStream 随机流
	 * @return 点位数组
	 */
	static TArray<FVector> GeneratePoisson(
		int32 PointCount,
		float Radius,
		bool bIs3D,
		float MinDistance,
		FRandomStream& RandomStream
	);

	/**
	 * 在圆弧上生成点位
	 * @param OutPoints 输出点位数组
	 * @param PointCount 要生成的点数
	 * @param Radius 圆弧半径
	 * @param StartAngleDeg 起始角度（度）
	 * @param EndAngleDeg 结束角度（度）
	 * @param bClockwise 是否顺时针
	 */
	static void GenerateArcPoints(
		TArray<FVector>& OutPoints,
		int32 PointCount,
		float Radius,
		float StartAngleDeg,
		float EndAngleDeg,
		bool bClockwise
	);

	/**
	 * 应用扰动到点位
	 * @param Points 点位数组
	 * @param JitterStrength 扰动强度(0-1)
	 * @param BaseRadius 基础半径（用于计算扰动范围）
	 * @param RandomStream 随机流
	 */
	static void ApplyJitter(
		TArray<FVector>& Points,
		float JitterStrength,
		float BaseRadius,
		FRandomStream& RandomStream
	);
};
