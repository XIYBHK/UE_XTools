/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h"
#include "PointSamplingTypes.h"

class USplineComponent;

/**
 * 阵型采样内部辅助函数命名空间
 * 提供各 Helper 类共享的底层算法实现
 */
namespace FormationSamplingInternal
{
	/**
	 * 将局部坐标转换到目标坐标空间
	 * @param LocalPoints 局部坐标点位
	 * @param CenterLocation 中心位置
	 * @param Rotation 旋转
	 * @param CoordinateSpace 坐标空间
	 * @return 转换后的点位数组
	 */
	TArray<FVector> TransformPoints(
		const TArray<FVector>& LocalPoints,
		const FVector& CenterLocation,
		const FRotator& Rotation,
		EPoissonCoordinateSpace CoordinateSpace
	);

	/**
	 * 应用位置扰动
	 * @param Points 点位数组
	 * @param JitterStrength 扰动强度（0-1）
	 * @param Scale 缩放系数（用于计算扰动范围）
	 * @param RandomStream 随机流
	 */
	void ApplyJitter(
		TArray<FVector>& Points,
		float JitterStrength,
		float Scale,
		FRandomStream& RandomStream
	);

	/**
	 * 应用高度参数（在Z轴上分布）
	 * @param Points 点位数组
	 * @param Height 高度范围
	 * @param RandomStream 随机流
	 */
	void ApplyHeightDistribution(
		TArray<FVector>& Points,
		float Height,
		FRandomStream& RandomStream
	);

	/**
	 * 从样条组件提取控制点（世界坐标）
	 * @param SplineComponent 样条组件
	 * @param OutControlPoints 输出控制点数组
	 * @param MinRequiredPoints 最小所需点数
	 * @param ContextName 上下文名称（用于日志）
	 * @return 是否成功提取
	 */
	bool ExtractSplineControlPoints(
		USplineComponent* SplineComponent,
		TArray<FVector>& OutControlPoints,
		int32 MinRequiredPoints,
		const FString& ContextName
	);

	/**
	 * 根据坐标空间转换点位（用于样条线和网格采样）
	 * @param Points 点位数组
	 * @param CoordinateSpace 坐标空间
	 * @param OriginOffset 原点偏移
	 */
	void ConvertPointsToCoordinateSpace(
		TArray<FVector>& Points,
		EPoissonCoordinateSpace CoordinateSpace,
		const FVector& OriginOffset
	);

	/**
	 * 计算点集的质心
	 * @param Points 点位数组
	 * @return 质心位置
	 */
	FVector CalculateCentroid(const TArray<FVector>& Points);

	/**
	 * 极坐标转换为笛卡尔坐标
	 * @param Radius 半径
	 * @param AngleRad 角度（弧度）
	 * @param Z Z坐标（默认为0）
	 * @return 笛卡尔坐标向量
	 */
	FVector PolarToCartesian(float Radius, float AngleRad, float Z = 0.0f);

	/**
	 * 应用点扰动（通用版本）
	 * @param Points 点位数组
	 * @param JitterStrength 扰动强度（0-1）
	 * @param Scale 缩放系数（用于计算扰动范围）
	 * @param RandomStream 随机流
	 */
	void ApplyJitter2D(
		TArray<FVector>& Points,
		float JitterStrength,
		float Scale,
		FRandomStream& RandomStream
	);
}
