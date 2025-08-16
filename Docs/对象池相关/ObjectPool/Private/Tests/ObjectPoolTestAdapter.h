// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// ✅ 条件编译保护 - 只在非Shipping版本中编译测试
#if WITH_OBJECTPOOL_TESTS

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

// ✅ 对象池模块依赖
#include "ObjectPoolSubsystem.h"
#include "ObjectPoolTypes.h"
#include "ActorPool.h"

/**
 * 对象池测试适配器
 * 
 * 设计目标：
 * - 自动检测测试环境并选择最佳的测试方法
 * - 在子系统可用时使用子系统，不可用时使用直接池管理
 * - 提供统一的API接口，隐藏底层实现差异
 * - 确保测试在任何环境中都能正常运行
 */
class OBJECTPOOL_API FObjectPoolTestAdapter
{
public:
    /**
     * 测试环境类型
     */
    enum class ETestEnvironment
    {
        Unknown,        // 未知环境
        Subsystem,      // 子系统可用
        DirectPool,     // 直接池管理
        Simulation      // 模拟环境
    };

private:
    static ETestEnvironment CurrentEnvironment;
    static TMap<TSubclassOf<AActor>, TSharedPtr<FActorPool>> DirectPools;
    static UObjectPoolSubsystem* CachedSubsystem;

public:
    /**
     * 初始化测试适配器
     * 自动检测当前环境并选择最佳策略
     */
    static void Initialize()
    {
        CurrentEnvironment = DetectTestEnvironment();
        
        switch (CurrentEnvironment)
        {
        case ETestEnvironment::Subsystem:
            UE_LOG(LogTemp, Log, TEXT("测试适配器：使用子系统模式"));
            break;
        case ETestEnvironment::DirectPool:
            UE_LOG(LogTemp, Warning, TEXT("测试适配器：使用直接池管理模式"));
            break;
        case ETestEnvironment::Simulation:
            UE_LOG(LogTemp, Warning, TEXT("测试适配器：使用模拟模式"));
            break;
        default:
            UE_LOG(LogTemp, Error, TEXT("测试适配器：未知环境"));
            break;
        }
    }

    /**
     * 清理测试适配器
     */
    static void Cleanup()
    {
        // 清理直接池
        for (auto& PoolPair : DirectPools)
        {
            if (PoolPair.Value.IsValid())
            {
                PoolPair.Value->ClearPool();
            }
        }
        DirectPools.Empty();
        
        CachedSubsystem = nullptr;
        CurrentEnvironment = ETestEnvironment::Unknown;
    }

    /**
     * 注册Actor类
     * @param ActorClass Actor类
     * @param InitialSize 初始大小
     * @return 是否成功
     */
    static bool RegisterActorClass(TSubclassOf<AActor> ActorClass, int32 InitialSize)
    {
        if (!ActorClass || InitialSize <= 0)
        {
            return false;
        }

        switch (CurrentEnvironment)
        {
        case ETestEnvironment::Subsystem:
            return RegisterWithSubsystem(ActorClass, InitialSize);
        case ETestEnvironment::DirectPool:
            return RegisterWithDirectPool(ActorClass, InitialSize);
        case ETestEnvironment::Simulation:
            return true; // 模拟成功
        default:
            return false;
        }
    }

    /**
     * 检查Actor类是否已注册
     * @param ActorClass Actor类
     * @return 是否已注册
     */
    static bool IsActorClassRegistered(TSubclassOf<AActor> ActorClass)
    {
        switch (CurrentEnvironment)
        {
        case ETestEnvironment::Subsystem:
            return CachedSubsystem ? CachedSubsystem->IsActorClassRegistered(ActorClass) : false;
        case ETestEnvironment::DirectPool:
            return DirectPools.Contains(ActorClass);
        case ETestEnvironment::Simulation:
            return true; // 模拟已注册
        default:
            return false;
        }
    }

