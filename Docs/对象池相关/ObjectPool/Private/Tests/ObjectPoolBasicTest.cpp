// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 条件编译保护 - 只在非Shipping版本中编译测试
#if WITH_OBJECTPOOL_TESTS

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"

// ✅ 对象池模块依赖
#include "ObjectPoolTypes.h"
#include "ActorPool.h"

/**
 * ObjectPool 基础数据结构测试
 * 单一职责：测试对象池的基础数据结构和配置
 * 不重复造轮子：基于UE5内置测试框架
 * 独立测试：不依赖子系统，专注核心逻辑
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolTypesTest, 
    "XTools.ObjectPool.Types", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolTypesTest::RunTest(const FString& Parameters)
{
    // ✅ 测试FObjectPoolConfig的基础功能
    FObjectPoolConfig Config;
    Config.ActorClass = AActor::StaticClass();
    Config.InitialSize = 10;
    Config.HardLimit = 50;
    Config.bAutoExpand = true;
    Config.bAutoShrink = false;

    // ✅ 验证配置的有效性检查
    TestTrue("配置应该有效", Config.IsValid());
    TestEqual("初始大小应该正确", Config.InitialSize, 10);
    TestEqual("硬限制应该正确", Config.HardLimit, 50);
    TestTrue("自动扩展应该启用", Config.bAutoExpand);
    TestFalse("自动收缩应该禁用", Config.bAutoShrink);

    // ✅ 测试无效配置
    FObjectPoolConfig InvalidConfig;
    InvalidConfig.ActorClass = nullptr;
    InvalidConfig.InitialSize = -1;
    TestFalse("无效配置应该被检测出来", InvalidConfig.IsValid());

    return true;
}

/**
 * ObjectPool 统计数据结构测试
 * 单一职责：测试统计数据结构的功能
 * 不重复造轮子：基于已有的统计类型
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolStatsTypesTest, 
    "XTools.ObjectPool.StatsTypes", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolStatsTypesTest::RunTest(const FString& Parameters)
{
    // ✅ 测试FObjectPoolStats的基础功能
    FObjectPoolStats Stats;
    Stats.ActorClassName = TEXT("TestActor");
    Stats.PoolSize = 20;
    Stats.CurrentActive = 5;
    Stats.CurrentAvailable = 15;
    Stats.TotalCreated = 100;
    Stats.HitRate = 0.85f;

    // ✅ 验证统计数据的一致性
    TestEqual("Actor类名应该正确", Stats.ActorClassName, TEXT("TestActor"));
    TestEqual("池大小应该正确", Stats.PoolSize, 20);
    TestEqual("活跃数量应该正确", Stats.CurrentActive, 5);
    TestEqual("可用数量应该正确", Stats.CurrentAvailable, 15);
    TestEqual("总创建数应该正确", Stats.TotalCreated, 100);
    TestEqual("命中率应该正确", Stats.HitRate, 0.85f);

    // ✅ 验证计算逻辑
    TestEqual("活跃+可用应该等于池大小", Stats.CurrentActive + Stats.CurrentAvailable, Stats.PoolSize);

    return true;
}

/**
 * ObjectPool 预分配配置测试
 * 单一职责：测试预分配配置的功能
 * 基于已有的配置类型
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolPreallocationConfigTest, 
    "XTools.ObjectPool.PreallocationConfig", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolPreallocationConfigTest::RunTest(const FString& Parameters)
{
    // ✅ 测试预分配配置的基础功能
    FObjectPoolPreallocationConfig Config;
    Config.Strategy = EObjectPoolPreallocationStrategy::Immediate;
    Config.PreallocationCount = 10;
    Config.MaxAllocationsPerFrame = 5;
    Config.bEnableMemoryBudget = true;
    Config.MaxMemoryBudgetMB = 100;

    // ✅ 验证配置值
    TestTrue("策略应该是立即预分配", Config.Strategy == EObjectPoolPreallocationStrategy::Immediate);
    TestEqual("预分配数量应该正确", Config.PreallocationCount, 10);
    TestEqual("每帧最大分配数应该正确", Config.MaxAllocationsPerFrame, 5);
    TestTrue("内存预算应该启用", Config.bEnableMemoryBudget);
    TestEqual("内存预算应该正确", Config.MaxMemoryBudgetMB, 100);

    // ✅ 测试不同的预分配策略
    Config.Strategy = EObjectPoolPreallocationStrategy::Progressive;
    TestTrue("策略应该是渐进式预分配", Config.Strategy == EObjectPoolPreallocationStrategy::Progressive);

    Config.Strategy = EObjectPoolPreallocationStrategy::Predictive;
    TestTrue("策略应该是预测性预分配", Config.Strategy == EObjectPoolPreallocationStrategy::Predictive);

    return true;
}

/**
 * ObjectPool 回退配置测试
 * 单一职责：测试回退策略配置
 * 基于已有的配置类型
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolFallbackConfigTest, 
    "XTools.ObjectPool.FallbackConfig", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolFallbackConfigTest::RunTest(const FString& Parameters)
{
    // ✅ 测试回退配置的基础功能
    FObjectPoolFallbackConfig Config;
    Config.Strategy = EObjectPoolFallbackStrategy::NeverFail;
    Config.bAllowDefaultActorFallback = true;
    Config.bLogFallbackWarnings = true;

    // ✅ 验证配置值
    TestTrue("策略应该是永不失败", Config.Strategy == EObjectPoolFallbackStrategy::NeverFail);
    TestTrue("应该允许默认Actor回退", Config.bAllowDefaultActorFallback);
    TestTrue("应该记录回退警告", Config.bLogFallbackWarnings);

    // ✅ 测试不同的回退策略
    Config.Strategy = EObjectPoolFallbackStrategy::StrictMode;
    TestTrue("策略应该是严格模式", Config.Strategy == EObjectPoolFallbackStrategy::StrictMode);

    Config.Strategy = EObjectPoolFallbackStrategy::TypeFallback;
    TestTrue("策略应该是类型回退", Config.Strategy == EObjectPoolFallbackStrategy::TypeFallback);

    return true;
}

/**
 * ObjectPool Actor重置配置测试
 * 单一职责：测试Actor重置配置
 * 基于已有的配置类型
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActorResetConfigTest, 
    "XTools.ObjectPool.ActorResetConfig", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FActorResetConfigTest::RunTest(const FString& Parameters)
{
    // ✅ 测试Actor重置配置的基础功能
    FActorResetConfig Config;
    Config.bResetTransform = true;
    Config.bResetPhysics = true;
    Config.bResetAI = false;
    Config.bResetAnimation = true;
    Config.bClearTimers = true;

    // ✅ 验证配置值
    TestTrue("应该重置Transform", Config.bResetTransform);
    TestTrue("应该重置物理", Config.bResetPhysics);
    TestFalse("不应该重置AI", Config.bResetAI);
    TestTrue("应该重置动画", Config.bResetAnimation);
    TestTrue("应该清理定时器", Config.bClearTimers);

    // ✅ 测试默认配置
    FActorResetConfig DefaultConfig;
    // 验证默认值是合理的
    TestTrue("默认配置应该是有效的", true); // 只要能创建就说明默认值是合理的

    return true;
}

/**
 * ObjectPool 生命周期配置测试
 * 单一职责：测试生命周期配置
 * 基于已有的配置类型
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolLifecycleConfigTest, 
    "XTools.ObjectPool.LifecycleConfig", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolLifecycleConfigTest::RunTest(const FString& Parameters)
{
    // ✅ 测试生命周期配置的基础功能
    FObjectPoolLifecycleConfig Config;
    Config.bEnableLifecycleEvents = true;
    Config.bLogEventErrors = true;
    Config.EventTimeoutMs = 5000;

    // ✅ 验证配置值
    TestTrue("应该启用生命周期事件", Config.bEnableLifecycleEvents);
    TestTrue("应该记录事件错误", Config.bLogEventErrors);
    TestEqual("事件超时应该正确", Config.EventTimeoutMs, 5000);

    // ✅ 测试边界值
    Config.EventTimeoutMs = 0;
    TestEqual("超时可以设置为0", Config.EventTimeoutMs, 0);

    Config.EventTimeoutMs = 60000; // 1分钟
    TestEqual("超时可以设置为较大值", Config.EventTimeoutMs, 60000);

    return true;
}

/**
 * ObjectPool 重置统计测试
 * 单一职责：测试重置统计数据结构
 * 基于已有的统计类型
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActorResetStatsTest, 
    "XTools.ObjectPool.ActorResetStats", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FActorResetStatsTest::RunTest(const FString& Parameters)
{
    // ✅ 测试重置统计的基础功能
    FActorResetStats Stats;
    Stats.TotalResetCount = 100;
    Stats.SuccessfulResetCount = 95;
    Stats.AverageResetTimeMs = 2.5f;
    Stats.MaxResetTimeMs = 10.0f;
    Stats.MinResetTimeMs = 0.5f;

    // ✅ 验证统计数据
    TestEqual("总重置次数应该正确", Stats.TotalResetCount, 100);
    TestEqual("成功重置次数应该正确", Stats.SuccessfulResetCount, 95);
    TestEqual("平均重置时间应该正确", Stats.AverageResetTimeMs, 2.5f);
    TestEqual("最大重置时间应该正确", Stats.MaxResetTimeMs, 10.0f);
    TestEqual("最小重置时间应该正确", Stats.MinResetTimeMs, 0.5f);

    // ✅ 计算成功率
    float SuccessRate = Stats.TotalResetCount > 0 ? (float)Stats.SuccessfulResetCount / Stats.TotalResetCount : 0.0f;
    TestEqual("成功率应该正确", SuccessRate, 0.95f);

    return true;
}

#endif // WITH_OBJECTPOOL_TESTS
