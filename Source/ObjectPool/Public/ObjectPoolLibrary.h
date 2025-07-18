// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// ✅ 对象池模块依赖
#include "ObjectPoolTypes.h"
#include "ObjectPoolSubsystem.h"

// ✅ 生成的头文件必须放在最后
#include "ObjectPoolLibrary.generated.h"

/**
 * 对象池蓝图函数库
 * 
 * 设计理念：
 * - 提供静态访问方法，无需手动获取子系统
 * - 简化蓝图使用流程，一步到位的便捷接口
 * - 自动处理WorldContext，减少用户操作
 * - 提供批量操作和高级功能的快捷方式
 * 
 * 核心优势：
 * - 静态函数调用，蓝图中更简洁
 * - 自动错误处理和回退机制
 * - 丰富的便捷接口和批量操作
 * - 完整的蓝图元数据和工具提示
 */
UCLASS(BlueprintType, meta = (
    DisplayName = "对象池函数库",
    ToolTip = "提供对象池功能的静态蓝图函数库。包含便捷的静态方法、批量操作、生命周期管理等功能，无需手动获取子系统。",
    Keywords = "对象池,函数库,静态方法,批量操作,便捷接口",
    Category = "XTools|函数库",
    ShortToolTip = "对象池静态函数库"))
class OBJECTPOOL_API UObjectPoolLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ✅ 核心静态接口 - 简化版本

    /**
     * 注册Actor类到对象池（静态版本）
     * @param WorldContext 世界上下文
     * @param ActorClass 要池化的Actor类
     * @param InitialSize 初始池大小
     * @param HardLimit 池的最大限制 (0表示无限制)
     * @return true if registration successful
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "注册Actor类（静态）",
        ToolTip = "静态方法：为指定Actor类创建对象池，无需手动获取子系统",
        WorldContext = "WorldContext"))
    static bool RegisterActorClass(
        const UObject* WorldContext,
        UPARAM(DisplayName = "Actor类") TSubclassOf<AActor> ActorClass,
        UPARAM(DisplayName = "初始大小") int32 InitialSize = 10,
        UPARAM(DisplayName = "硬限制") int32 HardLimit = 0);

    /**
     * 从对象池获取Actor（静态版本）
     * @param WorldContext 世界上下文
     * @param ActorClass Actor类型
     * @param SpawnTransform 生成位置和旋转
     * @return 池化的Actor实例，永不返回null
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "从池获取Actor（静态）",
        ToolTip = "静态方法：从对象池获取Actor，永不失败，自动处理所有错误情况",
        WorldContext = "WorldContext"))
    static AActor* SpawnActorFromPool(
        const UObject* WorldContext,
        UPARAM(DisplayName = "Actor类") TSubclassOf<AActor> ActorClass,
        UPARAM(DisplayName = "生成Transform") const FTransform& SpawnTransform);

    /**
     * 将Actor归还到对象池（静态版本）
     * @param WorldContext 世界上下文
     * @param Actor 要归还的Actor
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "归还Actor到池（静态）",
        ToolTip = "静态方法：将Actor归还到对象池，自动处理状态重置",
        WorldContext = "WorldContext"))
    static void ReturnActorToPool(
        const UObject* WorldContext,
        UPARAM(DisplayName = "Actor") AActor* Actor);

    // ✅ 便捷接口 - 常用操作的简化版本

    /**
     * 快速生成Actor（位置版本）
     * @param WorldContext 世界上下文
     * @param ActorClass Actor类型
     * @param Location 生成位置
     * @param Rotation 生成旋转（可选）
     * @return 池化的Actor实例
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "快速生成Actor",
        ToolTip = "便捷方法：使用位置和旋转快速生成Actor，自动构建Transform。比手动构建Transform更简单，适合大多数使用场景。",
        Keywords = "快速生成,便捷,位置,旋转,Transform",
        ShortToolTip = "快速生成Actor（位置+旋转）",
        CompactNodeTitle = "快速生成",
        WorldContext = "WorldContext",
        AdvancedDisplay = "Rotation"))
    static AActor* QuickSpawnActor(
        const UObject* WorldContext,
        UPARAM(DisplayName = "Actor类") TSubclassOf<AActor> ActorClass,
        UPARAM(DisplayName = "位置") const FVector& Location,
        UPARAM(DisplayName = "旋转") const FRotator& Rotation = FRotator::ZeroRotator);

    /**
     * 批量生成Actor
     * @param WorldContext 世界上下文
     * @param ActorClass Actor类型
     * @param SpawnTransforms 生成位置数组
     * @param OutActors 输出的Actor数组
     * @return 成功生成的数量
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "批量生成Actor",
        ToolTip = "高级功能：一次性生成多个Actor，提高批量操作效率。适合生成大量相同类型的Actor，如子弹群、敌人波次等。",
        Keywords = "批量生成,多个Actor,高效,批量操作,数组",
        ShortToolTip = "批量生成多个Actor",
        CompactNodeTitle = "批量生成",
        WorldContext = "WorldContext",
        AdvancedDisplay = ""))
    static int32 BatchSpawnActors(
        const UObject* WorldContext,
        UPARAM(DisplayName = "Actor类") TSubclassOf<AActor> ActorClass,
        UPARAM(DisplayName = "生成Transform数组") const TArray<FTransform>& SpawnTransforms,
        UPARAM(DisplayName = "输出Actor数组") TArray<AActor*>& OutActors);

    /**
     * 批量归还Actor
     * @param WorldContext 世界上下文
     * @param Actors 要归还的Actor数组
     * @return 成功归还的数量
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "批量归还Actor",
        ToolTip = "高级功能：一次性归还多个Actor到对象池",
        WorldContext = "WorldContext"))
    static int32 BatchReturnActors(
        const UObject* WorldContext,
        UPARAM(DisplayName = "Actor数组") const TArray<AActor*>& Actors);

    // ✅ 查询和管理接口

    /**
     * 检查Actor类是否已注册
     * @param WorldContext 世界上下文
     * @param ActorClass Actor类型
     * @return true if registered
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "检查类是否已注册",
        ToolTip = "查询指定Actor类是否已经注册到对象池",
        WorldContext = "WorldContext"))
    static bool IsActorClassRegistered(
        const UObject* WorldContext,
        UPARAM(DisplayName = "Actor类") TSubclassOf<AActor> ActorClass);

    /**
     * 获取池统计信息
     * @param WorldContext 世界上下文
     * @param ActorClass Actor类型
     * @return 池的统计信息
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "获取池统计信息",
        ToolTip = "获取指定Actor类型的对象池使用统计和性能数据",
        WorldContext = "WorldContext"))
    static FObjectPoolStats GetPoolStats(
        const UObject* WorldContext,
        UPARAM(DisplayName = "Actor类") TSubclassOf<AActor> ActorClass);

    /**
     * 预热对象池
     * @param WorldContext 世界上下文
     * @param ActorClass Actor类型
     * @param Count 预创建的数量
     * @return true if prewarming successful
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "预热对象池",
        ToolTip = "预先创建指定数量的Actor到池中，提高运行时性能",
        WorldContext = "WorldContext"))
    static bool PrewarmPool(
        const UObject* WorldContext,
        UPARAM(DisplayName = "Actor类") TSubclassOf<AActor> ActorClass,
        UPARAM(DisplayName = "预创建数量") int32 Count);

    /**
     * 清空对象池
     * @param WorldContext 世界上下文
     * @param ActorClass Actor类型
     * @return true if clearing successful
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "清空对象池",
        ToolTip = "清空指定Actor类型的对象池，销毁所有池化Actor",
        WorldContext = "WorldContext"))
    static bool ClearPool(
        const UObject* WorldContext,
        UPARAM(DisplayName = "Actor类") TSubclassOf<AActor> ActorClass);

    /**
     * 获取对象池子系统（辅助方法）
     * @param WorldContext 世界上下文
     * @return 对象池子系统实例
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "获取对象池子系统",
        ToolTip = "获取对象池子系统实例，用于高级操作",
        WorldContext = "WorldContext"))
    static UObjectPoolSubsystem* GetObjectPoolSubsystem(const UObject* WorldContext);

    // ✅ 生命周期事件管理接口

    /**
     * 手动调用Actor的生命周期事件
     * @param WorldContext 世界上下文
     * @param Actor 目标Actor
     * @param EventType 事件类型
     * @param bAsync 是否异步调用
     * @return 调用是否成功
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池|生命周期", meta = (
        DisplayName = "调用生命周期事件",
        ToolTip = "手动调用Actor的对象池生命周期事件",
        WorldContext = "WorldContext"))
    static bool CallLifecycleEvent(
        const UObject* WorldContext,
        UPARAM(DisplayName = "Actor") AActor* Actor,
        UPARAM(DisplayName = "事件类型") EObjectPoolLifecycleEvent EventType,
        UPARAM(DisplayName = "异步调用") bool bAsync = false);

    /**
     * 批量调用生命周期事件
     * @param WorldContext 世界上下文
     * @param Actors Actor数组
     * @param EventType 事件类型
     * @param bAsync 是否异步调用
     * @return 成功调用的数量
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池|生命周期", meta = (
        DisplayName = "批量调用生命周期事件",
        ToolTip = "批量调用多个Actor的生命周期事件",
        WorldContext = "WorldContext"))
    static int32 BatchCallLifecycleEvents(
        const UObject* WorldContext,
        UPARAM(DisplayName = "Actor数组") const TArray<AActor*>& Actors,
        UPARAM(DisplayName = "事件类型") EObjectPoolLifecycleEvent EventType,
        UPARAM(DisplayName = "异步调用") bool bAsync = false);

    /**
     * 检查Actor是否支持生命周期事件
     * @param WorldContext 世界上下文
     * @param Actor 目标Actor
     * @param EventType 事件类型
     * @return true if supported
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池|生命周期", meta = (
        DisplayName = "检查生命周期事件支持",
        ToolTip = "检查Actor是否实现了指定的生命周期事件",
        WorldContext = "WorldContext"))
    static bool HasLifecycleEventSupport(
        const UObject* WorldContext,
        UPARAM(DisplayName = "Actor") AActor* Actor,
        UPARAM(DisplayName = "事件类型") EObjectPoolLifecycleEvent EventType);

private:
    /**
     * 内部辅助方法：安全获取子系统
     * @param WorldContext 世界上下文
     * @return 子系统实例，如果失败返回nullptr
     */
    static UObjectPoolSubsystem* GetSubsystemSafe(const UObject* WorldContext);
};
