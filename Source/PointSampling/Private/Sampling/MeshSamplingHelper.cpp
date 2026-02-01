/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "MeshSamplingHelper.h"
#include "PointSamplingTypes.h"
#include "Engine/StaticMesh.h"
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

	FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
	if (!RenderData)
	{
		return Points;
	}

	if (!RenderData->LODResources.IsValidIndex(LODLevel))
	{
		LODLevel = 0;
		if (!RenderData->LODResources.IsValidIndex(LODLevel))
		{
			return Points;
		}
	}

	FStaticMeshLODResources& LOD = RenderData->LODResources[LODLevel];

	return bBoundaryVerticesOnly
		? GenerateBoundaryVertices(LOD, Transform, MaxPoints)
		: GenerateFromMeshTriangles(LOD, Transform, MaxPoints);
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

	const int32 NumTriangles = IndexBuffer.GetNumIndices() / 3;

	TArray<float> TriangleAreas;
	float TotalArea = 0.0f;
	TriangleAreas.Reserve(NumTriangles);

	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
	{
		const int32 Index0 = IndexBuffer.GetIndex(TriangleIndex * 3);
		const int32 Index1 = IndexBuffer.GetIndex(TriangleIndex * 3 + 1);
		const int32 Index2 = IndexBuffer.GetIndex(TriangleIndex * 3 + 2);

		// 修复：添加索引边界检查
		const int32 MaxVertexIndex = VertexBuffer.GetNumVertices() - 1;
		if (Index0 > MaxVertexIndex || Index1 > MaxVertexIndex || Index2 > MaxVertexIndex)
		{
			UE_LOG(LogPointSampling, Warning, TEXT("[网格采样] 三角形 %d 包含无效顶点索引: %d, %d, %d (最大: %d)"),
				TriangleIndex, Index0, Index1, Index2, MaxVertexIndex);
			continue;
		}

		const FVector V0(VertexBuffer.VertexPosition(Index0));
		const FVector V1(VertexBuffer.VertexPosition(Index1));
		const FVector V2(VertexBuffer.VertexPosition(Index2));

		const float Area = FVector::CrossProduct(V1 - V0, V2 - V0).Size() * 0.5f;

		TriangleAreas.Add(Area);
		TotalArea += Area;
	}

	if (TotalArea <= 0.0f)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[网格采样] 网格总面积为0或没有三角形"));
		return Points;
	}

	const int32 TargetPoints = (MaxPoints > 0) ? MaxPoints : FMath::Min(10000, NumTriangles * 2);
	Points.Reserve(TargetPoints);

	UE_LOG(LogPointSampling, Log, TEXT("[网格采样] 开始基于面积的采样: %d 个三角形, 总面积 %.2f, 目标点数 %d"),
		NumTriangles, TotalArea, TargetPoints);

	FRandomStream RandomStream(FMath::Rand());

	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles && Points.Num() < TargetPoints; ++TriangleIndex)
	{
		const float TriangleArea = TriangleAreas[TriangleIndex];
		if (TriangleArea <= 0.0f)
		{
			continue;
		}

		const int32 PointsForThisTriangle = FMath::Max(1, FMath::RoundToInt((TriangleArea / TotalArea) * TargetPoints));

		for (int32 PointIndex = 0; PointIndex < PointsForThisTriangle && Points.Num() < TargetPoints; ++PointIndex)
		{
			float U = RandomStream.FRand();
			float V = RandomStream.FRand();

			if (U + V > 1.0f)
			{
				U = 1.0f - U;
				V = 1.0f - V;
			}

			const float W = 1.0f - U - V;

			const int32 Index0 = IndexBuffer.GetIndex(TriangleIndex * 3);
			const int32 Index1 = IndexBuffer.GetIndex(TriangleIndex * 3 + 1);
			const int32 Index2 = IndexBuffer.GetIndex(TriangleIndex * 3 + 2);

			const FVector V0(VertexBuffer.VertexPosition(Index0));
			const FVector V1(VertexBuffer.VertexPosition(Index1));
			const FVector V2(VertexBuffer.VertexPosition(Index2));

			const FVector LocalPoint = V0 * W + V1 * U + V2 * V;
			Points.Add(Transform.TransformPosition(LocalPoint));
		}
	}

	UE_LOG(LogPointSampling, Log, TEXT("[网格采样] 完成，生成 %d 个点"), Points.Num());

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

	const int32 NumTriangles = IndexBuffer.GetNumIndices() / 3;

	TMap<TPair<int32, int32>, int32> EdgeUsageCount;

	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
	{
		const int32 I0 = IndexBuffer.GetIndex(TriangleIndex * 3);
		const int32 I1 = IndexBuffer.GetIndex(TriangleIndex * 3 + 1);
		const int32 I2 = IndexBuffer.GetIndex(TriangleIndex * 3 + 2);

		EdgeUsageCount.FindOrAdd(TPair<int32, int32>(FMath::Min(I0, I1), FMath::Max(I0, I1)))++;
		EdgeUsageCount.FindOrAdd(TPair<int32, int32>(FMath::Min(I1, I2), FMath::Max(I1, I2)))++;
		EdgeUsageCount.FindOrAdd(TPair<int32, int32>(FMath::Min(I2, I0), FMath::Max(I2, I0)))++;
	}

	TSet<int32> BoundaryVertexIndices;

	for (const auto& EdgeCount : EdgeUsageCount)
	{
		if (EdgeCount.Value == 1)
		{
			BoundaryVertexIndices.Add(EdgeCount.Key.Key);
			BoundaryVertexIndices.Add(EdgeCount.Key.Value);
		}
	}

	const int32 MaxBoundaryPoints = (MaxPoints > 0) ? MaxPoints : BoundaryVertexIndices.Num();
	Points.Reserve(MaxBoundaryPoints);

	UE_LOG(LogPointSampling, Log, TEXT("[网格采样] 找到 %d 个边界顶点，采样 %d 个"),
		BoundaryVertexIndices.Num(), MaxBoundaryPoints);

	TArray<int32> BoundaryVertices(BoundaryVertexIndices.Array());
	const int32 Step = FMath::Max(1, BoundaryVertices.Num() / MaxBoundaryPoints);

	for (int32 i = 0; i < BoundaryVertices.Num() && Points.Num() < MaxBoundaryPoints; i += Step)
	{
		const int32 VertexIndex = BoundaryVertices[i];
		const FVector LocalPoint(VertexBuffer.VertexPosition(VertexIndex));
		Points.Add(Transform.TransformPosition(LocalPoint));
	}

	UE_LOG(LogPointSampling, Log, TEXT("[网格采样] 生成 %d 个边界顶点"), Points.Num());

	return Points;
}
