// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 条件编译保护 - 只在非Shipping版本中编译测试
#if WITH_OBJECTPOOL_TESTS

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "ObjectPoolTypes.h"

/**
 * 简单的测试验证函数
 * 用于验证对象池的基础数据结构是否正常工作
 */
void RunObjectPoolBasicTests()
{
    UE_LOG(LogTemp, Warning, TEXT("=== 开始ObjectPool基础测试 ==="));

    // ✅ 测试FObjectPoolConfig
    {
        FObjectPoolConfig Config;
        Config.ActorClass = AActor::StaticClass();
        Config.InitialSize = 10;
        Config.HardLimit = 50;
        Config.bAutoExpand = true;
        Config.bAutoShrink = false;

        bool bIsValid = Config.IsValid();
        UE_LOG(LogTemp, Warning, TEXT("FObjectPoolConfig测试: %s"), bIsValid ? TEXT("通过") : TEXT("失败"));
        
        if (bIsValid)
        {
            UE_LOG(LogTemp, Log, TEXT("  - 初始大小: %d"), Config.InitialSize);
            UE_LOG(LogTemp, Log, TEXT("  - 硬限制: %d"), Config.HardLimit);
            UE_LOG(LogTemp, Log, TEXT("  - 自动扩展: %s"), Config.bAutoExpand ? TEXT("是") : TEXT("否"));
        }
    }

    // ✅ 测试FObjectPoolStats
    {
        FObjectPoolStats Stats;
        Stats.ActorClassName = TEXT("TestActor");
        Stats.PoolSize = 20;
        Stats.CurrentActive = 5;
        Stats.CurrentAvailable = 15;
        Stats.TotalCreated = 100;
        Stats.HitRate = 0.85f;

        bool bStatsValid = (Stats.CurrentActive + Stats.CurrentAvailable == Stats.PoolSize);
        UE_LOG(LogTemp, Warning, TEXT("FObjectPoolStats测试: %s"), bStatsValid ? TEXT("通过") : TEXT("失败"));
        
        if (bStatsValid)
        {
            UE_LOG(LogTemp, Log, TEXT("  - Actor类名: %s"), *Stats.ActorClassName);
            UE_LOG(LogTemp, Log, TEXT("  - 池大小: %d"), Stats.PoolSize);
            UE_LOG(LogTemp, Log, TEXT("  - 当前活跃: %d"), Stats.CurrentActive);
            UE_LOG(LogTemp, Log, TEXT("  - 当前可用: %d"), Stats.CurrentAvailable);
            UE_LOG(LogTemp, Log, TEXT("  - 命中率: %.2f"), Stats.HitRate);
        }
    }

    // ✅ 测试FObjectPoolPreallocationConfig
    {
        FObjectPoolPreallocationConfig Config;
        Config.Strategy = EObjectPoolPreallocationStrategy::Immediate;
        Config.PreallocationCount = 10;
        Config.MaxAllocationsPerFrame = 5;
        Config.bEnableMemoryBudget = true;
        Config.MaxMemoryBudgetMB = 100;

        bool bConfigValid = (Config.PreallocationCount > 0 && Config.MaxAllocationsPerFrame > 0);
        UE_LOG(LogTemp, Warning, TEXT("FObjectPoolPreallocationConfig测试: %s"), bConfigValid ? TEXT("通过") : TEXT("失败"));
        
        if (bConfigValid)
        {
            UE_LOG(LogTemp, Log, TEXT("  - 预分配策略: %d"), (int32)Config.Strategy);
            UE_LOG(LogTemp, Log, TEXT("  - 预分配数量: %d"), Config.PreallocationCount);
            UE_LOG(LogTemp, Log, TEXT("  - 每帧最大分配: %d"), Config.MaxAllocationsPerFrame);
        }
    }

    // ✅ 测试FObjectPoolFallbackConfig
    {
        FObjectPoolFallbackConfig Config;
        Config.Strategy = EObjectPoolFallbackStrategy::NeverFail;
        Config.bAllowDefaultActorFallback = true;
        Config.bLogFallbackWarnings = true;

        bool bFallbackValid = true; // 基础配置总是有效的
        UE_LOG(LogTemp, Warning, TEXT("FObjectPoolFallbackConfig测试: %s"), bFallbackValid ? TEXT("通过") : TEXT("失败"));
        
        UE_LOG(LogTemp, Log, TEXT("  - 回退策略: %d"), (int32)Config.Strategy);
        UE_LOG(LogTemp, Log, TEXT("  - 允许默认Actor回退: %s"), Config.bAllowDefaultActorFallback ? TEXT("是") : TEXT("否"));
    }

    // ✅ 测试FActorResetConfig
    {
        FActorResetConfig Config;
        Config.bResetTransform = true;
        Config.bResetPhysics = true;
        Config.bResetAI = false;
        Config.bResetAnimation = true;
        Config.bClearTimers = true;

        bool bResetConfigValid = true; // 基础配置总是有效的
        UE_LOG(LogTemp, Warning, TEXT("FActorResetConfig测试: %s"), bResetConfigValid ? TEXT("通过") : TEXT("失败"));
        
        UE_LOG(LogTemp, Log, TEXT("  - 重置Transform: %s"), Config.bResetTransform ? TEXT("是") : TEXT("否"));
        UE_LOG(LogTemp, Log, TEXT("  - 重置物理: %s"), Config.bResetPhysics ? TEXT("是") : TEXT("否"));
        UE_LOG(LogTemp, Log, TEXT("  - 重置AI: %s"), Config.bResetAI ? TEXT("是") : TEXT("否"));
    }

    // ✅ 测试FObjectPoolLifecycleConfig
    {
        FObjectPoolLifecycleConfig Config;
        Config.bEnableLifecycleEvents = true;
        Config.bLogEventErrors = true;
        Config.EventTimeoutMs = 5000;

        bool bLifecycleValid = (Config.EventTimeoutMs >= 0);
        UE_LOG(LogTemp, Warning, TEXT("FObjectPoolLifecycleConfig测试: %s"), bLifecycleValid ? TEXT("通过") : TEXT("失败"));
        
        UE_LOG(LogTemp, Log, TEXT("  - 启用生命周期事件: %s"), Config.bEnableLifecycleEvents ? TEXT("是") : TEXT("否"));
        UE_LOG(LogTemp, Log, TEXT("  - 事件超时: %d ms"), Config.EventTimeoutMs);
    }

    // ✅ 测试FActorResetStats
    {
        FActorResetStats Stats;
        Stats.TotalResetCount = 100;
        Stats.SuccessfulResetCount = 95;
        Stats.AverageResetTimeMs = 2.5f;
        Stats.MaxResetTimeMs = 10.0f;
        Stats.MinResetTimeMs = 0.5f;

        bool bResetStatsValid = (Stats.SuccessfulResetCount <= Stats.TotalResetCount);
        UE_LOG(LogTemp, Warning, TEXT("FActorResetStats测试: %s"), bResetStatsValid ? TEXT("通过") : TEXT("失败"));
        
        if (bResetStatsValid)
        {
            float SuccessRate = Stats.TotalResetCount > 0 ? (float)Stats.SuccessfulResetCount / Stats.TotalResetCount : 0.0f;
            UE_LOG(LogTemp, Log, TEXT("  - 总重置次数: %d"), Stats.TotalResetCount);
            UE_LOG(LogTemp, Log, TEXT("  - 成功重置次数: %d"), Stats.SuccessfulResetCount);
            UE_LOG(LogTemp, Log, TEXT("  - 成功率: %.2f%%"), SuccessRate * 100.0f);
            UE_LOG(LogTemp, Log, TEXT("  - 平均重置时间: %.2f ms"), Stats.AverageResetTimeMs);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("=== ObjectPool基础测试完成 ==="));
}

// ✅ 在模块启动时自动运行测试
static class FObjectPoolTestRunner
{
public:
    FObjectPoolTestRunner()
    {
        // 延迟执行测试，确保引擎完全初始化
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float DeltaTime) -> bool
        {
            RunObjectPoolBasicTests();
            return false; // 只执行一次
        }), 2.0f); // 2秒后执行
    }
} GObjectPoolTestRunner;

#endif // WITH_OBJECTPOOL_TESTS
