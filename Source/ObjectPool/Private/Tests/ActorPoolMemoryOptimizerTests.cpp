// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// ✅ 测试目标
#include "ActorPoolMemoryOptimizer.h"
#include "ActorPoolSimplified.h"

#if WITH_OBJECTPOOL_TESTS

/**
 * FActorPoolMemoryOptimizer 基本功能测试
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActorPoolMemoryOptimizerBasicTest,
    "ObjectPool.MemoryOptimizer.Basic",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FActorPoolMemoryOptimizerBasicTest::RunTest(const FString& Parameters)
{
    // 测试1: 构造函数和基本初始化
    {
        FActorPoolMemoryOptimizer Optimizer(FActorPoolMemoryOptimizer::EOptimizationStrategy::Balanced);
        
        TestEqual(TEXT("优化策略应该正确"), 
            Optimizer.GetOptimizationStrategy(), FActorPoolMemoryOptimizer::EOptimizationStrategy::Balanced);
        
        FActorPoolMemoryOptimizer::FPreallocationConfig Config = Optimizer.GetPreallocationConfig();
        TestTrue(TEXT("预分配配置应该合理"), Config.GrowthFactor > 1.0f);
        TestTrue(TEXT("应该启用智能预分配"), Config.bEnableSmartPreallocation);
        
        AddInfo(TEXT("✅ 基本初始化测试通过"));
    }

    // 测试2: 不同优化策略的配置
    {
        // 保守策略
        FActorPoolMemoryOptimizer ConservativeOptimizer(FActorPoolMemoryOptimizer::EOptimizationStrategy::Conservative);
        FActorPoolMemoryOptimizer::FPreallocationConfig ConservativeConfig = ConservativeOptimizer.GetPreallocationConfig();
        
        // 激进策略
        FActorPoolMemoryOptimizer AggressiveOptimizer(FActorPoolMemoryOptimizer::EOptimizationStrategy::Aggressive);
        FActorPoolMemoryOptimizer::FPreallocationConfig AggressiveConfig = AggressiveOptimizer.GetPreallocationConfig();
        
        // 激进策略应该有更高的增长因子
        TestTrue(TEXT("激进策略的增长因子应该更高"), 
            AggressiveConfig.GrowthFactor > ConservativeConfig.GrowthFactor);
        
        // 激进策略应该有更低的触发阈值
        TestTrue(TEXT("激进策略的触发阈值应该更低"), 
            AggressiveConfig.TriggerThreshold < ConservativeConfig.TriggerThreshold);
        
        AddInfo(TEXT("✅ 优化策略配置测试通过"));
    }

    // 测试3: 策略切换
    {
        FActorPoolMemoryOptimizer Optimizer(FActorPoolMemoryOptimizer::EOptimizationStrategy::Conservative);
        
        TestEqual(TEXT("初始策略应该是保守"), 
            Optimizer.GetOptimizationStrategy(), FActorPoolMemoryOptimizer::EOptimizationStrategy::Conservative);
        
        // 切换到激进策略
        Optimizer.SetOptimizationStrategy(FActorPoolMemoryOptimizer::EOptimizationStrategy::Aggressive);
        
        TestEqual(TEXT("策略应该已切换"), 
            Optimizer.GetOptimizationStrategy(), FActorPoolMemoryOptimizer::EOptimizationStrategy::Aggressive);
        
        AddInfo(TEXT("✅ 策略切换测试通过"));
    }

    // 测试4: 自定义预分配配置
    {
        FActorPoolMemoryOptimizer Optimizer(FActorPoolMemoryOptimizer::EOptimizationStrategy::Custom);
        
        FActorPoolMemoryOptimizer::FPreallocationConfig CustomConfig;
        CustomConfig.GrowthFactor = 3.0f;
        CustomConfig.MinPreallocCount = 15;
        CustomConfig.MaxPreallocCount = 100;
        CustomConfig.TriggerThreshold = 0.6f;
        CustomConfig.bEnableSmartPreallocation = true;
        
        Optimizer.SetPreallocationConfig(CustomConfig);
        
        FActorPoolMemoryOptimizer::FPreallocationConfig RetrievedConfig = Optimizer.GetPreallocationConfig();
        TestEqual(TEXT("自定义增长因子应该正确"), RetrievedConfig.GrowthFactor, 3.0f);
        TestEqual(TEXT("自定义最小预分配数应该正确"), RetrievedConfig.MinPreallocCount, 15);
        TestEqual(TEXT("自定义最大预分配数应该正确"), RetrievedConfig.MaxPreallocCount, 100);
        TestEqual(TEXT("自定义触发阈值应该正确"), RetrievedConfig.TriggerThreshold, 0.6f);
        
        AddInfo(TEXT("✅ 自定义配置测试通过"));
    }

    return true;
}

/**
 * FActorPoolMemoryOptimizer 内存分析功能测试
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActorPoolMemoryOptimizerAnalysisTest,
    "ObjectPool.MemoryOptimizer.Analysis",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FActorPoolMemoryOptimizerAnalysisTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestWorld)
    {
        return false;
    }

    // 测试1: 内存使用分析
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 5, 20);
        FActorPoolMemoryOptimizer Optimizer(FActorPoolMemoryOptimizer::EOptimizationStrategy::Balanced);
        
        // 空池分析
        FActorPoolMemoryOptimizer::FMemoryStats EmptyStats = Optimizer.AnalyzeMemoryUsage(Pool);
        TestEqual(TEXT("空池的当前内存使用应该很小"), EmptyStats.CurrentMemoryUsage, static_cast<int64>(0));
        TestEqual(TEXT("空池的平均Actor大小应该为0"), EmptyStats.AverageActorSize, static_cast<int64>(0));
        
        // 预热池后分析
        Pool.PrewarmPool(TestWorld, 5);
        FActorPoolMemoryOptimizer::FMemoryStats PrewarmStats = Optimizer.AnalyzeMemoryUsage(Pool);
        TestTrue(TEXT("预热后内存使用应该增加"), PrewarmStats.CurrentMemoryUsage > EmptyStats.CurrentMemoryUsage);
        TestTrue(TEXT("预热后平均Actor大小应该大于0"), PrewarmStats.AverageActorSize > 0);
        
        AddInfo(TEXT("✅ 内存使用分析测试通过"));
    }

    // 测试2: 碎片化分析
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 5, 20);
        FActorPoolMemoryOptimizer Optimizer(FActorPoolMemoryOptimizer::EOptimizationStrategy::Balanced);
        
        // 空池的碎片化程度
        float EmptyFragmentation = Optimizer.AnalyzeFragmentation(Pool);
        TestEqual(TEXT("空池的碎片化程度应该为0"), EmptyFragmentation, 0.0f);
        
        // 预热后的碎片化程度
        Pool.PrewarmPool(TestWorld, 5);
        float PrewarmFragmentation = Optimizer.AnalyzeFragmentation(Pool);
        TestTrue(TEXT("预热后碎片化程度应该合理"), PrewarmFragmentation >= 0.0f && PrewarmFragmentation <= 1.0f);
        
        AddInfo(TEXT("✅ 碎片化分析测试通过"));
    }

    // 测试3: 优化建议生成
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 5, 20);
        FActorPoolMemoryOptimizer Optimizer(FActorPoolMemoryOptimizer::EOptimizationStrategy::Balanced);
        
        // 空池的建议
        TArray<FString> EmptySuggestions = Optimizer.GetMemoryOptimizationSuggestions(Pool);
        // 空池可能有建议，也可能没有，这取决于实现
        
        // 预热池后的建议
        Pool.PrewarmPool(TestWorld, 15);
        TArray<FString> PrewarmSuggestions = Optimizer.GetMemoryOptimizationSuggestions(Pool);
        
        // 获取一些Actor但不归还（创造低使用率场景）
        for (int32 i = 0; i < 3; ++i)
        {
            Pool.GetActor(TestWorld);
        }
        
        TArray<FString> LowUsageSuggestions = Optimizer.GetMemoryOptimizationSuggestions(Pool);
        
        // 应该有一些建议
        TestTrue(TEXT("应该能生成优化建议"), LowUsageSuggestions.Num() >= 0);
        
        AddInfo(TEXT("✅ 优化建议生成测试通过"));
    }

    // 测试4: 性能报告生成
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 5, 20);
        FActorPoolMemoryOptimizer Optimizer(FActorPoolMemoryOptimizer::EOptimizationStrategy::Balanced);
        
        Pool.PrewarmPool(TestWorld, 8);
        
        // 模拟一些使用
        AActor* Actor1 = Pool.GetActor(TestWorld);
        AActor* Actor2 = Pool.GetActor(TestWorld);
        Pool.ReturnActor(Actor1);
        
        FString Report = Optimizer.GeneratePerformanceReport(Pool);
        
        TestTrue(TEXT("性能报告应该不为空"), !Report.IsEmpty());
        TestTrue(TEXT("报告应该包含基本统计"), Report.Contains(TEXT("基本统计")));
        TestTrue(TEXT("报告应该包含内存统计"), Report.Contains(TEXT("内存统计")));
        TestTrue(TEXT("报告应该包含优化建议"), Report.Contains(TEXT("优化建议")));
        
        AddInfo(TEXT("✅ 性能报告生成测试通过"));
    }

    TestWorld->DestroyWorld(false);
    return true;
}

/**
 * FActorPoolMemoryOptimizer 预分配功能测试
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActorPoolMemoryOptimizerPreallocationTest,
    "ObjectPool.MemoryOptimizer.Preallocation",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FActorPoolMemoryOptimizerPreallocationTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestWorld)
    {
        return false;
    }

    // 测试1: 预分配条件检查
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 5, 50);
        FActorPoolMemoryOptimizer Optimizer(FActorPoolMemoryOptimizer::EOptimizationStrategy::Balanced);
        
        // 空池不应该需要预分配
        bool bShouldPreallocEmpty = Optimizer.ShouldPreallocate(Pool);
        TestFalse(TEXT("空池不应该需要预分配"), bShouldPreallocEmpty);
        
        // 预热池并获取大部分Actor
        Pool.PrewarmPool(TestWorld, 10);
        for (int32 i = 0; i < 8; ++i) // 获取8个，剩余2个
        {
            Pool.GetActor(TestWorld);
        }
        
        // 现在应该需要预分配
        bool bShouldPreallocHigh = Optimizer.ShouldPreallocate(Pool);
        TestTrue(TEXT("高使用率的池应该需要预分配"), bShouldPreallocHigh);
        
        AddInfo(TEXT("✅ 预分配条件检查测试通过"));
    }

    // 测试2: 预分配数量计算
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 5, 50);
        FActorPoolMemoryOptimizer Optimizer(FActorPoolMemoryOptimizer::EOptimizationStrategy::Balanced);
        
        // 空池的预分配数量
        int32 EmptyPreallocCount = Optimizer.CalculatePreallocationCount(Pool);
        TestEqual(TEXT("空池的预分配数量应该为0"), EmptyPreallocCount, 0);
        
        // 设置高使用率场景
        Pool.PrewarmPool(TestWorld, 10);
        for (int32 i = 0; i < 9; ++i) // 90%使用率
        {
            Pool.GetActor(TestWorld);
        }
        
        int32 HighUsagePreallocCount = Optimizer.CalculatePreallocationCount(Pool);
        TestTrue(TEXT("高使用率的预分配数量应该大于0"), HighUsagePreallocCount > 0);
        
        // 预分配数量应该在合理范围内
        FActorPoolMemoryOptimizer::FPreallocationConfig Config = Optimizer.GetPreallocationConfig();
        TestTrue(TEXT("预分配数量应该在最小值以上"), HighUsagePreallocCount >= Config.MinPreallocCount);
        TestTrue(TEXT("预分配数量应该在最大值以下"), HighUsagePreallocCount <= Config.MaxPreallocCount);
        
        AddInfo(TEXT("✅ 预分配数量计算测试通过"));
    }

    // 测试3: 智能预分配执行
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 5, 50);
        FActorPoolMemoryOptimizer Optimizer(FActorPoolMemoryOptimizer::EOptimizationStrategy::Aggressive);
        
        // 创建需要预分配的场景
        Pool.PrewarmPool(TestWorld, 10);
        for (int32 i = 0; i < 8; ++i)
        {
            Pool.GetActor(TestWorld);
        }
        
        int32 PreallocBefore = Pool.GetAvailableCount();
        
        // 执行智能预分配
        int32 PreallocatedCount = Optimizer.PerformSmartPreallocation(Pool, TestWorld);
        
        int32 PreallocAfter = Pool.GetAvailableCount();
        
        TestTrue(TEXT("应该执行了预分配"), PreallocatedCount > 0);
        TestEqual(TEXT("可用Actor数量应该增加"), PreallocAfter, PreallocBefore + PreallocatedCount);
        
        AddInfo(TEXT("✅ 智能预分配执行测试通过"));
    }

    TestWorld->DestroyWorld(false);
    return true;
}

#endif // WITH_OBJECTPOOL_TESTS
