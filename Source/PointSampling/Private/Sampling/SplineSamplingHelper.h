/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "PointSamplingTypes.h"

// 前向声明
class FPoissonDiskSampling;

/**
 * 样条线采样算法辅助类
 *
 * 职责：实现沿样条线的点位生成算法
 * - 基于控制点的样条线插值
 * - 支持开放和闭合样条线
 * - 样条线边界内的泊松采样（使用Bridson算法）
 */
class FSplineSamplingHelper
{
public:
	/**
	 * 沿样条线生成点阵
	 * @param PointCount 总点数
	 * @param ControlPoints 样条线控制点数组
	 * @param bClosedSpline 是否为闭合样条线
	 * @return 样条线上的点位数组
	 */
	static TArray<FVector> GenerateAlongSpline(
		int32 PointCount,
		const TArray<FVector>& ControlPoints,
		bool bClosedSpline
	);

	/**
	 * 在样条线围成的区域内生成泊松采样点阵
	 * @param TargetPointCount 目标点数（0表示由MinDistance控制）
	 * @param ControlPoints 样条线控制点数组（必须形成闭合区域）
	 * @param MinDistance 最小点间距（<=0时自动计算）
	 * @param RandomStream 随机流
	 * @return 区域内的泊松采样点
	 */
	static TArray<FVector> GenerateWithinBoundary(
		int32 TargetPointCount,
		const TArray<FVector>& ControlPoints,
		float MinDistance,
		FRandomStream& RandomStream
	);

private:
	/**
	 * Catmull-Rom 样条插值
	 * @param P0 前一控制点
	 * @param P1 当前段起点
	 * @param P2 当前段终点
	 * @param P3 后一控制点
	 * @param T 插值参数 (0-1)
	 * @return 插值结果
	 */
	static FVector CatmullRomInterpolate(
		const FVector& P0,
		const FVector& P1,
		const FVector& P2,
		const FVector& P3,
		float T
	);

	/**
	 * 计算样条线的近似总长度
	 * @param ControlPoints 控制点数组
	 * @param bClosedSpline 是否闭合
	 * @param Segments 每段的细分数（用于长度估算）
	 * @return 近似总长度
	 */
	static float CalculateSplineLength(
		const TArray<FVector>& ControlPoints,
		bool bClosedSpline,
		int32 Segments = 10
	);

	/**
	 * 判断点是否在多边形内部（射线法）
	 * @param Point 待测试点（XY平面）
	 * @param Polygon 多边形顶点数组
	 * @return 是否在多边形内部
	 */
	static bool IsPointInPolygon(
		const FVector& Point,
		const TArray<FVector>& Polygon
	);

	/**
	 * 计算多边形的包围盒
	 * @param Polygon 多边形顶点数组
	 * @param OutMin 输出最小点
	 * @param OutMax 输出最大点
	 */
	static void CalculateBoundingBox(
		const TArray<FVector>& Polygon,
		FVector& OutMin,
		FVector& OutMax
	);
};
