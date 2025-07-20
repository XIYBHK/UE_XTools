// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "HAL/ThreadSafeBool.h"
#include "HAL/CriticalSection.h"
#include "HAL/RunnableThread.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include <atomic>
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// ✅ 对象池模块依赖
#include "ObjectPoolTypes.h"
#include "ObjectPoolInterface.h"

// ✅ 前向声明
class UWorld;
class FObjectPoolPreallocator;
class AActor;
class FReferenceCollector;

/**
 * Actor对象池核心实现类
 *
 * @deprecated 此类已被FActorPoolSimplified替代，将在未来版本中移除。
 *             请使用FActorPoolSimplified以获得更好的性能和更简洁的实现。
 *             迁移指南：通过UObjectPoolSubsystemSimplified访问新的简化实现。
 *
 * 设计理念：
 * - 基于UE最佳实践，使用FRWLock实现线程安全
 * - 支持Actor生命周期管理和自动状态重置
 * - 提供智能预分配和自动扩展功能
 * - 内置性能统计和监控功能
 *
 * 线程安全：
 * - 使用FRWLock读写锁，优化读多写少场景
 * - GetActor操作使用读锁（支持并发）
 * - ReturnActor和管理操作使用写锁
 */
class OBJECTPOOL_API FActorPool
{
    // ✅ 友元类：允许预分配管理器访问私有方法
    friend class FObjectPoolPreallocator;

public:
    /**
     * 构造函数
     * @param InActorClass 要池化的Actor类
     * @param InInitialSize 初始池大小
     * @param InHardLimit 池的硬限制（0表示无限制）
     */
    explicit FActorPool(UClass* InActorClass, int32 InInitialSize = 10, int32 InHardLimit = 0);

    /** 析构函数 */
    ~FActorPool();

    // ✅ 禁用拷贝构造和赋值，确保线程安全
    FActorPool(const FActorPool&) = delete;
    FActorPool& operator=(const FActorPool&) = delete;

    // ✅ 支持移动语义
    FActorPool(FActorPool&& Other) noexcept;
    FActorPool& operator=(FActorPool&& Other) noexcept;

public:
    /**
     * 从池中获取Actor
     * @param World 世界上下文
     * @param SpawnTransform Actor的生成位置和旋转
     * @return 池化的Actor实例，永不返回nullptr
     */
    AActor* GetActor(UWorld* World, const FTransform& SpawnTransform = FTransform::Identity);

    /**
     * 将Actor归还到池中
     * @param Actor 要归还的Actor
     * @return true if successfully returned
     */
    bool ReturnActor(AActor* Actor);

    /**
     * 预热池 - 预先创建指定数量的Actor
     * @param World 世界上下文
     * @param Count 要预创建的数量
     */
    void PrewarmPool(UWorld* World, int32 Count);

    // ✅ 智能预分配功能（委托给专门的管理器）

    /**
     * 启动智能预分配
     * @param World 世界上下文
     * @param Config 预分配配置
     * @return 是否成功启动预分配
     */
    bool StartSmartPreallocation(UWorld* World, const FObjectPoolPreallocationConfig& Config);

    /**
     * 停止智能预分配
     */
    void StopSmartPreallocation();

    /**
     * 更新智能预分配（每帧调用）
     * @param DeltaTime 帧时间间隔
     */
    void TickSmartPreallocation(float DeltaTime);

    /**
     * 获取预分配统计信息
     * @return 预分配统计数据
     */
    FObjectPoolPreallocationStats GetPreallocationStats() const;

    /**
     * 计算当前内存使用量
     * @return 内存使用量（字节）
     */
    int64 CalculateMemoryUsage() const;

    /**
     * 清空池 - 销毁所有池化的Actor
     */
    void ClearPool();

    /**
     * 获取池统计信息
     * @return 统计信息结构体
     */
    FObjectPoolStats GetStats() const;

    /**
     * 获取可用Actor数量
     * @return 当前可用的Actor数量
     */
    int32 GetAvailableCount() const;

    /**
     * 获取活跃Actor数量
     * @return 当前活跃的Actor数量
     */
    int32 GetActiveCount() const;

    /**
     * 获取池的总容量
     * @return 池的最大容量
     */
    int32 GetPoolCapacity() const { return HardLimit; }

    /**
     * 检查池是否为空
     * @return true if no actors available
     */
    bool IsEmpty() const;

    /**
     * 检查池是否已满
     * @return true if pool has reached hard limit
     */
    bool IsFull() const;

