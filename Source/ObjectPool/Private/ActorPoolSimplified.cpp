// Fill out your copyright notice in the Description page of Project Settings.

#include "ActorPoolSimplified.h"
#include "ObjectPool.h"

// ✅ UE核心依赖
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"

// ✅ 日志和统计
DEFINE_LOG_CATEGORY(LogActorPoolSimplified);

#if STATS
DEFINE_STAT(STAT_ActorPoolSimplified_GetActor);
DEFINE_STAT(STAT_ActorPoolSimplified_ReturnActor);
DEFINE_STAT(STAT_ActorPoolSimplified_CreateActor);
#endif

// ✅ 构造函数和析构函数

FActorPoolSimplified::FActorPoolSimplified(UClass* InActorClass, int32 InInitialSize, int32 InHardLimit)
    : ActorClass(InActorClass)
    , MaxPoolSize(InHardLimit > 0 ? InHardLimit : DEFAULT_HARD_LIMIT)
    , InitialSize(FMath::Clamp(InInitialSize, 1, MaxPoolSize))
    , TotalRequests(0)
    , PoolHits(0)
    , TotalCreated(0)
    , bIsInitialized(false)
{
    // ✅ 验证输入参数
    if (!IsValid(ActorClass))
    {
        ACTORPOOL_SIMPLIFIED_LOG(Error, TEXT("FActorPoolSimplified: 无效的Actor类"));
        return;
    }

    // ✅ 预分配数组空间
    AvailableActors.Reserve(InitialSize);
    ActiveActors.Reserve(InitialSize);

    bIsInitialized = true;

    ACTORPOOL_SIMPLIFIED_LOG(Log, TEXT("创建简化Actor池: %s, 初始大小=%d, 最大大小=%d"),
        *ActorClass->GetName(), InitialSize, MaxPoolSize);
}

FActorPoolSimplified::~FActorPoolSimplified()
{
    if (bIsInitialized)
    {
        ClearPool();
        ACTORPOOL_SIMPLIFIED_LOG(Log, TEXT("销毁简化Actor池: %s"), 
            ActorClass ? *ActorClass->GetName() : TEXT("Unknown"));
    }
}

// ✅ 移动语义实现

FActorPoolSimplified::FActorPoolSimplified(FActorPoolSimplified&& Other) noexcept
    : ActorClass(Other.ActorClass)
    , MaxPoolSize(Other.MaxPoolSize)
    , InitialSize(Other.InitialSize)
    , TotalRequests(Other.TotalRequests)
    , PoolHits(Other.PoolHits)
    , TotalCreated(Other.TotalCreated)
    , bIsInitialized(Other.bIsInitialized)
    , AvailableActors(MoveTemp(Other.AvailableActors))
    , ActiveActors(MoveTemp(Other.ActiveActors))
{
    // 重置源对象
    Other.ActorClass = nullptr;
    Other.bIsInitialized = false;
}

FActorPoolSimplified& FActorPoolSimplified::operator=(FActorPoolSimplified&& Other) noexcept
{
    if (this != &Other)
    {
        // 清理当前对象
        ClearPool();

        // 移动数据
        ActorClass = Other.ActorClass;
        MaxPoolSize = Other.MaxPoolSize;
        InitialSize = Other.InitialSize;
        TotalRequests = Other.TotalRequests;
        PoolHits = Other.PoolHits;
        TotalCreated = Other.TotalCreated;
        bIsInitialized = Other.bIsInitialized;
        AvailableActors = MoveTemp(Other.AvailableActors);
        ActiveActors = MoveTemp(Other.ActiveActors);

        // 重置源对象
        Other.ActorClass = nullptr;
        Other.bIsInitialized = false;
    }
    return *this;
}

// ✅ 核心池管理功能实现

