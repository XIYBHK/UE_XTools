/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "MeshSamplingHelper.h"
#include "PointDeduplicationHelper.h"
#include "PointSamplingTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "StaticMeshResources.h"

TArray<FVector> FMeshSamplingHelper::GenerateFromStaticMesh(
	UStaticMesh* StaticMesh,
	const FTransform& Transform,
	int32 LODLevel,
	bool bBoundaryVerticesOnly,
	int32 MaxPoints)
{
	TArray<FVector> Points;

	if (!StaticMesh || !StaticMesh->HasValidRenderData())
	{
		return Points;
	}

	// 获取渲染数据
	FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
	if (!RenderData || !RenderData->LODResources.IsValidIndex(LODLevel))
	{
		// 如果指定的 LOD 无效，使用 LOD 0
		LODLevel = 0;
		if (!RenderData->LODResources.IsValidIndex(LODLevel))
		{
			return Points;
		}
	}

	FStaticMeshLODResources& LOD = RenderData->LODResources[LODLevel];

	// 如果只需要边界顶点，使用拓扑分析方法
	if (bBoundaryVerticesOnly)
	{
		return GenerateBoundaryVertices(LOD, Transform, MaxPoints);
	}

	// 否则使用基于三角形面积的加权采样
	return GenerateFromMeshTriangles(LOD, Transform, MaxPoints);
}

/**
 * 从网格三角形生成基于面积加权的采样点
 */
TArray<FVector> FMeshSamplingHelper::GenerateFromMeshTriangles(
	const FStaticMeshLODResources& LOD,
	const FTransform& Transform,
	int32 MaxPoints)
{
	TArray<FVector> Points;

	const FPositionVertexBuffer& VertexBuffer = LOD.VertexBuffers.PositionVertexBuffer;
	const FRawStaticIndexBuffer& IndexBuffer = LOD.IndexBuffer;

	if (VertexBuffer.GetNumVertices() == 0 || IndexBuffer.GetNumIndices() == 0)
	{
		return Points;
	}

	// 计算所有三角形的面积和总面积
	TArray<float> TriangleAreas;
	float TotalArea = 0.0f;

	const int32 NumTriangles = IndexBuffer.GetNumIndices() / 3;
	TriangleAreas.Reserve(NumTriangles);

	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
	{
		const int32 Index0 = IndexBuffer.GetIndex(TriangleIndex * 3 + 0);
		const int32 Index1 = IndexBuffer.GetIndex(TriangleIndex * 3 + 1);
		const int32 Index2 = IndexBuffer.GetIndex(TriangleIndex * 3 + 2);

		const FVector V0 = FVector(VertexBuffer.VertexPosition(Index0));
		const FVector V1 = FVector(VertexBuffer.VertexPosition(Index1));
		const FVector V2 = FVector(VertexBuffer.VertexPosition(Index2));

		// 计算三角形面积（叉积的一半）
		const FVector Cross = FVector::CrossProduct(V1 - V0, V2 - V0);
		const float Area = Cross.Size() * 0.5f;

		TriangleAreas.Add(Area);
		TotalArea += Area;
	}

	if (TotalArea <= 0.0f || TriangleAreas.Num() == 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[网格采样] 网格总面积为0或没有三角形"));
		return Points;
	}

	// 计算目标采样点数
	const int32 TargetPoints = (MaxPoints > 0) ? MaxPoints : FMath::Min(10000, NumTriangles * 2);
	Points.Reserve(TargetPoints);

	UE_LOG(LogPointSampling, Log, TEXT("[网格采样] 开始基于面积的采样: %d 个三角形, 总面积 %.2f, 目标点数 %d"),
		NumTriangles, TotalArea, TargetPoints);

	// 基于面积加权采样
	float AccumulatedArea = 0.0f;
	FRandomStream RandomStream(FMath::Rand());

	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles && Points.Num() < TargetPoints; ++TriangleIndex)
	{
		const float TriangleArea = TriangleAreas[TriangleIndex];
		if (TriangleArea <= 0.0f) continue;

		// 计算该三角形的采样点数（基于面积比例）
		const float AreaRatio = TriangleArea / TotalArea;
		const int32 PointsForThisTriangle = FMath::Max(1, FMath::RoundToInt(AreaRatio * TargetPoints));

		AccumulatedArea += TriangleArea;

		// 在三角形内生成采样点
		for (int32 PointIndex = 0; PointIndex < PointsForThisTriangle && Points.Num() < TargetPoints; ++PointIndex)
		{
			// 在三角形内生成随机点（重心坐标）
			// 使用正确的均匀分布算法
			float U = RandomStream.FRand();
			float V = RandomStream.FRand();

			// 如果点在三角形外（上半部分），翻转到下半部分
			// 这样保证点均匀分布在整个三角形内
			if (U + V > 1.0f)
			{
				U = 1.0f - U;
				V = 1.0f - V;
			}

			const float W = 1.0f - U - V;

			const int32 Index0 = IndexBuffer.GetIndex(TriangleIndex * 3 + 0);
			const int32 Index1 = IndexBuffer.GetIndex(TriangleIndex * 3 + 1);
			const int32 Index2 = IndexBuffer.GetIndex(TriangleIndex * 3 + 2);

			const FVector V0 = FVector(VertexBuffer.VertexPosition(Index0));
			const FVector V1 = FVector(VertexBuffer.VertexPosition(Index1));
			const FVector V2 = FVector(VertexBuffer.VertexPosition(Index2));

			// 重心插值：Point = W * V0 + U * V1 + V * V2
			FVector LocalPoint = V0 * W + V1 * U + V2 * V;

			// 应用变换
			FVector WorldPoint = Transform.TransformPosition(LocalPoint);
			Points.Add(WorldPoint);
		}
	}

	UE_LOG(LogPointSampling, Log, TEXT("[网格采样] 完成，生成 %d 个点"), Points.Num());

	// 去除重复点（容差设为很小，因为是精确采样）
	if (Points.Num() > 1)
	{
		int32 OriginalCount, RemovedCount;
		FPointDeduplicationHelper::RemoveDuplicatePointsWithStats(
			Points,
			0.1f,  // 小容差
			OriginalCount,
			RemovedCount
		);

		if (RemovedCount > 0)
		{
			UE_LOG(LogPointSampling, Verbose, TEXT("[网格采样] 去重: %d -> %d (移除 %d)"),
				OriginalCount, Points.Num(), RemovedCount);
		}
	}

	return Points;
}

