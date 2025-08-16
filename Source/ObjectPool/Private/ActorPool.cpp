// Fill out your copyright notice in the Description page of Project Settings.

#include "ActorPool.h"
#include "ObjectPool.h"

// ✅ 生命周期接口
#include "ObjectPoolInterface.h"

// ✅ UE核心依赖
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/UObjectGlobals.h"



// ✅ 日志和统计
DEFINE_LOG_CATEGORY(LogActorPool);

#if STATS
DEFINE_STAT(STAT_ActorPool_GetActor);
DEFINE_STAT(STAT_ActorPool_ReturnActor);
DEFINE_STAT(STAT_ActorPool_CreateActor);
#endif

// ✅ 构造函数和析构函数

FActorPool::FActorPool(UClass* InActorClass, int32 InInitialSize, int32 InHardLimit)
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
        ACTORPOOL_LOG(Error, TEXT("FActorPool: 无效的Actor类"));
        return;
    }

    // ✅ UE标准优化：预分配和缓存初始化
    AvailableActors.Reserve(InitialSize);
    ActiveActors.Reserve(InitialSize);
    PreallocationCache.Reserve(InitialSize / 2); // 预分配缓存
    
    // ✅ 缓存Actor类引用，减少查找开销
    CachedActorClass = TSoftObjectPtr<UClass>(ActorClass);
    
    // ✅ 与UE垃圾回收系统协调
    if (GEngine)
    {
        // ✅ 注册垃圾回收回调，确保池与GC协调工作 - 保存句柄以便安全清理
        GCDelegateHandle = FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddLambda([this]()
        {
            FScopeLock Lock(&GCSection);
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
    // ✅ 首先清理GC委托，防止悬挂指针崩溃
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

// ✅ 移动语义实现

FActorPool::FActorPool(FActorPool&& Other) noexcept
    : ActorClass(Other.ActorClass)
    , MaxPoolSize(Other.MaxPoolSize)
    , InitialSize(Other.InitialSize)
    , TotalRequests(Other.TotalRequests)
    , PoolHits(Other.PoolHits)
    , TotalCreated(Other.TotalCreated)
    , bIsInitialized(Other.bIsInitialized)
    , AvailableActors(MoveTemp(Other.AvailableActors))
    , ActiveActors(MoveTemp(Other.ActiveActors))
    , GCDelegateHandle(MoveTemp(Other.GCDelegateHandle)) // ✅ 移动GC委托句柄
{
    // 重置源对象
    Other.ActorClass = nullptr;
    Other.bIsInitialized = false;
    Other.GCDelegateHandle.Reset(); // ✅ 重置源对象的委托句柄
}

FActorPool& FActorPool::operator=(FActorPool&& Other) noexcept
{
    if (this != &Other)
    {
        // ✅ 首先清理当前对象的GC委托，避免泄漏
        if (GCDelegateHandle.IsValid())
        {
            FCoreUObjectDelegates::GetPreGarbageCollectDelegate().Remove(GCDelegateHandle);
            GCDelegateHandle.Reset();
        }
        
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
        GCDelegateHandle = MoveTemp(Other.GCDelegateHandle); // ✅ 移动GC委托句柄

        // 重置源对象
        Other.ActorClass = nullptr;
        Other.bIsInitialized = false;
        Other.GCDelegateHandle.Reset(); // ✅ 重置源对象的委托句柄
    }
    return *this;
}

// ✅ 核心池管理功能实现

AActor* FActorPool::GetActor(UWorld* World, const FTransform& SpawnTransform)
{
    SCOPE_CYCLE_COUNTER(STAT_ActorPool_GetActor);

    if (!bIsInitialized || !IsValid(ActorClass) || !IsValid(World))
    {
        ACTORPOOL_LOG(Warning, TEXT("GetActor: 池未初始化或参数无效"));
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

        // 尝试从可用列表获取Actor（仅取出，不在锁内激活）
        if (AvailableActors.Num() > 0)
        {
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
    }

    // 如果成功取到可复用Actor，锁外激活并在成功后加入活跃列表
    if (ResultActor)
    {
        if (IsValid(ResultActor) && FObjectPoolUtils::ActivateActorFromPool(ResultActor, SpawnTransform))
        {
            // 写锁内加入活跃列表
            {
                FWriteScopeLock WriteLock(PoolLock);
                ActiveActors.Add(ResultActor);
            }
            UpdateStats(true, false);
            ACTORPOOL_DEBUG(TEXT("从池获取Actor: %s"), *ResultActor->GetName());
            return ResultActor;
        }
        else
        {
            ResultActor = nullptr;
        }
    }

    // ✅ 池中没有可用Actor，尝试创建新的（锁外创建与激活）
    if (!ResultActor && CanCreateMoreActors())
    {
        // 在锁外创建Actor以避免长时间持有锁
        AActor* NewActor = CreateNewActor(World);
        if (NewActor)
        {
            // 重新获取写锁添加到活跃列表
            if (FObjectPoolUtils::ActivateActorFromPool(NewActor, SpawnTransform))
            {
                FWriteScopeLock WriteLock(PoolLock);
                ActiveActors.Add(NewActor);
                UpdateStats(false, true); // 新创建
                ACTORPOOL_DEBUG(TEXT("创建新Actor: %s"), *NewActor->GetName());
                return NewActor;
            }
            else
            {
                if (IsValid(NewActor))
                {
                    NewActor->Destroy();
                }
            }
        }
    }

    // ✅ 无法获取或创建Actor
    UpdateStats(false, false);
    ACTORPOOL_LOG(Warning, TEXT("无法获取Actor: %s"), *ActorClass->GetName());
    return nullptr;
}

AActor* FActorPool::AcquireDeferred(UWorld* World)
{
    if (!bIsInitialized || !IsValid(ActorClass) || !IsValid(World))
    {
        ACTORPOOL_LOG(Warning, TEXT("AcquireDeferred: 池未初始化或参数无效"));
        return nullptr;
    }

    ++TotalRequests;

    AActor* ResultActor = nullptr;

    {
        FWriteScopeLock WriteLock(PoolLock);

        // 定期清理无效引用
        if (TotalRequests % CLEANUP_FREQUENCY == 0)
        {
            CleanupInvalidActors();
        }

        // 优先从可用列表取出一个实例（不激活）
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

    if (ResultActor)
    {
        // 仅取出，不激活；交由 FinalizeDeferred 处理
        UpdateStats(true, false);
        return ResultActor;
    }

    // 可用为空，尝试新建延迟构造Actor
    if (CanCreateMoreActors())
    {
        AActor* NewActor = CreateNewActor(World);
        if (NewActor)
        {
            UpdateStats(false, true);
            return NewActor; // 延迟构造状态，等待Finalize
        }
    }

    UpdateStats(false, false);
    ACTORPOOL_LOG(Warning, TEXT("AcquireDeferred: 无可用Actor且创建失败: %s"), *ActorClass->GetName());
    return nullptr;
}

bool FActorPool::FinalizeDeferred(AActor* Actor, const FTransform& SpawnTransform)
{
    if (!ValidateActor(Actor) || !bIsInitialized)
    {
        ACTORPOOL_LOG(Warning, TEXT("FinalizeDeferred: Actor无效或池未初始化"));
        return false;
    }

    // 如果是延迟构造的Actor，需要完成构造
    if (!Actor->IsActorInitialized())
    {
        ACTORPOOL_LOG(VeryVerbose, TEXT("FinalizeDeferred: FinishSpawning: %s"), *Actor->GetName());
        Actor->FinishSpawning(SpawnTransform);
        // 首次创建完成后，触发生命周期“创建”事件
        if (IObjectPoolInterface::DoesActorImplementInterface(Actor))
        {
            IObjectPoolInterface::Execute_OnPoolActorCreated(Actor);
        }
    }
    // 对于复用实例（已初始化），在激活前先重跑Construction Script，确保其读取的是本次ExposeOnSpawn赋的值
    if (Actor->IsActorInitialized())
    {
        ACTORPOOL_LOG(VeryVerbose, TEXT("FinalizeDeferred: 复用实例激活前重跑ConstructionScripts: %s"), *Actor->GetName());
#if WITH_EDITOR
        Actor->RerunConstructionScripts();
#endif
    }

    // 统一激活（内部会根据是否已初始化决定是否FinishSpawning/应用Transform/启用组件）
    FObjectPoolUtils::ActivateActorFromPool(Actor, SpawnTransform);

    // 加入活跃列表
    {
        FWriteScopeLock WriteLock(PoolLock);
        ActiveActors.Add(Actor);
    }

    return true;
}

bool FActorPool::ReturnActor(AActor* Actor)
{
    SCOPE_CYCLE_COUNTER(STAT_ActorPool_ReturnActor);

    if (!ValidateActor(Actor) || !bIsInitialized)
    {
        ACTORPOOL_LOG(Warning, TEXT("ReturnActor: Actor无效或池未初始化"));
        return false;
    }

    FWriteScopeLock WriteLock(PoolLock);

    // ✅ 从活跃列表中移除
    bool bFoundInActive = ActiveActors.RemoveSwap(Actor) > 0;

    if (!bFoundInActive)
    {
        ACTORPOOL_DEBUG(TEXT("Actor不在活跃列表中: %s"), *Actor->GetName());
        // 仍然尝试重置和添加到池中
    }

    // ✅ 使用工具类重置Actor状态
    if (!FObjectPoolUtils::ResetActorForPooling(Actor))
    {
        ACTORPOOL_LOG(Warning, TEXT("重置Actor状态失败: %s"), *Actor->GetName());
        return false;
    }

    // ✅ 检查池是否已满
    if (AvailableActors.Num() >= MaxPoolSize)
    {
        ACTORPOOL_DEBUG(TEXT("池已满，销毁Actor: %s"), *Actor->GetName());
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
        return true; // 仍然算作成功归还
    }

    // ✅ 添加到可用列表
    AvailableActors.Add(Actor);

    ACTORPOOL_DEBUG(TEXT("Actor归还到池: %s"), *Actor->GetName());
    return true;
}

void FActorPool::PrewarmPool(UWorld* World, int32 Count)
{
    if (!bIsInitialized || !IsValid(World) || !IsValid(ActorClass) || Count <= 0)
    {
        return;
    }

    ACTORPOOL_LOG(Log, TEXT("预热池: %s, 数量=%d"), *ActorClass->GetName(), Count);

    FWriteScopeLock WriteLock(PoolLock);

    // ✅ 直接计算池大小，避免调用GetPoolSize()导致的嵌套锁死锁
    int32 CurrentPoolSize = ActiveActors.Num() + AvailableActors.Num();
    int32 ActualCount = FMath::Min(Count, MaxPoolSize - CurrentPoolSize);

    if (ActualCount <= 0)
    {
        return;
    }

    for (int32 i = 0; i < ActualCount; ++i)
    {
        AActor* NewActor = CreateNewActor(World);
        
        if (NewActor)
        {
            // ✅ 预热阶段只需要基本的状态重置，不调用生命周期事件
            // 避免在预热时触发OnReturnToPool导致死锁
            
            // 隐藏Actor并禁用Tick
            NewActor->SetActorHiddenInGame(true);
            NewActor->SetActorTickEnabled(false);
            
            // 禁用碰撞
            if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(NewActor->GetRootComponent()))
            {
                RootPrimitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                RootPrimitive->SetSimulatePhysics(false);
            }
            
            // 移动到池外位置
            NewActor->SetActorLocation(FVector(0.0f, 0.0f, -100000.0f), false, nullptr, ETeleportType::ResetPhysics);
            
            AvailableActors.Add(NewActor);
        }
        else
        {
            ACTORPOOL_LOG(Warning, TEXT("预热时创建Actor失败: %s"), *ActorClass->GetName());
            break;
        }
    }

    ACTORPOOL_LOG(Log, TEXT("预热完成: %s, 实际创建=%d"), *ActorClass->GetName(), AvailableActors.Num());
}

// ✅ 状态查询功能实现

FObjectPoolStats FActorPool::GetStats() const
{
    FReadScopeLock ReadLock(PoolLock);

    FObjectPoolStats Stats;
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
    return ActiveActors.Num() + AvailableActors.Num();
}

bool FActorPool::IsEmpty() const
{
    FReadScopeLock ReadLock(PoolLock);
    return AvailableActors.Num() == 0;
}

bool FActorPool::IsFull() const
{
    FReadScopeLock ReadLock(PoolLock);
    // ✅ 直接计算池大小，避免嵌套锁调用
    int32 CurrentPoolSize = ActiveActors.Num() + AvailableActors.Num();
    return CurrentPoolSize >= MaxPoolSize;
}

bool FActorPool::ContainsActor(const AActor* Actor) const
{
    if (!IsValid(Actor))
    {
        return false;
    }

    FReadScopeLock ReadLock(PoolLock);

    for (const TWeakObjectPtr<AActor>& Ptr : ActiveActors)
    {
        if (Ptr.Get() == Actor)
        {
            return true;
        }
    }

    for (const TWeakObjectPtr<AActor>& Ptr : AvailableActors)
    {
        if (Ptr.Get() == Actor)
        {
            return true;
        }
    }

    return false;
}

// ✅ 管理功能实现

void FActorPool::ClearPool()
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

    ACTORPOOL_LOG(Log, TEXT("清空池: %s"), ActorClass ? *ActorClass->GetName() : TEXT("Unknown"));
}

void FActorPool::SetMaxSize(int32 NewMaxSize)
{
    if (NewMaxSize <= 0)
    {
        return;
    }

    FWriteScopeLock WriteLock(PoolLock);

    int32 OldMaxSize = MaxPoolSize;
    MaxPoolSize = NewMaxSize;

    // ✅ 直接计算池大小，避免嵌套锁调用
    int32 CurrentPoolSize = ActiveActors.Num() + AvailableActors.Num();
    
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
                ActorPtr->Destroy();
            }
            --ExcessCount;
        }
    }

    ACTORPOOL_LOG(Log, TEXT("设置池最大大小: %s, %d -> %d"),
        ActorClass ? *ActorClass->GetName() : TEXT("Unknown"), OldMaxSize, NewMaxSize);
}

