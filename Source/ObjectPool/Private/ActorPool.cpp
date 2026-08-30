/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#include "ActorPool.h"
#include "ObjectPool.h"
#include "ObjectPoolUtils.h"
#include "ObjectPoolPreallocator.h"

//  生命周期接口
#include "ObjectPoolInterface.h"

//  UE核心依赖
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "CoreGlobals.h"
#include "UObject/UObjectGlobals.h"



//  日志和统计
DEFINE_LOG_CATEGORY(LogActorPool);

// 日志宏（定义在 .cpp 中，避免头文件依赖）
#undef ACTORPOOL_LOG
#define ACTORPOOL_LOG(Verbosity, Format, ...) \
    UE_LOG(LogActorPool, Verbosity, Format, ##__VA_ARGS__)

#if STATS
DEFINE_STAT(STAT_ActorPool_GetActor);
DEFINE_STAT(STAT_ActorPool_ReturnActor);
DEFINE_STAT(STAT_ActorPool_CreateActor);
#endif

//  构造函数和析构函数

FActorPool::FActorPool(UClass* InActorClass, int32 InInitialSize, int32 InHardLimit)
    : ActorClass(InActorClass)
    , MaxPoolSize(InHardLimit > 0 ? InHardLimit : DEFAULT_HARD_LIMIT)
    , InitialSize(FMath::Clamp(InInitialSize, 1, MaxPoolSize))
    , bIsInitialized(false)
    , TotalRequests(0)
    , PoolHits(0)
    , TotalCreated(0)
    , TotalReturned(0)
{
    //  验证输入参数
    if (!IsValid(ActorClass))
    {
        ACTORPOOL_LOG(Error, TEXT("FActorPool: 无效的Actor类"));
        return;
    }

    //  预分配容器
    AvailableActors.Reserve(InitialSize);
    ActiveActors.Reserve(InitialSize);
    Preallocator = MakeUnique<FObjectPoolPreallocator>(this);

    //  注册GC回调
    if (GEngine)
    {
        GCDelegateHandle = FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddLambda([this]()
        {
            CleanupInvalidActors();
        });

        ACTORPOOL_LOG(VeryVerbose, TEXT("已注册GC委托句柄"));
    }

    bIsInitialized = true;

    ACTORPOOL_LOG(Log, TEXT("创建Actor池: %s, 初始大小=%d, 最大大小=%d"),
        *ActorClass->GetName(), InitialSize, MaxPoolSize);
}

FActorPool::~FActorPool()
{
    //  首先清理GC委托，防止悬挂指针崩溃
    if (GCDelegateHandle.IsValid())
    {
        FCoreUObjectDelegates::GetPreGarbageCollectDelegate().Remove(GCDelegateHandle);
        GCDelegateHandle.Reset();
        ACTORPOOL_LOG(VeryVerbose, TEXT("已清理GC委托句柄"));
    }
    
    if (bIsInitialized)
    {
        ClearPool();
        ACTORPOOL_LOG(Log, TEXT("销毁Actor池: %s"), 
            ActorClass ? *ActorClass->GetName() : TEXT("Unknown"));
    }
}

//  核心池管理功能实现

void FActorPool::AddReferencedObjects(FReferenceCollector& Collector, UObject* ReferencingObject)
{
    Collector.AddReferencedObject(ActorClass.GetGCPtr(), ReferencingObject);
}

AActor* FActorPool::GetActor(UWorld* World, const FTransform& SpawnTransform)
{
    checkf(IsInGameThread(), TEXT("FActorPool::GetActor 只能在游戏线程调用"));
    SCOPE_CYCLE_COUNTER(STAT_ActorPool_GetActor);

    if (!bIsInitialized || !IsValid(ActorClass) || !IsValid(World))
    {
        ACTORPOOL_LOG(Warning, TEXT("GetActor: 池未初始化或参数无效"));
        return nullptr;
    }

    AActor* ResultActor = nullptr;

    // 从可用列表获取Actor，登记 Activating 过渡状态
    {
        FWriteScopeLock WriteLock(PoolLock);
        ++TotalRequests;
        PeriodicCleanup_RequiresLock();
        ResultActor = TakeFromAvailable_RequiresLock();
        if (ResultActor)
        {
            ActivatingActors.Add(ResultActor);
        }
    }

    // 锁外激活复用的Actor
    if (ResultActor)
    {
        // 先按池侧登记补完延迟构造（复用实例通常已完成），再激活
        bool bFinishedNow = false;
        const bool bFinishOk = IsValid(ResultActor) && FinishSpawningOnce(ResultActor, SpawnTransform, bFinishedNow);
        const bool bActivateOk = bFinishOk && FObjectPoolUtils::ActivatePooledActorFromPool(ResultActor, SpawnTransform, false);

        // 回调后复核：确认 Activating 状态仍有效（ClearPool 可能已清除，Actor 可能已被销毁）
        bool bCommitOk = false;
        {
            FWriteScopeLock WriteLock(PoolLock);
            const bool bStillActivating = (ActivatingActors.Remove(ResultActor) > 0);
            const bool bStillOwned = AllActorsSet.Contains(ResultActor);

            if (bStillActivating && bStillOwned && IsValid(ResultActor) && bActivateOk)
            {
                ActiveActors.Add(ResultActor);
                UpdateStats(true);
                if (Preallocator.IsValid())
                {
                    Preallocator->RecordUsagePattern(ActiveActors.Num());
                }
                ACTORPOOL_DEBUG(TEXT("从池获取Actor: %s"), *ResultActor->GetName());
                bCommitOk = true;
            }
            else
            {
                // 回调期间池状态已变更或激活失败，确保不残留 AllActorsSet 引用
                AllActorsSet.Remove(ResultActor);
                ACTORPOOL_LOG(Warning, TEXT("GetActor: 激活后复核失败，放弃提交: %s"), *ResultActor->GetName());
            }
        }

        if (bCommitOk)
        {
            return ResultActor;
        }

        // 激活失败或复核失败：复核失败时 AllActorsSet 已移除，Actor 不再由池管理，直接销毁
        if (IsValid(ResultActor))
        {
            ACTORPOOL_LOG(Warning, TEXT("Actor激活失败且无法恢复，已销毁: %s"), *ResultActor->GetName());
            ResultActor->Destroy();
        }
        ResultActor = nullptr;
    }

    // 池中没有可用Actor，尝试创建新的
    if (CanCreateMoreActors())
    {
        AActor* NewActor = CreateNewActor(World);
        if (NewActor)
        {
            // 先登记 AllActorsSet + Activating，保证回调期间 ClearPool 能找到它
            {
                FWriteScopeLock WriteLock(PoolLock);
                AllActorsSet.Add(NewActor);
                ActivatingActors.Add(NewActor);
            }

            // 先按池侧登记补完延迟构造（新建实例必处于未完成态），再激活
            bool bFinishedNow = false;
            const bool bFinishOk = FinishSpawningOnce(NewActor, SpawnTransform, bFinishedNow);
            const bool bActivateOk = bFinishOk && FObjectPoolUtils::ActivatePooledActorFromPool(NewActor, SpawnTransform, false);

            // 回调后复核
            bool bCommitOk = false;
            {
                FWriteScopeLock WriteLock(PoolLock);
                const bool bStillActivating = (ActivatingActors.Remove(NewActor) > 0);
                const bool bStillOwned = AllActorsSet.Contains(NewActor);

                if (bStillActivating && bStillOwned && IsValid(NewActor) && bActivateOk)
                {
                    ActiveActors.Add(NewActor);
                    UpdateStats(false);
                    if (Preallocator.IsValid())
                    {
                        Preallocator->RecordUsagePattern(ActiveActors.Num());
                    }
                    ACTORPOOL_DEBUG(TEXT("创建新Actor: %s"), *NewActor->GetName());
                    bCommitOk = true;
                }
                else
                {
                    AllActorsSet.Remove(NewActor);
                    ACTORPOOL_LOG(Warning, TEXT("GetActor: 新建Actor激活后复核失败: %s"), *NewActor->GetName());
                }
            }

            if (bCommitOk)
            {
                return NewActor;
            }

            if (IsValid(NewActor))
            {
                NewActor->Destroy();
            }
        }
    }

    UpdateStats(false);
    ACTORPOOL_LOG(Warning, TEXT("无法获取Actor: %s"), *ActorClass->GetName());
    return nullptr;
}

AActor* FActorPool::AcquireDeferred(UWorld* World)
{
    checkf(IsInGameThread(), TEXT("FActorPool::AcquireDeferred 只能在游戏线程调用"));

    if (!bIsInitialized || !IsValid(ActorClass) || !IsValid(World))
    {
        ACTORPOOL_LOG(Warning, TEXT("AcquireDeferred: 池未初始化或参数无效"));
        return nullptr;
    }

    AActor* ResultActor = nullptr;

    {
        FWriteScopeLock WriteLock(PoolLock);
        ++TotalRequests;
        PeriodicCleanup_RequiresLock();
        ResultActor = TakeFromAvailable_RequiresLock();
        if (ResultActor)
        {
            // 复用实例也登记 Pending，确保 FinalizeDeferred 可验证来源
            PendingDeferredActors.Add(ResultActor);
            UpdateStats(true);
        }
    }

    if (ResultActor)
    {
        if (Preallocator.IsValid())
        {
            Preallocator->RecordUsagePattern(ActiveActors.Num() + 1);
        }
        return ResultActor;
    }

    // 可用为空，尝试新建延迟构造Actor
    if (CanCreateMoreActors())
    {
        AActor* NewActor = CreateNewActor(World);
        if (NewActor)
        {
            // 新建时即登记 AllActorsSet 和 Pending，确保 IsActorPooled 可查且容量计数正确
            FWriteScopeLock WriteLock(PoolLock);
            AllActorsSet.Add(NewActor);
            PendingDeferredActors.Add(NewActor);
            UpdateStats(false);
            if (Preallocator.IsValid())
            {
                Preallocator->RecordUsagePattern(ActiveActors.Num() + 1);
            }
            return NewActor;
        }
    }

    UpdateStats(false);
    ACTORPOOL_LOG(Warning, TEXT("AcquireDeferred: 无可用Actor且创建失败: %s"), *ActorClass->GetName());
    return nullptr;
}

bool FActorPool::FinalizeDeferred(AActor* Actor, const FTransform& SpawnTransform)
{
    checkf(IsInGameThread(), TEXT("FActorPool::FinalizeDeferred 只能在游戏线程调用"));

    if (!ValidateActor(Actor) || !bIsInitialized)
    {
        ACTORPOOL_LOG(Warning, TEXT("FinalizeDeferred: Actor无效或池未初始化"));
        return false;
    }

    // 原子消费 Pending 状态并转入 Finalizing，保证回调期间 Actor 始终属于某个状态集合。
    // 重入 Finalize 会因 Pending 已被消费而拒绝。
    {
        FWriteScopeLock WriteLock(PoolLock);
        if (PendingDeferredActors.Remove(Actor) == 0)
        {
            ACTORPOOL_LOG(Warning, TEXT("FinalizeDeferred: Actor不处于Pending状态，拒绝Finalize: %s"), *Actor->GetName());
            return false;
        }
        FinalizingActors.Add(Actor);
    }

    // 锁外执行构造/蓝图回调。
    // 判据使用池侧登记：IsActorInitialized() 在世界未完成初始化（加载期）时恒为 false，
    // 会把已完成 FinishSpawning 的复用实例误判为新实例并二次触发引擎 ensure
    bool bFinishedNow = false;
    const bool bFinishOk = FinishSpawningOnce(Actor, SpawnTransform, bFinishedNow);
    if (!bFinishOk)
    {
        ACTORPOOL_LOG(Warning, TEXT("FinalizeDeferred: Actor无效，无法补完延迟构造"));
        return false;
    }
    if (!bFinishedNow)
    {
        ACTORPOOL_LOG(VeryVerbose, TEXT("FinalizeDeferred: 复用实例激活前重跑ConstructionScripts: %s"), *Actor->GetName());
#if WITH_EDITOR
        Actor->RerunConstructionScripts();
#endif
    }

    // 激活（内部触发生命周期回调，可能调用 ClearPool 或销毁 Actor）
    const bool bActivateOk = FObjectPoolUtils::ActivatePooledActorFromPool(Actor, SpawnTransform, false);

    // 回调后复核并提交
    {
        FWriteScopeLock WriteLock(PoolLock);

        const bool bStillFinalizing = (FinalizingActors.Remove(Actor) > 0);
        const bool bStillOwned = AllActorsSet.Contains(Actor);

        if (!bStillFinalizing || !bStillOwned || !IsValid(Actor) || !bActivateOk)
        {
            // 回调期间池状态已变更（ClearPool / Actor 被销毁 / 激活失败），放弃提交。
            // 同步移除 AllActorsSet，避免残留弱引用（Finalizing 已移除，Cleanup 无法再触及）。
            AllActorsSet.Remove(Actor);
            ACTORPOOL_LOG(Warning, TEXT("FinalizeDeferred: 回调后复核失败，放弃提交: %s"), *Actor->GetName());
            return false;
        }

        ActiveActors.Add(Actor);
    }

    return true;
}

bool FActorPool::ReturnActor(AActor* Actor)
{
    checkf(IsInGameThread(), TEXT("FActorPool::ReturnActor 只能在游戏线程调用"));
    SCOPE_CYCLE_COUNTER(STAT_ActorPool_ReturnActor);

    if (!ValidateActor(Actor) || !bIsInitialized)
    {
        ACTORPOOL_LOG(Warning, TEXT("ReturnActor: Actor无效或池未初始化"));
        return false;
    }

    // Phase 1: 锁内摘除 — 校验归属与活跃状态，从 Active 移除，登记 Returning 状态
    {
        FWriteScopeLock WriteLock(PoolLock);

        // 归属校验：Actor 必须属于本池
        if (!AllActorsSet.Contains(Actor))
        {
            ACTORPOOL_LOG(Warning, TEXT("ReturnActor: Actor不属于本池: %s"), *Actor->GetName());
            return false;
        }

        // 必须处于活跃状态才能归还（防止重复归还及 Activated 回调中提前归还）
        if (ActiveActors.RemoveSwap(Actor) == 0)
        {
            ACTORPOOL_LOG(Warning, TEXT("ReturnActor: Actor不在活跃列表中，拒绝归还: %s"), *Actor->GetName());
            return false;
        }

        // 登记 Returning 状态，供 Phase 3 回调后复核
        ReturningActors.Add(Actor);
    }

    // Phase 2: 锁外重置 — 生命周期回调（OnReturnToPool）在锁外触发，避免蓝图重入对象池 API 时死锁
    bool bResetOk = FObjectPoolUtils::ResetActorForPooling(Actor);

    // Phase 3: 锁内复核并提交 — 回调可能销毁 Actor 或调用 ClearPool，必须重新验证状态
    bool bShouldDestroy = false;
    bool bReturnSucceeded = false;
    {
        FWriteScopeLock WriteLock(PoolLock);

        // 复核：确认 Returning 状态仍有效（ClearPool 可能已清除，Actor 可能已被销毁）
        const bool bStillReturning = (ReturningActors.Remove(Actor) > 0);
        const bool bStillOwned = AllActorsSet.Contains(Actor);

        if (!bStillReturning || !bStillOwned || !IsValid(Actor))
        {
            // 回调期间池状态已变更（如 ClearPool 或 Actor 被销毁），放弃提交。
            // 确保不残留 AllActorsSet 弱引用（幂等，ClearPool 可能已清除）。
            AllActorsSet.Remove(Actor);
            ACTORPOOL_LOG(Warning, TEXT("ReturnActor: 回调后复核失败，放弃提交: %s"), *Actor->GetName());
        }
        else if (!bResetOk)
        {
            ACTORPOOL_LOG(Warning, TEXT("重置Actor状态失败，移出池: %s"), *Actor->GetName());
            AllActorsSet.Remove(Actor);
            bShouldDestroy = true;
        }
        else if (GetManagedActorCount_RequiresLock() >= MaxPoolSize)
        {
            ACTORPOOL_DEBUG(TEXT("池已满，销毁Actor: %s"), *Actor->GetName());
            AllActorsSet.Remove(Actor);
            bShouldDestroy = true;
            bReturnSucceeded = true; // 归还流程正常完成，仅因容量满而销毁
        }
        else
        {
            AvailableActors.Add(Actor);
            ++TotalReturned;
            bReturnSucceeded = true;
            if (Preallocator.IsValid())
            {
                Preallocator->RecordUsagePattern(ActiveActors.Num());
            }
            ACTORPOOL_DEBUG(TEXT("Actor归还到池: %s"), *Actor->GetName());
        }
    }

    // Phase 4: 锁外销毁
    if (bShouldDestroy && IsValid(Actor))
    {
        Actor->Destroy();
    }

    return bReturnSucceeded;
}

bool FActorPool::FinishSpawningOnce(AActor* Actor, const FTransform& SpawnTransform, bool& OutFinishedNow)
{
    OutFinishedNow = false;

    if (!IsValid(Actor))
    {
        return false;
    }

    // 池侧真相：仅当登记过"未完成延迟构造"才执行 FinishSpawning（每实例至多一次）
    {
        FWriteScopeLock WriteLock(PoolLock);
        OutFinishedNow = (UnfinishedSpawnActors.Remove(Actor) > 0);
    }

    if (!OutFinishedNow)
    {
        return true;
    }

    Actor->FinishSpawning(SpawnTransform);
    if (IObjectPoolInterface::DoesActorImplementInterface(Actor))
    {
        IObjectPoolInterface::Execute_OnPoolActorCreated(Actor);
    }
    return true;
}

void FActorPool::PrewarmPool(UWorld* World, int32 Count)
{
    checkf(IsInGameThread(), TEXT("FActorPool::PrewarmPool 只能在游戏线程调用"));

    if (!bIsInitialized || !IsValid(World) || !IsValid(ActorClass) || Count <= 0)
    {
        return;
    }

    ACTORPOOL_LOG(Log, TEXT("预热池: %s, 数量=%d"), *ActorClass->GetName(), Count);

    int32 ActualCount = 0;
    {
        FReadScopeLock ReadLock(PoolLock);
        const int32 CurrentPoolSize = GetManagedActorCount_RequiresLock();
        ActualCount = FMath::Min(Count, MaxPoolSize - CurrentPoolSize);
    }

    if (ActualCount <= 0)
    {
        return;
    }

    int32 CreatedCount = 0;
    for (int32 i = 0; i < ActualCount; ++i)
    {
        AActor* NewActor = CreateNewActor(World);
        
        if (NewActor)
        {
            //  预热阶段只需要基本的状态重置，不调用生命周期事件
            // 避免在预热时触发OnReturnToPool导致死锁
            
            // 隐藏Actor并禁用Tick
            NewActor->SetActorHiddenInGame(true);
            NewActor->SetActorTickEnabled(false);
            
            // 禁用碰撞（必须先保存原始碰撞/物理设置，否则首次归还时
            // SaveOriginalCollisionSettings 会把已被破坏的 NoCollision 当作原始值永久固化）
            if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(NewActor->GetRootComponent()))
            {
                FObjectPoolUtils::SaveOriginalCollisionSettings(RootPrimitive);
                RootPrimitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                RootPrimitive->SetSimulatePhysics(false);
            }
            
            bool bAddedToPool = false;
            {
                FWriteScopeLock WriteLock(PoolLock);
                if (GetManagedActorCount_RequiresLock() < MaxPoolSize)
                {
                    AvailableActors.Add(NewActor);
                    AllActorsSet.Add(NewActor);
                    bAddedToPool = true;
                }
            }

            if (bAddedToPool)
            {
                ++CreatedCount;
            }
            else
            {
                // 在锁外销毁，避免重入导致锁竞争
                NewActor->Destroy();
                break;
            }
        }
        else
        {
            ACTORPOOL_LOG(Warning, TEXT("预热时创建Actor失败: %s"), *ActorClass->GetName());
            break;
        }
    }

    ACTORPOOL_LOG(Log, TEXT("预热完成: %s, 实际创建=%d"), *ActorClass->GetName(), CreatedCount);
}

