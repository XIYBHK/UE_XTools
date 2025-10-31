/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

//  遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "HAL/ThreadSafeBool.h"
#include "HAL/CriticalSection.h"
#include "Containers/Array.h"
#include "Templates/SharedPointer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

//  对象池模块依赖
#include "ObjectPoolTypes.h"
#include "ObjectPoolUtils.h"


/**
 * Actor对象池实现类
 * 
 * 设计原则：
 * - 遵循UE官方架构：继承UObject，融入GC系统
 * - 单一职责：专注于池的核心管理逻辑
 * - 简化架构：移除复杂的预分配管理器和详细统计
 * - 内联重置：直接使用FObjectPoolUtils进行状态重置
 * - 高性能：使用FRWLock优化读写操作
 * - 线程安全：支持并发的GetActor操作
 * 
 * UE官方垃圾回收集成：
 * - 继承UObject，自动参与GC系统
 * - 通过UPROPERTY管理Actor引用
 * - 与子系统生命周期完全同步
 * 
 * 核心功能：
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
/**
 * Actor对象池实现类 - 遵循UE官方架构设计
 */
class OBJECTPOOL_API FActorPool
{
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

    //  禁用拷贝构造和赋值，确保线程安全
    FActorPool(const FActorPool&) = delete;
    FActorPool& operator=(const FActorPool&) = delete;

    //  支持移动语义
    FActorPool(FActorPool&& Other) noexcept;
    FActorPool& operator=(FActorPool&& Other) noexcept;

public:
    //  核心池管理功能

    /**
     * 从池中获取Actor
     * @param World 世界上下文
     * @param SpawnTransform Actor的生成位置和旋转
     * @return 池化的Actor实例，失败时返回nullptr
     */
		AActor* GetActor(UWorld* World, const FTransform& SpawnTransform = FTransform::Identity);

		/**
		 * 从池中获取Actor（延迟激活版本）
		 * - 不调用激活/FinishSpawning
		 * - 仅从可用列表取出或新建延迟构造的实例
		 * - 最终应由 FinalizeDeferred 进行激活并加入活跃列表
		 */
		AActor* AcquireDeferred(UWorld* World);

		/**
		 * 完成延迟获取的Actor的激活过程
		 * - 应用Transform、启用碰撞/Tick
		 * - 对首次创建的延迟构造Actor调用FinishSpawning
		 * - 将Actor加入活跃列表
		 */
		bool FinalizeDeferred(AActor* Actor, const FTransform& SpawnTransform);

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

    //  状态查询功能

    /**
     * 获取池的统计信息
     * @return 统计信息
     */
    FObjectPoolStats GetStats() const;

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
     * 获取池的最大容量上限
     * @return 最大池大小
     */
    int32 GetMaxSize() const { return MaxPoolSize; }

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

    /**
     * 检查给定Actor是否由该池管理（在活跃或可用列表中）
     */
    bool ContainsActor(const AActor* Actor) const;

    //  管理功能

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
    //  数据成员（遵循单一职责原则）

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

    //  基于UE标准的Actor存储优化
    
    /** 可用的Actor列表 - 使用动态分配器支持压力测试 */
    TArray<TWeakObjectPtr<AActor>> AvailableActors;

    /** 活跃的Actor列表 - 使用动态分配器支持压力测试 */
    TArray<TWeakObjectPtr<AActor>> ActiveActors;
    
    /** Actor类的快速查找缓存 - UE标准哈希优化 */
    mutable TSoftObjectPtr<UClass> CachedActorClass;

    /** 读写锁，优化并发访问 - 基于UE线程安全最佳实践 */
    mutable FRWLock PoolLock;
    
    /** UE标准的垃圾回收协调 */
    mutable FCriticalSection GCSection;
    
    /** Actor预分配缓存 - 基于UE内存预分配策略 */
    mutable TArray<TSoftObjectPtr<AActor>> PreallocationCache;
    
    /** GC委托句柄 - 用于安全清理GC回调，避免悬挂指针 */
    FDelegateHandle GCDelegateHandle;

private:
    //  内部辅助方法
    


public:
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

public:
    /**
     * 清空整个池
     */
    void Clear();

    /**
     * 计算内存使用量（字节）
     * @return 内存使用量
     */
    int64 CalculateMemoryUsage() const;

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

    //  常量定义

    /** 默认的池大小 */
    static constexpr int32 DEFAULT_POOL_SIZE = 10;

    /** 默认的硬限制 */
    static constexpr int32 DEFAULT_HARD_LIMIT = 100;

    /** 清理无效引用的频率（每N次操作清理一次） */
    static constexpr int32 CLEANUP_FREQUENCY = 50;
};

/**
 * 性能统计宏
 */
#if STATS
DECLARE_CYCLE_STAT_EXTERN(TEXT("ActorPool_GetActor"), STAT_ActorPool_GetActor, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("ActorPool_ReturnActor"), STAT_ActorPool_ReturnActor, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("ActorPool_CreateActor"), STAT_ActorPool_CreateActor, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
#endif

/**
 * 日志宏
 */
DECLARE_LOG_CATEGORY_EXTERN(LogActorPool, Log, All);

#define ACTORPOOL_LOG(Verbosity, Format, ...) \
    UE_LOG(LogActorPool, Verbosity, Format, ##__VA_ARGS__)

/**
 * 调试宏
 */
#if !UE_BUILD_SHIPPING
#define ACTORPOOL_DEBUG(Format, ...) \
    ACTORPOOL_LOG(VeryVerbose, Format, ##__VA_ARGS__)
#else
#define ACTORPOOL_DEBUG(Format, ...)
#endif