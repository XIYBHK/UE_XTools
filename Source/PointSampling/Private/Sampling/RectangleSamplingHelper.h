/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "PointSamplingTypes.h"

/**
 * 矩形类采样算法辅助类
 *
 * 职责：实现矩形相关的点阵生成算法
 * - 实心矩形
 * - 空心矩形
 * - 螺旋矩形
 */
class FRectangleSamplingHelper
{
public:
	/**
	 * 生成实心矩形点阵（局部坐标）
	 * @return 相对于中心的点位数组
	 */
	static TArray<FVector> GenerateSolidRectangle(
		int32 PointCount,
		float Spacing,
		int32 RowCount,
		int32 ColumnCount,
		float JitterStrength,
		FRandomStream& RandomStream
	);

	/**
	 * 生成空心矩形点阵（局部坐标）
	 */
	static TArray<FVector> GenerateHollowRectangle(
		int32 PointCount,
		float Spacing,
		int32 RowCount,
		int32 ColumnCount,
		float JitterStrength,
		FRandomStream& RandomStream
	);

	/**
	 * 生成螺旋矩形点阵（局部坐标）
	 */
	static TArray<FVector> GenerateSpiralRectangle(
		int32 PointCount,
		float Spacing,
		float SpiralTurns,
		float JitterStrength,
		FRandomStream& RandomStream
	);

	/**
	 * 生成六边形网格阵型（蜂巢布局）
	 * 特点：最紧凑的2D填充，自然界最优分布模式
	 */
	static TArray<FVector> GenerateHexagonalGrid(
		int32 PointCount,
		float Spacing,
		int32 Rings,
		float JitterStrength,
		FRandomStream& RandomStream
	);

	/**
	 * 生成对角线阵型（斜线排列）
	 * @param Direction -1=左下到右上, 1=右下到左上
	 */
	static TArray<FVector> GenerateDiagonalFormation(
		int32 PointCount,
		float Spacing,
		int32 Direction,
		float JitterStrength,
		FRandomStream& RandomStream
	);

	/**
	 * 生成棋盘阵型（间隔排列）
	 * 特点：类似于国际象棋的黑白格子分布
	 */
	static TArray<FVector> GenerateCheckerboardFormation(
		int32 PointCount,
		float Spacing,
		float JitterStrength,
		FRandomStream& RandomStream
	);

private:
	/**
	 * 计算最佳行列数
	 * @param PointCount 总点数
	 * @param OutRows 输出行数
	 * @param OutCols 输出列数
	 */
	static void CalculateOptimalRowsCols(int32 PointCount, int32& OutRows, int32& OutCols);

	/**
	 * 应用抖动/扰动到点位
	 */
	static void ApplyJitter(TArray<FVector>& Points, float JitterStrength, float Spacing, FRandomStream& RandomStream);
};
