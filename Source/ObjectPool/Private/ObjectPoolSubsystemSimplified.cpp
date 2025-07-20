// Fill out your copyright notice in the Description page of Project Settings.

#include "ObjectPoolSubsystemSimplified.h"
#include "ObjectPool.h"

// ✅ 独立工具类
#include "ObjectPoolConfigManagerSimplified.h"
#include "ObjectPoolManager.h"
// #include "ObjectPoolMonitor.h" // 暂时移除，后续实现

// ✅ UE核心依赖
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"

// ✅ 日志和统计
DEFINE_LOG_CATEGORY(LogObjectPoolSubsystemSimplified);

#if STATS
DEFINE_STAT(STAT_ObjectPoolSubsystem_SpawnActor);
DEFINE_STAT(STAT_ObjectPoolSubsystem_ReturnActor);
DEFINE_STAT(STAT_ObjectPoolSubsystem_GetOrCreatePool);
#endif

// ✅ USubsystem接口实现

void UObjectPoolSubsystemSimplified::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 创建独立工具类
    ConfigManager = MakeUnique<FObjectPoolConfigManagerSimplified>();
    PoolManager = MakeUnique<FObjectPoolManager>();
    
    // 监控器是可选的
    bMonitoringEnabled = false;

    // ✅ 初始化缓存
    LastAccessedClass = nullptr;

    // 记录启动时间
    SubsystemStats.StartupTime = FPlatformTime::Seconds();
    SubsystemStats.LastMaintenanceTime = SubsystemStats.StartupTime;

    bIsInitialized = true;

    OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("对象池子系统已初始化"));
}

void UObjectPoolSubsystemSimplified::Deinitialize()
{
    if (bIsInitialized)
    {
        // ✅ 确保线程安全的清理
        // 清空所有池（内部已有锁保护）
        ClearAllPools();

        // 清理工具类（在主线程中执行，无需额外锁）
        PoolManager.Reset();
        ConfigManager.Reset();

        // 原子操作，无需锁
        bIsInitialized = false;

        OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("对象池子系统已清理"));
    }

    Super::Deinitialize();
}

bool UObjectPoolSubsystemSimplified::ShouldCreateSubsystem(UObject* Outer) const
{
    // 只在游戏世界中创建
    if (UWorld* World = Cast<UWorld>(Outer))
    {
        return World->IsGameWorld();
    }
    return false;
}

// ✅ 核心对象池API实现

AActor* UObjectPoolSubsystemSimplified::SpawnActorFromPool(UClass* ActorClass, const FTransform& SpawnTransform)
{
    SCOPE_CYCLE_COUNTER(STAT_ObjectPoolSubsystem_SpawnActor);

    // ✅ 更新统计信息
    ++SubsystemStats.TotalSpawnCalls;

    if (!ValidateActorClass(ActorClass))
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Warning, TEXT("SpawnActorFromPool: 无效的Actor类"));
        return nullptr;
    }

    // 获取或创建池
    TSharedPtr<FActorPoolSimplified> Pool = GetOrCreatePool(ActorClass);
    if (!Pool.IsValid())
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Error, TEXT("SpawnActorFromPool: 无法创建池 %s"), *ActorClass->GetName());
        return nullptr;
    }

    // 从池中获取Actor
    AActor* Actor = Pool->GetActor(GetWorld(), SpawnTransform);
    
    // 暂时禁用监控器功能
    // if (Actor && Monitor.IsValid())
    // {
    //     // 通知监控器
    //     Monitor->OnActorSpawned(ActorClass, Actor);
    // }

    OBJECTPOOL_SUBSYSTEM_LOG(VeryVerbose, TEXT("从池生成Actor: %s"), 
        Actor ? *Actor->GetName() : TEXT("失败"));

    return Actor;
}

AActor* UObjectPoolSubsystemSimplified::SpawnActorFromPoolSimple(UClass* ActorClass)
{
    return SpawnActorFromPool(ActorClass, FTransform::Identity);
}

