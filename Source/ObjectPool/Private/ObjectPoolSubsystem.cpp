/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#include "ObjectPoolSubsystem.h"
#include "ObjectPool.h"
#include "ActorPool.h"
#include "ObjectPoolConfigManager.h"
#include "ObjectPoolManager.h"
#include "ObjectPoolUtils.h"
#include "ObjectPoolInterface.h"

//  UE核心依赖
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"
#include "TimerManager.h"

//  日志和统计
DEFINE_LOG_CATEGORY(LogObjectPoolSubsystem);

#if STATS
// 定义统计项（STATGROUP 已在 ObjectPoolUtils.h 中定义）
DEFINE_STAT(STAT_ObjectPoolSubsystem_SpawnActor);
DEFINE_STAT(STAT_ObjectPoolSubsystem_ReturnActor);
DEFINE_STAT(STAT_ObjectPoolSubsystem_GetOrCreatePool);
#endif

//  USubsystem接口实现

void UObjectPoolSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("对象池子系统开始初始化"));

    //  UE官方智能指针最佳实践
    ConfigManager = MakeUnique<FObjectPoolConfigManager>();
    PoolManager = MakeUnique<FObjectPoolManager>();
    
    //  UE官方容器预分配优化
    ActorPools.Reserve(DefaultPoolCapacity);
    
    // 监控器是可选的
    bMonitoringEnabled = false;

    //  初始化缓存
    LastAccessedClass = nullptr;

    //  与UE垃圾回收系统深度集成
    if (UWorld* World = GetWorld())
    {
        // 注册GC回调，确保与UE垃圾回收系统协调工作
        FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddUObject(this, &UObjectPoolSubsystem::OnPreGarbageCollect);
        FCoreUObjectDelegates::GetPostGarbageCollect().AddUObject(this, &UObjectPoolSubsystem::OnPostGarbageCollect);
        
        OBJECTPOOL_SUBSYSTEM_LOG(Verbose, TEXT("已注册GC回调"));
    }

    // 记录启动时间
    SubsystemStats.StartupTime = FPlatformTime::Seconds();
    SubsystemStats.LastMaintenanceTime = SubsystemStats.StartupTime;

    bIsInitialized = true;

    OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("对象池子系统初始化完成"));
}

void UObjectPoolSubsystem::Deinitialize()
{
    if (bIsInitialized)
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("对象池子系统开始清理"));

        //  取消GC回调注册 - 防止野指针
        FCoreUObjectDelegates::GetPreGarbageCollectDelegate().RemoveAll(this);
        FCoreUObjectDelegates::GetPostGarbageCollect().RemoveAll(this);

        //  清理延迟预热Timer和队列
        ClearDelayedPrewarmTimer();

        //  确保线程安全的清理
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

bool UObjectPoolSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    //  检查插件设置，如果未启用则不创建子系统
    // 注意：这里不能直接include X_AssetEditorSettings.h，因为会产生循环依赖
    // 使用FindObject动态加载配置
    if (const UClass* SettingsClass = FindObject<UClass>(nullptr, TEXT("/Script/X_AssetEditor.X_AssetEditorSettings")))
    {
        if (const UObject* Settings = SettingsClass->GetDefaultObject())
        {
            const FProperty* Property = SettingsClass->FindPropertyByName(TEXT("bEnableObjectPoolSubsystem"));
            if (Property && Property->IsA<FBoolProperty>())
            {
                const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property);
                if (BoolProperty && !BoolProperty->GetPropertyValue_InContainer(Settings))
                {
                    // 设置中未启用对象池
                    return false;
                }
            }
        }
    }
    
    // 只在游戏世界中创建
    if (UWorld* World = Cast<UWorld>(Outer))
    {
        return World->IsGameWorld();
    }
    return false;
}

//  核心对象池API实现 - 设计文档第177-210行的极简API设计

