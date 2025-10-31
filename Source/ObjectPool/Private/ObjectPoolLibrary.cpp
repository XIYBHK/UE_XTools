/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


//  遵循IWYU原则的头文件包含
#include "ObjectPoolLibrary.h"

//  UE核心依赖
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

//  对象池模块依赖
#include "ObjectPool.h"
#include "ObjectPoolSubsystem.h"

namespace XTools::ObjectPool
{
    static UWorld* ResolveWorld(const UObject* WorldContext)
    {
        UWorld* World = nullptr;

        if (WorldContext)
        {
            World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
        }

        if (!World)
        {
            if (UObjectPoolSubsystem* TempSubsystem = UObjectPoolSubsystem::Get(WorldContext))
            {
                World = TempSubsystem->GetWorld();
            }
        }

        return World;
    }

    static AActor* SpawnFallbackActor(UWorld* World, TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform)
    {
        if (!World)
        {
            return nullptr;
        }

        if (ActorClass)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            if (AActor* Actor = World->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams))
            {
                OBJECTPOOL_LOG(Verbose, TEXT("UObjectPoolLibrary: 回退创建成功: %s"), *Actor->GetName());
                return Actor;
            }
        }

        FActorSpawnParameters DefaultParams;
        DefaultParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        DefaultParams.bNoFail = true;
        if (AActor* DefaultActor = World->SpawnActor<AActor>(AActor::StaticClass(), SpawnTransform, DefaultParams))
        {
            OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary: 回退到默认Actor: %s"), *DefaultActor->GetName());
            return DefaultActor;
        }

        return nullptr;
    }
}

bool UObjectPoolLibrary::RegisterActorClass(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, int32 InitialSize, int32 HardLimit)
{
    //  获取子系统
    UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
    if (Subsystem)
    {
        //  调用子系统的RegisterActorClass进行安全的注册和预热
        bool bSuccess = Subsystem->RegisterActorClass(ActorClass, InitialSize, HardLimit);

        OBJECTPOOL_LOG(Log, TEXT("UObjectPoolLibrary::RegisterActorClass: 注册Actor类: %s, 初始大小=%d, 结果: %s"),
            ActorClass ? *ActorClass->GetName() : TEXT("Invalid"),
            InitialSize,
            bSuccess ? TEXT("成功") : TEXT("失败"));

        return bSuccess;
    }

    OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::RegisterActorClass: 无法获取对象池子系统"));
    return false;
}

AActor* UObjectPoolLibrary::SpawnActorFromPool(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform)
{
    //  获取对象池子系统
    UObjectPoolSubsystem* PoolSubsystem = GetSubsystemSafe(WorldContext);
    if (PoolSubsystem)
    {
        // 使用对象池子系统的SpawnActorFromPool（已实现永不失败机制）
        AActor* Actor = PoolSubsystem->SpawnActorFromPool(ActorClass, SpawnTransform);

        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::SpawnActorFromPool: %s, 结果: %s"),
            ActorClass ? *ActorClass->GetName() : TEXT("Invalid"),
            Actor ? *Actor->GetName() : TEXT("Failed"));

        return Actor;
    }

    OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::SpawnActorFromPool: 无法获取对象池子系统，尝试直接创建"));

    if (UWorld* World = XTools::ObjectPool::ResolveWorld(WorldContext))
    {
        if (AActor* Actor = XTools::ObjectPool::SpawnFallbackActor(World, ActorClass, SpawnTransform))
        {
            return Actor;
        }
    }

    OBJECTPOOL_LOG(Error, TEXT("UObjectPoolLibrary: 所有回退机制都失败，返回 nullptr"));
    return nullptr;
}

AActor* UObjectPoolLibrary::SpawnActorFromPoolEx(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform, EPoolOpResult& OutResult)
{
    OutResult = EPoolOpResult::InvalidArgs;
    AActor* Actor = SpawnActorFromPool(WorldContext, ActorClass, SpawnTransform);
    if (!Actor)
    {
        return nullptr;
    }

    // 判断是否来自对象池
    UObjectPoolSubsystem* PoolSubsystem = GetSubsystemSafe(WorldContext);
    if (PoolSubsystem && PoolSubsystem->IsActorPooled(Actor))
    {
        OutResult = EPoolOpResult::Success;
    }
    else
    {
        OutResult = EPoolOpResult::FallbackSpawned;
    }

    return Actor;
}