//  状态查询功能实现

FObjectPoolStats FActorPool::GetStats() const
{
    FReadScopeLock ReadLock(PoolLock);

    FObjectPoolStats Stats;
    Stats.TotalCreated = TotalCreated;
    Stats.CurrentActive = ActiveActors.Num();
    Stats.CurrentAvailable = AvailableActors.Num();
    Stats.PoolSize = GetManagedActorCount_RequiresLock();
    Stats.ActorClassName = ActorClass ? ActorClass->GetName() : TEXT("Unknown");

    // 修复：填充获取和归还统计信息
    Stats.TotalAcquired = TotalRequests;
    Stats.TotalReleased = TotalReturned;

    // 计算命中率
    if (TotalRequests > 0)
    {
        Stats.HitRate = static_cast<float>(PoolHits) / static_cast<float>(TotalRequests);
    }
    else
    {
        Stats.HitRate = 0.0f;
    }

    return Stats;
}

int32 FActorPool::GetAvailableCount() const
{
    FReadScopeLock ReadLock(PoolLock);
    return AvailableActors.Num();
}

int32 FActorPool::GetActiveCount() const
{
    FReadScopeLock ReadLock(PoolLock);
    return ActiveActors.Num();
}

int32 FActorPool::GetPoolSize() const
{
    FReadScopeLock ReadLock(PoolLock);
    return GetManagedActorCount_RequiresLock();
}