bool UObjectPoolSubsystem::RegisterActorClass(TSubclassOf<AActor> ActorClass, int32 InitialSize, int32 HardLimit)
{
    if (!ValidateActorClass(ActorClass))
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Warning, TEXT("RegisterActorClass: 无效的Actor类"));
        return false;
    }

    // 检查是否已经注册
    if (GetPool(ActorClass))
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Warning, TEXT("RegisterActorClass: Actor类已经注册: %s"), *ActorClass->GetName());
        return true; // 已存在视为成功
    }

    // 创建配置
    FObjectPoolConfig Config;
    Config.ActorClass = ActorClass; // 确保配置校验通过并与该类绑定
    Config.InitialSize = InitialSize;
    Config.HardLimit = HardLimit;

    // 设置配置（如果配置管理器存在）
    if (ConfigManager.IsValid())
    {
        ConfigManager->SetConfig(ActorClass, Config);
    }

    // 创建池
    TSharedPtr<FActorPool> NewPool = GetOrCreatePool(ActorClass);
    if (!NewPool.IsValid())
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Error, TEXT("RegisterActorClass: 创建池失败: %s"), *ActorClass->GetName());
        return false;
    }

    //  使用延迟预热避免开始游戏时卡顿
    if (InitialSize > 0)
    {
        // 队列延迟预热，避免在同一帧创建大量Actor
        QueueDelayedPrewarm(ActorClass, InitialSize);
        OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("注册Actor类并队列延迟预热: %s, 预热数量=%d"), 
            *ActorClass->GetName(), InitialSize);
    }
    else
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("注册Actor类（无预热）: %s"), *ActorClass->GetName());
    }

    OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("RegisterActorClass: 成功注册Actor类: %s (初始大小=%d, 硬限制=%d)"),
        *ActorClass->GetName(), InitialSize, HardLimit);

    return true;
}

AActor* UObjectPoolSubsystem::SpawnActorFromPool(UClass* ActorClass, const FTransform& SpawnTransform)
{
    SCOPE_CYCLE_COUNTER(STAT_ObjectPoolSubsystem_SpawnActor);

    //  更新统计信息
    ++SubsystemStats.TotalSpawnCalls;

    if (!ValidateActorClass(ActorClass))
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Warning, TEXT("SpawnActorFromPool: 无效的Actor类"));
        return nullptr;
    }

    // 获取或创建池
    TSharedPtr<FActorPool> Pool = GetOrCreatePool(ActorClass);
    if (!Pool.IsValid())
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Error, TEXT("SpawnActorFromPool: 无法创建池 %s"), *ActorClass->GetName());
        return nullptr;
    }

    // 从池中获取Actor
    AActor* Actor = Pool->GetActor(GetWorld(), SpawnTransform);
    
    //  永不失败机制：如果从池中获取失败，自动回退到正常生成
    if (!Actor)
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Verbose, TEXT("池中无可用Actor，回退到正常生成: %s"), *ActorClass->GetName());
        
        // 自动回退到UE标准的SpawnActor
        Actor = GetWorld()->SpawnActor<AActor>(ActorClass, SpawnTransform);
        
        if (Actor)
        {
            OBJECTPOOL_SUBSYSTEM_LOG(VeryVerbose, TEXT("回退生成成功: %s"), *Actor->GetName());
            ++SubsystemStats.TotalFallbackSpawns;
        }
        else
        {
            OBJECTPOOL_SUBSYSTEM_LOG(Error, TEXT("连回退生成都失败了: %s"), *ActorClass->GetName());
        }
    }
    else
    {
        OBJECTPOOL_SUBSYSTEM_LOG(VeryVerbose, TEXT("从池成功获取Actor: %s"), *Actor->GetName());
        ++SubsystemStats.TotalPoolHits;
        // 生命周期事件由 FObjectPoolUtils 统一触发
        OBJECTPOOL_SUBSYSTEM_LOG(VeryVerbose, TEXT("从池获取Actor成功: %s"), *Actor->GetName());
    }

    return Actor;
}

AActor* UObjectPoolSubsystem::AcquireDeferredFromPool(UClass* ActorClass)
{
    if (!ValidateActorClass(ActorClass))
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Warning, TEXT("AcquireDeferredFromPool: 无效的Actor类"));
        return nullptr;
    }

    TSharedPtr<FActorPool> Pool = GetOrCreatePool(ActorClass);
    if (!Pool.IsValid())
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Error, TEXT("AcquireDeferredFromPool: 无法创建池 %s"), *ActorClass->GetName());
        return nullptr;
    }
    return Pool->AcquireDeferred(GetWorld());
}

