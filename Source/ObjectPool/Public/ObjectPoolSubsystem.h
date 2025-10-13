// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "Engine/Engine.h"

// ✅ 对象池模块依赖
#include "ObjectPoolTypes.h"
#include "ActorPool.h"
#include "ObjectPoolConfigManager.h"
#include "ObjectPoolManager.h"

#include "ObjectPoolSubsystem.generated.h"

// 前向声明
class FObjectPoolMonitor;

/**
 * 子系统级别的统计信息
 */
USTRUCT(BlueprintType)
struct OBJECTPOOL_API FObjectPoolSubsystemStats
{
    GENERATED_BODY()

    /** 总的SpawnActor调用次数 */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 TotalSpawnCalls;

    /** 总的ReturnActor调用次数 */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 TotalReturnCalls;

    /** 总的池创建次数 */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 TotalPoolsCreated;

    /** 总的池销毁次数 */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 TotalPoolsDestroyed;

    /** 子系统启动时间 */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    double StartupTime;

    /** 上次维护时间 */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    double LastMaintenanceTime;

    /** 池命中次数（从池中成功获取）- 衡量对象池效率 */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 TotalPoolHits;

    /** 回退生成次数（池空时的正常生成）- 衡量"永不失败"机制使用 */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 TotalFallbackSpawns;

    FObjectPoolSubsystemStats()
        : TotalSpawnCalls(0)
        , TotalReturnCalls(0)
        , TotalPoolsCreated(0)
        , TotalPoolsDestroyed(0)
        , StartupTime(0.0)
        , LastMaintenanceTime(0.0)
        , TotalPoolHits(0)
        , TotalFallbackSpawns(0)
    {
    }
};

/**
 * 对象池子系统
 * 
 * 设计原则：
 * - 单一职责：专注于子系统的核心协调功能
 * - 简化架构：移除复杂的内部管理器，使用独立工具类
 * - 组合优于继承：通过组合使用专门的工具类
 * - 清晰接口：提供简单易用的公共API
 * 
 * 核心职责：
 * - 作为全局访问点（子系统职责）
 * - 池的生命周期管理（创建、销毁、清理）
 * - 基本的SpawnActorFromPool/ReturnActorToPool API
 * - 与UE生命周期的集成
 * 
 * 移除的功能及新归属：
 * - 复杂配置管理 → FObjectPoolConfigManager
 * - 性能监控统计 → FObjectPoolMonitor
 * - 智能池管理 → FObjectPoolManager
 */
UCLASS()
class OBJECTPOOL_API UObjectPoolSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // ✅ UE官方子系统完整生命周期 - 深度集成引擎机制
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;