// ✅ 内部辅助方法实现

AActor* FActorPool::CreateNewActor(UWorld* World)
{
    SCOPE_CYCLE_COUNTER(STAT_ActorPool_CreateActor);

    if (!IsValid(World) || !IsValid(ActorClass))
    {
        return nullptr;
    }

    // ✅ 网络最佳实践：正确的预热Actor创建
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.bDeferConstruction = true; // 延迟构造，避免BeginPlay中的问题
    
    ACTORPOOL_LOG(VeryVerbose, TEXT("创建Actor用于对象池: %s"), *ActorClass->GetName());

    AActor* NewActor = World->SpawnActor<AActor>(ActorClass, FTransform::Identity, SpawnParams);

    if (IsValid(NewActor))
    {
        // ✅ 最安全策略：预热时完全不调用FinishSpawning，避免任何BeginPlay相关问题
        // Actor保持延迟构造状态，直到从池中获取时才完成初始化
        
        ACTORPOOL_LOG(VeryVerbose, TEXT("Actor预热创建成功（延迟构造状态）: %s"), *NewActor->GetName());
        
        // 不调用FinishSpawning，不调用DisableAutoActivateComponents
        // 保持最原始的延迟构造状态，最大程度避免死锁
        
        ++TotalCreated;

        // ✅ OnPoolActorCreated事件已移动到ActivateActorFromPool中的FinishSpawning之后调用
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

void FActorPool::Clear()
{
    // 获取写锁
    FWriteScopeLock WriteLock(PoolLock);
    
    // 销毁所有活跃的Actor
    for (const TWeakObjectPtr<AActor>& ActorPtr : ActiveActors)
    {
        if (AActor* Actor = ActorPtr.Get())
        {
            Actor->Destroy();
        }
    }
    
    // 销毁所有可用的Actor
    for (const TWeakObjectPtr<AActor>& ActorPtr : AvailableActors)
    {
        if (AActor* Actor = ActorPtr.Get())
        {
            Actor->Destroy();
        }
    }
    
    // 清空容器
    ActiveActors.Empty();
    AvailableActors.Empty();
    
    // 重置统计 - 注意：需要通过GetStats()方法或其他方式重置
    // 暂时移除直接赋值，避免编译错误
    // TODO: 实现正确的统计重置逻辑
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
    
    // 估算每个Actor的内存使用（简化计算）
    int32 TotalActors = ActiveActors.Num() + AvailableActors.Num();
    MemoryUsage += TotalActors * 1024; // 假设每个Actor约1KB
    
    return MemoryUsage;
}

void FActorPool::CleanupInvalidActors()
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

    ACTORPOOL_DEBUG(TEXT("清理无效引用完成: %s"), *ActorClass->GetName());
}

void FActorPool::UpdateStats(bool bWasPoolHit, bool bActorCreated)
{
    // 注意：这个方法可能在锁内或锁外调用，所以要小心

    if (bWasPoolHit)
    {
        ++PoolHits;
    }

    // TotalCreated 在 CreateNewActor 中更新
    // TotalRequests 在 GetActor 开始时更新
}

bool FActorPool::CanCreateMoreActors() const
{
    FReadScopeLock ReadLock(PoolLock);
    // ✅ 直接计算池大小，避免嵌套锁调用
    int32 CurrentPoolSize = ActiveActors.Num() + AvailableActors.Num();
    return CurrentPoolSize < MaxPoolSize;
}

void FActorPool::InitializePool(UWorld* World)
{
    if (!bIsInitialized || !IsValid(World) || InitialSize <= 0)
    {
        return;
    }

    // ✅ 重新启用安全预热机制 - 预热池到初始大小
    if (InitialSize > 0)
    {
        PrewarmPool(World, InitialSize);
        ACTORPOOL_LOG(Log, TEXT("InitializePool预热完成: %s, 请求数量=%d"), 
            *ActorClass->GetName(), InitialSize);
    }
}

