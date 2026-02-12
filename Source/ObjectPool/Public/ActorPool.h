/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/Actor.h"
#include "ObjectPoolTypes.h"
#include "ObjectPoolUtils.h"

// 包含日志头文件以支持宏
DECLARE_LOG_CATEGORY_EXTERN(LogActorPool, Log, All);

// 性能统计宏
#if STATS
DECLARE_CYCLE_STAT_EXTERN(TEXT("ActorPool_GetActor"), STAT_ActorPool_GetActor, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("ActorPool_ReturnActor"), STAT_ActorPool_ReturnActor, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("ActorPool_CreateActor"), STAT_ActorPool_CreateActor, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
#endif

// 调试宏
#if !UE_BUILD_SHIPPING
#define ACTORPOOL_DEBUG(Format, ...) UE_LOG(LogActorPool, VeryVerbose, Format, ##__VA_ARGS__)
#else
#define ACTORPOOL_DEBUG(Format, ...)
#endif

/**
 * 单个Actor类的对象池管理类
 *
 * 负责特定Actor类的对象生命周期管理，包括预热、分配、回收和销毁。
 * 支持灵活的策略调整和详细的统计信息。
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

public:
    /**
     * 从池中获取一个Actor
     * @param World 世界上下文
     * @param SpawnTransform 生成变换
     * @return 获取到的Actor，如果失败则返回nullptr
     */
    AActor* GetActor(UWorld* World, const FTransform& SpawnTransform = FTransform::Identity);

    /**
     * 归还Actor到池中
     * @param Actor要归还的Actor
     * @return 是否成功归还
     */
    bool ReturnActor(AActor* Actor);

    /**
     * 预热池（预先创建指定数量的Actor）
     * @param World 世界上下文
     * @param Count 要预热的数量
     */
    void PrewarmPool(UWorld* World, int32 Count);
    
    /**
     * 初始化池（兼容旧API）
     */
    void InitializePool(UWorld* World);

    /**
     * 获取当前统计信息
     * @return 统计信息结构体
     */
    FObjectPoolStats GetStats() const;

    /**
     * 获取当前可用的Actor数量
     */
    int32 GetAvailableCount() const;

    /**
     * 获取当前活跃的Actor数量
     */
    int32 GetActiveCount() const;

    /**
     * 获取池的总大小
     */
    int32 GetPoolSize() const;

    /**
     * 获取池的最大容量上限
     */
    int32 GetMaxSize() const { return MaxPoolSize; }
    
    /**
     * 检查池是否为空
     */
    bool IsEmpty() const;

    /**
     * 检查池是否已满
     */
    bool IsFull() const;

    /**
     * 设置池的最大大小
     */
    void SetMaxSize(int32 NewMaxSize);

    /**
     * 清空整个池
     */
    void ClearPool();

    /**
     * 检查给定Actor是否由该池管理
     */
    bool ContainsActor(const AActor* Actor) const;

    /**
     * 获取池管理的Actor类
     */
    TSubclassOf<AActor> GetActorClass() const { return ActorClass; }

    /**
     * 检查池是否已初始化
     */
    bool IsInitialized() const { return bIsInitialized; }

    /**
     * 延迟获取Actor（仅逻辑取出，暂不激活）
     */
    AActor* AcquireDeferred(UWorld* World);
    
    /**
     * 完成延迟获取的Actor的初始化
     */
    bool FinalizeDeferred(AActor* Actor, const FTransform& SpawnTransform);

    /**
     * 销毁所有不可用的Actor（用于强制清理）
     */
    void CleanupInvalidActors();

    /**
     * 计算内存使用量（字节）
     */
    int64 CalculateMemoryUsage() const;

    // =========================================================================================
    //  内部状态与配置常量
    // =========================================================================================

    /** 池的最大容量限制，防止内存耗尽 */
    static constexpr int32 MAX_POOL_CAPACITY = 10000;
    
    /** 清理频率（每多少次请求清理一次） */
    static constexpr int32 CLEANUP_FREQUENCY = 100;
    
    /** 池收缩阈值（当闲置过多久后清理）- 单位：秒 */
    static constexpr float SHRINK_THRESHOLD_SECONDS = 60.0f;
    
    /** 默认的硬限制 */
    static constexpr int32 DEFAULT_HARD_LIMIT = 100;

private:
    /** 池管理的Actor类 */
    TSubclassOf<AActor> ActorClass;

    /** 池的最大大小限制 */
    int32 MaxPoolSize;

    /** 初始池大小 */
    int32 InitialSize;

    /** 是否已初始化 */
    bool bIsInitialized;

    /** 总请求次数 */
    int64 TotalRequests;

    /** 池命中次数 */
    int32 PoolHits;

    /** 总创建的Actor数量 */
    int32 TotalCreated;

    /** 总归还次数（用于平衡统计信息） */
    int32 TotalReturned;

    /** 统计数据(缓存) */
    mutable FObjectPoolStats PoolStats; 
    
    /** 可用的Actor列表 */
    TArray<TWeakObjectPtr<AActor>> AvailableActors;

    /** 活跃的Actor列表 */
    TArray<TWeakObjectPtr<AActor>> ActiveActors;

    /** 快速查找索引（用于ContainsActor O(1)查找） */
    TSet<TWeakObjectPtr<AActor>> AllActorsSet;

    /** 读写锁，优化并发访问 */
    mutable FRWLock PoolLock;

    /** GC委托句柄 - 用于安全清理GC回调 */
    FDelegateHandle GCDelegateHandle;

    /** 声明 Preallocator 为友元，允许其访问 CreateNewActor 以执行预分配 */
    friend class FObjectPoolPreallocator;

private:    //  内部辅助方法
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
     * 检查是否可以创建更多Actor
     */
    bool CanCreateMoreActors() const;

    /**
     * 更新统计信息
     * @param bWasPoolHit 是否为池命中
     */
    void UpdateStats(bool bWasPoolHit);

    /**
     * 从可用列表中取出一个有效Actor（需持有写锁）
     * @return 取出的Actor，无可用时返回nullptr
     */
    AActor* TakeFromAvailable_RequiresLock();

    /**
     * 定期清理检查（需持有写锁）
     */
    void PeriodicCleanup_RequiresLock();
};
