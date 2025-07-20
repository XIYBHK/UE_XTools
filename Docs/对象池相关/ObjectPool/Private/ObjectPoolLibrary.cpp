// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 遵循IWYU原则的头文件包含
#include "ObjectPoolLibrary.h"

// ✅ UE核心依赖
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

// ✅ 对象池模块依赖
#include "ObjectPool.h"
#include "ObjectPoolSubsystem.h"

bool UObjectPoolLibrary::RegisterActorClass(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, int32 InitialSize, int32 HardLimit)
{
    UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::RegisterActorClass: 无法获取对象池子系统"));
        return false;
    }

    // ✅ 调用子系统方法（子系统内部已有完整的错误处理）
    Subsystem->RegisterActorClass(ActorClass, InitialSize, HardLimit);
    
    // ✅ 验证注册是否成功
    bool bSuccess = Subsystem->IsActorClassRegistered(ActorClass);
    
    OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::RegisterActorClass: %s, 结果: %s"), 
        ActorClass ? *ActorClass->GetName() : TEXT("Invalid"), 
        bSuccess ? TEXT("成功") : TEXT("失败"));
    
    return bSuccess;
}

AActor* UObjectPoolLibrary::SpawnActorFromPool(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform)
{
    UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::SpawnActorFromPool: 无法获取对象池子系统，尝试直接创建"));
        
        // ✅ 多级回退机制：即使子系统不可用也要尝试创建Actor
        UWorld* World = nullptr;

        // 首先尝试从WorldContext获取
        if (WorldContext)
        {
            World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
        }

        // 如果失败，尝试使用子系统的智能获取方法
        if (!World)
        {
            if (UObjectPoolSubsystem* TempSubsystem = UObjectPoolSubsystem::Get(WorldContext))
            {
                World = TempSubsystem->GetValidWorld();
            }
        }

        if (World)
        {
            // 第一级回退：尝试创建指定类型
            if (ActorClass)
            {
                FActorSpawnParameters SpawnParams;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                AActor* Actor = World->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);
                if (Actor)
                {
                    OBJECTPOOL_LOG(Verbose, TEXT("UObjectPoolLibrary: 回退创建成功: %s"), *Actor->GetName());
                    return Actor;
                }
            }

            // 第二级回退：创建默认Actor
            FActorSpawnParameters DefaultParams;
            DefaultParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            DefaultParams.bNoFail = true;
            AActor* DefaultActor = World->SpawnActor<AActor>(AActor::StaticClass(), SpawnTransform, DefaultParams);
            if (DefaultActor)
            {
                OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary: 回退到默认Actor: %s"), *DefaultActor->GetName());
                return DefaultActor;
            }
        }

        // ✅ 最后的保障：遵循永不失败原则
        OBJECTPOOL_LOG(Error, TEXT("UObjectPoolLibrary: 所有回退机制都失败，创建静态紧急Actor"));

        // 创建一个静态的紧急Actor，确保永不返回nullptr
        static AActor* LibraryEmergencyActor = nullptr;
        if (!LibraryEmergencyActor)
        {
            LibraryEmergencyActor = NewObject<AActor>();
            LibraryEmergencyActor->AddToRoot(); // 防止被垃圾回收
        }
        return LibraryEmergencyActor;
    }

    // ✅ 调用子系统方法（已实现永不失败机制）
    AActor* Actor = Subsystem->SpawnActorFromPool(ActorClass, SpawnTransform);
    
    OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::SpawnActorFromPool: %s, 结果: %s"), 
        ActorClass ? *ActorClass->GetName() : TEXT("Invalid"),
        Actor ? *Actor->GetName() : TEXT("Failed"));
    
    return Actor;
}

void UObjectPoolLibrary::ReturnActorToPool(const UObject* WorldContext, AActor* Actor)
{
    UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::ReturnActorToPool: 无法获取对象池子系统，直接销毁Actor"));
        
        // ✅ 即使子系统不可用也要处理Actor（避免内存泄漏）
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
        return;
    }

    // ✅ 调用子系统方法（已有完整的错误处理）
    Subsystem->ReturnActorToPool(Actor);
    
    OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::ReturnActorToPool: %s"), 
        Actor ? *Actor->GetName() : TEXT("Invalid"));
}

AActor* UObjectPoolLibrary::QuickSpawnActor(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, const FVector& Location, const FRotator& Rotation)
{
    // ✅ 构建Transform并调用标准方法
    FTransform SpawnTransform(Rotation, Location, FVector::OneVector);
    return SpawnActorFromPool(WorldContext, ActorClass, SpawnTransform);
}

int32 UObjectPoolLibrary::BatchSpawnActors(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, const TArray<FTransform>& SpawnTransforms, TArray<AActor*>& OutActors)
{
    OutActors.Empty();
    
    if (SpawnTransforms.Num() == 0)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::BatchSpawnActors: 空的Transform数组"));
        return 0;
    }

    // ✅ 预分配输出数组
    OutActors.Reserve(SpawnTransforms.Num());
    
    int32 SuccessCount = 0;
    
    // ✅ 批量生成Actor
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
            // ✅ 即使单个失败也继续处理其他的
            OutActors.Add(nullptr);
            OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::BatchSpawnActors: 生成Actor失败"));
        }
    }
    
    OBJECTPOOL_LOG(Verbose, TEXT("UObjectPoolLibrary::BatchSpawnActors: 请求 %d 个，成功 %d 个"), 
        SpawnTransforms.Num(), SuccessCount);
    
    return SuccessCount;
}

bool UObjectPoolLibrary::IsActorClassRegistered(const UObject* WorldContext, TSubclassOf<AActor> ActorClass)
{
    UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::IsActorClassRegistered: 无法获取对象池子系统"));
        return false;
    }

    bool bRegistered = Subsystem->IsActorClassRegistered(ActorClass);

    OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::IsActorClassRegistered: %s, 结果: %s"),
        ActorClass ? *ActorClass->GetName() : TEXT("Invalid"),
        bRegistered ? TEXT("已注册") : TEXT("未注册"));

    return bRegistered;
}

FObjectPoolStats UObjectPoolLibrary::GetPoolStats(const UObject* WorldContext, TSubclassOf<AActor> ActorClass)
{
    UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::GetPoolStats: 无法获取对象池子系统"));
        return FObjectPoolStats();
    }

    FObjectPoolStats Stats = Subsystem->GetPoolStats(ActorClass);

    OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::GetPoolStats: %s"), *Stats.ToString());

    return Stats;
}

bool UObjectPoolLibrary::PrewarmPool(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, int32 Count)
{
    UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::PrewarmPool: 无法获取对象池子系统"));
        return false;
    }

    if (Count <= 0)
    {
        OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::PrewarmPool: 无效的预热数量: %d"), Count);
        return false;
    }

    // ✅ 调用子系统预热方法
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

    // ✅ 调用子系统清空方法
    Subsystem->ClearPool(ActorClass);

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

    // ✅ 使用子系统的静态方法获取实例
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

    // ✅ 使用增强的生命周期事件系统
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

    // ✅ 使用接口的批量调用方法
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

    // ✅ 使用接口的检查方法
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
    
    // ✅ 批量归还Actor
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
