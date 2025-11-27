/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "PointSamplingTypes.h"

/**
 * 三角形类采样算法辅助类
 *
 * 职责：实现三角形相关的点阵生成算法
 * - 实心三角形
 * - 空心三角形
 */
class FTriangleSamplingHelper
{
public:
	/**
	 * 生成实心三角形点阵（局部坐标）
	 * @param PointCount 总点数
	 * @param Spacing 点间距
	 * @param bInverted 是否为倒三角（尖端朝下）
	 * @param JitterStrength 扰动强度(0-1)
	 * @param RandomStream 随机流
	 * @return 相对于中心的点位数组
	 */
	static TArray<FVector> GenerateSolidTriangle(
		int32 PointCount,
		float Spacing,
		bool bInverted,
		float JitterStrength,
		FRandomStream& RandomStream
	);

	/**
	 * 生成空心三角形点阵（局部坐标）
	 * @param PointCount 总点数
	 * @param Spacing 点间距
	 * @param bInverted 是否为倒三角（尖端朝下）
	 * @param JitterStrength 扰动强度(0-1)
	 * @param RandomStream 随机流
	 * @return 相对于中心的点位数组
	 */
	static TArray<FVector> GenerateHollowTriangle(
		int32 PointCount,
		float Spacing,
		bool bInverted,
		float JitterStrength,
		FRandomStream& RandomStream
	);

private:
	/**
	 * 计算给定点数需要的三角形层数
	 * @param PointCount 点数
	 * @return 三角形层数
	 */
	static int32 CalculateTriangleLayers(int32 PointCount);

	/**
	 * 应用扰动到点位
	 * @param Points 点位数组
	 * @param JitterStrength 扰动强度(0-1)
	 * @param Spacing 间距（用于计算扰动范围）
	 * @param RandomStream 随机流
	 */
	static void ApplyJitter(
		TArray<FVector>& Points,
		float JitterStrength,
		float Spacing,
		FRandomStream& RandomStream
	);
};