bool FActorPool::IsEmpty() const
{
    FReadScopeLock ReadLock(PoolLock);
    return AvailableActors.Num() == 0;
}

bool FActorPool::IsFull() const
{
    FReadScopeLock ReadLock(PoolLock);
    return GetManagedActorCount_RequiresLock() >= MaxPoolSize;
}

bool FActorPool::ContainsActor(const AActor* Actor) const
{
    if (!IsValid(Actor))
    {
        return false;
    }

    FReadScopeLock ReadLock(PoolLock);

    // 使用 TSet 实现 O(1) 查找
    return AllActorsSet.Contains(Actor);
}

//  管理功能实现

void FActorPool::ClearPool()
{
    checkf(IsInGameThread(), TEXT("FActorPool::ClearPool 只能在游戏线程调用"));

    if (!bIsInitialized)
    {
        return;
    }

    // 按状态分组收集，锁外按状态区分生命周期事件
    TArray<AActor*> NormalActors;    // Active/Available：触发 OnReturnToPool
    TArray<AActor*> TransitionActors; // Pending/Finalizing/Returning：不触发生命周期事件
    {
        FWriteScopeLock WriteLock(PoolLock);

        for (const TWeakObjectPtr<AActor>& ActorPtr : AvailableActors)
        {
            if (AActor* Actor = ActorPtr.Get())
            {
                NormalActors.Add(Actor);
            }
        }

        for (const TWeakObjectPtr<AActor>& ActorPtr : ActiveActors)
        {
            if (AActor* Actor = ActorPtr.Get())
            {
                NormalActors.Add(Actor);
            }
        }

        // 过渡状态：未初始化 Pending 不应执行蓝图事件；Returning 已处于 OnReturnToPool 中；
        // Finalizing 正在执行激活回调。均不重复触发生命周期事件。
        for (const TWeakObjectPtr<AActor>& ActorPtr : PendingDeferredActors)
        {
            if (AActor* Actor = ActorPtr.Get())
            {
                TransitionActors.AddUnique(Actor);
            }
        }

        for (const TWeakObjectPtr<AActor>& ActorPtr : FinalizingActors)
        {
            if (AActor* Actor = ActorPtr.Get())
            {
                TransitionActors.AddUnique(Actor);
            }
        }

        for (const TWeakObjectPtr<AActor>& ActorPtr : ActivatingActors)
        {
            if (AActor* Actor = ActorPtr.Get())
            {
                TransitionActors.AddUnique(Actor);
            }
        }

        for (const TWeakObjectPtr<AActor>& ActorPtr : ReturningActors)
        {
            if (AActor* Actor = ActorPtr.Get())
            {
                TransitionActors.AddUnique(Actor);
            }
        }

        AvailableActors.Empty();
        ActiveActors.Empty();
        AllActorsSet.Empty();
        PendingDeferredActors.Empty();
        FinalizingActors.Empty();
        ActivatingActors.Empty();
        ReturningActors.Empty();
        UnfinishedSpawnActors.Empty();

        // 重置统计
        TotalRequests = 0;
        PoolHits = 0;
        TotalCreated = 0;
        TotalReturned = 0;
    }

    // 锁外执行销毁，降低回调重入风险
    // 正常 Active/Available：仅对已完成构造的 Actor 触发生命周期事件
    for (AActor* Actor : NormalActors)
    {
        if (!IsValid(Actor))
        {
            continue;
        }

        // PrewarmPool 创建的 Actor 可能未 FinishSpawning，不应触发蓝图事件
        if (Actor->IsActorInitialized() && IObjectPoolInterface::DoesActorImplementInterface(Actor))
        {
            IObjectPoolInterface::Execute_OnReturnToPool(Actor);
        }
        Actor->Destroy();
    }

    // 过渡状态：直接销毁，不触发生命周期事件
    for (AActor* Actor : TransitionActors)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
    }

    ACTORPOOL_LOG(Log, TEXT("清空池: %s"), ActorClass ? *ActorClass->GetName() : TEXT("Unknown"));
}

