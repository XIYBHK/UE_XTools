/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

//  对象池模块依赖
#include "ObjectPoolTypes.h"

// 前向声明
class FActorPool;

/**
 * 对象池管理器
 * 
 * 设计原则：
 * - 单一职责：专注于池的智能管理和优化
 * - 策略模式：支持不同的管理策略
 * - 自动化：提供自动维护和优化功能
 * - 可观测：提供详细的管理统计信息
 * 
 * 职责：
 * - 自动池创建和销毁
 * - 池大小动态调整
 * - 预分配策略执行
 * - 定期维护和清理
 */
class OBJECTPOOL_API FObjectPoolManager
{
public:
    /**
     * 管理策略枚举
     */
    enum class EManagementStrategy : uint8
    {
        Conservative,   // 保守管理：最小化资源使用
        Adaptive,       // 自适应管理：根据使用情况调整
        Aggressive,     // 激进管理：最大化性能
        Manual          // 手动管理：不自动调整
    };

    /**
     * 维护操作类型
     */
    enum class EMaintenanceType : uint8
    {
        Cleanup,        // 清理无效对象
        Resize,         // 调整池大小
        Preallocation,  // 执行预分配
        Optimization,   // 性能优化
        All             // 所有维护操作
    };

    /**
     * 管理统计信息
     */
    struct FManagementStats
    {
        /** 管理的池数量 */
        int32 ManagedPoolCount;

        /** 总维护次数 */
        int32 TotalMaintenanceCount;

        /** 自动调整次数 */
        int32 AutoResizeCount;

        /** 预分配次数 */
        int32 PreallocationCount;

        /** 清理次数 */
        int32 CleanupCount;

        /** 上次维护时间 */
        double LastMaintenanceTime;

        /** 总管理时间 */
        double TotalManagementTime;

        FManagementStats()
            : ManagedPoolCount(0)
            , TotalMaintenanceCount(0)
            , AutoResizeCount(0)
            , PreallocationCount(0)
            , CleanupCount(0)
            , LastMaintenanceTime(0.0)
            , TotalManagementTime(0.0)
        {
        }
    };

public:
    /**
     * 构造函数
     * @param InStrategy 管理策略
     */
    explicit FObjectPoolManager(EManagementStrategy InStrategy = EManagementStrategy::Adaptive);

    /** 析构函数 */
    ~FObjectPoolManager();

    //  禁用拷贝构造和赋值
    FObjectPoolManager(const FObjectPoolManager&) = delete;
    FObjectPoolManager& operator=(const FObjectPoolManager&) = delete;

    //  支持移动语义
    FObjectPoolManager(FObjectPoolManager&& Other) noexcept;
    FObjectPoolManager& operator=(FObjectPoolManager&& Other) noexcept;

public:
    //  池生命周期管理

    /**
     * 通知池已创建
     * @param ActorClass Actor类
     * @param Pool 池实例
     */
    void OnPoolCreated(UClass* ActorClass, TSharedPtr<FActorPool> Pool);

    /**
     * 通知池即将销毁
     * @param ActorClass Actor类
     */
    void OnPoolDestroying(UClass* ActorClass);

    /**
     * 检查是否应该创建池
     * @param ActorClass Actor类
     * @return 是否应该创建
     */
    bool ShouldCreatePool(UClass* ActorClass) const;

    /**
     * 检查是否应该销毁池
     * @param ActorClass Actor类
     * @param Pool 池实例
     * @return 是否应该销毁
     */
    bool ShouldDestroyPool(UClass* ActorClass, const FActorPool& Pool) const;

    //  智能管理功能

    /**
     * 执行定期维护
     * @param AllPools 所有池的映射
     * @param MaintenanceType 维护类型
     */
    void PerformMaintenance(const TMap<UClass*, TSharedPtr<FActorPool>>& AllPools, 
                           EMaintenanceType MaintenanceType = EMaintenanceType::All);

    /**
     * 分析池的使用情况并提供建议
     * @param ActorClass Actor类
     * @param Pool 池实例
     * @return 管理建议列表
     */
    TArray<FString> AnalyzePoolUsage(UClass* ActorClass, const FActorPool& Pool) const;

    /**
     * 自动调整池大小
     * @param ActorClass Actor类
     * @param Pool 池实例
     * @return 是否进行了调整
     */
    bool AutoResizePool(UClass* ActorClass, FActorPool& Pool) const;

    /**
     * 执行智能预分配
     * @param ActorClass Actor类
     * @param Pool 池实例
     * @param World 世界上下文
     * @return 预分配的数量
     */
    int32 PerformSmartPreallocation(UClass* ActorClass, FActorPool& Pool, UWorld* World) const;