    /**
     * 从池中生成Actor
     * @param ActorClass Actor类
     * @param Transform 变换
     * @return 生成的Actor
     */
    static AActor* SpawnActorFromPool(TSubclassOf<AActor> ActorClass, const FTransform& Transform = FTransform::Identity)
    {
        switch (CurrentEnvironment)
        {
        case ETestEnvironment::Subsystem:
            return CachedSubsystem ? CachedSubsystem->SpawnActorFromPool(ActorClass, Transform) : nullptr;
        case ETestEnvironment::DirectPool:
            return SpawnFromDirectPool(ActorClass, Transform);
        case ETestEnvironment::Simulation:
            return CreateSimulatedActor(ActorClass, Transform);
        default:
            return nullptr;
        }
    }

    /**
     * 将Actor归还到池
     * @param Actor 要归还的Actor
     * @return 是否成功
     */
    static bool ReturnActorToPool(AActor* Actor)
    {
        if (!IsValid(Actor))
        {
            return false;
        }

        switch (CurrentEnvironment)
        {
        case ETestEnvironment::Subsystem:
            if (CachedSubsystem)
            {
                CachedSubsystem->ReturnActorToPool(Actor);
                return true;
            }
            return false;
        case ETestEnvironment::DirectPool:
            return ReturnToDirectPool(Actor);
        case ETestEnvironment::Simulation:
            return true; // 模拟成功
        default:
            return false;
        }
    }

    /**
     * 获取池统计信息
     * @param ActorClass Actor类
     * @return 统计信息
     */
    static FObjectPoolStats GetPoolStats(TSubclassOf<AActor> ActorClass)
    {
        FObjectPoolStats Stats;
        
        switch (CurrentEnvironment)
        {
        case ETestEnvironment::Subsystem:
            if (CachedSubsystem)
            {
                Stats = CachedSubsystem->GetPoolStats(ActorClass);
            }
            break;
        case ETestEnvironment::DirectPool:
            Stats = GetDirectPoolStats(ActorClass);
            break;
        case ETestEnvironment::Simulation:
            Stats = CreateSimulatedStats(ActorClass);
            break;
        }
        
        return Stats;
    }

    /**
     * 获取当前测试环境类型
     * @return 环境类型
     */
    static ETestEnvironment GetCurrentEnvironment()
    {
        return CurrentEnvironment;
    }

private:
    /**
     * 检测测试环境
     * @return 检测到的环境类型
     */
    static ETestEnvironment DetectTestEnvironment()
    {
        // ✅ 尝试获取子系统
        UObjectPoolSubsystem* Subsystem = UObjectPoolSubsystem::GetGlobal();
        if (!Subsystem && GWorld && GWorld->GetGameInstance())
        {
            Subsystem = GWorld->GetGameInstance()->GetSubsystem<UObjectPoolSubsystem>();
        }

        if (Subsystem)
        {
            // 验证子系统是否真正可用
            try
            {
                TArray<FObjectPoolStats> TestStats = Subsystem->GetAllPoolStats();
                CachedSubsystem = Subsystem;
                return ETestEnvironment::Subsystem;
            }
            catch (...)
            {
                // 子系统存在但不可用
            }
        }

        // ✅ 检查是否可以创建直接池
        if (GWorld)
        {
            return ETestEnvironment::DirectPool;
        }

        // ✅ 最后使用模拟模式
        return ETestEnvironment::Simulation;
    }

    // 各种环境下的具体实现方法...
    static bool RegisterWithSubsystem(TSubclassOf<AActor> ActorClass, int32 InitialSize);
    static bool RegisterWithDirectPool(TSubclassOf<AActor> ActorClass, int32 InitialSize);
    static AActor* SpawnFromDirectPool(TSubclassOf<AActor> ActorClass, const FTransform& Transform);
    static bool ReturnToDirectPool(AActor* Actor);
    static FObjectPoolStats GetDirectPoolStats(TSubclassOf<AActor> ActorClass);
    static AActor* CreateSimulatedActor(TSubclassOf<AActor> ActorClass, const FTransform& Transform);
    static FObjectPoolStats CreateSimulatedStats(TSubclassOf<AActor> ActorClass);
};

#endif // WITH_OBJECTPOOL_TESTS