void FActorPool::SetMaxSize(int32 NewMaxSize)
{
    checkf(IsInGameThread(), TEXT("FActorPool::SetMaxSize 只能在游戏线程调用"));

    if (NewMaxSize <= 0)
    {
        return;
    }

    TArray<AActor*> ActorsToDestroy;
    int32 OldMaxSize = 0;
    {
        FWriteScopeLock WriteLock(PoolLock);

        OldMaxSize = MaxPoolSize;
        MaxPoolSize = NewMaxSize;

        //  统一容量计数
        int32 CurrentPoolSize = GetManagedActorCount_RequiresLock();
    
        // 如果新大小小于当前池大小，需要移除多余的Actor
        if (NewMaxSize < CurrentPoolSize)
        {
            int32 ExcessCount = CurrentPoolSize - NewMaxSize;

            // 优先移除可用的Actor
            while (ExcessCount > 0 && AvailableActors.Num() > 0)
            {
                TWeakObjectPtr<AActor> ActorPtr = AvailableActors.Pop();
                if (ActorPtr.IsValid())
                {
                    AllActorsSet.Remove(ActorPtr);
                    ActorsToDestroy.Add(ActorPtr.Get());
                }
                --ExcessCount;
            }
        }

        ACTORPOOL_LOG(Log, TEXT("设置池最大大小: %s, %d -> %d"),
            ActorClass ? *ActorClass->GetName() : TEXT("Unknown"), OldMaxSize, NewMaxSize);
    }

    // 锁外销毁，避免重入导致锁竞争
    for (AActor* Actor : ActorsToDestroy)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
    }
}

