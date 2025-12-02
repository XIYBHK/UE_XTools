/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "X_PivotTypes.generated.h"

/**
 * 枢轴点位置枚举
 * 定义边界盒上的标准位置点
 */
UENUM(BlueprintType)
enum class EPivotBoundsPoint : uint8
{
    /** 边界盒中心 */
    Center      UMETA(DisplayName = "边界盒中心"),
    
    /** 底部中心 */
    Bottom      UMETA(DisplayName = "底部中心"),
    
    /** 顶部中心 */
    Top         UMETA(DisplayName = "顶部中心"),
    
    /** 左面中心 */
    Left        UMETA(DisplayName = "左面中心"),
    
    /** 右面中心 */
    Right       UMETA(DisplayName = "右面中心"),
    
    /** 前面中心 */
    Front       UMETA(DisplayName = "前面中心"),
    
    /** 后面中心 */
    Back        UMETA(DisplayName = "后面中心"),
    
    /** 世界原点 (0,0,0) */
    WorldOrigin UMETA(DisplayName = "世界原点")
};

/**
 * Pivot 操作结果结构体
 */
USTRUCT(BlueprintType)
struct FX_PivotOperationResult
{
    GENERATED_BODY()

    /** 成功处理的资产数量 */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    int32 SuccessCount = 0;

    /** 失败的资产数量 */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    int32 FailureCount = 0;

    /** 跳过的资产数量（非静态网格体） */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    int32 SkippedCount = 0;

    /** 错误信息列表 */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    TArray<FString> ErrorMessages;

    /** 成功消息列表 */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    TArray<FString> SuccessMessages;

    /** 操作是否完全成功 */
    bool IsSuccess() const { return FailureCount == 0; }

    /** 获取总处理数量 */
    int32 GetTotalCount() const { return SuccessCount + FailureCount + SkippedCount; }
};

/**
 * Pivot 快照结构体
 * 用于记录和还原网格的 Pivot 状态（相对位置）
 */
USTRUCT(BlueprintType)
struct FX_PivotSnapshot
{
    GENERATED_BODY()

    /** 网格资产路径 */
    UPROPERTY()
    FSoftObjectPath MeshPath;

    /** 记录时的边界盒中心（本地空间） */
    UPROPERTY()
    FVector BoundsCenter = FVector::ZeroVector;

    /** 记录时间戳 */
    UPROPERTY()
    FDateTime Timestamp;

    /** 是否有效 */
    bool IsValid() const { return !MeshPath.IsNull(); }
};