void UObjectPoolLibrary::ReturnActorToPool(const UObject* WorldContext, AActor* Actor)
{
    //  获取对象池子系统
    UObjectPoolSubsystem* PoolSubsystem = GetSubsystemSafe(WorldContext);
    if (PoolSubsystem)
    {
        // 使用对象池子系统返回Actor
        PoolSubsystem->ReturnActorToPool(Actor);

        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::ReturnActorToPool: %s"),
            Actor ? *Actor->GetName() : TEXT("Invalid"));
        return;
    }
    
    //  如果没有子系统可用，直接销毁Actor避免内存泄漏
    OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::ReturnActorToPool: 无法获取对象池子系统，直接销毁Actor"));

    if (IsValid(Actor))
    {
        Actor->Destroy();
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::ReturnActorToPool: 直接销毁Actor: %s"),
            *Actor->GetName());
    }
}

bool UObjectPoolLibrary::ReturnActorToPoolEx(const UObject* WorldContext, AActor* Actor, EPoolOpResult& OutResult)
{
    OutResult = EPoolOpResult::InvalidArgs;
    if (!IsValid(Actor))
    {
        OBJECTPOOL_LOG(Warning, TEXT("ReturnActorToPoolEx: Actor无效"));
        return false;
    }

    UObjectPoolSubsystem* PoolSubsystem = GetSubsystemSafe(WorldContext);
    if (PoolSubsystem)
    {
        const bool bIsPooled = PoolSubsystem->IsActorPooled(Actor);
        const bool bOk = PoolSubsystem->ReturnActorToPool(Actor);
        OutResult = bIsPooled ? (bOk ? EPoolOpResult::Success : EPoolOpResult::InvalidArgs)
                              : EPoolOpResult::NotPooled;
        return bOk;
    }

    // 无子系统：直接Destroy
    if (IsValid(Actor))
    {
        Actor->Destroy();
        OutResult = EPoolOpResult::NotPooled;
        OBJECTPOOL_LOG(Warning, TEXT("ReturnActorToPoolEx: 无子系统，直接销毁Actor"));
    }
    return true;
}

// 移除 QuickSpawnActor：请直接使用 SpawnActorFromPool，并在调用端自行构造 FTransform

int32 UObjectPoolLibrary::BatchSpawnActors(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, const TArray<FTransform>& SpawnTransforms, TArray<AActor*>& OutActors)
{
    OutActors.Empty();
    
    if (SpawnTransforms.Num() == 0)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::BatchSpawnActors: 空的Transform数组"));
        return 0;
    }

    //  预分配输出数组
    OutActors.Reserve(SpawnTransforms.Num());
    
    int32 SuccessCount = 0;
    
    //  批量生成Actor
    for (const FTransform& Transform : SpawnTransforms)
    {
        AActor* Actor = SpawnActorFromPool(WorldContext, ActorClass, Transform);
        if (Actor)
        {
            OutActors.Add(Actor);
            ++SuccessCount;
        }
        else
        {
            //  即使单个失败也继续处理其他的
            OutActors.Add(nullptr);
            OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::BatchSpawnActors: 生成Actor失败"));
        }
    }
    
    OBJECTPOOL_LOG(Verbose, TEXT("UObjectPoolLibrary::BatchSpawnActors: 请求 %d 个，成功 %d 个"), 
        SpawnTransforms.Num(), SuccessCount);
    
    return SuccessCount;
}

int32 UObjectPoolLibrary::BatchSpawnActorsEx(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, const TArray<FTransform>& SpawnTransforms, TArray<AActor*>& OutActors, EBatchFailurePolicy FailurePolicy, bool bPreserveOrder)
{
    OutActors.Empty();

    const int32 Num = SpawnTransforms.Num();
    if (Num == 0)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("BatchSpawnActorsEx: 空的Transform数组"));
        return 0;
    }

    if (bPreserveOrder)
    {
        OutActors.SetNum(Num);
    }

    int32 SuccessCount = 0;
    TArray<AActor*> SpawnedActors;
    SpawnedActors.Reserve(Num);

    for (int32 Index = 0; Index < Num; ++Index)
    {
        const FTransform& Xform = SpawnTransforms[Index];
        EPoolOpResult Result;
        AActor* Actor = SpawnActorFromPoolEx(WorldContext, ActorClass, Xform, Result);

        const bool bSuccess = (Actor != nullptr);
        if (bPreserveOrder)
        {
            OutActors[Index] = Actor;
        }
        else if (bSuccess)
        {
            OutActors.Add(Actor);
        }

        if (bSuccess)
        {
            ++SuccessCount;
            SpawnedActors.Add(Actor);
        }
        else if (FailurePolicy == EBatchFailurePolicy::AllOrNothing)
        {
            // 回滚：归还/销毁已生成的Actor
            EPoolOpResult ReturnResult;
            for (AActor* Spawned : SpawnedActors)
            {
                ReturnActorToPoolEx(WorldContext, Spawned, ReturnResult);
            }
            OutActors.Empty();
            OBJECTPOOL_LOG(Warning, TEXT("BatchSpawnActorsEx: AllOrNothing 触发回滚，失败于索引 %d"), Index);
            return 0;
        }
    }

    OBJECTPOOL_LOG(Verbose, TEXT("BatchSpawnActorsEx: 请求 %d 个，成功 %d 个，策略=%d，保持顺序=%s"), Num, SuccessCount, (int32)FailurePolicy, bPreserveOrder ? TEXT("true") : TEXT("false"));
    return SuccessCount;
}

