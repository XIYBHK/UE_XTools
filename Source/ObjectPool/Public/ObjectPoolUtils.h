/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

//  遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"

//  对象池模块依赖
#include "ObjectPoolTypes.h"
#include "ObjectPoolInterface.h"

/**
 * 对象池工具类
 * 
 * 设计原则：
 * - 静态方法为主，无状态设计
 * - 整合原有辅助类的核心功能
 * - 简化的API，易于使用和维护
 * - 高性能，适合频繁调用
 * 
 * 整合的功能：
 * - Actor状态重置（来自FActorStateResetter）
 * - 基本配置管理（来自FObjectPoolConfigManager）
 * - 简化调试工具（来自FObjectPoolDebugManager）
 * - 性能分析工具
 */
class OBJECTPOOL_API FObjectPoolUtils
{
public:
    //  Actor状态重置工具方法

    /**
     * 重置Actor到池化状态（归还时调用）
     * 简化版本，只包含最核心的重置逻辑
     * 
     * @param Actor 要重置的Actor
     * @return 重置是否成功
     */
    static bool ResetActorForPooling(AActor* Actor);

    /**
     * 激活Actor从池化状态（获取时调用）
     * 简化版本，设置Actor到可用状态
     * 
     * @param Actor 要激活的Actor
     * @param SpawnTransform 激活时的Transform
     * @return 激活是否成功
     */
    static bool ActivateActorFromPool(AActor* Actor, const FTransform& SpawnTransform);



    /**
     * 基础的Actor状态重置
     * 提供最基本的重置功能，作为回退方案
     * 
     * @param Actor 要重置的Actor
     * @param NewTransform 新的Transform
     * @param bResetPhysics 是否重置物理状态
     * @return 重置是否成功
     */
    static bool BasicActorReset(AActor* Actor, const FTransform& NewTransform, bool bResetPhysics = true);

    //  配置管理工具方法

    /**
     * 验证配置的有效性
     * 
     * @param Config 要验证的配置
     * @param OutErrorMessage 错误信息输出
     * @return 配置是否有效
     */
    static bool ValidateConfig(const FObjectPoolConfig& Config, FString& OutErrorMessage);

    /**
     * 应用默认配置值
     * 为不完整的配置填充合理的默认值
     * 
     * @param Config 要处理的配置（会被修改）
     */
    static void ApplyDefaultConfig(FObjectPoolConfig& Config);

    /**
     * 创建常用类型的默认配置
     * 
     * @param ActorClass Actor类
     * @param PoolType 池类型（子弹、敌人、特效等）
     * @return 默认配置
     */
    static FObjectPoolConfig CreateDefaultConfig(TSubclassOf<AActor> ActorClass, const FString& PoolType = TEXT(""));

    //  调试和监控工具方法

    /**
     * 获取池的调试信息
     * 
     * @param Stats 池统计信息
     * @param PoolName 池名称
     * @return 调试信息结构
     */
    static FObjectPoolDebugInfo GetDebugInfo(const FObjectPoolStats& Stats, const FString& PoolName);

    /**
     * 记录池统计信息到日志
     *
     * @param Stats 统计信息
     * @param PoolName 池名称
     * @param Verbosity 日志级别
     */
    static void LogPoolStats(const FObjectPoolStats& Stats, const FString& PoolName, ELogVerbosity::Type Verbosity = ELogVerbosity::Log);

    /**
     * 检查池的健康状态
     * 
     * @param Stats 统计信息
     * @return 是否健康
     */
    static bool IsPoolHealthy(const FObjectPoolStats& Stats);

    /**
     * 获取性能建议
     * 
     * @param Stats 统计信息
     * @return 建议列表
     */
    static TArray<FString> GetPerformanceSuggestions(const FObjectPoolStats& Stats);

    //  性能分析工具方法

    /**
     * 计算池的内存使用估算
     * 
     * @param ActorClass Actor类
     * @param PoolSize 池大小
     * @return 内存使用字节数（估算）
     */
    static int64 EstimateMemoryUsage(TSubclassOf<AActor> ActorClass, int32 PoolSize);

    /**
     * 分析池的使用模式
     * 
     * @param Stats 统计信息
     * @return 使用模式描述
     */
    static FString AnalyzeUsagePattern(const FObjectPoolStats& Stats);

    /**
     * 获取优化建议
     * 
     * @param Config 当前配置
     * @param Stats 当前统计
     * @return 优化建议
     */
    static TArray<FString> GetOptimizationSuggestions(const FObjectPoolConfig& Config, const FObjectPoolStats& Stats);

    //  实用工具方法