//  内部辅助方法实现

AActor* FActorPool::CreateNewActor(UWorld* World)
{
    SCOPE_CYCLE_COUNTER(STAT_ActorPool_CreateActor);

    if (!IsValid(World) || !IsValid(ActorClass))
    {
        return nullptr;
    }

    //  网络最佳实践：正确的预热Actor创建
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.bDeferConstruction = true; // 延迟构造，避免BeginPlay中的问题
    
    ACTORPOOL_LOG(VeryVerbose, TEXT("创建Actor用于对象池: %s"), *ActorClass->GetName());

    AActor* NewActor = World->SpawnActor<AActor>(ActorClass, FTransform::Identity, SpawnParams);

    if (IsValid(NewActor))
    {
        //  最安全策略：预热时完全不调用FinishSpawning，避免任何BeginPlay相关问题
        // Actor保持延迟构造状态，直到从池中获取时才完成初始化
        
        ACTORPOOL_LOG(VeryVerbose, TEXT("Actor预热创建成功（延迟构造状态）: %s"), *NewActor->GetName());
        
        // 不调用FinishSpawning，不调用DisableAutoActivateComponents
        // 保持最原始的延迟构造状态，最大程度避免死锁
        
        ++TotalCreated;

        // 登记"未完成延迟构造"状态：池是这些实例的唯一创建方与真相来源
        //（引擎 IsActorInitialized() 受世界初始化进度门控，判据见 FinishSpawningOnce）
        UnfinishedSpawnActors.Add(NewActor);

        //  OnPoolActorCreated事件已移动到ActivateActorFromPool中的FinishSpawning之后调用
        // 确保在Actor完全初始化后才触发生命周期事件，避免递归调用
        
        ACTORPOOL_DEBUG(TEXT("创建新Actor用于池化: %s"), *NewActor->GetName());
        return NewActor;
    }
    else
    {
        ACTORPOOL_LOG(Warning, TEXT("创建Actor失败: %s"), *ActorClass->GetName());
        return nullptr;
    }
}

