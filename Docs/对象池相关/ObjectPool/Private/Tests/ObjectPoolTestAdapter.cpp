// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 条件编译保护 - 只在非Shipping版本中编译测试
#if WITH_OBJECTPOOL_TESTS

#include "ObjectPoolTestAdapter.h"
#include "Engine/Engine.h"

// 静态成员定义
FObjectPoolTestAdapter::ETestEnvironment FObjectPoolTestAdapter::CurrentEnvironment = ETestEnvironment::Unknown;
TMap<TSubclassOf<AActor>, TSharedPtr<FActorPool>> FObjectPoolTestAdapter::DirectPools;
UObjectPoolSubsystem* FObjectPoolTestAdapter::CachedSubsystem = nullptr;

bool FObjectPoolTestAdapter::RegisterWithSubsystem(TSubclassOf<AActor> ActorClass, int32 InitialSize)
{
    if (!CachedSubsystem)
    {
        return false;
    }

    CachedSubsystem->RegisterActorClass(ActorClass, InitialSize);
    return true;
}

bool FObjectPoolTestAdapter::RegisterWithDirectPool(TSubclassOf<AActor> ActorClass, int32 InitialSize)
{
    if (!ActorClass || !GWorld)
    {
        return false;
    }

    // 创建直接池
    TSharedPtr<FActorPool> Pool = MakeShared<FActorPool>(ActorClass, InitialSize);
    if (Pool.IsValid())
    {
        // 预热池
        Pool->PrewarmPool(GWorld, InitialSize);
        DirectPools.Add(ActorClass, Pool);
        
        UE_LOG(LogTemp, Log, TEXT("直接池注册成功: %s, 大小: %d"), 
               ActorClass ? *ActorClass->GetName() : TEXT("Unknown"), InitialSize);
        return true;
    }

    return false;
}

AActor* FObjectPoolTestAdapter::SpawnFromDirectPool(TSubclassOf<AActor> ActorClass, const FTransform& Transform)
{
    TSharedPtr<FActorPool>* PoolPtr = DirectPools.Find(ActorClass);
    if (PoolPtr && PoolPtr->IsValid() && GWorld)
    {
        return (*PoolPtr)->GetActor(GWorld, Transform);
    }
    return nullptr;
}

bool FObjectPoolTestAdapter::ReturnToDirectPool(AActor* Actor)
{
    if (!IsValid(Actor))
    {
        return false;
    }

    TSubclassOf<AActor> ActorClass = Actor->GetClass();
    TSharedPtr<FActorPool>* PoolPtr = DirectPools.Find(ActorClass);
    if (PoolPtr && PoolPtr->IsValid())
    {
        return (*PoolPtr)->ReturnActor(Actor);
    }
    return false;
}

FObjectPoolStats FObjectPoolTestAdapter::GetDirectPoolStats(TSubclassOf<AActor> ActorClass)
{
    FObjectPoolStats Stats;
    TSharedPtr<FActorPool>* PoolPtr = DirectPools.Find(ActorClass);
    if (PoolPtr && PoolPtr->IsValid())
    {
        Stats = (*PoolPtr)->GetStats();
    }
    return Stats;
}

AActor* FObjectPoolTestAdapter::CreateSimulatedActor(TSubclassOf<AActor> ActorClass, const FTransform& Transform)
{
    if (!ActorClass || !GWorld)
    {
        return nullptr;
    }

    // 在模拟模式下，直接创建Actor（不使用池）
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AActor* Actor = GWorld->SpawnActor<AActor>(ActorClass, Transform, SpawnParams);
    if (Actor)
    {
        UE_LOG(LogTemp, Log, TEXT("模拟模式创建Actor: %s"), *Actor->GetName());
    }
    
    return Actor;
}

FObjectPoolStats FObjectPoolTestAdapter::CreateSimulatedStats(TSubclassOf<AActor> ActorClass)
{
    FObjectPoolStats Stats;
    Stats.ActorClassName = ActorClass ? ActorClass->GetName() : TEXT("Unknown");
    Stats.PoolSize = 5;
    Stats.CurrentAvailable = 3;
    Stats.CurrentActive = 2;
    Stats.TotalCreated = 5;
    Stats.HitRate = 0.8f;
    
    return Stats;
}

#endif // WITH_OBJECTPOOL_TESTS
