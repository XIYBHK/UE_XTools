/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "PivotTools/X_PivotTypes.h"

// 前向声明
class UStaticMesh;
class AActor;
struct FAssetData;

DECLARE_LOG_CATEGORY_EXTERN(LogX_PivotTools, Log, All);

/**
 * Pivot 管理器
 * 负责批量修改静态网格体的枢轴点
 * 职责：
 * - 提供批量操作的静态方法入口
 * - 遍历选中资产，调用单资产操作
 * - 收集并返回操作结果
 * - 显示进度通知
 */
class X_ASSETEDITOR_API FX_PivotManager
{
public:
    /**
     * 批量设置 Pivot 到边界盒指定位置（资产浏览器场景）
     * @param SelectedAssets 选中的资产列表
     * @param BoundsPoint 目标边界盒位置
     * @return 操作结果
     */
    static FX_PivotOperationResult SetPivotForAssets(
        const TArray<FAssetData>& SelectedAssets,
        EPivotBoundsPoint BoundsPoint);

    /**
     * 批量设置 Pivot 到边界盒指定位置（关卡编辑器场景）
     * @param SelectedActors 选中的 Actor 列表
     * @param BoundsPoint 目标边界盒位置
     * @return 操作结果
     */
    static FX_PivotOperationResult SetPivotForActors(
        const TArray<AActor*>& SelectedActors,
        EPivotBoundsPoint BoundsPoint);

    /**
     * 快速设置到中心（常用快捷方式）
     * @param SelectedAssets 选中的资产列表
     * @return 操作结果
     */
    static FX_PivotOperationResult SetPivotToCenterForAssets(const TArray<FAssetData>& SelectedAssets);

    /**
     * 快速设置到中心（关卡编辑器场景）
     * @param SelectedActors 选中的 Actor 列表
     * @return 操作结果
     */
    static FX_PivotOperationResult SetPivotToCenterForActors(const TArray<AActor*>& SelectedActors);

    /**
     * 检查资产是否为静态网格体
     * @param AssetData 资产数据
     * @return 是否为静态网格体
     */
    static bool IsStaticMeshAsset(const FAssetData& AssetData);

    /**
     * 获取静态网格体资产对象
     * @param AssetData 资产数据
     * @return 静态网格体对象，如果不是静态网格体则返回nullptr
     */
    static UStaticMesh* GetStaticMeshFromAsset(const FAssetData& AssetData);

    /**
     * 显示操作结果通知
     * @param Result 操作结果
     * @param OperationName 操作名称
     */
    static void ShowOperationResult(const FX_PivotOperationResult& Result, const FString& OperationName);

    /**
     * 计算边界盒上的目标点
     * @param BoundingBox 边界盒
     * @param BoundsPoint 目标位置枚举
     * @return 目标点坐标
     */
    static FVector CalculateTargetPoint(
        const FBox& BoundingBox,
        EPivotBoundsPoint BoundsPoint);

    /**
     * 记录选中资产的 Pivot 状态
     * @param SelectedAssets 选中的资产列表
     * @return 操作结果
     */
    static FX_PivotOperationResult RecordPivotSnapshots(const TArray<FAssetData>& SelectedAssets);

    /**
     * 还原之前记录的 Pivot 状态
     * @param SelectedAssets 选中的资产列表
     * @return 操作结果
     */
    static FX_PivotOperationResult RestorePivotSnapshots(const TArray<FAssetData>& SelectedAssets);

    /**
     * 还原之前记录的 Pivot 状态（Actor版本，保持世界位置）
     * @param SelectedActors 选中的 Actor 列表
     * @return 操作结果
     */
    static FX_PivotOperationResult RestorePivotSnapshotsForActors(const TArray<AActor*>& SelectedActors);

    /**
     * 清除所有 Pivot 记录
     */
    static void ClearPivotSnapshots();

    /**
     * 获取当前记录的快照数量
     */
    static int32 GetSnapshotCount();

    /**
     * 保存快照到磁盘
     * @return 是否成功
     */
    static bool SaveSnapshotsToDisk();

    /**
     * 从磁盘加载快照
     * @return 是否成功
     */
    static bool LoadSnapshotsFromDisk();

    /**
     * 获取快照文件路径
     */
    static FString GetSnapshotFilePath();

private:
    /**
     * 设置单个 StaticMesh 的 Pivot
     * @param StaticMesh 静态网格体
     * @param BoundsPoint 目标边界盒位置
     * @param OutErrorMessage 输出错误消息
     * @return 是否成功
     */
    static bool SetPivotForStaticMesh(
        UStaticMesh* StaticMesh,
        EPivotBoundsPoint BoundsPoint,
        FString& OutErrorMessage);

    /**
     * 设置单个 Actor 的 Pivot（保持世界位置）
     * @param SMActor 静态网格体 Actor
     * @param BoundsPoint 目标边界盒位置
     * @param OutErrorMessage 输出错误消息
     * @return 是否成功
     */
    static bool SetPivotForStaticMeshActor(
        AActor* SMActor,
        EPivotBoundsPoint BoundsPoint,
        FString& OutErrorMessage);

    /**
     * 记录操作日志
     * @param Message 日志消息
     * @param bIsError 是否为错误日志
     */
    static void LogOperation(const FString& Message, bool bIsError = false);

    /** Pivot 快照存储（静态成员） */
    static TMap<FSoftObjectPath, FX_PivotSnapshot> PivotSnapshots;
};
