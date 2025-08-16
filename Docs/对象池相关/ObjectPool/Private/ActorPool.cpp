// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 遵循IWYU原则的头文件包含
#include "ActorPool.h"

// ✅ UE核心依赖
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "HAL/PlatformFilemanager.h"
#include "UObject/GarbageCollection.h"

// ✅ 对象池模块依赖
#include "ObjectPool.h"
#include "ObjectPoolPreallocator.h"
#include "ObjectPoolSubsystem.h"
#include "ObjectPoolInterface.h"

FActorPool::FActorPool(UClass* InActorClass, int32 InInitialSize, int32 InHardLimit)
    : ActorClass(InActorClass)
    , HardLimit(InHardLimit)
    , InitialSize(InInitialSize)
    , TotalRequests(0)
    , PoolHits(0)
    , bIsInitialized(false)
    , Preallocator(MakeUnique<FObjectPoolPreallocator>(this))
    , CreationTime(FDateTime::Now())
{
    // ✅ 验证输入参数
    if (!IsValid(ActorClass))
    {
        OBJECTPOOL_LOG(Error, TEXT("FActorPool: 无效的Actor类"));
        return;
    }

    if (InInitialSize < 0)
    {
        OBJECTPOOL_LOG(Warning, TEXT("FActorPool: 初始大小不能为负数，设置为0"));
        InitialSize = 0;
    }

    if (InHardLimit < 0)
    {
        OBJECTPOOL_LOG(Warning, TEXT("FActorPool: 硬限制不能为负数，设置为0（无限制）"));
        HardLimit = 0;
    }

    // ✅ 初始化统计信息
    Stats = FObjectPoolStats(ActorClass->GetName(), HardLimit);
    
    // ✅ 预分配容器空间
    AvailableActors.Reserve(InitialSize);
    ActiveActors.Reserve(InitialSize);

    bIsInitialized = true;

    OBJECTPOOL_LOG(Log, TEXT("创建Actor池: %s, 初始大小: %d, 硬限制: %d"), 
        *ActorClass->GetName(), InitialSize, HardLimit);
}

FActorPool::~FActorPool()
{
    OBJECTPOOL_LOG(Log, TEXT("销毁Actor池: %s"), ActorClass ? *ActorClass->GetName() : TEXT("Unknown"));
    
    // ✅ 清理所有Actor
    ClearPool();
}

FActorPool::FActorPool(FActorPool&& Other) noexcept
    : ActorClass(Other.ActorClass)
    , AvailableActors(MoveTemp(Other.AvailableActors))
    , ActiveActors(MoveTemp(Other.ActiveActors))
    , HardLimit(Other.HardLimit)
    , InitialSize(Other.InitialSize)
    , Stats(MoveTemp(Other.Stats))
    , TotalRequests(Other.TotalRequests)
    , PoolHits(Other.PoolHits)
    , bIsInitialized(Other.bIsInitialized)
    , CreationTime(Other.CreationTime)
{
    // ✅ 重置源对象
    Other.ActorClass = nullptr;
    Other.HardLimit = 0;
    Other.InitialSize = 0;
    Other.TotalRequests = 0;
    Other.PoolHits = 0;
    Other.bIsInitialized = false;
}

FActorPool& FActorPool::operator=(FActorPool&& Other) noexcept
{
    if (this != &Other)
    {
        // ✅ 清理当前资源
        ClearPool();

        // ✅ 移动资源
        ActorClass = Other.ActorClass;
        AvailableActors = MoveTemp(Other.AvailableActors);
        ActiveActors = MoveTemp(Other.ActiveActors);
        HardLimit = Other.HardLimit;
        InitialSize = Other.InitialSize;
        Stats = MoveTemp(Other.Stats);
        TotalRequests = Other.TotalRequests;
        PoolHits = Other.PoolHits;
        bIsInitialized = Other.bIsInitialized;
        CreationTime = Other.CreationTime;

        // ✅ 重置源对象
        Other.ActorClass = nullptr;
        Other.HardLimit = 0;
        Other.InitialSize = 0;
        Other.TotalRequests = 0;
        Other.PoolHits = 0;
        Other.bIsInitialized = false;
    }
    return *this;
}

