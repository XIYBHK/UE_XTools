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
	const FPositionVertexBuffer& VertexBuffer = LOD.VertexBuffers.PositionVertexBuffer;

	// 获取所有顶点位置
	uint32 NumVertices = VertexBuffer.GetNumVertices();

	// 智能降采样：使用用户指定的 MaxPoints，如果未指定则使用防御性限制（10万）
	const uint32 MaxVertices = (MaxPoints > 0) ? static_cast<uint32>(MaxPoints) : 100000;
	uint32 Step = 1;

	if (NumVertices > MaxVertices)
	{
		// 计算采样步长以达到目标点数（采样前降采样，保持形状分布）
		Step = FMath::CeilToInt((float)NumVertices / MaxVertices);

		UE_LOG(LogPointSampling, Log,
			TEXT("[静态网格采样] 顶点数 %d 超过目标 %d，每隔 %d 个顶点采样一次（预期采样 ~%d 个点）"),
			NumVertices, MaxVertices, Step, NumVertices / Step);
	}

	Points.Reserve(FMath::Min(NumVertices, MaxVertices));

	for (uint32 i = 0; i < NumVertices; i += Step)
	{
		// 获取顶点位置（局部坐标）
		FVector VertexPosition = FVector(VertexBuffer.VertexPosition(i));

		// 应用变换
		FVector WorldPosition = Transform.TransformPosition(VertexPosition);
		Points.Add(WorldPosition);
	}

	// TODO: 边界顶点过滤（bBoundaryVerticesOnly）
	// 这需要分析网格拓扑，找出只被一个三角形使用的顶点
	// 由于实现复杂度较高，暂时保留所有顶点

	// 去除重复/重叠点位（静态模型通常有大量重复顶点）
	if (Points.Num() > 0)
	{
		int32 OriginalCount, RemovedCount;
		FPointDeduplicationHelper::RemoveDuplicatePointsWithStats(
			Points,
			1.0f,  // 容差：1cm（UE单位）
			OriginalCount,
			RemovedCount
		);

		if (RemovedCount > 0)
		{
			UE_LOG(LogPointSampling, Log,
				TEXT("[静态网格采样] 去重：原始 %d 个点 -> 去除 %d 个重复点 -> 剩余 %d 个点"),
				OriginalCount, RemovedCount, Points.Num());
		}
	}

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