bool UObjectPoolSubsystem::FinalizeSpawnFromPool(AActor* Actor, const FTransform& SpawnTransform)
{
    if (!IsValid(Actor))
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Warning, TEXT("FinalizeSpawnFromPool: Actor无效"));
        return false;
    }

    UClass* ActorClass = Actor->GetClass();
    TSharedPtr<FActorPool> Pool = GetPool(ActorClass);
    if (!Pool.IsValid())
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Warning, TEXT("FinalizeSpawnFromPool: 找不到对应池，直接回退FinishSpawning+激活"));
        // 非池管理对象：直接完成构造并返回
        if (!Actor->IsActorInitialized())
        {
            Actor->FinishSpawning(SpawnTransform);
        }
        FObjectPoolUtils::ActivateActorFromPool(Actor, SpawnTransform);
        return true;
    }

    return Pool->FinalizeDeferred(Actor, SpawnTransform);
}



bool UObjectPoolSubsystem::ReturnActorToPool(AActor* Actor)
{
    SCOPE_CYCLE_COUNTER(STAT_ObjectPoolSubsystem_ReturnActor);

    //  更新统计信息
    ++SubsystemStats.TotalReturnCalls;

    if (!IsValid(Actor))
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Warning, TEXT("ReturnActorToPool: 无效的Actor"));
        return false;
    }

    UClass* ActorClass = Actor->GetClass();
    
    // 获取对应的池
    TSharedPtr<FActorPool> Pool = GetPool(ActorClass);
    if (!Pool.IsValid())
    {
        OBJECTPOOL_SUBSYSTEM_LOG(Warning, TEXT("ReturnActorToPool: 找不到对应的池 %s"), *ActorClass->GetName());
        return false;
    }

    // 生命周期事件由 FObjectPoolUtils 统一触发

    OBJECTPOOL_SUBSYSTEM_LOG(VeryVerbose, TEXT("归还Actor到池: %s"), *Actor->GetName());

    // 归还到池
    bool bSuccess = Pool->ReturnActor(Actor);

    OBJECTPOOL_SUBSYSTEM_LOG(VeryVerbose, TEXT("归还Actor到池: %s, 结果=%s"),
        *Actor->GetName(), bSuccess ? TEXT("成功") : TEXT("失败"));

    return bSuccess;
}

int32 UObjectPoolSubsystem::PrewarmPool(UClass* ActorClass, int32 Count)
{
    if (!ValidateActorClass(ActorClass) || Count <= 0)
    {
        return 0;
    }

    TSharedPtr<FActorPool> Pool = GetOrCreatePool(ActorClass);
    if (!Pool.IsValid())
    {
        return 0;
    }

    //  标准对象池预热机制 - 安全的组件处理
    Pool->PrewarmPool(GetWorld(), Count);
    
    OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("子系统预热池完成: %s, 预热数量=%d"), 
        *ActorClass->GetName(), Count);
    
    return Pool->GetAvailableCount();
}

//  池管理功能实现

