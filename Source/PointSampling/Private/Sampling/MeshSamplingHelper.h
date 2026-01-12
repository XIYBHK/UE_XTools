/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "PointSamplingTypes.h"

class UStaticMesh;
class USkeletalMesh;

/**
 * 网格采样算法辅助类
 *
 * 职责：实现从网格体获取点位的算法
 * - 静态网格体顶点采样
 * - 骨骼网格体插槽采样
 */
class FMeshSamplingHelper
{
public:
	/**
	 * 从静态网格体顶点生成点阵
	 * @param StaticMesh 静态网格体引用
	 * @param Transform 变换矩阵
	 * @param LODLevel LOD 级别
	 * @param bBoundaryVerticesOnly 是否仅使用边界顶点
	 * @param MaxPoints 最大点数（0=不限制，>0=智能降采样到目标数量）
	 * @return 顶点位置数组
	 */
	static TArray<FVector> GenerateFromStaticMesh(
		UStaticMesh* StaticMesh,
		const FTransform& Transform,
		int32 LODLevel,
		bool bBoundaryVerticesOnly,
		int32 MaxPoints = 0
	);

private:
	/**
	 * 从网格三角形生成基于面积加权的采样点
	 */
	static TArray<FVector> GenerateFromMeshTriangles(
		const FStaticMeshLODResources& LOD,
		const FTransform& Transform,
		int32 MaxPoints
	);

	/**
	 * 生成网格边界顶点
	 */
	static TArray<FVector> GenerateBoundaryVertices(
		const FStaticMeshLODResources& LOD,
		const FTransform& Transform,
		int32 MaxPoints
	);

	/**
	 * 从骨骼网格体插槽生成点阵
	 * @param SkeletalMesh 骨骼网格体引用
	 * @param Transform 变换矩阵
	 * @param SocketNamePrefix 插槽名称前缀过滤（空字符串表示所有插槽）
	 * @return 插槽位置数组
	 */
	static TArray<FVector> GenerateFromSkeletalSockets(
		USkeletalMesh* SkeletalMesh,
		const FTransform& Transform,
		const FString& SocketNamePrefix
	);
};