AActor* FActorPool::GetActor(UWorld* World, const FTransform& SpawnTransform)
{
    // OBJECTPOOL_STAT(ObjectPool_GetActor); // TODO: 实现性能统计

    if (!bIsInitialized || !IsValid(ActorClass) || !IsValid(World))
    {
        OBJECTPOOL_LOG(Error, TEXT("FActorPool::GetActor: 池未初始化或参数无效"));
        return nullptr;
    }

    // ✅ 更新统计
    ++TotalRequests;

    AActor* ResultActor = nullptr;

    // ✅ 首先尝试从池中获取（使用智能读锁）
    {
        FScopedSmartReadLock ReadLock(*this, ThreadSafetyConfig.LockTimeoutMs);

        if (!ReadLock.IsLocked())
        {
            OBJECTPOOL_LOG(Warning, TEXT("获取读锁失败，回退到创建新Actor"));
            // 继续执行，会在后面创建新Actor
        }
        else
        {
            // ✅ 清理无效引用
            for (int32 i = AvailableActors.Num() - 1; i >= 0; --i)
            {
                if (!AvailableActors[i].IsValid())
                {
                    AvailableActors.RemoveAtSwap(i);
                }
            }

            // ✅ 如果有可用Actor，获取最后一个
            if (AvailableActors.Num() > 0)
            {
                TWeakObjectPtr<AActor> ActorPtr = AvailableActors.Pop();
                if (ActorPtr.IsValid())
                {
                    ResultActor = ActorPtr.Get();
                    ++PoolHits;
                    OBJECTPOOL_LOG(VeryVerbose, TEXT("从池获取Actor: %s"), *ResultActor->GetName());
                }
            }
        }
    }

    // ✅ 如果池中没有可用Actor，创建新的
    if (!ResultActor)
    {
        // ✅ 检查硬限制
        if (HardLimit > 0 && (AvailableActors.Num() + ActiveActors.Num()) >= HardLimit)
        {
            OBJECTPOOL_LOG(Warning, TEXT("Actor池已达到硬限制: %d"), HardLimit);
            // ✅ 即使达到限制，也要创建Actor（永不失败原则）
        }

        ResultActor = CreateNewActor(World, SpawnTransform);
        if (!ResultActor)
        {
            OBJECTPOOL_LOG(Error, TEXT("创建新Actor失败: %s"), *ActorClass->GetName());
            return nullptr;
        }

        UpdateStats(EObjectPoolEvent::ActorCreated);
    }

    // ✅ 重置Actor状态并激活
    if (ResultActor)
    {
        // ✅ 使用智能写锁更新活跃列表
        {
            FScopedSmartWriteLock WriteLock(*this, ThreadSafetyConfig.LockTimeoutMs);
            if (WriteLock.IsLocked())
            {
                ActiveActors.Add(ResultActor);
            }
            else
            {
                OBJECTPOOL_LOG(Warning, TEXT("获取写锁失败，无法更新活跃列表"));
            }
        }

        ResetActorState(ResultActor, SpawnTransform);

        // ✅ 使用增强的生命周期事件系统
        bool bEventSuccess = IObjectPoolInterface::CallLifecycleEventEnhanced(
            ResultActor, EObjectPoolLifecycleEvent::Activated, false, 1000);

        if (!bEventSuccess)
        {
            // ✅ 回退到传统方法
            CallLifecycleEvent(ResultActor, TEXT("OnPoolActorActivated"));
        }

        UpdateStats(EObjectPoolEvent::ActorAcquired);

        OBJECTPOOL_LOG(VeryVerbose, TEXT("从池获取Actor: %s"), *ResultActor->GetName());
    }

    return ResultActor;
}