    /**
     * 安全地调用Actor的生命周期接口
     * 
     * @param Actor 目标Actor
     * @param EventType 事件类型（Created, Activated, ReturnedToPool）
     */
    static void SafeCallLifecycleInterface(AActor* Actor, const FString& EventType);

    /**
     * 检查Actor是否适合池化
     * 
     * @param ActorClass Actor类
     * @return 是否适合池化
     */
    static bool IsActorSuitableForPooling(TSubclassOf<AActor> ActorClass);

    /**
     * 格式化统计信息为可读字符串
     * 
     * @param Stats 统计信息
     * @param bDetailed 是否显示详细信息
     * @return 格式化的字符串
     */
    static FString FormatStatsString(const FObjectPoolStats& Stats, bool bDetailed = false);

    /**
     * 生成池的唯一标识符
     * 
     * @param ActorClass Actor类
     * @return 唯一标识符
     */
    static FString GeneratePoolId(TSubclassOf<AActor> ActorClass);

private:
    //  内部辅助方法

    /**
     * 重置Actor的基本属性
     */
    static void ResetBasicActorProperties(AActor* Actor, bool bHideActor = true);

    /**
     * 重置Actor的物理状态
     */
    static void ResetActorPhysics(AActor* Actor);

    /**
     * 重置Actor的组件状态
     */
    static void ResetActorComponents(AActor* Actor);

    /**
     * 应用Transform到Actor
     */
    static void ApplyTransformToActor(AActor* Actor, const FTransform& NewTransform);

    /**
     * 重置ProjectileMovement组件
     */
    static void ResetProjectileMovementComponent(class UProjectileMovementComponent* ProjectileComp);

    /**
     * 获取Actor类的默认配置参数
     */
    static void GetDefaultConfigForActorClass(TSubclassOf<AActor> ActorClass, int32& OutInitialSize, int32& OutHardLimit);

    /**
     * 计算Actor类的内存占用估算
     */
    static int64 CalculateActorMemoryFootprint(TSubclassOf<AActor> ActorClass);

    /**
     * 保存PrimitiveComponent的原始碰撞设置
     */
    static void SaveOriginalCollisionSettings(class UPrimitiveComponent* PrimComp);

    /**
     * 恢复PrimitiveComponent的原始碰撞设置
     */
    static void RestoreOriginalCollisionSettings(class UPrimitiveComponent* PrimComp);

    /**
     * 从CDO重置组件到默认状态（通用方法）
     */
    static void ResetComponentFromCDO(class UActorComponent* Component);

    /**
     * 保存组件的运行时特定属性（如InitialSpeed的实际值）
     */
    static void SaveRuntimeSpecificProperties(class UActorComponent* Component);

    /**
     * 恢复组件的运行时特定属性
     */
    static void RestoreRuntimeSpecificProperties(class UActorComponent* Component);

    //  常量定义

    /** 默认的池大小配置 */
    static constexpr int32 DEFAULT_POOL_SIZE = 10;
    static constexpr int32 DEFAULT_HARD_LIMIT = 100;

    /** 性能阈值 */
    static constexpr float MIN_HEALTHY_HIT_RATE = 0.3f;
    static constexpr float MAX_HEALTHY_UNUSED_RATIO = 0.8f;

    /** 内存估算常量 */
    static constexpr int64 BASE_ACTOR_MEMORY = 1024; // 基础Actor内存占用（字节）
    static constexpr int64 COMPONENT_MEMORY_ESTIMATE = 256; // 每个组件的估算内存
};

/**
 * 对象池工具类的性能计时器
 * 用于测量工具方法的执行时间
 */
class OBJECTPOOL_API FObjectPoolUtilsTimer
{
public:
    explicit FObjectPoolUtilsTimer(const FString& OperationName);
    ~FObjectPoolUtilsTimer();

private:
    FString Operation;
    double StartTime;
};

/**
 * 便利宏，用于自动计时
 */
#define OBJECTPOOL_UTILS_TIMER(Name) FObjectPoolUtilsTimer Timer(Name)

/**
 * 对象池工具类的日志宏
 */
DECLARE_LOG_CATEGORY_EXTERN(LogObjectPoolUtils, Log, All);

#define OBJECTPOOL_UTILS_LOG(Verbosity, Format, ...) \
    UE_LOG(LogObjectPoolUtils, Verbosity, Format, ##__VA_ARGS__)

/**
 * 对象池工具类的统计宏
 */
#if STATS
DECLARE_STATS_GROUP(TEXT("ObjectPoolUtils"), STATGROUP_ObjectPoolUtils, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("ResetActorForPooling"), STAT_ResetActorForPooling, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("ActivateActorFromPool"), STAT_ActivateActorFromPool, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("ValidateConfig"), STAT_ValidateConfig, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
#endif