bool UObjectPoolSubsystemSimplified::ReturnActorToPool(AActor* Actor)
{
    SCOPE_CYCLE_COUNTER(STAT_ObjectPoolSubsystem_ReturnActor);

    // ✅ 更新统计信息
    ++SubsystemStats.TotalReturnCalls;

    if (!IsValid(Actor))
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Warning, TEXT("ReturnActorToPool: 无效的Actor"));
        return false;
    }

    UClass* ActorClass = Actor->GetClass();
    
    // 获取对应的池
    TSharedPtr<FActorPoolSimplified> Pool = GetPool(ActorClass);
    if (!Pool.IsValid())
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Warning, TEXT("ReturnActorToPool: 找不到对应的池 %s"), *ActorClass->GetName());
        return false;
    }

    // 归还到池
    bool bSuccess = Pool->ReturnActor(Actor);
    
    // 暂时禁用监控器功能
    // if (bSuccess && Monitor.IsValid())
    // {
    //     // 通知监控器
    //     Monitor->OnActorReturned(ActorClass, Actor);
    // }

    OBJECTPOOL_SUBSYSTEM_LOG(VeryVerbose, TEXT("归还Actor到池: %s, 结果=%s"), 
        *Actor->GetName(), bSuccess ? TEXT("成功") : TEXT("失败"));

    return bSuccess;
}

int32 UObjectPoolSubsystemSimplified::PrewarmPool(UClass* ActorClass, int32 Count)
{
    if (!ValidateActorClass(ActorClass) || Count <= 0)
    {
        return 0;
    }

    TSharedPtr<FActorPoolSimplified> Pool = GetOrCreatePool(ActorClass);
    if (!Pool.IsValid())
    {
        return 0;
    }

    Pool->PrewarmPool(GetWorld(), Count);
    
    OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("预热池: %s, 数量=%d"), *ActorClass->GetName(), Count);
    
    return Pool->GetAvailableCount();
}

// ✅ 池管理功能实现

TSharedPtr<FActorPoolSimplified> UObjectPoolSubsystemSimplified::GetOrCreatePool(UClass* ActorClass)
{
    SCOPE_CYCLE_COUNTER(STAT_ObjectPoolSubsystem_GetOrCreatePool);

    if (!ValidateActorClass(ActorClass))
    {
        return nullptr;
    }

    // ✅ 快速缓存检查（无锁优化）
    {
        FScopeLock CacheReadLock(&CacheLock);
        if (LastAccessedClass == ActorClass && LastAccessedPool.IsValid())
        {
            TSharedPtr<FActorPoolSimplified> CachedPool = LastAccessedPool.Pin();
            if (CachedPool.IsValid())
            {
                return CachedPool;
            }
        }
    }

    // ✅ 优化的双重检查锁定模式
    // 首先使用读锁进行快速检查
    TSharedPtr<FActorPoolSimplified> FoundPool;
    {
        FReadScopeLock ReadLock(PoolsRWLock);
        if (TSharedPtr<FActorPoolSimplified>* ExistingPool = ActorPools.Find(ActorClass))
        {
            FoundPool = *ExistingPool;
        }
    }

    if (FoundPool.IsValid())
    {
        // ✅ 更新缓存
        UpdatePoolCache(ActorClass, FoundPool);
        return FoundPool;
    }

    // 如果不存在，使用写锁创建新池
    {
        FWriteScopeLock WriteLock(PoolsRWLock);

        // 再次检查，防止在锁切换期间其他线程已创建
        if (TSharedPtr<FActorPoolSimplified>* ExistingPool = ActorPools.Find(ActorClass))
        {
            TSharedPtr<FActorPoolSimplified> ExistingPoolPtr = *ExistingPool;
            UpdatePoolCache(ActorClass, ExistingPoolPtr);
            return ExistingPoolPtr;
        }

        // 创建新池
        TSharedPtr<FActorPoolSimplified> NewPool = CreatePool(ActorClass);
        if (NewPool.IsValid())
        {
            UpdatePoolCache(ActorClass, NewPool);
        }
        return NewPool;
    }
}