AActor* FActorPoolSimplified::GetActor(UWorld* World, const FTransform& SpawnTransform)
{
    SCOPE_CYCLE_COUNTER(STAT_ActorPoolSimplified_GetActor);

    if (!bIsInitialized || !IsValid(ActorClass) || !IsValid(World))
    {
        ACTORPOOL_SIMPLIFIED_LOG(Warning, TEXT("GetActor: 池未初始化或参数无效"));
        return nullptr;
    }

    ++TotalRequests;
    AActor* ResultActor = nullptr;

    // ✅ 直接使用写锁避免锁升级死锁问题
    {
        FWriteScopeLock WriteLock(PoolLock);

        // 定期清理无效引用
        if (TotalRequests % CLEANUP_FREQUENCY == 0)
        {
            CleanupInvalidActors();
        }

        // 尝试从可用列表获取Actor
        if (AvailableActors.Num() > 0)
        {
            // 从可用列表获取最后一个有效Actor
            for (int32 i = AvailableActors.Num() - 1; i >= 0; --i)
            {
                if (AvailableActors[i].IsValid())
                {
                    ResultActor = AvailableActors[i].Get();
                    AvailableActors.RemoveAtSwap(i);
                    break;
                }
            }
        }

        // 如果找到了可用Actor，添加到活跃列表
        if (ResultActor)
        {
            // 验证Actor仍然有效
            if (IsValid(ResultActor))
            {
                // 添加到活跃列表
                ActiveActors.Add(ResultActor);

                // 使用工具类激活Actor
                if (FObjectPoolUtils::ActivateActorFromPool(ResultActor, SpawnTransform))
                {
                    UpdateStats(true, false); // 池命中
                    ACTORPOOL_SIMPLIFIED_DEBUG(TEXT("从池获取Actor: %s"), *ResultActor->GetName());
                    return ResultActor;
                }
                else
                {
                    // 激活失败，从活跃列表移除
                    ActiveActors.RemoveSwap(ResultActor);
                    ResultActor = nullptr;
                }
            }
            else
            {
                ResultActor = nullptr;
            }
        }
    }

    // ✅ 池中没有可用Actor，尝试创建新的
    if (!ResultActor && CanCreateMoreActors())
    {
        // 在锁外创建Actor以避免长时间持有锁
        AActor* NewActor = CreateNewActor(World);
        if (NewActor)
        {
            // 重新获取写锁添加到活跃列表
            FWriteScopeLock WriteLock(PoolLock);

            // 添加到活跃列表
            ActiveActors.Add(NewActor);

            // 锁会自动释放

            // 使用工具类激活Actor
            if (FObjectPoolUtils::ActivateActorFromPool(NewActor, SpawnTransform))
            {
                UpdateStats(false, true); // 新创建
                ACTORPOOL_SIMPLIFIED_DEBUG(TEXT("创建新Actor: %s"), *NewActor->GetName());
                return NewActor;
            }
            else
            {
                // 激活失败，需要重新获取锁来清理
                {
                    FWriteScopeLock CleanupLock(PoolLock);
                    ActiveActors.RemoveSwap(NewActor);
                }

                if (IsValid(NewActor))
                {
                    NewActor->Destroy();
                }
            }
        }
    }

    // ✅ 无法获取或创建Actor
    UpdateStats(false, false);
    ACTORPOOL_SIMPLIFIED_LOG(Warning, TEXT("无法获取Actor: %s"), *ActorClass->GetName());
    return nullptr;
}

bool FActorPoolSimplified::ReturnActor(AActor* Actor)
{
    SCOPE_CYCLE_COUNTER(STAT_ActorPoolSimplified_ReturnActor);

    if (!ValidateActor(Actor) || !bIsInitialized)
    {
        ACTORPOOL_SIMPLIFIED_LOG(Warning, TEXT("ReturnActor: Actor无效或池未初始化"));
        return false;
    }

    FWriteScopeLock WriteLock(PoolLock);

    // ✅ 从活跃列表中移除
    bool bFoundInActive = ActiveActors.RemoveSwap(Actor) > 0;
    
    if (!bFoundInActive)
    {
        ACTORPOOL_SIMPLIFIED_DEBUG(TEXT("Actor不在活跃列表中: %s"), *Actor->GetName());
        // 仍然尝试重置和添加到池中
    }

    // ✅ 使用工具类重置Actor状态
    if (!FObjectPoolUtils::ResetActorForPooling(Actor))
    {
        ACTORPOOL_SIMPLIFIED_LOG(Warning, TEXT("重置Actor状态失败: %s"), *Actor->GetName());
        return false;
    }

    // ✅ 检查池是否已满
    if (AvailableActors.Num() >= MaxPoolSize)
    {
        ACTORPOOL_SIMPLIFIED_DEBUG(TEXT("池已满，销毁Actor: %s"), *Actor->GetName());
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
        return true; // 仍然算作成功归还
    }

    // ✅ 添加到可用列表
    AvailableActors.Add(Actor);
    
    ACTORPOOL_SIMPLIFIED_DEBUG(TEXT("Actor归还到池: %s"), *Actor->GetName());
    return true;
}