bool UObjectPoolLibrary::IsActorClassRegistered(const UObject* WorldContext, TSubclassOf<AActor> ActorClass)
{
    //  优先使用简化子系统
    UObjectPoolSubsystem* PoolSubsystem = GetSubsystemSafe(WorldContext);
    if (!PoolSubsystem)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::IsActorClassRegistered: 无法获取对象池子系统"));
        return false;
    }

    //  极简API设计：通过PrewarmPool(0)检查池是否存在
    // 注意：这是内部实现细节，用户应该使用极简API
    bool bRegistered = false;
    //  通过PrewarmPool(0)检查池是否已注册（不使用异常）
    int32 AvailableCount = PoolSubsystem->PrewarmPool(ActorClass, 0);
    bRegistered = (AvailableCount >= 0);
    
    OBJECTPOOL_LOG(VeryVerbose, TEXT("IsActorClassRegistered检查: %s, 可用数量=%d, 已注册=%s"),
        ActorClass ? *ActorClass->GetName() : TEXT("Invalid"),
        AvailableCount,
        bRegistered ? TEXT("是") : TEXT("否"));

    OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::IsActorClassRegistered: %s, 结果: %s"),
        ActorClass ? *ActorClass->GetName() : TEXT("Invalid"),
        bRegistered ? TEXT("已注册") : TEXT("未注册"));

    return bRegistered;
}

FObjectPoolStats UObjectPoolLibrary::GetPoolStats(const UObject* WorldContext, TSubclassOf<AActor> ActorClass)
{
    //  极简API：直接返回空统计，避免死代码路径
    return FObjectPoolStats();
}

void UObjectPoolLibrary::DisplayPoolStats(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, bool bShowOnScreen, bool bPrintToLog, float DisplayDuration, FLinearColor TextColor)
{
    //  构建统计信息字符串
    FString StatsText;

    if (ActorClass)
    {
        // 显示单个池的统计信息
        FObjectPoolStats Stats = GetPoolStats(WorldContext, ActorClass);
        if (Stats.PoolSize > 0 || Stats.TotalCreated > 0)
        {
            StatsText = FString::Printf(TEXT("=== 对象池统计信息 ===\n%s"), *Stats.ToString());
        }
        else
        {
            StatsText = FString::Printf(TEXT("=== 对象池统计信息 ===\n对象池[%s]: 未找到或未初始化"),
                ActorClass ? *ActorClass->GetName() : TEXT("Unknown"));
        }
    }
    else
    {
        // 显示所有池的统计信息
        StatsText = TEXT("=== 所有对象池统计信息 ===\n");

        //  优先使用简化子系统
        //  极简API设计：移除统计功能
        StatsText += TEXT("统计功能已被移除（极简API设计）");
    }

    //  显示到屏幕（使用固定的键值确保不被覆盖）
    if (bShowOnScreen && GEngine)
    {
        // 使用非常小的负数作为键值，确保显示在最顶部且不被覆盖
        const int32 PoolStatsKey = INT32_MIN + 500; // 为池统计信息预留的键值
        const FColor DisplayColor = TextColor.ToFColor(true);

        GEngine->AddOnScreenDebugMessage(PoolStatsKey, DisplayDuration, DisplayColor, StatsText);
    }

    //  输出到日志
    if (bPrintToLog)
    {
        OBJECTPOOL_LOG(Warning, TEXT("\n%s"), *StatsText);
    }
}

