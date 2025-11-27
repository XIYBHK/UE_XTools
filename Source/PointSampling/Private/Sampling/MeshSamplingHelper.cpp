/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "MeshSamplingHelper.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "StaticMeshResources.h"

TArray<FVector> FMeshSamplingHelper::GenerateFromStaticMesh(
	UStaticMesh* StaticMesh,
	const FTransform& Transform,
	int32 LODLevel,
	bool bBoundaryVerticesOnly)
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
	Points.Reserve(NumVertices);

	for (uint32 i = 0; i < NumVertices; ++i)
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
		FRotator SocketRotation = Socket->RelativeRotation;

		// 应用插槽变换和外部变换
		FTransform SocketTransform(SocketRotation, SocketLocation);
		FVector WorldPosition = Transform.TransformPosition(SocketLocation);

		Points.Add(WorldPosition);
	}

	return Points;
}