bool FActorPool::ValidateActor(AActor* Actor) const
{
    if (!IsValid(Actor))
    {
        return false;
    }

    if (!Actor->IsA(ActorClass))
    {
        ACTORPOOL_LOG(Warning, TEXT("Actor类型不匹配: %s, 期望: %s"),
            *Actor->GetClass()->GetName(), *ActorClass->GetName());
        return false;
    }

    return true;
}

int64 FActorPool::CalculateMemoryUsage() const
{
    // 获取读锁
    FReadScopeLock ReadLock(PoolLock);
    
    // 基础内存使用估算
    int64 MemoryUsage = sizeof(FActorPool);
    
    // 活跃和可用Actor容器的内存
    MemoryUsage += ActiveActors.GetAllocatedSize();
    MemoryUsage += AvailableActors.GetAllocatedSize();
    MemoryUsage += PendingDeferredActors.GetAllocatedSize();
    MemoryUsage += FinalizingActors.GetAllocatedSize();
    MemoryUsage += ActivatingActors.GetAllocatedSize();
    MemoryUsage += ReturningActors.GetAllocatedSize();
    MemoryUsage += AllActorsSet.GetAllocatedSize();
    
    // 估算每个Actor的内存使用（简化计算，计入所有状态）
    int32 TotalActors = GetManagedActorCount_RequiresLock();
    MemoryUsage += TotalActors * 1024; // 假设每个Actor约1KB
    
    return MemoryUsage;
}