/**
 * 生成网格边界顶点
 */
TArray<FVector> FMeshSamplingHelper::GenerateBoundaryVertices(
	const FStaticMeshLODResources& LOD,
	const FTransform& Transform,
	int32 MaxPoints)
{
	TArray<FVector> Points;

	const FPositionVertexBuffer& VertexBuffer = LOD.VertexBuffers.PositionVertexBuffer;
	const FRawStaticIndexBuffer& IndexBuffer = LOD.IndexBuffer;

	if (VertexBuffer.GetNumVertices() == 0 || IndexBuffer.GetNumIndices() == 0)
	{
		return Points;
	}

	const int32 NumVertices = VertexBuffer.GetNumVertices();
	const int32 NumTriangles = IndexBuffer.GetNumIndices() / 3;

	// 使用边计数来识别边界边
	TMap<TPair<int32, int32>, int32> EdgeUsageCount;

	// 遍历所有三角形，统计每条边的使用次数
	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
	{
		const int32 I0 = IndexBuffer.GetIndex(TriangleIndex * 3 + 0);
		const int32 I1 = IndexBuffer.GetIndex(TriangleIndex * 3 + 1);
		const int32 I2 = IndexBuffer.GetIndex(TriangleIndex * 3 + 2);

		// 三条边（使用排序的顶点索引作为键）
		TArray<TPair<int32, int32>> Edges = {
			TPair<int32, int32>(FMath::Min(I0, I1), FMath::Max(I0, I1)),
			TPair<int32, int32>(FMath::Min(I1, I2), FMath::Max(I1, I2)),
			TPair<int32, int32>(FMath::Min(I2, I0), FMath::Max(I2, I0))
		};

		for (const auto& Edge : Edges)
		{
			EdgeUsageCount.FindOrAdd(Edge)++;
		}
	}

	// 找出边界顶点（连接到边界边的顶点）
	TSet<int32> BoundaryVertexIndices;

	for (const auto& EdgeCount : EdgeUsageCount)
	{
		if (EdgeCount.Value == 1) // 边界边只被一个三角形使用
		{
			// EdgeCount.Key 是 TPair<int32, int32>，包含边的两个顶点索引
			BoundaryVertexIndices.Add(EdgeCount.Key.Key);   // 第一个顶点索引
			BoundaryVertexIndices.Add(EdgeCount.Key.Value); // 第二个顶点索引
		}
	}

	// 采样边界顶点
	const int32 MaxBoundaryPoints = (MaxPoints > 0) ? MaxPoints : BoundaryVertexIndices.Num();
	Points.Reserve(MaxBoundaryPoints);

	UE_LOG(LogPointSampling, Log, TEXT("[网格采样] 找到 %d 个边界顶点，采样 %d 个"),
		BoundaryVertexIndices.Num(), MaxBoundaryPoints);

	// 简单采样：均匀采样边界顶点
	TArray<int32> BoundaryVertices(BoundaryVertexIndices.Array());
	const int32 Step = FMath::Max(1, BoundaryVertices.Num() / MaxBoundaryPoints);

	for (int32 i = 0; i < BoundaryVertices.Num() && Points.Num() < MaxBoundaryPoints; i += Step)
	{
		const int32 VertexIndex = BoundaryVertices[i];
		const FVector LocalPoint = FVector(VertexBuffer.VertexPosition(VertexIndex));
		const FVector WorldPoint = Transform.TransformPosition(LocalPoint);
		Points.Add(WorldPoint);
	}

	UE_LOG(LogPointSampling, Log, TEXT("[网格采样] 生成 %d 个边界顶点"), Points.Num());

	return Points;
}

TArray<FVector> FMeshSamplingHelper::GenerateFromSkeletalSockets(
	USkeletalMesh* SkeletalMesh,
	const FTransform& Transform,
	const FString& SocketNamePrefix)
{
	TArray<FVector> Points;

	if (!SkeletalMesh)
	{
		return Points;
	}

	// 获取所有插槽
	const TArray<USkeletalMeshSocket*>& Sockets = SkeletalMesh->GetMeshOnlySocketList();

	for (USkeletalMeshSocket* Socket : Sockets)
	{
		if (!Socket)
		{
			continue;
		}

		// 如果指定了前缀，检查插槽名称是否匹配
		if (!SocketNamePrefix.IsEmpty())
		{
			if (!Socket->SocketName.ToString().StartsWith(SocketNamePrefix))
			{
				continue;
			}
		}

		// 获取插槽的相对位置和旋转
		FVector SocketLocation = Socket->RelativeLocation;
		FVector WorldPosition = Transform.TransformPosition(SocketLocation);

		Points.Add(WorldPosition);
	}

	return Points;
}