    /**
     * 获取Actor类
     * @return Actor类指针
     */
    UClass* GetActorClass() const { return ActorClass; }

    /**
     * 获取当前World（用于预分配管理器）
     * @return 当前World实例
     */
    UWorld* GetWorld() const;

    /**
     * 设置池的硬限制
     * @param NewLimit 新的硬限制（0表示无限制）
     */
    void SetHardLimit(int32 NewLimit);

    /**
     * 验证池的完整性
     * @return true if pool is in valid state
     */
    bool ValidatePool() const;

private:
    /**
     * 创建新的Actor实例
     * @param World 世界上下文
     * @param SpawnTransform 生成位置
     * @return 新创建的Actor
     */
    AActor* CreateNewActor(UWorld* World, const FTransform& SpawnTransform);

    /**
     * 重置Actor状态（委托给子系统）
     * @param Actor 要重置的Actor
     * @param SpawnTransform 新的Transform
     */
    void ResetActorState(AActor* Actor, const FTransform& SpawnTransform);

    /**
     * 基础重置回退方法（当子系统不可用时）
     * @param Actor 目标Actor
     * @param SpawnTransform 新的Transform
     */
    void FallbackBasicReset(AActor* Actor, const FTransform& SpawnTransform);

    /**
     * 验证Actor是否有效
     * @param Actor 要验证的Actor
     * @return true if actor is valid for pooling
     */
    bool ValidateActor(AActor* Actor) const;

    /**
     * 调用Actor的生命周期事件
     * @param Actor 目标Actor
     * @param EventType 事件类型
     */
    void CallLifecycleEvent(AActor* Actor, const FString& EventType) const;

    /**
     * 清理无效的Actor引用
     */
    void CleanupInvalidActors();

public:
    /**
     * 向GC报告引用的对象
     * @param Collector GC引用收集器
     */
    void AddReferencedObjects(FReferenceCollector& Collector);

    /**
     * 获取池统计信息
     * @return 池的详细统计信息
     */
    FActorPoolStats GetPoolStats() const;

    // ✅ 增强的线程安全功能

    /**
     * 智能读锁：带超时和死锁检测
     * @param TimeoutMs 超时时间（毫秒）
     * @return 是否成功获取锁
     */
    bool TryReadLock(int32 TimeoutMs = 5000) const;

    /**
     * 智能写锁：带超时和死锁检测
     * @param TimeoutMs 超时时间（毫秒）
     * @return 是否成功获取锁
     */
    bool TryWriteLock(int32 TimeoutMs = 5000) const;

    /**
     * 释放读锁
     */
    void ReleaseReadLock() const;

    /**
     * 释放写锁
     */
    void ReleaseWriteLock() const;

    /**
     * 检查当前线程是否持有锁
     * @return true if current thread holds any lock
     */
    bool IsLockHeldByCurrentThread() const;

    /**
     * 获取锁性能统计
     * @return 锁使用统计信息
     */
    struct FLockPerformanceStats
    {
        uint64 ReadLockCount = 0;
        uint64 WriteLockCount = 0;
        uint64 ContentionCount = 0;
        double AverageLockTimeMs = 0.0;
        double MaxLockTimeMs = 0.0;
    };
    FLockPerformanceStats GetLockStats() const;

    // ✅ 智能锁包装器

    /**
     * RAII智能读锁
     * 自动管理锁的获取和释放，支持超时和死锁检测
     */
    class FScopedSmartReadLock
    {
    public:
        explicit FScopedSmartReadLock(const FActorPool& InPool, int32 TimeoutMs = 5000)
            : Pool(InPool), bLockAcquired(false)
        {
            bLockAcquired = Pool.TryReadLock(TimeoutMs);
        }

        ~FScopedSmartReadLock()
        {
            if (bLockAcquired)
            {
                Pool.ReleaseReadLock();
            }
        }

        bool IsLocked() const { return bLockAcquired; }

        // 禁止拷贝和移动
        FScopedSmartReadLock(const FScopedSmartReadLock&) = delete;
        FScopedSmartReadLock& operator=(const FScopedSmartReadLock&) = delete;
        FScopedSmartReadLock(FScopedSmartReadLock&&) = delete;
        FScopedSmartReadLock& operator=(FScopedSmartReadLock&&) = delete;

    private:
        const FActorPool& Pool;
        bool bLockAcquired;
    };

