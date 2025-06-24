// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/StaticMesh.h"
#include "CollisionTools/X_CollisionManager.h"
#include "X_CollisionBlueprintLibrary.generated.h"

/**
 * 碰撞管理蓝图函数库
 * 提供蓝图可调用的碰撞管理功能
 */
UCLASS()
class X_ASSETEDITOR_API UX_CollisionBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * 移除静态网格体的碰撞
     * @param StaticMesh 静态网格体
     * @return 是否成功
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|碰撞管理", meta = (DisplayName = "移除静态网格体碰撞"))
    static bool RemoveStaticMeshCollision(UStaticMesh* StaticMesh);

    /**
     * 为静态网格体添加凸包碰撞
     * @param StaticMesh 静态网格体
     * @return 是否成功
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|碰撞管理", meta = (DisplayName = "添加静态网格体凸包碰撞"))
    static bool AddStaticMeshConvexCollision(UStaticMesh* StaticMesh);

    /**
     * 设置静态网格体的碰撞复杂度
     * @param StaticMesh 静态网格体
     * @param ComplexityType 碰撞复杂度类型
     * @return 是否成功
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|碰撞管理", meta = (DisplayName = "设置静态网格体碰撞复杂度"))
    static bool SetStaticMeshCollisionComplexity(UStaticMesh* StaticMesh, EX_CollisionComplexity ComplexityType);

    /**
     * 批量移除静态网格体数组的碰撞
     * @param StaticMeshes 静态网格体数组
     * @return 操作结果
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|碰撞管理", meta = (DisplayName = "批量移除静态网格体碰撞"))
    static FX_CollisionOperationResult BatchRemoveStaticMeshCollision(const TArray<UStaticMesh*>& StaticMeshes);

    /**
     * 批量为静态网格体数组添加凸包碰撞
     * @param StaticMeshes 静态网格体数组
     * @return 操作结果
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|碰撞管理", meta = (DisplayName = "批量添加静态网格体凸包碰撞"))
    static FX_CollisionOperationResult BatchAddStaticMeshConvexCollision(const TArray<UStaticMesh*>& StaticMeshes);

    /**
     * 批量设置静态网格体数组的碰撞复杂度
     * @param StaticMeshes 静态网格体数组
     * @param ComplexityType 碰撞复杂度类型
     * @return 操作结果
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|碰撞管理", meta = (DisplayName = "批量设置静态网格体碰撞复杂度"))
    static FX_CollisionOperationResult BatchSetStaticMeshCollisionComplexity(const TArray<UStaticMesh*>& StaticMeshes, EX_CollisionComplexity ComplexityType);

    /**
     * 获取静态网格体当前的碰撞复杂度
     * @param StaticMesh 静态网格体
     * @return 当前的碰撞复杂度
     */
    UFUNCTION(BlueprintPure, Category = "XTools|碰撞管理", meta = (DisplayName = "获取静态网格体碰撞复杂度"))
    static EX_CollisionComplexity GetStaticMeshCollisionComplexity(UStaticMesh* StaticMesh);

    /**
     * 检查静态网格体是否有简单碰撞
     * @param StaticMesh 静态网格体
     * @return 是否有简单碰撞
     */
    UFUNCTION(BlueprintPure, Category = "XTools|碰撞管理", meta = (DisplayName = "静态网格体是否有简单碰撞"))
    static bool DoesStaticMeshHaveSimpleCollision(UStaticMesh* StaticMesh);

    /**
     * 检查静态网格体是否有复杂碰撞
     * @param StaticMesh 静态网格体
     * @return 是否有复杂碰撞
     */
    UFUNCTION(BlueprintPure, Category = "XTools|碰撞管理", meta = (DisplayName = "静态网格体是否有复杂碰撞"))
    static bool DoesStaticMeshHaveComplexCollision(UStaticMesh* StaticMesh);

    /**
     * 获取静态网格体的简单碰撞形状数量
     * @param StaticMesh 静态网格体
     * @return 简单碰撞形状数量
     */
    UFUNCTION(BlueprintPure, Category = "XTools|碰撞管理", meta = (DisplayName = "获取静态网格体简单碰撞形状数量"))
    static int32 GetStaticMeshSimpleCollisionCount(UStaticMesh* StaticMesh);

    /**
     * 获取碰撞复杂度的显示名称
     * @param ComplexityType 碰撞复杂度类型
     * @return 显示名称
     */
    UFUNCTION(BlueprintPure, Category = "XTools|碰撞管理", meta = (DisplayName = "获取碰撞复杂度显示名称"))
    static FString GetCollisionComplexityDisplayName(EX_CollisionComplexity ComplexityType);

    /**
     * 将UE的碰撞追踪标志转换为自定义的碰撞复杂度类型
     * @param TraceFlag UE的碰撞追踪标志
     * @return 自定义的碰撞复杂度类型
     */
    UFUNCTION(BlueprintPure, Category = "XTools|碰撞管理", meta = (DisplayName = "转换碰撞追踪标志"))
    static EX_CollisionComplexity ConvertFromCollisionTraceFlag(TEnumAsByte<ECollisionTraceFlag> TraceFlag);

private:
    /**
     * 将静态网格体数组转换为资产数据数组
     * @param StaticMeshes 静态网格体数组
     * @return 资产数据数组
     */
    static TArray<FAssetData> ConvertToAssetDataArray(const TArray<UStaticMesh*>& StaticMeshes);
};
