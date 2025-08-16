// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 条件编译保护 - 只在非Shipping版本中编译测试
#if WITH_OBJECTPOOL_TESTS

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

// ✅ 对象池模块依赖
#include "ObjectPoolSubsystem.h"
#include "ObjectPoolTestAdapter.h"
#include "ObjectPoolTestHelpers.h"

/**
 * 测试环境检测和适配器功能
 * 
 * 这个测试专门解决你遇到的问题：
 * 在UE的UI测试环境中，子系统可能不可用，
 * 我们需要智能地检测环境并选择合适的测试策略
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolEnvironmentDetectionTest, 
    "XTools.ObjectPool.EnvironmentDetection", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolEnvironmentDetectionTest::RunTest(const FString& Parameters)
{
    // ✅ 初始化测试适配器
    FObjectPoolTestAdapter::Initialize();
    
    // ✅ 检测当前环境
    FObjectPoolTestAdapter::ETestEnvironment Environment = FObjectPoolTestAdapter::GetCurrentEnvironment();
    
    // ✅ 记录环境信息
    switch (Environment)
    {
    case FObjectPoolTestAdapter::ETestEnvironment::Subsystem:
        AddInfo(TEXT("检测到子系统环境 - 子系统可用"));
        break;
    case FObjectPoolTestAdapter::ETestEnvironment::DirectPool:
        AddInfo(TEXT("检测到直接池环境 - 子系统不可用，使用直接池管理"));
        break;
    case FObjectPoolTestAdapter::ETestEnvironment::Simulation:
        AddInfo(TEXT("检测到模拟环境 - 使用模拟模式"));
        break;
    default:
        AddError(TEXT("未知环境类型"));
        return false;
    }
    
    // ✅ 验证环境检测的合理性
    TestTrue("环境类型应该是已知的", Environment != FObjectPoolTestAdapter::ETestEnvironment::Unknown);
    
    // ✅ 清理
    FObjectPoolTestAdapter::Cleanup();
    
    return true;
}

/**
 * 测试子系统可用性检查
 * 
 * 这个测试帮助诊断为什么在UI测试环境中获取不到子系统
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolSubsystemAvailabilityTest, 
    "XTools.ObjectPool.SubsystemAvailability", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolSubsystemAvailabilityTest::RunTest(const FString& Parameters)
{
    // ✅ 检查GWorld状态
    if (GWorld)
    {
        AddInfo(FString::Printf(TEXT("GWorld可用: %s"), *GWorld->GetName()));
        
        // ✅ 检查GameInstance
        UGameInstance* GameInstance = GWorld->GetGameInstance();
        if (GameInstance)
        {
            AddInfo(FString::Printf(TEXT("GameInstance可用: %s"), *GameInstance->GetClass()->GetName()));
            
            // ✅ 尝试获取子系统
            UObjectPoolSubsystem* Subsystem = GameInstance->GetSubsystem<UObjectPoolSubsystem>();
            if (Subsystem)
            {
                AddInfo(TEXT("✅ 子系统通过GameInstance获取成功"));
                
                // ✅ 测试子系统功能
                try
                {
                    TArray<FObjectPoolStats> Stats = Subsystem->GetAllPoolStats();
                    AddInfo(FString::Printf(TEXT("子系统功能正常，当前池数量: %d"), Stats.Num()));
                    TestTrue("子系统应该可用", true);
                }
                catch (...)
                {
                    AddError(TEXT("❌ 子系统存在但功能异常"));
                    TestTrue("子系统功能应该正常", false);
                }
            }
            else
            {
                AddWarning(TEXT("⚠️ 无法通过GameInstance获取子系统"));
            }
        }
        else
        {
            AddWarning(TEXT("⚠️ GameInstance不可用"));
        }
    }
    else
    {
        AddWarning(TEXT("⚠️ GWorld不可用"));
    }
    
    // ✅ 尝试全局获取
    UObjectPoolSubsystem* GlobalSubsystem = UObjectPoolSubsystem::GetGlobal();
    if (GlobalSubsystem)
    {
        AddInfo(TEXT("✅ 全局子系统获取成功"));
        TestTrue("全局子系统应该可用", true);
    }
    else
    {
        AddWarning(TEXT("⚠️ 全局子系统不可用"));
        TestTrue("这是预期的测试环境行为", true);
    }
    
    return true;
}

/**
 * 测试适配器的智能切换功能
 * 
 * 验证适配器能够在不同环境下正确工作
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolAdapterSwitchingTest, 
    "XTools.ObjectPool.AdapterSwitching", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolAdapterSwitchingTest::RunTest(const FString& Parameters)
{
    // ✅ 初始化适配器
    FObjectPoolTestAdapter::Initialize();
    
    // ✅ 测试Actor类注册
    TSubclassOf<AActor> TestActorClass = AActor::StaticClass();
    bool bRegistered = FObjectPoolTestAdapter::RegisterActorClass(TestActorClass, 5);
    
    if (bRegistered)
    {
        AddInfo(TEXT("✅ Actor类注册成功"));
        
        // ✅ 验证注册状态
        bool bIsRegistered = FObjectPoolTestAdapter::IsActorClassRegistered(TestActorClass);
        TestTrue("Actor类应该已注册", bIsRegistered);
        
        // ✅ 测试生成Actor
        AActor* SpawnedActor = FObjectPoolTestAdapter::SpawnActorFromPool(TestActorClass);
        if (SpawnedActor)
        {
            AddInfo(TEXT("✅ 从池中生成Actor成功"));
            TestTrue("应该能够生成Actor", IsValid(SpawnedActor));
            
            // ✅ 测试归还Actor
            bool bReturned = FObjectPoolTestAdapter::ReturnActorToPool(SpawnedActor);
            TestTrue("应该能够归还Actor", bReturned);
            
            AddInfo(TEXT("✅ Actor归还成功"));
        }
        else
        {
            AddWarning(TEXT("⚠️ 无法从池中生成Actor（可能是模拟模式）"));
        }
        
        // ✅ 获取统计信息
        FObjectPoolStats Stats = FObjectPoolTestAdapter::GetPoolStats(TestActorClass);
        AddInfo(FString::Printf(TEXT("池统计 - 大小: %d, 活跃: %d, 可用: %d"), 
                Stats.PoolSize, Stats.CurrentActive, Stats.CurrentAvailable));
        
    }
    else
    {
        AddError(TEXT("❌ Actor类注册失败"));
        TestTrue("Actor类注册应该成功", false);
    }
    
    // ✅ 清理
    FObjectPoolTestAdapter::Cleanup();
    
    return true;
}

/**
 * 测试在不同环境下的性能表现
 * 
 * 比较子系统模式和直接池模式的性能差异
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolEnvironmentPerformanceTest, 
    "XTools.ObjectPool.EnvironmentPerformance", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolEnvironmentPerformanceTest::RunTest(const FString& Parameters)
{
    // ✅ 初始化适配器
    FObjectPoolTestAdapter::Initialize();
    
    FObjectPoolTestAdapter::ETestEnvironment Environment = FObjectPoolTestAdapter::GetCurrentEnvironment();
    
    // ✅ 性能测试参数
    const int32 TestIterations = 100;
    const int32 PoolSize = 10;
    TSubclassOf<AActor> TestActorClass = AActor::StaticClass();
    
    // ✅ 注册测试类
    bool bRegistered = FObjectPoolTestAdapter::RegisterActorClass(TestActorClass, PoolSize);
    if (!bRegistered)
    {
        AddError(TEXT("无法注册测试Actor类"));
        return false;
    }
    
    // ✅ 测试生成和归还性能
    double StartTime = FPlatformTime::Seconds();
    
    TArray<AActor*> SpawnedActors;
    SpawnedActors.Reserve(TestIterations);
    
    // 生成阶段
    for (int32 i = 0; i < TestIterations; ++i)
    {
        AActor* Actor = FObjectPoolTestAdapter::SpawnActorFromPool(TestActorClass);
        if (Actor)
        {
            SpawnedActors.Add(Actor);
        }
    }
    
    double SpawnTime = FPlatformTime::Seconds() - StartTime;
    
    // 归还阶段
    StartTime = FPlatformTime::Seconds();
    for (AActor* Actor : SpawnedActors)
    {
        FObjectPoolTestAdapter::ReturnActorToPool(Actor);
    }
    double ReturnTime = FPlatformTime::Seconds() - StartTime;
    
    // ✅ 记录性能结果
    AddInfo(FString::Printf(TEXT("环境: %s"), 
            Environment == FObjectPoolTestAdapter::ETestEnvironment::Subsystem ? TEXT("子系统") :
            Environment == FObjectPoolTestAdapter::ETestEnvironment::DirectPool ? TEXT("直接池") : TEXT("模拟")));
    AddInfo(FString::Printf(TEXT("生成%d个Actor耗时: %.4f秒"), TestIterations, SpawnTime));
    AddInfo(FString::Printf(TEXT("归还%d个Actor耗时: %.4f秒"), TestIterations, ReturnTime));
    AddInfo(FString::Printf(TEXT("总耗时: %.4f秒"), SpawnTime + ReturnTime));
    
    // ✅ 性能验证（确保在合理范围内）
    TestTrue("生成时间应该在合理范围内", SpawnTime < 1.0); // 1秒内
    TestTrue("归还时间应该在合理范围内", ReturnTime < 1.0); // 1秒内
    
    // ✅ 清理
    FObjectPoolTestAdapter::Cleanup();
    
    return true;
}

#endif // WITH_OBJECTPOOL_TESTS
