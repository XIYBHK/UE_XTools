/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"

/**
 * 点去重辅助类
 *
 * 职责：使用空间哈希高效去除重叠/重复点位
 * - O(n) 时间复杂度去重（vs 暴力 O(n²)）
 * - 基于体素网格的空间哈希
 * - 支持可配置的重叠阈值距离
 */
class FPointDeduplicationHelper
{
public:
	/**
	 * 去除重叠点位（空间哈希算法）
	 *
	 * @param Points 输入点位数组（会被修改）
	 * @param Tolerance 重叠容差距离（小于此距离的点被视为重叠）
	 * @return 去重后的点数量
	 *
	 * 算法原理：
	 * 1. 将空间划分为网格（CellSize = Tolerance）
	 * 2. 每个点根据坐标映射到网格单元
	 * 3. 只需检查相邻 27 个单元（3x3x3）中的点
	 * 4. 时间复杂度：O(n)，空间复杂度：O(n)
	 */
	static int32 RemoveDuplicatePoints(TArray<FVector>& Points, float Tolerance = 1.0f);

	/**
	 * 统计并移除重复点（返回统计信息）
	 *
	 * @param Points 输入点位数组（会被修改）
	 * @param Tolerance 重叠容差距离
	 * @param OutOriginalCount 原始点数
	 * @param OutRemovedCount 移除的重复点数
	 */
	static void RemoveDuplicatePointsWithStats(
		TArray<FVector>& Points,
		float Tolerance,
		int32& OutOriginalCount,
		int32& OutRemovedCount
	);

	/**
	 * 基于网格对齐的去重（保持规则排列）
	 *
	 * 将所有点对齐到指定间距的网格上，每个网格单元只保留一个点。
	 * 结果点位呈规则的网格排列。
	 *
	 * @param Points 输入点位数组（会被修改）
	 * @param GridSpacing 网格间距（点位将对齐到此间距的网格）
	 * @param OutOriginalCount 原始点数
	 * @param OutRemovedCount 移除的重复点数
	 */
	static void RemoveDuplicatePointsGridAligned(
		TArray<FVector>& Points,
		float GridSpacing,
		int32& OutOriginalCount,
		int32& OutRemovedCount
	);

private:
	/**
	 * 将 3D 坐标映射到体素网格单元索引
	 *
	 * @param Point 点坐标
	 * @param CellSize 单元尺寸
	 * @return 体素单元的整数索引
	 */
	static FIntVector GetCellIndex(const FVector& Point, float CellSize);

	/**
	 * 将体素单元索引哈希为整数（用于 TMap）
	 *
	 * @param CellIndex 体素单元索引
	 * @return 哈希值
	 */
	static FORCEINLINE uint32 HashCellIndex(const FIntVector& CellIndex)
	{
		// 使用 FNV-1a 哈希算法
		uint32 Hash = 2166136261u;
		Hash = (Hash ^ static_cast<uint32>(CellIndex.X)) * 16777619u;
		Hash = (Hash ^ static_cast<uint32>(CellIndex.Y)) * 16777619u;
		Hash = (Hash ^ static_cast<uint32>(CellIndex.Z)) * 16777619u;
		return Hash;
	}

	/**
	 * 检查点是否与已存在的点重复
	 *
	 * @param Point 要检查的点
	 * @param ExistingPoints 已存在的点列表
	 * @param ToleranceSq 容差距离的平方
	 * @return 如果重复返回 true
	 */
	static bool IsPointDuplicate(
		const FVector& Point,
		const TArray<int32>& ExistingPoints,
		const TArray<FVector>& AllPoints,
		float ToleranceSq
	);
};
