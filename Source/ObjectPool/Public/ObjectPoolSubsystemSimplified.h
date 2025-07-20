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
#include "ObjectPoolTypesSimplified.h"
#include "ActorPoolSimplified.h"
#include "ObjectPoolConfigManagerSimplified.h"
#include "ObjectPoolManager.h"

#include "ObjectPoolSubsystemSimplified.generated.h"

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

    FObjectPoolSubsystemStats()
        : TotalSpawnCalls(0)
        , TotalReturnCalls(0)
        , TotalPoolsCreated(0)
        , TotalPoolsDestroyed(0)
        , StartupTime(0.0)
        , LastMaintenanceTime(0.0)
    {
    }
};

/**
 * 简化的对象池子系统
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
class OBJECTPOOL_API UObjectPoolSubsystemSimplified : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // ✅ USubsystem接口实现
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

public:
    // ✅ 核心对象池API

    /**
     * 从池中生成Actor
     * @param ActorClass 要生成的Actor类
     * @param SpawnTransform 生成位置和旋转
     * @return 生成的Actor，失败时返回nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    AActor* SpawnActorFromPool(UClass* ActorClass, const FTransform& SpawnTransform);

    /**
     * 从池中生成Actor（使用默认Transform）
     * @param ActorClass 要生成的Actor类
     * @return 生成的Actor，失败时返回nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    AActor* SpawnActorFromPoolSimple(UClass* ActorClass);

    /**
     * 将Actor归还到池中
     * @param Actor 要归还的Actor
     * @return 归还是否成功
     */
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    bool ReturnActorToPool(AActor* Actor);

    /**
     * 预热指定类型的池
     * @param ActorClass 要预热的Actor类
     * @param Count 预热数量
     * @return 实际预热的数量
     */
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    int32 PrewarmPool(UClass* ActorClass, int32 Count);

    // ✅ 池管理功能

    /**
     * 获取或创建指定类型的池
     * @param ActorClass Actor类
     * @return 池的共享指针
     */
    TSharedPtr<FActorPoolSimplified> GetOrCreatePool(UClass* ActorClass);

    /**
     * 获取现有的池（不创建新的）
     * @param ActorClass Actor类
     * @return 池的共享指针，不存在时返回nullptr
     */
    TSharedPtr<FActorPoolSimplified> GetPool(UClass* ActorClass) const;

    /**
     * 移除指定类型的池
     * @param ActorClass Actor类
     * @return 是否成功移除
     */
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    bool RemovePool(UClass* ActorClass);

    /**
     * 清空所有池
     */
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    void ClearAllPools();

    /**
     * 获取当前池的数量
     * @return 池数量
     */
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    int32 GetPoolCount() const;

    // ✅ 状态查询功能

    /**
     * 获取指定池的统计信息
     * @param ActorClass Actor类
     * @return 统计信息
     */
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    FObjectPoolStatsSimplified GetPoolStats(UClass* ActorClass) const;

    /**
     * 获取所有池的统计信息
     * @return 统计信息数组
     */
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    TArray<FObjectPoolStatsSimplified> GetAllPoolStats() const;

    /**
     * 检查指定Actor类是否有对应的池
     * @param ActorClass Actor类
     * @return 是否存在池
     */
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    bool HasPool(UClass* ActorClass) const;

    // ✅ 配置管理（委托给配置管理器）

    /**
     * 设置池的配置
     * @param ActorClass Actor类
     * @param Config 配置信息
     * @return 是否设置成功
     */
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    bool SetPoolConfig(UClass* ActorClass, const FObjectPoolConfigSimplified& Config);

    /**
     * 获取池的配置
     * @param ActorClass Actor类
     * @return 配置信息
     */
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    FObjectPoolConfigSimplified GetPoolConfig(UClass* ActorClass) const;

    // ✅ 工具类访问接口

    /**
     * 获取配置管理器
     * @return 配置管理器引用
     */
    class FObjectPoolConfigManagerSimplified& GetConfigManager() const;

    /**
     * 获取池管理器
     * @return 池管理器引用
     */
    class FObjectPoolManager& GetPoolManager() const;



    /**
     * 启用/禁用性能监控
     * @param bEnable 是否启用
     */
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    void SetMonitoringEnabled(bool bEnable);

    // ✅ 性能统计功能

    /**
     * 获取子系统级别的统计信息
     * @return 子系统统计信息
     */
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    FObjectPoolSubsystemStats GetSubsystemStats() const;

    /**
     * 重置子系统统计信息
     */
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    void ResetSubsystemStats();

    /**
     * 生成性能报告
     * @return 格式化的性能报告
     */
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    FString GeneratePerformanceReport() const;