TSharedPtr<FActorPool> UObjectPoolSubsystem::GetOrCreatePool(UClass* ActorClass)
{
    SCOPE_CYCLE_COUNTER(STAT_ObjectPoolSubsystem_GetOrCreatePool);

    if (!ValidateActorClass(ActorClass))
    {
        return nullptr;
    }

    //  快速缓存检查（无锁优化）
    {
        FScopeLock CacheReadLock(&CacheLock);
        if (LastAccessedClass == ActorClass && LastAccessedPool.IsValid())
        {
            TSharedPtr<FActorPool> CachedPool = LastAccessedPool.Pin();
            if (CachedPool.IsValid())
            {
                return CachedPool;
            }
        }
    }

    //  优化的双重检查锁定模式
    // 首先使用读锁进行快速检查
    TSharedPtr<FActorPool> FoundPool;
    {
        FReadScopeLock ReadLock(PoolsRWLock);
        if (TSharedPtr<FActorPool>* ExistingPool = ActorPools.Find(ActorClass))
        {
            FoundPool = *ExistingPool;
        }
    }

    if (FoundPool.IsValid())
    {
        //  更新缓存
        UpdatePoolCache(ActorClass, FoundPool);
        return FoundPool;
    }

    // 如果不存在，使用写锁创建新池
    {
        FWriteScopeLock WriteLock(PoolsRWLock);

        // 再次检查，防止在锁切换期间其他线程已创建
        if (TSharedPtr<FActorPool>* ExistingPool = ActorPools.Find(ActorClass))
        {
            TSharedPtr<FActorPool> ExistingPoolPtr = *ExistingPool;
            UpdatePoolCache(ActorClass, ExistingPoolPtr);
            return ExistingPoolPtr;
        }

        // 创建新池
        TSharedPtr<FActorPool> NewPool = CreatePool(ActorClass);
        if (NewPool.IsValid())
        {
            UpdatePoolCache(ActorClass, NewPool);
        }
        return NewPool;
    }
}

TSharedPtr<FActorPool> UObjectPoolSubsystem::GetPool(UClass* ActorClass) const
{
    if (!ValidateActorClass(ActorClass))
    {
        return nullptr;
    }

    //  使用读锁进行高效查找
    FReadScopeLock ReadLock(PoolsRWLock);

    if (const TSharedPtr<FActorPool>* Pool = ActorPools.Find(ActorClass))
    {
        return *Pool;
    }

    return nullptr;
}



void UObjectPoolSubsystem::ClearAllPools()
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

    //  清理缓存
    ClearPoolCache();

    OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("清空所有池"));
}





//  静态访问方法

UObjectPoolSubsystem* UObjectPoolSubsystem::Get(const UObject* WorldContext)
{
    if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull))
    {
        return World->GetSubsystem<UObjectPoolSubsystem>();
    }
    return nullptr;
}



bool UObjectPoolSubsystem::IsActorPooled(const AActor* Actor) const
{
    if (!IsValid(Actor))
    {
        return false;
    }

    UClass* Cls = Actor->GetClass();
    TSharedPtr<FActorPool> Pool = GetPool(Cls);
    if (!Pool.IsValid())
    {
        return false;
    }
    return Pool->ContainsActor(Actor);
}

//  内部辅助方法实现

