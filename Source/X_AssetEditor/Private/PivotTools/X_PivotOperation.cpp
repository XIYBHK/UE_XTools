/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "PivotTools/X_PivotOperation.h"
#include "PivotTools/X_PivotManager.h"
#include "X_AssetEditor.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/ConvexElem.h"
#include "Engine/StaticMeshSocket.h"

#define LOCTEXT_NAMESPACE "X_PivotOperation"

FX_PivotOperation::FX_PivotOperation(UStaticMesh* InTargetMesh)
    : TargetMesh(InTargetMesh)
{
}

bool FX_PivotOperation::Execute(EPivotBoundsPoint BoundsPoint, FString& OutErrorMessage)
{
    if (!TargetMesh)
    {
        OutErrorMessage = TEXT("目标网格为空");
        return false;
    }

    // 计算边界盒
    FBox MeshBounds = CalculateMeshBounds();
    if (!MeshBounds.IsValid)
    {
        OutErrorMessage = TEXT("无法计算网格边界盒");
        return false;
    }

    // 计算目标点
    FVector TargetPoint = FX_PivotManager::CalculateTargetPoint(MeshBounds, BoundsPoint);
    
    // 计算偏移量（负值，因为要将顶点向相反方向移动）
    // 特殊处理世界原点：需要将边界盒中心移动到原点
    FVector Offset;
    if (BoundsPoint == EPivotBoundsPoint::WorldOrigin)
    {
        // 世界原点：将当前边界盒中心移动到原点
        Offset = -MeshBounds.GetCenter();
        UE_LOG(LogX_PivotTools, Log, TEXT("网格 %s [世界原点模式]: 边界盒中心=(%s), 偏移量=(%s)"),
            *TargetMesh->GetName(),
            *MeshBounds.GetCenter().ToString(),
            *Offset.ToString());
    }
    else
    {
        // 其他位置：将目标点移动到原点
        Offset = -TargetPoint;
        UE_LOG(LogX_PivotTools, Log, TEXT("网格 %s: 边界盒中心=(%s), 目标点=(%s), 偏移量=(%s)"),
            *TargetMesh->GetName(),
            *MeshBounds.GetCenter().ToString(),
            *TargetPoint.ToString(),
            *Offset.ToString());
    }

    return Execute(Offset, OutErrorMessage);
}

bool FX_PivotOperation::Execute(const FVector& CustomOffset, FString& OutErrorMessage)
{
    if (!TargetMesh)
    {
        OutErrorMessage = TEXT("目标网格为空");
        return false;
    }

    // 开始 Undo 事务
    BeginUndoTransaction(TEXT("设置网格 Pivot"));

    // 变换顶点
    if (!TransformVertices(CustomOffset))
    {
        OutErrorMessage = TEXT("变换顶点失败");
        EndUndoTransaction();
        return false;
    }

    // 变换碰撞
    if (!TransformSimpleCollision(CustomOffset))
    {
        UE_LOG(LogX_PivotTools, Warning, TEXT("变换简单碰撞失败，继续处理"));
    }

    if (!TransformComplexCollision(CustomOffset))
    {
        UE_LOG(LogX_PivotTools, Warning, TEXT("变换复杂碰撞失败，继续处理"));
    }

    // 变换 Sockets
    if (!TransformSockets(CustomOffset))
    {
        UE_LOG(LogX_PivotTools, Warning, TEXT("变换 Sockets 失败，继续处理"));
    }

    // 重建网格
    if (!RebuildMesh())
    {
        OutErrorMessage = TEXT("重建网格失败");
        EndUndoTransaction();
        return false;
    }

    // 结束 Undo 事务
    EndUndoTransaction();

    return true;
}