void FActorPoolSimplified::PrewarmPool(UWorld* World, int32 Count)
{
    if (!bIsInitialized || !IsValid(World) || !IsValid(ActorClass) || Count <= 0)
    {
        return;
    }

    ACTORPOOL_SIMPLIFIED_LOG(Log, TEXT("预热池: %s, 数量=%d"), *ActorClass->GetName(), Count);

    FWriteScopeLock WriteLock(PoolLock);

    int32 ActualCount = FMath::Min(Count, MaxPoolSize - GetPoolSize());

    for (int32 i = 0; i < ActualCount; ++i)
    {
        AActor* NewActor = CreateNewActor(World);
        if (NewActor)
        {
            // 使用工具类重置到池化状态
            FObjectPoolUtils::ResetActorForPooling(NewActor);
            AvailableActors.Add(NewActor);
        }
        else
        {
            ACTORPOOL_SIMPLIFIED_LOG(Warning, TEXT("预热时创建Actor失败: %s"), *ActorClass->GetName());
            break;
        }
    }

    ACTORPOOL_SIMPLIFIED_LOG(Log, TEXT("预热完成: %s, 实际创建=%d"), *ActorClass->GetName(), AvailableActors.Num());
}

// ✅ 状态查询功能实现

FObjectPoolStatsSimplified FActorPoolSimplified::GetStats() const
{
    FReadScopeLock ReadLock(PoolLock);

    FObjectPoolStatsSimplified Stats;
    Stats.TotalCreated = TotalCreated;
    Stats.CurrentActive = ActiveActors.Num();
    Stats.CurrentAvailable = AvailableActors.Num();
    Stats.PoolSize = Stats.CurrentActive + Stats.CurrentAvailable;
    Stats.ActorClassName = ActorClass ? ActorClass->GetName() : TEXT("Unknown");

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

int32 FActorPoolSimplified::GetAvailableCount() const
{
    FReadScopeLock ReadLock(PoolLock);
    return AvailableActors.Num();
}

int32 FActorPoolSimplified::GetActiveCount() const
{
    FReadScopeLock ReadLock(PoolLock);
    return ActiveActors.Num();
}

int32 FActorPoolSimplified::GetPoolSize() const
{
    FReadScopeLock ReadLock(PoolLock);
    return ActiveActors.Num() + AvailableActors.Num();
}

bool FActorPoolSimplified::IsEmpty() const
{
    FReadScopeLock ReadLock(PoolLock);
    return AvailableActors.Num() == 0;
}

bool FActorPoolSimplified::IsFull() const
{
    FReadScopeLock ReadLock(PoolLock);
    return GetPoolSize() >= MaxPoolSize;
}

// ✅ 管理功能实现

void FActorPoolSimplified::ClearPool()
{
    if (!bIsInitialized)
    {
        return;
    }

    FWriteScopeLock WriteLock(PoolLock);

    // 销毁所有可用Actor
    for (const TWeakObjectPtr<AActor>& ActorPtr : AvailableActors)
    {
        if (ActorPtr.IsValid())
        {
            ActorPtr->Destroy();
        }
    }
    AvailableActors.Empty();

    // 销毁所有活跃Actor
    for (const TWeakObjectPtr<AActor>& ActorPtr : ActiveActors)
    {
        if (ActorPtr.IsValid())
        {
            ActorPtr->Destroy();
        }
    }
    ActiveActors.Empty();

    // 重置统计
    TotalRequests = 0;
    PoolHits = 0;
    TotalCreated = 0;

    ACTORPOOL_SIMPLIFIED_LOG(Log, TEXT("清空池: %s"), ActorClass ? *ActorClass->GetName() : TEXT("Unknown"));
}

void FActorPoolSimplified::SetMaxSize(int32 NewMaxSize)
{
    if (NewMaxSize <= 0)
    {
        return;
    }

    FWriteScopeLock WriteLock(PoolLock);

    int32 OldMaxSize = MaxPoolSize;
    MaxPoolSize = NewMaxSize;

    // 如果新大小小于当前池大小，需要移除多余的Actor
    if (NewMaxSize < GetPoolSize())
    {
        int32 ExcessCount = GetPoolSize() - NewMaxSize;

        // 优先移除可用的Actor
        while (ExcessCount > 0 && AvailableActors.Num() > 0)
        {
            TWeakObjectPtr<AActor> ActorPtr = AvailableActors.Pop();
            if (ActorPtr.IsValid())
            {
                ActorPtr->Destroy();
            }
            --ExcessCount;
        }
    }

    ACTORPOOL_SIMPLIFIED_LOG(Log, TEXT("设置池最大大小: %s, %d -> %d"),
        ActorClass ? *ActorClass->GetName() : TEXT("Unknown"), OldMaxSize, NewMaxSize);
}

// ✅ 内部辅助方法实现

AActor* FActorPoolSimplified::CreateNewActor(UWorld* World)
{
    SCOPE_CYCLE_COUNTER(STAT_ActorPoolSimplified_CreateActor);

    if (!IsValid(World) || !IsValid(ActorClass))
    {
        return nullptr;
    }

    // ✅ 创建Actor
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.bDeferConstruction = false;

    AActor* NewActor = World->SpawnActor<AActor>(ActorClass, FTransform::Identity, SpawnParams);

    if (IsValid(NewActor))
    {
        ++TotalCreated;

        // 调用生命周期接口
        FObjectPoolUtils::SafeCallLifecycleInterface(NewActor, TEXT("Created"));

        ACTORPOOL_SIMPLIFIED_DEBUG(TEXT("创建新Actor: %s"), *NewActor->GetName());
        return NewActor;
    }
    else
    {
        ACTORPOOL_SIMPLIFIED_LOG(Warning, TEXT("创建Actor失败: %s"), *ActorClass->GetName());
        return nullptr;
    }
}

bool FActorPoolSimplified::ValidateActor(AActor* Actor) const
{
    if (!IsValid(Actor))
    {
        return false;
    }

    if (!Actor->IsA(ActorClass))
    {
        ACTORPOOL_SIMPLIFIED_LOG(Warning, TEXT("Actor类型不匹配: %s, 期望: %s"),
            *Actor->GetClass()->GetName(), *ActorClass->GetName());
        return false;
    }

    return true;
}

void FActorPoolSimplified::CleanupInvalidActors()
{
    // 注意：调用者必须持有写锁

    // 清理可用列表中的无效引用
    for (int32 i = AvailableActors.Num() - 1; i >= 0; --i)
    {
        if (!AvailableActors[i].IsValid())
        {
            AvailableActors.RemoveAtSwap(i);
        }
    }

    // 清理活跃列表中的无效引用
    for (int32 i = ActiveActors.Num() - 1; i >= 0; --i)
    {
        if (!ActiveActors[i].IsValid())
        {
            ActiveActors.RemoveAtSwap(i);
        }
    }

    ACTORPOOL_SIMPLIFIED_DEBUG(TEXT("清理无效引用完成: %s"), *ActorClass->GetName());
}

void FActorPoolSimplified::UpdateStats(bool bWasPoolHit, bool bActorCreated)
{
    // 注意：这个方法可能在锁内或锁外调用，所以要小心

    if (bWasPoolHit)
    {
        ++PoolHits;
    }

    // TotalCreated 在 CreateNewActor 中更新
    // TotalRequests 在 GetActor 开始时更新
}

bool FActorPoolSimplified::CanCreateMoreActors() const
{
    // 注意：调用者应该持有适当的锁
    return GetPoolSize() < MaxPoolSize;
}

void FActorPoolSimplified::InitializePool(UWorld* World)
{
    if (!bIsInitialized || !IsValid(World) || InitialSize <= 0)
    {
        return;
    }

    // 预热池到初始大小
    PrewarmPool(World, InitialSize);
}