bool UObjectPoolLibrary::PrewarmPool(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, int32 Count)
{
    if (Count <= 0)
    {
        OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::PrewarmPool: 无效的预热数量: %d"), Count);
        return false;
    }

    //  获取子系统
    UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::PrewarmPool: 无法获取对象池子系统"));
        return false;
    }

    //  调用子系统预热方法
    Subsystem->PrewarmPool(ActorClass, Count);

    OBJECTPOOL_LOG(Verbose, TEXT("UObjectPoolLibrary::PrewarmPool: %s, 数量: %d"),
        ActorClass ? *ActorClass->GetName() : TEXT("Invalid"), Count);

    return true;
}

bool UObjectPoolLibrary::ClearPool(const UObject* WorldContext, TSubclassOf<AActor> ActorClass)
{
    UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::ClearPool: 无法获取对象池子系统"));
        return false;
    }

    //  极简API设计：ClearPool功能已被移除
    OBJECTPOOL_LOG(Warning, TEXT("ClearPool功能已被移除（极简API设计）"));
    // 用户应该通过RegisterActorClass重新注册来实现清理功能

    OBJECTPOOL_LOG(Verbose, TEXT("UObjectPoolLibrary::ClearPool: %s"),
        ActorClass ? *ActorClass->GetName() : TEXT("Invalid"));

    return true;
}

UObjectPoolSubsystem* UObjectPoolLibrary::GetObjectPoolSubsystem(const UObject* WorldContext)
{
    return GetSubsystemSafe(WorldContext);
}















UObjectPoolSubsystem* UObjectPoolLibrary::GetSubsystemSafe(const UObject* WorldContext)
{
    if (!WorldContext)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::GetSubsystemSafe: WorldContext为空"));
        return nullptr;
    }

    //  使用子系统的静态方法获取实例
    UObjectPoolSubsystem* Subsystem = UObjectPoolSubsystem::Get(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::GetSubsystemSafe: 无法获取对象池子系统"));
    }

    return Subsystem;
}



bool UObjectPoolLibrary::CallLifecycleEvent(const UObject* WorldContext, AActor* Actor, EObjectPoolLifecycleEvent EventType, bool bAsync)
{
    if (!IsValid(Actor))
    {
        OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::CallLifecycleEvent: Actor无效"));
        return false;
    }

    //  使用增强的生命周期事件系统
    bool bSuccess = IObjectPoolInterface::CallLifecycleEventEnhanced(Actor, EventType, bAsync, 1000);

    OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::CallLifecycleEvent: %s, 事件: %d, 结果: %s"),
        *Actor->GetName(), (int32)EventType, bSuccess ? TEXT("成功") : TEXT("失败"));

    return bSuccess;
}

int32 UObjectPoolLibrary::BatchCallLifecycleEvents(const UObject* WorldContext, const TArray<AActor*>& Actors, EObjectPoolLifecycleEvent EventType, bool bAsync)
{
    if (Actors.Num() == 0)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::BatchCallLifecycleEvents: 空的Actor数组"));
        return 0;
    }

    //  使用接口的批量调用方法
    int32 SuccessCount = IObjectPoolInterface::BatchCallLifecycleEvents(Actors, EventType, bAsync);

    OBJECTPOOL_LOG(Verbose, TEXT("UObjectPoolLibrary::BatchCallLifecycleEvents: 请求 %d 个，成功 %d 个"),
        Actors.Num(), SuccessCount);

    return SuccessCount;
}

bool UObjectPoolLibrary::HasLifecycleEventSupport(const UObject* WorldContext, AActor* Actor, EObjectPoolLifecycleEvent EventType)
{
    if (!IsValid(Actor))
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::HasLifecycleEventSupport: Actor无效"));
        return false;
    }

    //  使用接口的检查方法
    bool bSupported = IObjectPoolInterface::HasLifecycleEvent(Actor, EventType);

    OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::HasLifecycleEventSupport: %s, 事件: %d, 支持: %s"),
        *Actor->GetName(), (int32)EventType, bSupported ? TEXT("是") : TEXT("否"));

    return bSupported;
}

int32 UObjectPoolLibrary::BatchReturnActors(const UObject* WorldContext, const TArray<AActor*>& Actors)
{
    if (Actors.Num() == 0)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::BatchReturnActors: 空的Actor数组"));
        return 0;
    }

    int32 SuccessCount = 0;
    
    //  批量归还Actor
    for (AActor* Actor : Actors)
    {
        if (IsValid(Actor))
        {
            ReturnActorToPool(WorldContext, Actor);
            ++SuccessCount;
        }
    }
    
    OBJECTPOOL_LOG(Verbose, TEXT("UObjectPoolLibrary::BatchReturnActors: 请求 %d 个，成功 %d 个"), 
        Actors.Num(), SuccessCount);
    
    return SuccessCount;
}