TSharedPtr<FActorPool> UObjectPoolSubsystem::CreatePool(UClass* ActorClass)
{
    // 注意：调用者应该持有锁

    if (!ValidateActorClass(ActorClass))
    {
        return nullptr;
    }

    // 获取配置
    FObjectPoolConfig Config = ConfigManager.IsValid() ?
        ConfigManager->GetConfig(ActorClass) : FObjectPoolConfig();

    // 创建池
    TSharedPtr<FActorPool> NewPool = MakeShared<FActorPool>(
        ActorClass,
        Config.InitialSize > 0 ? Config.InitialSize : DEFAULT_POOL_INITIAL_SIZE,
        Config.HardLimit > 0 ? Config.HardLimit : DEFAULT_POOL_MAX_SIZE
    );

    if (NewPool.IsValid())
    {
        // 添加到映射
        ActorPools.Add(ActorClass, NewPool);

        //  更新统计信息
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

bool UObjectPoolSubsystem::ValidateActorClass(UClass* ActorClass) const
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

void UObjectPoolSubsystem::CleanupInvalidPools()
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

void UObjectPoolSubsystem::PerformMaintenance()
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

    // 更新维护时间
    SubsystemStats.LastMaintenanceTime = FPlatformTime::Seconds();

    OBJECTPOOL_SUBSYSTEM_LOG(VeryVerbose, TEXT("执行定期维护"));
}

//  性能统计功能实现

FObjectPoolSubsystemStats UObjectPoolSubsystem::GetSubsystemStats() const
{
    FReadScopeLock ReadLock(PoolsRWLock);
    return SubsystemStats;
}

TArray<FObjectPoolStats> UObjectPoolSubsystem::GetAllPoolStats() const
{
    TArray<FObjectPoolStats> AllStats;

    FReadScopeLock ReadLock(PoolsRWLock);
    AllStats.Reserve(ActorPools.Num());

    for (const auto& PoolPair : ActorPools)
    {
        if (PoolPair.Value.IsValid() && IsValid(PoolPair.Key))
        {
            AllStats.Add(PoolPair.Value->GetStats());
        }
    }

    return AllStats;
}

int32 UObjectPoolSubsystem::GetPoolCount() const
{
    FReadScopeLock ReadLock(PoolsRWLock);
    return ActorPools.Num();
}

//  UE官方垃圾回收系统集成实现

void UObjectPoolSubsystem::OnPreGarbageCollect()
{
    OBJECTPOOL_SUBSYSTEM_LOG(VeryVerbose, TEXT("GC前清理：开始清理无效的Actor类引用"));
    
    FWriteScopeLock WriteLock(PoolsRWLock);
    
    // 清理无效的Actor类引用
    TArray<UClass*> InvalidKeys;
    for (auto& PoolPair : ActorPools)
    {
        if (!IsValid(PoolPair.Key))
        {
            InvalidKeys.Add(PoolPair.Key);
        }
        else if (PoolPair.Value.IsValid())
        {
            // 池将在 CleanupInvalidActors 中自动清理无效引用
        }
    }
    
    // 移除无效的池
    for (UClass* InvalidKey : InvalidKeys)
    {
        ActorPools.Remove(InvalidKey);
        OBJECTPOOL_SUBSYSTEM_LOG(Verbose, TEXT("GC前清理：移除无效Actor类的池"));
    }
}

void UObjectPoolSubsystem::OnPostGarbageCollect()
{
    OBJECTPOOL_SUBSYSTEM_LOG(VeryVerbose, TEXT("GC后清理：验证对象池状态"));
    
    FReadScopeLock ReadLock(PoolsRWLock);
    
    // 验证所有池的状态
    for (auto& PoolPair : ActorPools)
    {
        if (PoolPair.Value.IsValid() && IsValid(PoolPair.Key))
        {
            // 池会在内部自动清理无效引用
        }
    }
}



//  性能优化方法实现

void UObjectPoolSubsystem::UpdatePoolCache(UClass* ActorClass, TSharedPtr<FActorPool> Pool) const
{
    if (!IsValid(ActorClass) || !Pool.IsValid())
    {
        return;
    }

    FScopeLock CacheWriteLock(&CacheLock);
    LastAccessedClass = ActorClass;
    LastAccessedPool = Pool;
}

void UObjectPoolSubsystem::ClearPoolCache() const
{
    FScopeLock CacheWriteLock(&CacheLock);
    LastAccessedClass = nullptr;
    LastAccessedPool.Reset();
}

//  安全延迟预热机制实现

void UObjectPoolSubsystem::QueueDelayedPrewarm(UClass* ActorClass, int32 Count)
{
    if (!IsValid(ActorClass) || Count <= 0)
    {
        return;
    }

    // 添加到延迟预热队列
    DelayedPrewarmQueue.Add(FDelayedPrewarmInfo(ActorClass, Count));
    
    // 如果Timer还没有设置，创建一个0.1秒后的延迟Timer
    if (!DelayedPrewarmTimerHandle.IsValid())
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                DelayedPrewarmTimerHandle,
                this,
                &UObjectPoolSubsystem::ProcessDelayedPrewarmQueue,
                0.1f,  // 0.1秒延迟，确保不在同一帧
                false  // 不重复
            );
            
            OBJECTPOOL_SUBSYSTEM_LOG(VeryVerbose, TEXT("设置延迟预热Timer: 0.1秒后执行"));
        }
    }
    
    OBJECTPOOL_SUBSYSTEM_LOG(VeryVerbose, TEXT("已队列延迟预热: %s, 数量=%d, 队列大小=%d"), 
        *ActorClass->GetName(), Count, DelayedPrewarmQueue.Num());
}