    //  策略管理

    /**
     * 设置管理策略
     * @param NewStrategy 新策略
     */
    void SetManagementStrategy(EManagementStrategy NewStrategy);

    /**
     * 获取当前管理策略
     * @return 当前策略
     */
    EManagementStrategy GetManagementStrategy() const { return CurrentStrategy; }

    /**
     * 启用/禁用自动管理
     * @param bEnable 是否启用
     */
    void SetAutoManagementEnabled(bool bEnable);

    /**
     * 检查是否启用自动管理
     * @return 是否启用
     */
    bool IsAutoManagementEnabled() const { return bAutoManagementEnabled; }

    //  统计和监控

    /**
     * 获取管理统计信息
     * @return 统计信息
     */
    FManagementStats GetManagementStats() const;

    /**
     * 生成管理报告
     * @param AllPools 所有池的映射
     * @return 格式化的报告
     */
    FString GenerateManagementReport(const TMap<UClass*, TSharedPtr<FActorPool>>& AllPools) const;

    /**
     * 重置统计信息
     */
    void ResetStats();

private:
    //  内部数据成员

    /** 当前管理策略 */
    EManagementStrategy CurrentStrategy;

    /** 是否启用自动管理 */
    bool bAutoManagementEnabled;

    /** 管理统计信息 */
    mutable FManagementStats Stats;

    /** 池使用历史记录 */
    mutable TMap<UClass*, TArray<float>> UsageHistory;

    /** 管理锁 */
    mutable FCriticalSection ManagementLock;

private:
    //  内部辅助方法

    /**
     * 分析池的使用趋势
     * @param ActorClass Actor类
     * @param Pool 池实例
     * @return 使用趋势（正数表示增长，负数表示下降）
     */
    float AnalyzeUsageTrend(UClass* ActorClass, const FActorPool& Pool) const;

    /**
     * 计算推荐的池大小
     * @param ActorClass Actor类
     * @param Pool 池实例
     * @return 推荐大小
     */
    int32 CalculateRecommendedSize(UClass* ActorClass, const FActorPool& Pool) const;

    /**
     * 检查是否需要清理
     * @param Pool 池实例
     * @return 是否需要清理
     */
    bool ShouldPerformCleanup(const FActorPool& Pool) const;

    /**
     * 检查是否需要预分配
     * @param Pool 池实例
     * @return 是否需要预分配
     */
    bool ShouldPerformPreallocation(const FActorPool& Pool) const;

    /**
     * 更新使用历史
     * @param ActorClass Actor类
     * @param UsageRatio 当前使用率
     */
    void UpdateUsageHistory(UClass* ActorClass, float UsageRatio) const;

    /**
     * 获取策略名称
     * @param Strategy 策略枚举
     * @return 策略名称
     */
    static FString GetStrategyName(EManagementStrategy Strategy);

    /**
     * 获取维护类型名称
     * @param MaintenanceType 维护类型
     * @return 类型名称
     */
    static FString GetMaintenanceTypeName(EMaintenanceType MaintenanceType);

    //  常量定义

    /** 使用历史记录的最大长度 */
    static constexpr int32 MAX_USAGE_HISTORY = 10;

    /** 自动调整的使用率阈值 */
    static constexpr float AUTO_RESIZE_THRESHOLD = 0.8f;

    /** 预分配的使用率阈值 */
    static constexpr float PREALLOCATION_THRESHOLD = 0.7f;

    /** 清理的空闲率阈值 */
    static constexpr float CLEANUP_THRESHOLD = 0.3f;
};

/**
 * 池管理器的日志宏
 */
DECLARE_LOG_CATEGORY_EXTERN(LogObjectPoolManager, Log, All);

#define POOL_MANAGER_LOG(Verbosity, Format, ...) \
    UE_LOG(LogObjectPoolManager, Verbosity, Format, ##__VA_ARGS__)

/**
 * 池管理器的性能统计宏
 */
#if STATS
DECLARE_CYCLE_STAT_EXTERN(TEXT("PoolManager_PerformMaintenance"), STAT_PoolManager_PerformMaintenance, STATGROUP_Game, OBJECTPOOL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("PoolManager_AutoResize"), STAT_PoolManager_AutoResize, STATGROUP_Game, OBJECTPOOL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("PoolManager_SmartPreallocation"), STAT_PoolManager_SmartPreallocation, STATGROUP_Game, OBJECTPOOL_API);
#endif