bool FActorPool::ReturnActor(AActor* Actor)
{
    // OBJECTPOOL_STAT(ObjectPool_ReturnActor); // TODO: 实现性能统计

    if (!IsValid(Actor) || !bIsInitialized)
    {
        OBJECTPOOL_LOG(Warning, TEXT("FActorPool::ReturnActor: Actor无效或池未初始化"));
        return false;
    }

    // ✅ 验证Actor类型
    if (!Actor->IsA(ActorClass))
    {
        OBJECTPOOL_LOG(Warning, TEXT("尝试归还错误类型的Actor: %s, 期望: %s"),
            *Actor->GetClass()->GetName(), *ActorClass->GetName());
        return false;
    }

    FWriteScopeLock WriteLock(PoolLock);

    // ✅ 从活跃列表中移除
    bool bFoundInActive = false;
    for (int32 i = 0; i < ActiveActors.Num(); ++i)
    {
        if (ActiveActors[i].Get() == Actor)
        {
            ActiveActors.RemoveAtSwap(i);
            bFoundInActive = true;
            break;
        }
    }

    if (!bFoundInActive)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("Actor不在活跃列表中，可能已经归还: %s"), *Actor->GetName());
    }

    // ✅ 使用增强的生命周期事件系统
    bool bEventSuccess = IObjectPoolInterface::CallLifecycleEventEnhanced(
        Actor, EObjectPoolLifecycleEvent::ReturnedToPool, false, 1000);

    if (!bEventSuccess)
    {
        // ✅ 回退到传统方法
        CallLifecycleEvent(Actor, TEXT("OnReturnToPool"));
    }

    // ✅ 隐藏和停用Actor
    Actor->SetActorHiddenInGame(true);
    Actor->SetActorEnableCollision(false);
    Actor->SetActorTickEnabled(false);

    // ✅ 添加到可用列表
    AvailableActors.Add(Actor);
    UpdateStats(EObjectPoolEvent::ActorReturned);

    OBJECTPOOL_LOG(VeryVerbose, TEXT("Actor归还到池: %s"), *Actor->GetName());
    return true;
}

void FActorPool::PrewarmPool(UWorld* World, int32 Count)
{
    if (!IsValid(World) || !IsValid(ActorClass) || Count <= 0)
    {
        return;
    }

    OBJECTPOOL_LOG(Log, TEXT("预热Actor池: %s, 数量: %d"), *ActorClass->GetName(), Count);

    FWriteScopeLock WriteLock(PoolLock);

    for (int32 i = 0; i < Count; ++i)
    {
        // ✅ 检查硬限制
        if (HardLimit > 0 && (AvailableActors.Num() + ActiveActors.Num()) >= HardLimit)
        {
            OBJECTPOOL_LOG(Log, TEXT("预热时达到硬限制，停止预热"));
            break;
        }

        AActor* NewActor = CreateNewActor(World, FTransform::Identity);
        if (NewActor)
        {
            // ✅ 立即隐藏预热的Actor
            NewActor->SetActorHiddenInGame(true);
            NewActor->SetActorEnableCollision(false);
            NewActor->SetActorTickEnabled(false);

            AvailableActors.Add(NewActor);

            // ✅ 使用增强的生命周期事件系统
            bool bEventSuccess = IObjectPoolInterface::CallLifecycleEventEnhanced(
                NewActor, EObjectPoolLifecycleEvent::Created, false, 1000);

            if (!bEventSuccess)
            {
                // ✅ 回退到传统方法
                CallLifecycleEvent(NewActor, TEXT("OnPoolActorCreated"));
            }

            UpdateStats(EObjectPoolEvent::ActorCreated);
        }
    }

    OBJECTPOOL_LOG(Log, TEXT("预热完成，可用Actor数量: %d"), AvailableActors.Num());
}