public:
    // ✅ 核心对象池API - 设计文档第177-210行的极简API设计

    /**
     * 注册Actor类到对象池
     * @param ActorClass 要池化的Actor类
     * @param InitialSize 初始池大小  
     * @param HardLimit 池的最大限制 (0表示无限制)
     * @return 注册是否成功
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "注册Actor类",
        ToolTip = "为指定Actor类创建对象池，这是使用对象池的第一步",
        Keywords = "对象池,注册,创建,初始化"))
    bool RegisterActorClass(TSubclassOf<AActor> ActorClass, int32 InitialSize = 10, int32 HardLimit = 0);

    /**
     * 从池中生成Actor - 永不失败设计
     * @param ActorClass 要生成的Actor类
     * @param SpawnTransform 生成位置和旋转
     * @return 池化的Actor实例，永不返回null（自动回退到正常生成）
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "从池获取Actor",
        ToolTip = "从对象池获取Actor，永不失败！如果池为空会自动创建新Actor",
        Keywords = "对象池,生成,获取,永不失败"))
    AActor* SpawnActorFromPool(UClass* ActorClass, const FTransform& SpawnTransform);

    /** 延迟获取：仅取出或创建延迟构造实例，不激活 */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "延迟获取Actor（池）",
        ToolTip = "仅从池取出或创建延迟构造实例，不激活，由后续Finalize完成",
        Keywords = "对象池,延迟,Defer"))
    AActor* AcquireDeferredFromPool(UClass* ActorClass);

    /** 完成延迟激活：应用Transform并激活（必要时FinishSpawning） */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "完成延迟激活（池）",
        ToolTip = "对延迟获取的Actor应用Transform并激活，触发生命周期事件",
        Keywords = "对象池,Finalize,激活"))
    bool FinalizeSpawnFromPool(AActor* Actor, const FTransform& SpawnTransform);



    /**
     * 将Actor归还到池中 - 智能归还
     * @param Actor 要归还的Actor
     * @return 归还是否成功（自动处理各种异常情况）
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "归还Actor到池",
        ToolTip = "将Actor归还到对象池，自动处理状态重置和生命周期事件",
        Keywords = "对象池,归还,回收"))
    bool ReturnActorToPool(AActor* Actor);

    /**
     * 预热对象池 - 可选的优化功能
     * @param ActorClass 要预热的Actor类
     * @param Count 预热数量
     * @return 实际预热的数量
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "预热对象池",
        ToolTip = "预先创建指定数量的Actor，提升运行时性能（可选功能）",
        Keywords = "对象池,预热,优化,性能"))
    int32 PrewarmPool(UClass* ActorClass, int32 Count);

    // ✅ 静态访问方法（设计文档第295行要求）

    /**
     * 获取对象池子系统实例
     * @param World 世界上下文
     * @return 子系统实例
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (WorldContext = "WorldContext", 
        DisplayName = "获取对象池子系统",
        ToolTip = "获取全局对象池管理器"))
    static UObjectPoolSubsystem* Get(const UObject* WorldContext);

    /**
     * 查询给定Actor是否由某个池管理
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池|查询")
    bool IsActorPooled(const AActor* Actor) const;



private:
    // ✅ 核心数据成员

    /** 池存储：Actor类 -> 池实例 (UE官方智能指针最佳实践) */
    TMap<UClass*, TSharedPtr<FActorPool>> ActorPools;
    
    /** UE官方推荐：预分配容器减少运行时重分配 */
    static constexpr int32 DefaultPoolCapacity = 16;

    /** 池访问读写锁（优化并发性能） */
    mutable FRWLock PoolsRWLock;

    // ✅ 性能优化缓存

    /** 最近访问的池缓存（提高热点访问性能） */
    mutable TWeakPtr<FActorPool> LastAccessedPool;
    mutable UClass* LastAccessedClass;

    /** 缓存访问锁 */
    mutable FCriticalSection CacheLock;

    /** 是否已初始化 */
    bool bIsInitialized;

    // ✅ UE官方垃圾回收系统集成
    
    /** GC回调：垃圾回收前的清理 */
    UFUNCTION()
    void OnPreGarbageCollect();
    
    /** GC回调：垃圾回收后的清理 */  
    UFUNCTION()
    void OnPostGarbageCollect();

    // ✅ 独立工具类（组合模式，基于UE智能指针）

    /** 配置管理器 */
    TUniquePtr<FObjectPoolConfigManager> ConfigManager;

    /** 池管理器 */
    TUniquePtr<FObjectPoolManager> PoolManager;

    /** 是否启用监控 */
    bool bMonitoringEnabled;

    // ✅ 安全延迟预热机制
    
    /** 待预热信息结构体 */
    struct FDelayedPrewarmInfo
    {
        UClass* ActorClass;
        int32 Count;
        FString PoolName;
        
        FDelayedPrewarmInfo(UClass* InActorClass, int32 InCount)
            : ActorClass(InActorClass), Count(InCount)
        {
            PoolName = InActorClass ? InActorClass->GetName() : TEXT("Unknown");
        }
    };

    /** 延迟预热队列 */
    TArray<FDelayedPrewarmInfo> DelayedPrewarmQueue;

    /** 延迟预热Timer句柄 */
    FTimerHandle DelayedPrewarmTimerHandle;

    // ✅ 基本性能统计

    /** 子系统统计信息实例 */
    FObjectPoolSubsystemStats SubsystemStats;

    // ✅ 内部辅助方法

    /**
     * 获取或创建指定类型的池
     * @param ActorClass Actor类
     * @return 池的共享指针
     */
    TSharedPtr<FActorPool> GetOrCreatePool(UClass* ActorClass);

    /**
     * 获取现有的池（不创建新的）
     * @param ActorClass Actor类
     * @return 池的共享指针，不存在时返回nullptr
     */
    TSharedPtr<FActorPool> GetPool(UClass* ActorClass) const;

    /**
     * 清空所有池
     */
    void ClearAllPools();

    /**
     * 创建新的池
     * @param ActorClass Actor类
     * @return 创建的池
     */
    TSharedPtr<FActorPool> CreatePool(UClass* ActorClass);

    /**
     * 验证Actor类是否有效
     * @param ActorClass Actor类
     * @return 是否有效
     */
    bool ValidateActorClass(UClass* ActorClass) const;

    /**
     * 清理无效的池
     */
    void CleanupInvalidPools();

    /**
     * 执行定期维护
     */
    void PerformMaintenance();

    // ✅ 性能优化方法

    /**
     * 更新池缓存
     * @param ActorClass Actor类
     * @param Pool 池实例
     */
    void UpdatePoolCache(UClass* ActorClass, TSharedPtr<FActorPool> Pool) const;

    /**
     * 清理池缓存
     */
    void ClearPoolCache() const;

    /**
     * 获取子系统级别的统计信息
     * @return 子系统统计信息
     */
    FObjectPoolSubsystemStats GetSubsystemStats() const;

    // ✅ 安全延迟预热方法

    /**
     * 队列延迟预热请求（避免同帧死锁）
     * @param ActorClass Actor类
     * @param Count 预热数量
     */
    void QueueDelayedPrewarm(UClass* ActorClass, int32 Count);

    /**
     * 执行延迟预热（在下一帧安全执行）
     */
    void ProcessDelayedPrewarmQueue();

    /**
     * 清理延迟预热Timer
     */
    void ClearDelayedPrewarmTimer();

    // ✅ 常量定义

    /** 默认池初始大小 */
    static constexpr int32 DEFAULT_POOL_INITIAL_SIZE = 10;

    /** 默认池最大大小 */
    static constexpr int32 DEFAULT_POOL_MAX_SIZE = 100;

    /** 维护间隔（秒） */
    static constexpr float MAINTENANCE_INTERVAL = 30.0f;

    /** 延迟预热：每帧最大创建数量（避免卡顿） */
    static constexpr int32 MAX_ACTORS_PER_FRAME_PREWARM = 10;
};

/**
 * 子系统的日志宏
 */
DECLARE_LOG_CATEGORY_EXTERN(LogObjectPoolSubsystem, Log, All);

#define OBJECTPOOL_SUBSYSTEM_LOG(Verbosity, Format, ...) \
    UE_LOG(LogObjectPoolSubsystem, Verbosity, Format, ##__VA_ARGS__)

/**
 * 子系统的性能统计宏
 */
#if STATS
DECLARE_CYCLE_STAT_EXTERN(TEXT("ObjectPoolSubsystem_SpawnActor"), STAT_ObjectPoolSubsystem_SpawnActor, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("ObjectPoolSubsystem_ReturnActor"), STAT_ObjectPoolSubsystem_ReturnActor, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("ObjectPoolSubsystem_GetOrCreatePool"), STAT_ObjectPoolSubsystem_GetOrCreatePool, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
#endif