    /**
     * RAII智能写锁
     * 自动管理锁的获取和释放，支持超时和死锁检测
     */
    class FScopedSmartWriteLock
    {
    public:
        explicit FScopedSmartWriteLock(const FActorPool& InPool, int32 TimeoutMs = 5000)
            : Pool(InPool), bLockAcquired(false)
        {
            bLockAcquired = Pool.TryWriteLock(TimeoutMs);
        }

        ~FScopedSmartWriteLock()
        {
            if (bLockAcquired)
            {
                Pool.ReleaseWriteLock();
            }
        }

        bool IsLocked() const { return bLockAcquired; }

        // 禁止拷贝和移动
        FScopedSmartWriteLock(const FScopedSmartWriteLock&) = delete;
        FScopedSmartWriteLock& operator=(const FScopedSmartWriteLock&) = delete;
        FScopedSmartWriteLock(FScopedSmartWriteLock&&) = delete;
        FScopedSmartWriteLock& operator=(FScopedSmartWriteLock&&) = delete;

    private:
        const FActorPool& Pool;
        bool bLockAcquired;
    };

    /**
     * 更新统计信息
     * @param EventType 事件类型
     */
    void UpdateStats(EObjectPoolEvent EventType);

private:
    /** Actor类 */
    UClass* ActorClass;

    /** 可用的Actor列表 */
    TArray<TWeakObjectPtr<AActor>> AvailableActors;

    /** 活跃的Actor列表 */
    TArray<TWeakObjectPtr<AActor>> ActiveActors;

    /** 池的硬限制 */
    int32 HardLimit;

    /** 初始池大小 */
    int32 InitialSize;

    /** ✅ 读写锁 - 优化读多写少场景 */
    mutable FRWLock PoolLock;

    /** 统计锁，保护统计数据的并发访问 */
    mutable FRWLock StatsLock;

    /** 统计信息 */
    mutable FObjectPoolStats Stats;

    /** 性能计数器 */
    mutable int32 TotalRequests;
    mutable int32 PoolHits;

    /** 扩展统计变量 */
    mutable int32 TotalCreatedCount = 0;
    mutable int32 TotalRequestCount = 0;
    mutable int32 PoolHitCount = 0;
    mutable int32 MaxPoolSize = 0;
    mutable FDateTime LastUsedTime;

    /** 池是否已初始化 */
    bool bIsInitialized;

    // ✅ 增强的线程安全数据

    /** 线程安全配置 */
    struct FThreadSafetyConfig
    {
        /** 是否启用死锁检测 */
        bool bEnableDeadlockDetection = true;

        /** 锁超时时间（毫秒） */
        int32 LockTimeoutMs = 5000;

        /** 是否启用锁性能统计 */
        bool bEnableLockProfiling = false;

        /** 最大并发读取线程数 */
        int32 MaxConcurrentReaders = 16;
    } ThreadSafetyConfig;

    /** 锁性能统计（原子操作保证线程安全） */
    mutable std::atomic<uint64> ReadLockCount{0};
    mutable std::atomic<uint64> WriteLockCount{0};
    mutable std::atomic<uint64> LockContentionCount{0};
    mutable std::atomic<double> TotalLockTimeMs{0.0};
    mutable std::atomic<double> MaxLockTimeMs{0.0};

    /** 当前持有锁的线程ID（用于死锁检测） */
    mutable std::atomic<uint32> CurrentLockHolderThreadId{0};

    /** 锁类型：0=无锁，1=读锁，2=写锁 */
    mutable std::atomic<int32> CurrentLockType{0};

    // ✅ 智能预分配管理器（单一职责）
    TUniquePtr<FObjectPoolPreallocator> Preallocator;

    // ✅ 智能预分配相关数据

    /** 预分配配置 */
    FObjectPoolPreallocationConfig PreallocationConfig;

    /** 预分配统计信息 */
    mutable FObjectPoolPreallocationStats PreallocationStats;

    /** 是否正在进行预分配 */
    std::atomic<bool> bIsPreallocating{false};

    /** 预分配进度（已分配数量） */
    std::atomic<int32> PreallocationProgress{0};

    /** 预分配累计时间 */
    float PreallocationAccumulatedTime = 0.0f;

    /** 上次动态调整时间 */
    double LastDynamicAdjustmentTime = 0.0;

    /** 使用模式历史记录（用于预测） */
    TArray<int32> UsageHistory;

    /** 最大历史记录数量 */
    static constexpr int32 MaxUsageHistorySize = 100;

    /** 池创建时间 */
    FDateTime CreationTime;
};
