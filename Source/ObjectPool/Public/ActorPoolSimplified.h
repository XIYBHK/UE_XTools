// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "HAL/ThreadSafeBool.h"
#include "HAL/CriticalSection.h"
#include "Containers/Array.h"
#include "Templates/SharedPointer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// ✅ 对象池模块依赖
#include "ObjectPoolTypesSimplified.h"
#include "ObjectPoolUtils.h"

/**
 * 简化的Actor对象池实现类
 * 
 * 设计原则：
 * - 单一职责：专注于池的核心管理逻辑
 * - 简化架构：移除复杂的预分配管理器和详细统计
 * - 内联重置：直接使用FObjectPoolUtils进行状态重置
 * - 高性能：使用FRWLock优化读写操作
 * - 线程安全：支持并发的GetActor操作
 * 
 * 简化内容：
 * - 移除复杂的智能预分配功能
 * - 简化统计信息收集
 * - 内联Actor状态重置逻辑
 * - 移除过度设计的配置系统
 * 
 * 保留功能：
 * - 核心的GetActor/ReturnActor逻辑
 * - 基本的预热功能
 * - 线程安全保证
 * - 基本统计信息
 */
class OBJECTPOOL_API FActorPoolSimplified
{
public:
    /**
     * 构造函数
     * @param InActorClass 要池化的Actor类
     * @param InInitialSize 初始池大小
     * @param InHardLimit 池的硬限制（0表示无限制）
     */
    explicit FActorPoolSimplified(UClass* InActorClass, int32 InInitialSize = 10, int32 InHardLimit = 0);

    /** 析构函数 */
    ~FActorPoolSimplified();

    // ✅ 禁用拷贝构造和赋值，确保线程安全
    FActorPoolSimplified(const FActorPoolSimplified&) = delete;
    FActorPoolSimplified& operator=(const FActorPoolSimplified&) = delete;

    // ✅ 支持移动语义
    FActorPoolSimplified(FActorPoolSimplified&& Other) noexcept;
    FActorPoolSimplified& operator=(FActorPoolSimplified&& Other) noexcept;

public:
    // ✅ 核心池管理功能

    /**
     * 从池中获取Actor
     * @param World 世界上下文
     * @param SpawnTransform Actor的生成位置和旋转
     * @return 池化的Actor实例，失败时返回nullptr
     */
    AActor* GetActor(UWorld* World, const FTransform& SpawnTransform = FTransform::Identity);

    /**
     * 将Actor归还到池中
     * @param Actor 要归还的Actor
     * @return 归还是否成功
     */
    bool ReturnActor(AActor* Actor);

    /**
     * 预热池 - 预先创建指定数量的Actor
     * @param World 世界上下文
     * @param Count 要预创建的数量
     */
    void PrewarmPool(UWorld* World, int32 Count);

    // ✅ 状态查询功能

    /**
     * 获取池的统计信息
     * @return 简化的统计信息
     */
    FObjectPoolStatsSimplified GetStats() const;

    /**
     * 获取当前可用的Actor数量
     * @return 可用Actor数量
     */
    int32 GetAvailableCount() const;

    /**
     * 获取当前活跃的Actor数量
     * @return 活跃Actor数量
     */
    int32 GetActiveCount() const;

    /**
     * 获取池的总大小
     * @return 池的总大小（活跃+可用）
     */
    int32 GetPoolSize() const;

    /**
     * 检查池是否为空
     * @return 池是否为空
     */
    bool IsEmpty() const;

    /**
     * 检查池是否已满
     * @return 池是否已满
     */
    bool IsFull() const;

    // ✅ 管理功能

    /**
     * 清空池中的所有Actor
     */
    void ClearPool();

    /**
     * 设置池的最大大小
     * @param NewMaxSize 新的最大大小
     */
    void SetMaxSize(int32 NewMaxSize);

    /**
     * 获取Actor类
     * @return Actor类
     */
    UClass* GetActorClass() const { return ActorClass; }

    /**
     * 检查池是否已初始化
     * @return 是否已初始化
     */
    bool IsInitialized() const { return bIsInitialized; }

public:



private:
    // ✅ 简化的数据成员（遵循单一职责原则）

    /** 要池化的Actor类 */
    UClass* ActorClass;

    /** 池的最大大小限制 */
    int32 MaxPoolSize;

    /** 初始池大小 */
    int32 InitialSize;

    /** 总请求次数 */
    int32 TotalRequests;

    /** 池命中次数 */
    int32 PoolHits;

    /** 总创建的Actor数量 */
    int32 TotalCreated;

    /** 是否已初始化 */
    bool bIsInitialized;

    // ✅ Actor存储（使用弱引用避免循环引用）

    /** 可用的Actor列表 */
    TArray<TWeakObjectPtr<AActor>> AvailableActors;

    /** 活跃的Actor列表 */
    TArray<TWeakObjectPtr<AActor>> ActiveActors;

    /** 读写锁，优化并发访问 */
    mutable FRWLock PoolLock;

private:
    // ✅ 内部辅助方法

    /**
     * 创建新的Actor实例
     * @param World 世界上下文
     * @return 创建的Actor，失败时返回nullptr
     */
    AActor* CreateNewActor(UWorld* World);

    /**
     * 验证Actor是否有效且属于正确类型
     * @param Actor 要验证的Actor
     * @return 验证是否通过
     */
    bool ValidateActor(AActor* Actor) const;

    /**
     * 清理无效的Actor引用
     * 注意：调用者必须持有写锁
     */
    void CleanupInvalidActors();

    /**
     * 更新统计信息
     * @param bWasPoolHit 是否为池命中
     * @param bActorCreated 是否创建了新Actor
     */
    void UpdateStats(bool bWasPoolHit, bool bActorCreated);

    /**
     * 检查是否可以创建更多Actor
     * @return 是否可以创建
     */
    bool CanCreateMoreActors() const;

    /**
     * 初始化池
     * @param World 世界上下文
     */
    void InitializePool(UWorld* World);

    // ✅ 常量定义

    /** 默认的池大小 */
    static constexpr int32 DEFAULT_POOL_SIZE = 10;

    /** 默认的硬限制 */
    static constexpr int32 DEFAULT_HARD_LIMIT = 100;

    /** 清理无效引用的频率（每N次操作清理一次） */
    static constexpr int32 CLEANUP_FREQUENCY = 50;
};

/**
 * 简化池的性能统计宏
 */
#if STATS
DECLARE_CYCLE_STAT_EXTERN(TEXT("ActorPoolSimplified_GetActor"), STAT_ActorPoolSimplified_GetActor, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("ActorPoolSimplified_ReturnActor"), STAT_ActorPoolSimplified_ReturnActor, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("ActorPoolSimplified_CreateActor"), STAT_ActorPoolSimplified_CreateActor, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
#endif

/**
 * 简化池的日志宏
 */
DECLARE_LOG_CATEGORY_EXTERN(LogActorPoolSimplified, Log, All);

#define ACTORPOOL_SIMPLIFIED_LOG(Verbosity, Format, ...) \
    UE_LOG(LogActorPoolSimplified, Verbosity, Format, ##__VA_ARGS__)

/**
 * 简化池的调试宏
 */
#if !UE_BUILD_SHIPPING
#define ACTORPOOL_SIMPLIFIED_DEBUG(Format, ...) \
    ACTORPOOL_SIMPLIFIED_LOG(VeryVerbose, Format, ##__VA_ARGS__)
#else
#define ACTORPOOL_SIMPLIFIED_DEBUG(Format, ...)
#endif