TSharedPtr<FActorPoolSimplified> UObjectPoolSubsystemSimplified::GetPool(UClass* ActorClass) const
{
    if (!ValidateActorClass(ActorClass))
    {
        return nullptr;
    }

    // ✅ 使用读锁进行高效查找
    FReadScopeLock ReadLock(PoolsRWLock);

    if (const TSharedPtr<FActorPoolSimplified>* Pool = ActorPools.Find(ActorClass))
    {
        return *Pool;
    }

    return nullptr;
}

bool UObjectPoolSubsystemSimplified::RemovePool(UClass* ActorClass)
{
    if (!ValidateActorClass(ActorClass))
    {
        return false;
    }

    FWriteScopeLock WriteLock(PoolsRWLock);

    if (TSharedPtr<FActorPoolSimplified>* Pool = ActorPools.Find(ActorClass))
    {
        // 清空池
        (*Pool)->ClearPool();

        // 从映射中移除
        ActorPools.Remove(ActorClass);

        OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("移除池: %s"), *ActorClass->GetName());
        return true;
    }

    return false;
}

void UObjectPoolSubsystemSimplified::ClearAllPools()
{
    FWriteScopeLock WriteLock(PoolsRWLock);

    for (auto& PoolPair : ActorPools)
    {
        if (PoolPair.Value.IsValid())
        {
            PoolPair.Value->ClearPool();
        }
    }

    ActorPools.Empty();

    // ✅ 清理缓存
    ClearPoolCache();

    OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("清空所有池"));
}

int32 UObjectPoolSubsystemSimplified::GetPoolCount() const
{
    FReadScopeLock ReadLock(PoolsRWLock);
    return ActorPools.Num();
}

// ✅ 状态查询功能实现

FObjectPoolStatsSimplified UObjectPoolSubsystemSimplified::GetPoolStats(UClass* ActorClass) const
{
    TSharedPtr<FActorPoolSimplified> Pool = GetPool(ActorClass);
    if (Pool.IsValid())
    {
        return Pool->GetStats();
    }

    // 返回空统计
    FObjectPoolStatsSimplified EmptyStats;
    EmptyStats.ActorClassName = ActorClass ? ActorClass->GetName() : TEXT("Unknown");
    return EmptyStats;
}

TArray<FObjectPoolStatsSimplified> UObjectPoolSubsystemSimplified::GetAllPoolStats() const
{
    TArray<FObjectPoolStatsSimplified> AllStats;

    FReadScopeLock ReadLock(PoolsRWLock);

    for (const auto& PoolPair : ActorPools)
    {
        if (PoolPair.Value.IsValid())
        {
            AllStats.Add(PoolPair.Value->GetStats());
        }
    }

    return AllStats;
}

bool UObjectPoolSubsystemSimplified::HasPool(UClass* ActorClass) const
{
    return GetPool(ActorClass).IsValid();
}

// ✅ 静态访问方法

UObjectPoolSubsystemSimplified* UObjectPoolSubsystemSimplified::Get(const UObject* WorldContext)
{
    if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull))
    {
        return World->GetSubsystem<UObjectPoolSubsystemSimplified>();
    }
    return nullptr;
}

// ✅ 配置管理（委托给配置管理器）

bool UObjectPoolSubsystemSimplified::SetPoolConfig(UClass* ActorClass, const FObjectPoolConfigSimplified& Config)
{
    if (!ValidateActorClass(ActorClass) || !ConfigManager.IsValid())
    {
        return false;
    }

    // ✅ 检查是否已经配置过，避免重复配置导致的问题
    if (ConfigManager->HasConfig(ActorClass))
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Verbose, TEXT("Actor类已配置，跳过重复配置: %s"), *ActorClass->GetName());
        return true;
    }

    // 委托给配置管理器
    bool bSuccess = ConfigManager->SetConfig(ActorClass, Config);

    if (bSuccess)
    {
        // 如果池已存在，应用新配置
        TSharedPtr<FActorPoolSimplified> Pool = GetPool(ActorClass);
        if (Pool.IsValid())
        {
            ConfigManager->ApplyConfigToPool(*Pool, Config);
        }

        OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("设置池配置: %s"), *ActorClass->GetName());
    }

    return bSuccess;
}