public:
    // ✅ 静态访问方法

    /**
     * 获取对象池子系统实例
     * @param World 世界上下文
     * @return 子系统实例
     */
    UFUNCTION(BlueprintCallable, Category = "Object Pool", meta = (WorldContext = "WorldContext"))
    static UObjectPoolSubsystemSimplified* Get(const UObject* WorldContext);

private:
    // ✅ 核心数据成员

    /** 池存储：Actor类 -> 池实例 */
    TMap<UClass*, TSharedPtr<FActorPoolSimplified>> ActorPools;

    /** 池访问读写锁（优化并发性能） */
    mutable FRWLock PoolsRWLock;

    // ✅ 性能优化缓存

    /** 最近访问的池缓存（提高热点访问性能） */
    mutable TWeakPtr<FActorPoolSimplified> LastAccessedPool;
    mutable UClass* LastAccessedClass;

    /** 缓存访问锁 */
    mutable FCriticalSection CacheLock;

    /** 是否已初始化 */
    bool bIsInitialized;

    // ✅ 独立工具类（组合模式）

    /** 配置管理器 */
    TUniquePtr<FObjectPoolConfigManagerSimplified> ConfigManager;

    /** 池管理器 */
    TUniquePtr<FObjectPoolManager> PoolManager;

    /** 是否启用监控 */
    bool bMonitoringEnabled;

    // ✅ 基本性能统计

    /** 子系统统计信息实例 */
    FObjectPoolSubsystemStats SubsystemStats;

private:
    // ✅ 内部辅助方法

    /**
     * 创建新的池
     * @param ActorClass Actor类
     * @return 创建的池
     */
    TSharedPtr<FActorPoolSimplified> CreatePool(UClass* ActorClass);

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
    void UpdatePoolCache(UClass* ActorClass, TSharedPtr<FActorPoolSimplified> Pool) const;

    /**
     * 清理池缓存
     */
    void ClearPoolCache() const;

    // ✅ 常量定义

    /** 默认池初始大小 */
    static constexpr int32 DEFAULT_POOL_INITIAL_SIZE = 10;

    /** 默认池最大大小 */
    static constexpr int32 DEFAULT_POOL_MAX_SIZE = 100;

    /** 维护间隔（秒） */
    static constexpr float MAINTENANCE_INTERVAL = 30.0f;
};

/**
 * 子系统的日志宏
 */
DECLARE_LOG_CATEGORY_EXTERN(LogObjectPoolSubsystemSimplified, Log, All);

#define OBJECTPOOL_SUBSYSTEM_LOG(Verbosity, Format, ...) \
    UE_LOG(LogObjectPoolSubsystemSimplified, Verbosity, Format, ##__VA_ARGS__)

/**
 * 子系统的性能统计宏
 */
#if STATS
DECLARE_CYCLE_STAT_EXTERN(TEXT("ObjectPoolSubsystem_SpawnActor"), STAT_ObjectPoolSubsystem_SpawnActor, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("ObjectPoolSubsystem_ReturnActor"), STAT_ObjectPoolSubsystem_ReturnActor, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("ObjectPoolSubsystem_GetOrCreatePool"), STAT_ObjectPoolSubsystem_GetOrCreatePool, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
#endif