void FActorPool::CleanupInvalidActors()
{
    checkf(IsInGameThread(), TEXT("FActorPool::CleanupInvalidActors 只能在游戏线程调用"));
    FWriteScopeLock WriteLock(PoolLock);
    CleanupInvalidActors_RequiresLock();
}

void FActorPool::CleanupInvalidActors_RequiresLock()
{
    // 调用者必须持有写锁

    // 清理可用列表中的无效引用
    for (int32 i = AvailableActors.Num() - 1; i >= 0; --i)
    {
        if (!AvailableActors[i].IsValid())
        {
            AllActorsSet.Remove(AvailableActors[i]);
            AvailableActors.RemoveAtSwap(i);
        }
    }

    // 清理活跃列表中的无效引用
    for (int32 i = ActiveActors.Num() - 1; i >= 0; --i)
    {
        if (!ActiveActors[i].IsValid())
        {
            AllActorsSet.Remove(ActiveActors[i]);
            ActiveActors.RemoveAtSwap(i);
        }
    }

    // 清理过渡状态集合中的无效引用
    for (auto It = PendingDeferredActors.CreateIterator(); It; ++It)
    {
        if (!It->IsValid())
        {
            AllActorsSet.Remove(*It);
            It.RemoveCurrent();
        }
    }

    for (auto It = ReturningActors.CreateIterator(); It; ++It)
    {
        if (!It->IsValid())
        {
            AllActorsSet.Remove(*It);
            It.RemoveCurrent();
        }
    }

    for (auto It = FinalizingActors.CreateIterator(); It; ++It)
    {
        if (!It->IsValid())
        {
            AllActorsSet.Remove(*It);
            It.RemoveCurrent();
        }
    }

    for (auto It = ActivatingActors.CreateIterator(); It; ++It)
    {
        if (!It->IsValid())
        {
            AllActorsSet.Remove(*It);
            It.RemoveCurrent();
        }
    }

    for (auto It = UnfinishedSpawnActors.CreateIterator(); It; ++It)
    {
        if (!It->IsValid())
        {
            AllActorsSet.Remove(*It);
            It.RemoveCurrent();
        }
    }

    ACTORPOOL_DEBUG(TEXT("清理无效引用完成: %s"), *ActorClass->GetName());
}