FObjectPoolConfigSimplified UObjectPoolSubsystemSimplified::GetPoolConfig(UClass* ActorClass) const
{
    if (ValidateActorClass(ActorClass) && ConfigManager.IsValid())
    {
        return ConfigManager->GetConfig(ActorClass);
    }

    // 返回默认配置
    return ConfigManager.IsValid() ? ConfigManager->GetDefaultConfig() : FObjectPoolConfigSimplified();
}

// ✅ 工具类访问接口

FObjectPoolConfigManagerSimplified& UObjectPoolSubsystemSimplified::GetConfigManager() const
{
    check(ConfigManager.IsValid());
    return *ConfigManager;
}

FObjectPoolManager& UObjectPoolSubsystemSimplified::GetPoolManager() const
{
    check(PoolManager.IsValid());
    return *PoolManager;
}

void UObjectPoolSubsystemSimplified::SetMonitoringEnabled(bool bEnable)
{
    // 暂时禁用监控器功能
    bMonitoringEnabled = bEnable;
    OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("%s性能监控（暂未实现）"), bEnable ? TEXT("启用") : TEXT("禁用"));

    // TODO: 实现监控器功能
    // if (bEnable && !Monitor.IsValid())
    // {
    //     // 创建监控器
    //     Monitor = MakeUnique<FObjectPoolMonitor>();
    //     bMonitoringEnabled = true;
    //     OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("启用性能监控"));
    // }
    // else if (!bEnable && Monitor.IsValid())
    // {
    //     // 销毁监控器
    //     Monitor.Reset();
    //     bMonitoringEnabled = false;
    //     OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("禁用性能监控"));
    // }
}

// ✅ 内部辅助方法实现

TSharedPtr<FActorPoolSimplified> UObjectPoolSubsystemSimplified::CreatePool(UClass* ActorClass)
{
    // 注意：调用者应该持有锁

    if (!ValidateActorClass(ActorClass))
    {
        return nullptr;
    }

    // 获取配置
    FObjectPoolConfigSimplified Config = ConfigManager.IsValid() ?
        ConfigManager->GetConfig(ActorClass) : FObjectPoolConfigSimplified();

    // 创建池
    TSharedPtr<FActorPoolSimplified> NewPool = MakeShared<FActorPoolSimplified>(
        ActorClass,
        Config.InitialSize > 0 ? Config.InitialSize : DEFAULT_POOL_INITIAL_SIZE,
        Config.HardLimit > 0 ? Config.HardLimit : DEFAULT_POOL_MAX_SIZE
    );

    if (NewPool.IsValid())
    {
        // 添加到映射
        ActorPools.Add(ActorClass, NewPool);

        // ✅ 更新统计信息
        ++SubsystemStats.TotalPoolsCreated;

        // 通知池管理器
        if (PoolManager.IsValid())
        {
            PoolManager->OnPoolCreated(ActorClass, NewPool);
        }

        OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("创建新池: %s, 初始大小=%d, 最大大小=%d"),
            *ActorClass->GetName(), Config.InitialSize, Config.HardLimit);
    }

    return NewPool;
}

bool UObjectPoolSubsystemSimplified::ValidateActorClass(UClass* ActorClass) const
{
    if (!IsValid(ActorClass))
    {
        return false;
    }

    if (!ActorClass->IsChildOf<AActor>())
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Warning, TEXT("类不是Actor的子类: %s"), *ActorClass->GetName());
        return false;
    }

    return true;
}

void UObjectPoolSubsystemSimplified::CleanupInvalidPools()
{
    FWriteScopeLock WriteLock(PoolsRWLock);

    TArray<UClass*> InvalidClasses;

    for (auto& PoolPair : ActorPools)
    {
        if (!PoolPair.Value.IsValid() || !IsValid(PoolPair.Key))
        {
            InvalidClasses.Add(PoolPair.Key);
        }
    }

    for (UClass* InvalidClass : InvalidClasses)
    {
        ActorPools.Remove(InvalidClass);
        OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("清理无效池: %s"),
            InvalidClass ? *InvalidClass->GetName() : TEXT("Unknown"));
    }
}