void FActorPool::ClearPool()
{
    FWriteScopeLock WriteLock(PoolLock);

    OBJECTPOOL_LOG(Log, TEXT("清空Actor池: %s"), ActorClass ? *ActorClass->GetName() : TEXT("Unknown"));

    // ✅ 销毁所有可用Actor
    for (const TWeakObjectPtr<AActor>& ActorPtr : AvailableActors)
    {
        if (AActor* Actor = ActorPtr.Get())
        {
            Actor->Destroy();
            UpdateStats(EObjectPoolEvent::ActorDestroyed);
        }
    }
    AvailableActors.Empty();

    // ✅ 销毁所有活跃Actor
    for (const TWeakObjectPtr<AActor>& ActorPtr : ActiveActors)
    {
        if (AActor* Actor = ActorPtr.Get())
        {
            Actor->Destroy();
            UpdateStats(EObjectPoolEvent::ActorDestroyed);
        }
    }
    ActiveActors.Empty();

    // ✅ 重置统计信息
    Stats.CurrentActive = 0;
    Stats.CurrentAvailable = 0;
    TotalRequests = 0;
    PoolHits = 0;

    UpdateStats(EObjectPoolEvent::PoolCleared);
}

FObjectPoolStats FActorPool::GetStats() const
{
    FReadScopeLock ReadLock(PoolLock);

    // ✅ 更新当前状态
    FObjectPoolStats CurrentStats = Stats;
    CurrentStats.CurrentActive = ActiveActors.Num();
    CurrentStats.CurrentAvailable = AvailableActors.Num();
    CurrentStats.UpdateHitRate(PoolHits, TotalRequests);

    return CurrentStats;
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

bool FActorPool::IsEmpty() const
{
    FReadScopeLock ReadLock(PoolLock);
    return AvailableActors.Num() == 0;
}

bool FActorPool::IsFull() const
{
    if (HardLimit <= 0)
    {
        return false; // 无限制
    }

    FReadScopeLock ReadLock(PoolLock);
    return (AvailableActors.Num() + ActiveActors.Num()) >= HardLimit;
}

void FActorPool::SetHardLimit(int32 NewLimit)
{
    FWriteScopeLock WriteLock(PoolLock);

    if (NewLimit < 0)
    {
        NewLimit = 0; // 0表示无限制
    }

    HardLimit = NewLimit;
    Stats.PoolSize = NewLimit;

    OBJECTPOOL_LOG(Log, TEXT("设置Actor池硬限制: %s, 新限制: %d"),
        *ActorClass->GetName(), NewLimit);
}

bool FActorPool::ValidatePool() const
{
    FReadScopeLock ReadLock(PoolLock);

    // ✅ 检查基本状态
    if (!IsValid(ActorClass) || !bIsInitialized)
    {
        return false;
    }

    // ✅ 检查Actor引用有效性
    for (const TWeakObjectPtr<AActor>& ActorPtr : AvailableActors)
    {
        if (!ActorPtr.IsValid())
        {
            OBJECTPOOL_LOG(Warning, TEXT("发现无效的可用Actor引用"));
            return false;
        }
    }

    for (const TWeakObjectPtr<AActor>& ActorPtr : ActiveActors)
    {
        if (!ActorPtr.IsValid())
        {
            OBJECTPOOL_LOG(Warning, TEXT("发现无效的活跃Actor引用"));
            return false;
        }
    }

    return true;
}

AActor* FActorPool::CreateNewActor(UWorld* World, const FTransform& SpawnTransform)
{
    if (!IsValid(World) || !IsValid(ActorClass))
    {
        return nullptr;
    }

    // ✅ 使用UE的延迟生成机制
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.bDeferConstruction = true;

    AActor* NewActor = World->SpawnActorDeferred<AActor>(ActorClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!NewActor)
    {
        OBJECTPOOL_LOG(Error, TEXT("延迟生成Actor失败: %s"), *ActorClass->GetName());
        return nullptr;
    }

    // ✅ 完成生成
    NewActor->FinishSpawning(SpawnTransform);

    // ✅ 使用增强的生命周期事件系统
    bool bEventSuccess = IObjectPoolInterface::CallLifecycleEventEnhanced(
        NewActor, EObjectPoolLifecycleEvent::Created, false, 1000);

    if (!bEventSuccess)
    {
        // ✅ 回退到传统方法
        CallLifecycleEvent(NewActor, TEXT("OnPoolActorCreated"));
    }

    OBJECTPOOL_LOG(VeryVerbose, TEXT("创建新Actor: %s"), *NewActor->GetName());
    return NewActor;
}

void FActorPool::ResetActorState(AActor* Actor, const FTransform& SpawnTransform)
{
    if (!IsValid(Actor))
    {
        OBJECTPOOL_LOG(Warning, TEXT("ResetActorState: Actor无效"));
        return;
    }

    // ✅ 使用子系统的统一状态重置API（遵循单一职责原则）
    if (UObjectPoolSubsystem* Subsystem = UObjectPoolSubsystem::GetGlobal())
    {
        FActorResetConfig ResetConfig; // 使用默认配置
        bool bSuccess = Subsystem->ResetActorState(Actor, SpawnTransform, ResetConfig);

        if (bSuccess)
        {
            OBJECTPOOL_LOG(VeryVerbose, TEXT("通过子系统成功重置Actor状态: %s"), *Actor->GetName());
        }
        else
        {
            OBJECTPOOL_LOG(Warning, TEXT("通过子系统重置Actor状态失败: %s"), *Actor->GetName());
            // 回退到基础重置
            FallbackBasicReset(Actor, SpawnTransform);
        }
    }
    else
    {
        OBJECTPOOL_LOG(Warning, TEXT("无法获取子系统，使用基础重置: %s"), *Actor->GetName());
        // 回退到基础重置
        FallbackBasicReset(Actor, SpawnTransform);
    }
}

void FActorPool::FallbackBasicReset(AActor* Actor, const FTransform& SpawnTransform)
{
    if (!IsValid(Actor))
    {
        return;
    }

    OBJECTPOOL_LOG(VeryVerbose, TEXT("执行基础重置回退: %s"), *Actor->GetName());

    // ✅ 基础状态重置（最小必要功能）
    Actor->SetActorTransform(SpawnTransform);
    Actor->SetActorHiddenInGame(false);
    Actor->SetActorEnableCollision(true);
    Actor->SetActorTickEnabled(true);

    // ✅ 基础物理重置
    if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
    {
        RootPrimitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
        RootPrimitive->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    }

    OBJECTPOOL_LOG(VeryVerbose, TEXT("完成基础重置回退: %s"), *Actor->GetName());
}

bool FActorPool::ValidateActor(AActor* Actor) const
{
    if (!IsValid(Actor))
    {
        return false;
    }

    // ✅ 检查Actor类型
    if (!Actor->IsA(ActorClass))
    {
        return false;
    }

    // ✅ 检查Actor是否在正确的世界中
    if (!IsValid(Actor->GetWorld()))
    {
        return false;
    }

    return true;
}

void FActorPool::CallLifecycleEvent(AActor* Actor, const FString& EventType) const
{
    if (!IsValid(Actor))
    {
        return;
    }

    // ✅ 使用接口的静态方法调用生命周期事件
    IObjectPoolInterface::SafeCallLifecycleEvent(Actor, EventType);
}

void FActorPool::CleanupInvalidActors()
{
    FWriteScopeLock WriteLock(PoolLock);

    // ✅ 清理可用列表中的无效Actor
    for (int32 i = AvailableActors.Num() - 1; i >= 0; --i)
    {
        if (!AvailableActors[i].IsValid())
        {
            AvailableActors.RemoveAtSwap(i);
        }
    }

    // ✅ 清理活跃列表中的无效Actor
    for (int32 i = ActiveActors.Num() - 1; i >= 0; --i)
    {
        if (!ActiveActors[i].IsValid())
        {
            ActiveActors.RemoveAtSwap(i);
        }
    }
}

void FActorPool::AddReferencedObjects(FReferenceCollector& Collector)
{
    FReadScopeLock ReadLock(PoolLock);

    // ✅ 向GC报告可用Actor列表中的所有有效引用
    for (const TWeakObjectPtr<AActor>& WeakActor : AvailableActors)
    {
        if (AActor* Actor = WeakActor.Get())
        {
            Collector.AddReferencedObject(Actor);
        }
    }

    // ✅ 向GC报告活跃Actor列表中的所有有效引用
    for (const TWeakObjectPtr<AActor>& WeakActor : ActiveActors)
    {
        if (AActor* Actor = WeakActor.Get())
        {
            Collector.AddReferencedObject(Actor);
        }
    }

    OBJECTPOOL_LOG(VeryVerbose, TEXT("AddReferencedObjects: 已向GC报告 %d 个可用Actor和 %d 个活跃Actor"),
        AvailableActors.Num(), ActiveActors.Num());
}

FActorPoolStats FActorPool::GetPoolStats() const
{
    FReadScopeLock ReadLock(PoolLock);

    FActorPoolStats PoolStats;
    PoolStats.TotalCreated = TotalCreatedCount;
    PoolStats.CurrentActive = ActiveActors.Num();
    PoolStats.CurrentAvailable = AvailableActors.Num();
    PoolStats.PoolSize = MaxPoolSize;
    PoolStats.HitRate = TotalRequestCount > 0 ? static_cast<float>(PoolHitCount) / static_cast<float>(TotalRequestCount) : 0.0f;
    PoolStats.ActorClassName = ActorClass ? ActorClass->GetName() : TEXT("Unknown");
    PoolStats.LastUsedTime = LastUsedTime;

    return PoolStats;
}

void FActorPool::UpdateStats(EObjectPoolEvent EventType)
{
    // ✅ 更新统计信息（不需要锁，因为调用者已经持有锁）
    switch (EventType)
    {
    case EObjectPoolEvent::ActorCreated:
        ++Stats.TotalCreated;
        break;
    case EObjectPoolEvent::ActorAcquired:
        Stats.LastUsedTime = FDateTime::Now();
        break;
    case EObjectPoolEvent::ActorReturned:
        Stats.LastUsedTime = FDateTime::Now();
        break;
    case EObjectPoolEvent::PoolCleared:
        Stats.TotalCreated = 0;
        break;
    default:
        break;
    }
}

// ✅ 增强的线程安全功能实现

bool FActorPool::TryReadLock(int32 TimeoutMs) const
{
    if (!ThreadSafetyConfig.bEnableDeadlockDetection)
    {
        // 简单模式：直接获取读锁
        PoolLock.ReadLock();

        if (ThreadSafetyConfig.bEnableLockProfiling)
        {
            ReadLockCount.fetch_add(1);
        }

        return true;
    }

    // ✅ 死锁检测模式
    uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
    double StartTime = FPlatformTime::Seconds();

    // 检查是否已经持有写锁（避免死锁）
    if (CurrentLockHolderThreadId.load() == CurrentThreadId && CurrentLockType.load() == 2)
    {
        OBJECTPOOL_LOG(Warning, TEXT("检测到潜在死锁：线程 %d 尝试获取读锁但已持有写锁"), CurrentThreadId);
        LockContentionCount.fetch_add(1);
        return false;
    }

    // 尝试获取读锁（带超时）
    bool bLockAcquired = false;
    double TimeoutSeconds = TimeoutMs / 1000.0;

    while (!bLockAcquired && (FPlatformTime::Seconds() - StartTime) < TimeoutSeconds)
    {
        if (PoolLock.TryReadLock())
        {
            bLockAcquired = true;
            CurrentLockType.store(1); // 标记为读锁
            break;
        }

        // 短暂等待后重试
        FPlatformProcess::Sleep(0.001f); // 1ms
    }

    if (bLockAcquired)
    {
        if (ThreadSafetyConfig.bEnableLockProfiling)
        {
            double LockTime = (FPlatformTime::Seconds() - StartTime) * 1000.0;
            ReadLockCount.fetch_add(1);
            TotalLockTimeMs.fetch_add(LockTime);

            // 更新最大锁时间
            double CurrentMax = MaxLockTimeMs.load();
            while (LockTime > CurrentMax && !MaxLockTimeMs.compare_exchange_weak(CurrentMax, LockTime)) {}
        }
    }
    else
    {
        OBJECTPOOL_LOG(Warning, TEXT("读锁获取超时：线程 %d，超时时间 %d ms"), CurrentThreadId, TimeoutMs);
        LockContentionCount.fetch_add(1);
    }

    return bLockAcquired;
}

bool FActorPool::TryWriteLock(int32 TimeoutMs) const
{
    if (!ThreadSafetyConfig.bEnableDeadlockDetection)
    {
        // 简单模式：直接获取写锁
        PoolLock.WriteLock();

        if (ThreadSafetyConfig.bEnableLockProfiling)
        {
            WriteLockCount.fetch_add(1);
        }

        return true;
    }

    // ✅ 死锁检测模式
    uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
    double StartTime = FPlatformTime::Seconds();

    // 检查是否已经持有任何锁（避免死锁）
    if (CurrentLockHolderThreadId.load() == CurrentThreadId)
    {
        OBJECTPOOL_LOG(Warning, TEXT("检测到潜在死锁：线程 %d 尝试获取写锁但已持有锁"), CurrentThreadId);
        LockContentionCount.fetch_add(1);
        return false;
    }

    // 尝试获取写锁（带超时）
    bool bLockAcquired = false;
    double TimeoutSeconds = TimeoutMs / 1000.0;

    while (!bLockAcquired && (FPlatformTime::Seconds() - StartTime) < TimeoutSeconds)
    {
        if (PoolLock.TryWriteLock())
        {
            bLockAcquired = true;
            CurrentLockHolderThreadId.store(CurrentThreadId);
            CurrentLockType.store(2); // 标记为写锁
            break;
        }

        // 短暂等待后重试
        FPlatformProcess::Sleep(0.001f); // 1ms
    }

    if (bLockAcquired)
    {
        if (ThreadSafetyConfig.bEnableLockProfiling)
        {
            double LockTime = (FPlatformTime::Seconds() - StartTime) * 1000.0;
            WriteLockCount.fetch_add(1);
            TotalLockTimeMs.fetch_add(LockTime);

            // 更新最大锁时间
            double CurrentMax = MaxLockTimeMs.load();
            while (LockTime > CurrentMax && !MaxLockTimeMs.compare_exchange_weak(CurrentMax, LockTime)) {}
        }
    }
    else
    {
        OBJECTPOOL_LOG(Warning, TEXT("写锁获取超时：线程 %d，超时时间 %d ms"), CurrentThreadId, TimeoutMs);
        LockContentionCount.fetch_add(1);
    }

    return bLockAcquired;
}

void FActorPool::ReleaseReadLock() const
{
    PoolLock.ReadUnlock();
    CurrentLockType.store(0); // 清除锁标记
}

void FActorPool::ReleaseWriteLock() const
{
    CurrentLockHolderThreadId.store(0); // 清除线程ID
    CurrentLockType.store(0); // 清除锁标记
    PoolLock.WriteUnlock();
}

bool FActorPool::IsLockHeldByCurrentThread() const
{
    uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
    return CurrentLockHolderThreadId.load() == CurrentThreadId;
}

FActorPool::FLockPerformanceStats FActorPool::GetLockStats() const
{
    FLockPerformanceStats LockStats;

    LockStats.ReadLockCount = ReadLockCount.load();
    LockStats.WriteLockCount = WriteLockCount.load();
    LockStats.ContentionCount = LockContentionCount.load();

    uint64 TotalLocks = LockStats.ReadLockCount + LockStats.WriteLockCount;
    if (TotalLocks > 0)
    {
        LockStats.AverageLockTimeMs = TotalLockTimeMs.load() / TotalLocks;
    }

    LockStats.MaxLockTimeMs = MaxLockTimeMs.load();

    return LockStats;
}

// ✅ 智能预分配功能实现（委托给专门的管理器）

bool FActorPool::StartSmartPreallocation(UWorld* World, const FObjectPoolPreallocationConfig& Config)
{
    if (!Preallocator.IsValid())
    {
        OBJECTPOOL_LOG(Error, TEXT("StartSmartPreallocation: 预分配管理器无效"));
        return false;
    }

    return Preallocator->StartPreallocation(World, Config);
}

void FActorPool::StopSmartPreallocation()
{
    if (Preallocator.IsValid())
    {
        Preallocator->StopPreallocation();
    }
}

void FActorPool::TickSmartPreallocation(float DeltaTime)
{
    if (Preallocator.IsValid())
    {
        Preallocator->Tick(DeltaTime);
    }
}

FObjectPoolPreallocationStats FActorPool::GetPreallocationStats() const
{
    if (Preallocator.IsValid())
    {
        return Preallocator->GetStats();
    }

    return FObjectPoolPreallocationStats();
}

int64 FActorPool::CalculateMemoryUsage() const
{
    FScopedSmartReadLock ReadLock(*this);
    if (!ReadLock.IsLocked())
    {
        OBJECTPOOL_LOG(Warning, TEXT("CalculateMemoryUsage: 无法获取读锁"));
        return 0;
    }

    int64 TotalMemory = 0;

    // ✅ 计算可用Actor的内存
    for (const TWeakObjectPtr<AActor>& ActorPtr : AvailableActors)
    {
        if (ActorPtr.IsValid())
        {
            AActor* Actor = ActorPtr.Get();
            if (Actor)
            {
                // 简单估算：Actor大小 + 组件开销
                TotalMemory += sizeof(AActor);
                TotalMemory += Actor->GetClass()->GetStructureSize();
                TotalMemory += 512; // 估算组件和其他开销
            }
        }
    }

    // ✅ 计算活跃Actor的内存
    for (const TWeakObjectPtr<AActor>& ActorPtr : ActiveActors)
    {
        if (ActorPtr.IsValid())
        {
            AActor* Actor = ActorPtr.Get();
            if (Actor)
            {
                TotalMemory += sizeof(AActor);
                TotalMemory += Actor->GetClass()->GetStructureSize();
                TotalMemory += 512;
            }
        }
    }

    OBJECTPOOL_LOG(VeryVerbose, TEXT("CalculateMemoryUsage: 总内存使用量 %lld 字节"), TotalMemory);

    return TotalMemory;
}

UWorld* FActorPool::GetWorld() const
{
    // ✅ 首先尝试从现有Actor获取World（最快的方式，避免子系统调用开销）
    {
        FScopedSmartReadLock ReadLock(*this);
        if (ReadLock.IsLocked())
        {
            // 从可用Actor中获取
            for (const TWeakObjectPtr<AActor>& ActorPtr : AvailableActors)
            {
                if (ActorPtr.IsValid())
                {
                    AActor* Actor = ActorPtr.Get();
                    if (Actor && Actor->GetWorld())
                    {
                        return Actor->GetWorld();
                    }
                }
            }

            // 从活跃Actor中获取
            for (const TWeakObjectPtr<AActor>& ActorPtr : ActiveActors)
            {
                if (ActorPtr.IsValid())
                {
                    AActor* Actor = ActorPtr.Get();
                    if (Actor && Actor->GetWorld())
                    {
                        return Actor->GetWorld();
                    }
                }
            }
        }
    }

    // ✅ 如果池中没有Actor，完全依赖子系统的智能获取方法
    // 使用GetGlobal()方法，让子系统自动查找最合适的实例
    if (UObjectPoolSubsystem* Subsystem = UObjectPoolSubsystem::GetGlobal())
    {
        return Subsystem->GetValidWorld();
    }

    OBJECTPOOL_LOG(Warning, TEXT("GetWorld: 无法通过子系统获取World，这表明子系统可能未正确初始化"));
    return nullptr;
}
