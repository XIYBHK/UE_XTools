// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
// Public头尽量轻：用前向声明替代重头
class UStaticMesh;
struct FAssetData;
enum ECollisionTraceFlag : int;
#include "X_CollisionManager.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogX_CollisionManager, Log, All);

/**
 * 碰撞复杂度设置枚举
 * 对应UE中的ECollisionTraceFlag
 */
UENUM(BlueprintType)
enum class EX_CollisionComplexity : uint8
{
    /** 使用项目默认设置 */
    UseDefault          UMETA(DisplayName = "项目默认"),
    
    /** 简单与复杂碰撞都使用 */
    UseSimpleAndComplex UMETA(DisplayName = "简单与复杂"),
    
    /** 将简单碰撞用作复杂碰撞 */
    UseSimpleAsComplex  UMETA(DisplayName = "将简单碰撞用作复杂碰撞"),
    
    /** 将复杂碰撞用作简单碰撞 */
    UseComplexAsSimple  UMETA(DisplayName = "将复杂碰撞用作简单碰撞")
};

/**
 * 碰撞操作结果结构体
 */
USTRUCT(BlueprintType)
struct FX_CollisionOperationResult
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

    /** 操作是否完全成功 */
    bool IsSuccess() const { return FailureCount == 0; }

    /** 获取总处理数量 */
    int32 GetTotalCount() const { return SuccessCount + FailureCount + SkippedCount; }
};

/**
 * 静态网格体碰撞管理器
 * 提供批量碰撞操作功能
 */
class X_ASSETEDITOR_API FX_CollisionManager
{
public:
    /**
     * 移除选中静态网格体的碰撞
     * @param SelectedAssets 选中的资产列表
     * @return 操作结果
     */
    static FX_CollisionOperationResult RemoveCollisionFromAssets(const TArray<FAssetData>& SelectedAssets);

    /**
     * 为选中静态网格体添加简单凸包碰撞
     * @param SelectedAssets 选中的资产列表
     * @return 操作结果
     */
    static FX_CollisionOperationResult AddConvexCollisionToAssets(const TArray<FAssetData>& SelectedAssets);

    /**
     * 批量添加UE原生简单碰撞（盒、球、胶囊、KDOP等）
     * 形状类型沿用UE编辑器脚本的 `EScriptCollisionShapeType`
     */
    static FX_CollisionOperationResult AddSimpleCollisionToAssets(const TArray<FAssetData>& SelectedAssets, uint8 ShapeType /*EScriptCollisionShapeType*/);

    /**
     * 批量设置静态网格体的碰撞复杂度
     * @param SelectedAssets 选中的资产列表
     * @param ComplexityType 碰撞复杂度类型
     * @return 操作结果
     */
    static FX_CollisionOperationResult SetCollisionComplexity(const TArray<FAssetData>& SelectedAssets, EX_CollisionComplexity ComplexityType);

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
     * 将EX_CollisionComplexity转换为UE的ECollisionTraceFlag
     * @param ComplexityType 自定义碰撞复杂度类型
     * @return UE的碰撞追踪标志
     */
    static ECollisionTraceFlag ConvertToCollisionTraceFlag(EX_CollisionComplexity ComplexityType);

    /**
     * 显示操作结果通知
     * @param Result 操作结果
     * @param OperationName 操作名称
     */
    static void ShowOperationResult(const FX_CollisionOperationResult& Result, const FString& OperationName);

    /**
     * 移除单个静态网格体的碰撞
     * @param StaticMesh 静态网格体
     * @return 是否成功
     */
    static bool RemoveCollisionFromMesh(UStaticMesh* StaticMesh);

    /**
     * 为单个静态网格体添加凸包碰撞
     * @param StaticMesh 静态网格体
     * @return 是否成功
     */
    static bool AddConvexCollisionToMesh(UStaticMesh* StaticMesh);

    /**
     * 设置单个静态网格体的碰撞复杂度
     * @param StaticMesh 静态网格体
     * @param TraceFlag 碰撞追踪标志
     * @return 是否成功
     */
    static bool SetMeshCollisionComplexity(UStaticMesh* StaticMesh, ECollisionTraceFlag TraceFlag);

private:

    /**
     * 保存静态网格体的修改
     * @param StaticMesh 静态网格体
     */
    static void SaveStaticMeshChanges(UStaticMesh* StaticMesh);

    /**
     * 记录操作日志
     * @param Message 日志消息
     * @param bIsError 是否为错误日志
     */
    static void LogOperation(const FString& Message, bool bIsError = false);
};