void UObjectPoolSubsystem::ProcessDelayedPrewarmQueue()
{
    if (DelayedPrewarmQueue.Num() == 0)
    {
        return;
    }
    
    OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("开始处理延迟预热队列，队列大小=%d"), DelayedPrewarmQueue.Num());
    
    //  分帧创建：每帧最多创建指定数量的Actor，避免单帧卡顿
    // 使用硬编码默认值（流畅优先）
    const int32 MaxActorsPerFrame = MAX_ACTORS_PER_FRAME_PREWARM;
    int32 ActorsCreatedThisFrame = 0;
    
    for (int32 i = DelayedPrewarmQueue.Num() - 1; i >= 0; --i)
    {
        if (ActorsCreatedThisFrame >= MaxActorsPerFrame)
        {
            // 本帧已达上限，设置Timer在下一帧继续
            if (UWorld* World = GetWorld())
            {
                World->GetTimerManager().SetTimer(
                    DelayedPrewarmTimerHandle,
                    this,
                    &UObjectPoolSubsystem::ProcessDelayedPrewarmQueue,
                    0.016f,  // ~1帧后（60fps）
                    false
                );
            }
            
            OBJECTPOOL_SUBSYSTEM_LOG(VeryVerbose, TEXT("本帧已创建 %d 个Actor，延迟到下一帧继续"), 
                ActorsCreatedThisFrame);
            return;
        }
        
        FDelayedPrewarmInfo& PrewarmInfo = DelayedPrewarmQueue[i];
        
        if (!IsValid(PrewarmInfo.ActorClass))
        {
            OBJECTPOOL_SUBSYSTEM_LOG(Warning, TEXT("延迟预热失败，Actor类无效: %s"), 
                *PrewarmInfo.PoolName);
            DelayedPrewarmQueue.RemoveAtSwap(i);
            continue;
        }
        
        // 获取或创建池
        TSharedPtr<FActorPool> Pool = GetOrCreatePool(PrewarmInfo.ActorClass);
        if (!Pool.IsValid())
        {
            OBJECTPOOL_SUBSYSTEM_LOG(Warning, TEXT("延迟预热失败，无法获取池: %s"), 
                *PrewarmInfo.PoolName);
            DelayedPrewarmQueue.RemoveAtSwap(i);
            continue;
        }
        
        // 本帧可创建的数量
        const int32 RemainingBudget = MaxActorsPerFrame - ActorsCreatedThisFrame;
        const int32 CountThisFrame = FMath::Min(PrewarmInfo.Count, RemainingBudget);
        
        // 创建Actor
        Pool->PrewarmPool(GetWorld(), CountThisFrame);
        ActorsCreatedThisFrame += CountThisFrame;
        
        OBJECTPOOL_SUBSYSTEM_LOG(Verbose, TEXT("延迟预热进度: %s, 本次创建=%d, 剩余=%d"), 
            *PrewarmInfo.PoolName, CountThisFrame, PrewarmInfo.Count - CountThisFrame);
        
        // 更新剩余数量
        PrewarmInfo.Count -= CountThisFrame;
        
        // 如果这个池已完成，移除
        if (PrewarmInfo.Count <= 0)
        {
            OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("延迟预热完成: %s"), *PrewarmInfo.PoolName);
            DelayedPrewarmQueue.RemoveAtSwap(i);
        }
    }
    
    // 队列已清空
    if (DelayedPrewarmQueue.Num() == 0)
    {
        DelayedPrewarmTimerHandle.Invalidate();
        OBJECTPOOL_SUBSYSTEM_LOG(Log, TEXT("延迟预热队列全部处理完成"));
    }
}

void UObjectPoolSubsystem::ClearDelayedPrewarmTimer()
{
    if (DelayedPrewarmTimerHandle.IsValid())
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(DelayedPrewarmTimerHandle);
        }
        DelayedPrewarmTimerHandle.Invalidate();
        
        OBJECTPOOL_SUBSYSTEM_LOG(VeryVerbose, TEXT("已清理延迟预热Timer"));
    }
    
    DelayedPrewarmQueue.Empty();
}