void UObjectPoolSubsystemSimplified::PerformMaintenance()
{
    if (!bIsInitialized)
    {
        return;
    }

    // 清理无效池
    CleanupInvalidPools();

    // 委托给池管理器执行维护
    if (PoolManager.IsValid())
    {
        PoolManager->PerformMaintenance(ActorPools);
    }

    // 更新监控器（暂时禁用）
    // if (Monitor.IsValid())
    // {
    //     Monitor->UpdateGlobalStats(ActorPools);
    // }

    // ✅ 更新维护时间
    SubsystemStats.LastMaintenanceTime = FPlatformTime::Seconds();

    OBJECTPOOL_SUBSYSTEM_LOG(VeryVerbose, TEXT("执行定期维护"));
}

// ✅ 性能统计功能实现

FObjectPoolSubsystemStats UObjectPoolSubsystemSimplified::GetSubsystemStats() const
{
    FReadScopeLock ReadLock(PoolsRWLock);
    return SubsystemStats;
}

void UObjectPoolSubsystemSimplified::ResetSubsystemStats()
{
    FWriteScopeLock WriteLock(PoolsRWLock);

    double CurrentTime = FPlatformTime::Seconds();
    SubsystemStats = FObjectPoolSubsystemStats();
    SubsystemStats.StartupTime = CurrentTime;
    SubsystemStats.LastMaintenanceTime = CurrentTime;

    OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("子系统统计信息已重置"));
}

FString UObjectPoolSubsystemSimplified::GeneratePerformanceReport() const
{
    FReadScopeLock ReadLock(PoolsRWLock);

    double CurrentTime = FPlatformTime::Seconds();
    double UpTime = CurrentTime - SubsystemStats.StartupTime;

    FString Report = FString::Printf(TEXT(
        "=== 对象池子系统性能报告 ===\n"
        "运行时间: %.2f 秒\n"
        "总SpawnActor调用: %d\n"
        "总ReturnActor调用: %d\n"
        "总池创建数: %d\n"
        "总池销毁数: %d\n"
        "当前池数量: %d\n"
        "\n"
        "=== 性能指标 ===\n"
        "平均Spawn频率: %.2f 次/秒\n"
        "平均Return频率: %.2f 次/秒\n"
        "池重用率: %.1f%%\n"
        "\n"
        "=== 池详细信息 ===\n"
    ),
        UpTime,
        SubsystemStats.TotalSpawnCalls,
        SubsystemStats.TotalReturnCalls,
        SubsystemStats.TotalPoolsCreated,
        SubsystemStats.TotalPoolsDestroyed,
        ActorPools.Num(),
        UpTime > 0 ? SubsystemStats.TotalSpawnCalls / UpTime : 0.0f,
        UpTime > 0 ? SubsystemStats.TotalReturnCalls / UpTime : 0.0f,
        SubsystemStats.TotalSpawnCalls > 0 ?
            (static_cast<float>(SubsystemStats.TotalReturnCalls) / SubsystemStats.TotalSpawnCalls) * 100.0f : 0.0f
    );

    // 添加每个池的详细信息
    for (const auto& PoolPair : ActorPools)
    {
        if (PoolPair.Value.IsValid())
        {
            FObjectPoolStatsSimplified PoolStats = PoolPair.Value->GetStats();
            Report += FString::Printf(TEXT("- %s: 大小=%d, 活跃=%d, 可用=%d, 命中率=%.1f%%\n"),
                *PoolPair.Key->GetName(),
                PoolStats.PoolSize,
                PoolStats.CurrentActive,
                PoolStats.CurrentAvailable,
                PoolStats.HitRate * 100.0f
            );
        }
    }

    return Report;
}

// ✅ 性能优化方法实现

void UObjectPoolSubsystemSimplified::UpdatePoolCache(UClass* ActorClass, TSharedPtr<FActorPoolSimplified> Pool) const
{
    if (!IsValid(ActorClass) || !Pool.IsValid())
    {
        return;
    }

    FScopeLock CacheWriteLock(&CacheLock);
    LastAccessedClass = ActorClass;
    LastAccessedPool = Pool;
}

void UObjectPoolSubsystemSimplified::ClearPoolCache() const
{
    FScopeLock CacheWriteLock(&CacheLock);
    LastAccessedClass = nullptr;
    LastAccessedPool.Reset();
}
