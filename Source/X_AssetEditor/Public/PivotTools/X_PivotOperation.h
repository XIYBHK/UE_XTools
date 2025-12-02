/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "PivotTools/X_PivotTypes.h"

// 前向声明
class UStaticMesh;

/**
 * Pivot 单资产操作类
 * 职责：
 * - 封装对单个 StaticMesh 的 Pivot 修改逻辑
 * - 处理顶点数据、碰撞几何、Sockets 等
 * - 实现实际的烘焙变换逻辑
 */
class X_ASSETEDITOR_API FX_PivotOperation
{
public:
    /**
     * 构造函数
     * @param InTargetMesh 目标静态网格体
     */
    explicit FX_PivotOperation(UStaticMesh* InTargetMesh);

    /**
     * 设置目标 Pivot 位置并执行
     * @param BoundsPoint 目标边界盒位置
     * @param OutErrorMessage 输出错误消息
     * @return 是否成功
     */
    bool Execute(EPivotBoundsPoint BoundsPoint, FString& OutErrorMessage);

    /**
     * 设置自定义偏移（高级用法）
     * @param CustomOffset 自定义偏移向量
     * @param OutErrorMessage 输出错误消息
     * @return 是否成功
     */
    bool Execute(const FVector& CustomOffset, FString& OutErrorMessage);

private:
    /** 目标静态网格体 */
    UStaticMesh* TargetMesh;

    /**
     * 计算网格边界盒（考虑所有 LOD）
     * @return 边界盒
     */
    FBox CalculateMeshBounds();

    /**
     * 修改所有 LOD 的顶点位置
     * @param Offset 偏移向量
     * @return 是否成功
     */
    bool TransformVertices(const FVector& Offset);

    /**
     * 修改简单碰撞
     * @param Offset 偏移向量
     * @return 是否成功
     */
    bool TransformSimpleCollision(const FVector& Offset);

    /**
     * 修改复杂碰撞（Convex、TriMesh）
     * @param Offset 偏移向量
     * @return 是否成功
     */
    bool TransformComplexCollision(const FVector& Offset);

    /**
     * 修改 Sockets 位置
     * @param Offset 偏移向量
     * @return 是否成功
     */
    bool TransformSockets(const FVector& Offset);

    /**
     * 重建网格
     * @return 是否成功
     */
    bool RebuildMesh();

    /**
     * 记录 Undo 事务
     * @param TransactionName 事务名称
     */
    void BeginUndoTransaction(const FString& TransactionName);

    /**
     * 结束 Undo 事务
     */
    void EndUndoTransaction();
};
