// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CollisionTools/X_CoACDAdapter.h" // 定义 FCoACD_Mesh / FCoACD_MeshArray

// 前向声明，避免不必要包含
class UStaticMesh;
struct FRawMesh;
// 由 X_CoACDAdapter.h 提供，已包含

/** DLL 输入承载：TArray 承载真实数据，MeshView 仅指向其内存 */
struct FCoACDInputBuffers
{
    TArray<double> Vertices;   // size = NumVerts * 3
    TArray<int> Indices;       // size = NumWedges
    FCoACD_Mesh MeshView{};    // 指向上面两块内存
};

/** 将 FRawMesh 构建为 DLL 需要的轻量输入视图 */
void BuildInputFromRawMesh(const FRawMesh& Raw, FCoACDInputBuffers& Out);

/** 压缩未使用顶点，确保顶点索引连续合法 */
void CompactUnusedVertices(FRawMesh& RawMesh);

/** 根据材质槽ID移除三角面及相关楔数据 */
bool DeleteWedgesByMaterialIDs(FRawMesh& RawMesh, const TArray<int32>& MaterialIDs, bool bCleanUpVertexPositions);

/** 依据关键词（槽名/材质名/路径/索引）过滤 RawMesh 三角面 */
void FilterRawMeshByKeywords(UStaticMesh* StaticMesh, FRawMesh& Raw, const TArray<FString>& Keywords);