FBox FX_PivotOperation::CalculateMeshBounds()
{
    if (!TargetMesh)
    {
        return FBox(ForceInit);
    }

    FBox TotalBounds(ForceInit);

    // 遍历所有 LOD
    const int32 NumLODs = TargetMesh->GetNumLODs();
    for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
    {
        const FMeshDescription* MeshDesc = TargetMesh->GetMeshDescription(LODIndex);
        if (!MeshDesc)
        {
            continue;
        }

        // 获取顶点位置属性
        FStaticMeshConstAttributes Attributes(*MeshDesc);
        TVertexAttributesConstRef<FVector3f> Positions = Attributes.GetVertexPositions();

        // 遍历所有顶点
        for (FVertexID VertexID : MeshDesc->Vertices().GetElementIDs())
        {
            TotalBounds += (FVector)Positions[VertexID];
        }
    }

    // 如果没有有效顶点，使用默认边界盒
    if (!TotalBounds.IsValid)
    {
        TotalBounds = TargetMesh->GetBoundingBox();
    }

    return TotalBounds;
}

bool FX_PivotOperation::TransformVertices(const FVector& Offset)
{
    if (!TargetMesh)
    {
        return false;
    }

    const int32 NumLODs = TargetMesh->GetNumLODs();
    for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
    {
        FMeshDescription* MeshDesc = TargetMesh->GetMeshDescription(LODIndex);
        if (!MeshDesc)
        {
            continue;
        }

        // 获取顶点位置属性
        FStaticMeshAttributes Attributes(*MeshDesc);
        TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();

        // 应用偏移到每个顶点
        for (FVertexID VertexID : MeshDesc->Vertices().GetElementIDs())
        {
            Positions[VertexID] += (FVector3f)Offset;
        }

        // 提交修改
        TargetMesh->CommitMeshDescription(LODIndex);
    }

    return true;
}

bool FX_PivotOperation::TransformSimpleCollision(const FVector& Offset)
{
    if (!TargetMesh)
    {
        return false;
    }

    UBodySetup* BodySetup = TargetMesh->GetBodySetup();
    if (!BodySetup)
    {
        // 没有碰撞，不算错误
        return true;
    }

    // 盒体碰撞
    for (FKBoxElem& Box : BodySetup->AggGeom.BoxElems)
    {
        Box.Center += Offset;
    }

    // 球体碰撞
    for (FKSphereElem& Sphere : BodySetup->AggGeom.SphereElems)
    {
        Sphere.Center += Offset;
    }

    // 胶囊碰撞
    for (FKSphylElem& Capsule : BodySetup->AggGeom.SphylElems)
    {
        Capsule.Center += Offset;
    }

    // Convex 碰撞
    for (FKConvexElem& Convex : BodySetup->AggGeom.ConvexElems)
    {
        FTransform ConvexTransform = Convex.GetTransform();
        ConvexTransform.SetTranslation(ConvexTransform.GetTranslation() + Offset);
        Convex.SetTransform(ConvexTransform);
    }

    // 标记需要重建碰撞
    BodySetup->InvalidatePhysicsData();

    return true;
}

bool FX_PivotOperation::TransformComplexCollision(const FVector& Offset)
{
    if (!TargetMesh)
    {
        return false;
    }

    UBodySetup* BodySetup = TargetMesh->GetBodySetup();
    if (!BodySetup)
    {
        return true;
    }

    // Convex 碰撞已在 TransformSimpleCollision 中处理
    // 这里可以处理其他复杂碰撞类型（如果需要）

    return true;
}

bool FX_PivotOperation::TransformSockets(const FVector& Offset)
{
    if (!TargetMesh)
    {
        return false;
    }

    for (UStaticMeshSocket* Socket : TargetMesh->Sockets)
    {
        if (Socket)
        {
            Socket->RelativeLocation += Offset;
        }
    }

    return true;
}

bool FX_PivotOperation::RebuildMesh()
{
    if (!TargetMesh)
    {
        return false;
    }

    // 重建网格
    TargetMesh->Build();

    // 标记包为脏
    TargetMesh->MarkPackageDirty();

    return true;
}

void FX_PivotOperation::BeginUndoTransaction(const FString& TransactionName)
{
    if (TargetMesh)
    {
        TargetMesh->Modify();
    }
}

void FX_PivotOperation::EndUndoTransaction()
{
    if (TargetMesh)
    {
        TargetMesh->MarkPackageDirty();
    }
}

#undef LOCTEXT_NAMESPACE