void FActorPool::UpdateStats(bool bWasPoolHit)
{
    if (bWasPoolHit)
    {
        ++PoolHits;
    }
}

AActor* FActorPool::TakeFromAvailable_RequiresLock()
{
    // 注意：调用者必须持有写锁
    for (int32 i = AvailableActors.Num() - 1; i >= 0; --i)
    {
        if (AvailableActors[i].IsValid())
        {
            AActor* Actor = AvailableActors[i].Get();
            AvailableActors.RemoveAtSwap(i);
            return Actor;
        }
    }
    return nullptr;
}

void FActorPool::PeriodicCleanup_RequiresLock()
{
    // 注意：调用者必须持有写锁
    if (TotalRequests % CLEANUP_FREQUENCY == 0)
    {
        CleanupInvalidActors_RequiresLock();
    }
}

bool FActorPool::CanCreateMoreActors() const
{
    FReadScopeLock ReadLock(PoolLock);
    return GetManagedActorCount_RequiresLock() < MaxPoolSize;
}

int32 FActorPool::GetManagedActorCount_RequiresLock() const
{
    return ActiveActors.Num() + AvailableActors.Num() + PendingDeferredActors.Num() + FinalizingActors.Num() + ActivatingActors.Num() + ReturningActors.Num();
}

void FActorPool::InitializePool(UWorld* World)
{
    checkf(IsInGameThread(), TEXT("FActorPool::InitializePool 只能在游戏线程调用"));

    if (!bIsInitialized || !IsValid(World) || InitialSize <= 0)
    {
        return;
    }

    //  重新启用安全预热机制 - 预热池到初始大小
    if (InitialSize > 0)
    {
        PrewarmPool(World, InitialSize);
        ACTORPOOL_LOG(Log, TEXT("InitializePool预热完成: %s, 请求数量=%d"), 
            *ActorClass->GetName(), InitialSize);
    }
}

void FActorPool::ConfigurePreallocator(UWorld* World, const FObjectPoolConfig& Config)
{
    checkf(IsInGameThread(), TEXT("FActorPool::ConfigurePreallocator 只能在游戏线程调用"));

    if (!Preallocator.IsValid() || !IsValid(World))
    {
        return;
    }

    if (!Config.bEnablePrewarm || Config.PreallocationCount <= 0)
    {
        Preallocator->StopPreallocation();
        return;
    }

    Preallocator->StartPreallocation(World, Config);
}

FObjectPoolPreallocationStats FActorPool::GetPreallocationStats() const
{
    if (Preallocator.IsValid())
    {
        return Preallocator->GetStats();
    }

    return FObjectPoolPreallocationStats();
}