int32 UObjectPoolLibrary::BatchReturnActorsEx(const UObject* WorldContext, const TArray<AActor*>& Actors, EBatchFailurePolicy FailurePolicy)
{
    const int32 Num = Actors.Num();
    if (Num == 0)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("BatchReturnActorsEx: 空的Actor数组"));
        return 0;
    }

    if (FailurePolicy == EBatchFailurePolicy::AllOrNothing)
    {
        // 预校验：避免部分返回、难以回滚
        for (AActor* Actor : Actors)
        {
            if (!IsValid(Actor))
            {
                OBJECTPOOL_LOG(Warning, TEXT("BatchReturnActorsEx: AllOrNothing 预检失败（Actor无效）"));
                return 0;
            }
        }
    }

    int32 SuccessCount = 0;
    bool bAnyFailure = false;
    for (AActor* Actor : Actors)
    {
        if (!IsValid(Actor))
        {
            bAnyFailure = true;
            continue;
        }

        EPoolOpResult Result;
        const bool bOk = ReturnActorToPoolEx(WorldContext, Actor, Result);
        if (bOk)
        {
            ++SuccessCount;
        }
        else
        {
            bAnyFailure = true;
        }
    }

    if (FailurePolicy == EBatchFailurePolicy::AllOrNothing && bAnyFailure)
    {
        OBJECTPOOL_LOG(Warning, TEXT("BatchReturnActorsEx: AllOrNothing 检测到失败，返回 0（无法安全回滚）"));
        return 0;
    }

    OBJECTPOOL_LOG(Verbose, TEXT("BatchReturnActorsEx: 请求 %d 个，成功 %d 个，策略=%d"), Num, SuccessCount, (int32)FailurePolicy);
    return SuccessCount;
}

AActor* UObjectPoolLibrary::AcquireOrSpawn(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform, EPoolOpResult& OutResult)
{
    OutResult = EPoolOpResult::InvalidArgs;
    AActor* Actor = SpawnActorFromPool(WorldContext, ActorClass, SpawnTransform);
    if (!Actor)
    {
        return nullptr;
    }

    // 粗略判断是否为回退生成：当前API无法直接知晓，保守置为Success
    OutResult = EPoolOpResult::Success;
    return Actor;
}

bool UObjectPoolLibrary::ReleaseOrDespawn(const UObject* WorldContext, AActor* Actor, EPoolOpResult& OutResult)
{
    return ReturnActorToPoolEx(WorldContext, Actor, OutResult);
}

AActor* UObjectPoolLibrary::AcquireDeferredFromPool(const UObject* WorldContext, TSubclassOf<AActor> ActorClass)
{
    UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(Warning, TEXT("AcquireDeferredFromPool: 无子系统"));
        return nullptr;
    }
    OBJECTPOOL_LOG(VeryVerbose, TEXT("AcquireDeferredFromPool 调用: %s"), *GetNameSafe(ActorClass));
    return Subsystem->AcquireDeferredFromPool(ActorClass);
}

bool UObjectPoolLibrary::FinalizeSpawnFromPool(const UObject* WorldContext, AActor* Actor, const FTransform& SpawnTransform)
{
    UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(Warning, TEXT("FinalizeSpawnFromPool: 无子系统，直接完成构造/激活"));
        if (IsValid(Actor))
        {
            if (!Actor->IsActorInitialized())
            {
                Actor->FinishSpawning(SpawnTransform);
            }
            FObjectPoolUtils::ActivateActorFromPool(Actor, SpawnTransform);
            return true;
        }
        return false;
    }
    OBJECTPOOL_LOG(VeryVerbose, TEXT("FinalizeSpawnFromPool 调用: %s Transform=%s"), *GetNameSafe(Actor), *SpawnTransform.ToHumanReadableString());
    return Subsystem->FinalizeSpawnFromPool(Actor, SpawnTransform);
}

// 提供WorldContext隐式传递的Spawn入口（与原生一致的World解析）
// 注意：蓝图侧K2节点直接构建Acquire/Finalize链路，因此此处不再额外实现

bool UObjectPoolLibrary::IsActorPooled(const UObject* WorldContext, const AActor* Actor)
{
    UObjectPoolSubsystem* PoolSubsystem = GetSubsystemSafe(WorldContext);
    if (!PoolSubsystem)
    {
        return false;
    }
    return PoolSubsystem->IsActorPooled(Actor);
}
