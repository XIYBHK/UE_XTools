// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 条件编译保护 - 只在测试环境中编译
#if WITH_OBJECTPOOL_TESTS

// ✅ 核心依赖
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Components/StaticMeshComponent.h"

// ✅ 对象池模块依赖
#include "ObjectPoolUtils.h"
#include "ObjectPoolTypesSimplified.h"
#include "ObjectPoolInterface.h"

/**
 * 测试专用的简单Actor类
 * 用于验证工具类的Actor操作功能
 */
class ATestUtilsActor : public AActor, public IObjectPoolInterface
{
public:
    ATestUtilsActor()
    {
        PrimaryActorTick.bCanEverTick = true;
        bReplicates = false;
        
        // 添加一个简单的静态网格组件
        MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
        RootComponent = MeshComponent;
    }

    // 测试状态标记
    bool bWasReset = false;
    bool bWasActivated = false;
    bool bWasCreated = false;
    int32 ResetCount = 0;
    int32 ActivationCount = 0;

    // IObjectPoolInterface 实现
    virtual void OnPoolActorActivated_Implementation() override
    {
        bWasActivated = true;
        ActivationCount++;
    }

    virtual void OnReturnToPool_Implementation() override
    {
        bWasReset = true;
        ResetCount++;
    }

    virtual void OnPoolActorCreated_Implementation() override
    {
        bWasCreated = true;
    }

    // 重置测试状态
    void ResetTestState()
    {
        bWasReset = false;
        bWasActivated = false;
        bWasCreated = false;
        ResetCount = 0;
        ActivationCount = 0;
    }

private:
    UPROPERTY()
    UStaticMeshComponent* MeshComponent;
};

/**
 * FObjectPoolUtils Actor状态重置功能测试
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolUtilsActorResetTest,
    "ObjectPool.Utils.ActorReset",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolUtilsActorResetTest::RunTest(const FString& Parameters)
{
    // 创建测试世界
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestWorld)
    {
        AddError(TEXT("无法创建测试世界"));
        return false;
    }

    // 创建测试Actor
    ATestUtilsActor* TestActor = TestWorld->SpawnActor<ATestUtilsActor>();
    if (!TestActor)
    {
        AddError(TEXT("无法创建测试Actor"));
        TestWorld->DestroyWorld(false);
        return false;
    }

    // 测试1: ResetActorForPooling
    {
        TestActor->ResetTestState();
        
        bool bResult = FObjectPoolUtils::ResetActorForPooling(TestActor);
        TestTrue(TEXT("ResetActorForPooling应该成功"), bResult);
        TestTrue(TEXT("Actor应该被隐藏"), TestActor->IsHidden());
        TestFalse(TEXT("Actor的Tick应该被禁用"), TestActor->IsActorTickEnabled());
        TestTrue(TEXT("应该调用OnReturnToPool接口"), TestActor->bWasReset);
        TestEqual(TEXT("重置次数应为1"), TestActor->ResetCount, 1);
        
        AddInfo(TEXT("✅ ResetActorForPooling 测试通过"));
    }

    // 测试2: ActivateActorFromPool
    {
        TestActor->ResetTestState();
        FTransform NewTransform(FRotator::ZeroRotator, FVector(100, 200, 300), FVector::OneVector);
        
        bool bResult = FObjectPoolUtils::ActivateActorFromPool(TestActor, NewTransform);
        TestTrue(TEXT("ActivateActorFromPool应该成功"), bResult);
        TestFalse(TEXT("Actor应该可见"), TestActor->IsHidden());
        TestTrue(TEXT("Actor的Tick应该启用"), TestActor->IsActorTickEnabled());
        TestTrue(TEXT("应该调用OnPoolActorActivated接口"), TestActor->bWasActivated);
        TestEqual(TEXT("激活次数应为1"), TestActor->ActivationCount, 1);
        
        // 验证Transform是否正确应用
        FVector ActorLocation = TestActor->GetActorLocation();
        TestTrue(TEXT("Actor位置应该正确设置"), ActorLocation.Equals(FVector(100, 200, 300), 1.0f));
        
        AddInfo(TEXT("✅ ActivateActorFromPool 测试通过"));
    }

    // 测试3: BasicActorReset
    {
        FTransform ResetTransform(FRotator(45, 90, 0), FVector(500, 600, 700), FVector(2, 2, 2));
        
        bool bResult = FObjectPoolUtils::BasicActorReset(TestActor, ResetTransform, true);
        TestTrue(TEXT("BasicActorReset应该成功"), bResult);
        
        // 验证Transform
        FVector ActorLocation = TestActor->GetActorLocation();
        TestTrue(TEXT("Actor位置应该正确重置"), ActorLocation.Equals(FVector(500, 600, 700), 1.0f));
        
        AddInfo(TEXT("✅ BasicActorReset 测试通过"));
    }

    // 测试4: 无效Actor处理
    {
        bool bResult = FObjectPoolUtils::ResetActorForPooling(nullptr);
        TestFalse(TEXT("空Actor应该返回false"), bResult);
        
        bResult = FObjectPoolUtils::ActivateActorFromPool(nullptr, FTransform::Identity);
        TestFalse(TEXT("空Actor激活应该返回false"), bResult);
        
        AddInfo(TEXT("✅ 无效Actor处理测试通过"));
    }

    // 清理
    TestWorld->DestroyWorld(false);
    return true;
}

/**
 * FObjectPoolUtils 配置管理功能测试
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolUtilsConfigTest,
    "ObjectPool.Utils.Config",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolUtilsConfigTest::RunTest(const FString& Parameters)
{
    // 测试1: ValidateConfig - 有效配置
    {
        FObjectPoolConfigSimplified ValidConfig;
        ValidConfig.ActorClass = AActor::StaticClass();
        ValidConfig.InitialSize = 10;
        ValidConfig.HardLimit = 50;
        ValidConfig.bEnablePrewarm = true;
        
        FString ErrorMessage;
        bool bResult = FObjectPoolUtils::ValidateConfig(ValidConfig, ErrorMessage);
        TestTrue(TEXT("有效配置应该通过验证"), bResult);
        TestTrue(TEXT("有效配置不应该有错误信息"), ErrorMessage.IsEmpty());
        
        AddInfo(TEXT("✅ 有效配置验证测试通过"));
    }

    // 测试2: ValidateConfig - 无效配置
    {
        FObjectPoolConfigSimplified InvalidConfig;
        InvalidConfig.ActorClass = nullptr; // 无效的Actor类
        InvalidConfig.InitialSize = 10;
        
        FString ErrorMessage;
        bool bResult = FObjectPoolUtils::ValidateConfig(InvalidConfig, ErrorMessage);
        TestFalse(TEXT("无效配置应该验证失败"), bResult);
        TestFalse(TEXT("应该有错误信息"), ErrorMessage.IsEmpty());
        TestTrue(TEXT("错误信息应该提到Actor类"), ErrorMessage.Contains(TEXT("Actor类")));
        
        AddInfo(TEXT("✅ 无效配置验证测试通过"));
    }

    // 测试3: ValidateConfig - 初始大小为0
    {
        FObjectPoolConfigSimplified InvalidConfig;
        InvalidConfig.ActorClass = AActor::StaticClass();
        InvalidConfig.InitialSize = 0; // 无效的初始大小
        
        FString ErrorMessage;
        bool bResult = FObjectPoolUtils::ValidateConfig(InvalidConfig, ErrorMessage);
        TestFalse(TEXT("初始大小为0应该验证失败"), bResult);
        TestTrue(TEXT("错误信息应该提到初始大小"), ErrorMessage.Contains(TEXT("初始大小")));
        
        AddInfo(TEXT("✅ 初始大小验证测试通过"));
    }

    // 测试4: ValidateConfig - 硬限制小于初始大小
    {
        FObjectPoolConfigSimplified InvalidConfig;
        InvalidConfig.ActorClass = AActor::StaticClass();
        InvalidConfig.InitialSize = 20;
        InvalidConfig.HardLimit = 10; // 硬限制小于初始大小
        
        FString ErrorMessage;
        bool bResult = FObjectPoolUtils::ValidateConfig(InvalidConfig, ErrorMessage);
        TestFalse(TEXT("硬限制小于初始大小应该验证失败"), bResult);
        TestTrue(TEXT("错误信息应该提到硬限制"), ErrorMessage.Contains(TEXT("硬限制")));
        
        AddInfo(TEXT("✅ 硬限制验证测试通过"));
    }

    // 测试5: ApplyDefaultConfig
    {
        FObjectPoolConfigSimplified Config;
        Config.ActorClass = ACharacter::StaticClass();
        Config.InitialSize = 0; // 需要应用默认值
        Config.HardLimit = 0;   // 需要应用默认值
        
        FObjectPoolUtils::ApplyDefaultConfig(Config);
        
        TestTrue(TEXT("初始大小应该被设置为正值"), Config.InitialSize > 0);
        TestTrue(TEXT("硬限制应该被设置为正值"), Config.HardLimit > 0);
        TestTrue(TEXT("硬限制应该大于等于初始大小"), Config.HardLimit >= Config.InitialSize);
        
        AddInfo(TEXT("✅ ApplyDefaultConfig 测试通过"));
    }

    // 测试6: CreateDefaultConfig
    {
        FObjectPoolConfigSimplified BulletConfig = FObjectPoolUtils::CreateDefaultConfig(AActor::StaticClass(), TEXT("子弹"));
        TestEqual(TEXT("子弹配置初始大小应为50"), BulletConfig.InitialSize, 50);
        TestEqual(TEXT("子弹配置硬限制应为200"), BulletConfig.HardLimit, 200);
        
        FObjectPoolConfigSimplified EnemyConfig = FObjectPoolUtils::CreateDefaultConfig(AActor::StaticClass(), TEXT("敌人"));
        TestEqual(TEXT("敌人配置初始大小应为20"), EnemyConfig.InitialSize, 20);
        TestEqual(TEXT("敌人配置硬限制应为100"), EnemyConfig.HardLimit, 100);
        
        AddInfo(TEXT("✅ CreateDefaultConfig 测试通过"));
    }

    return true;
}

/**
 * FObjectPoolUtils 调试和监控功能测试
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolUtilsDebugTest,
    "ObjectPool.Utils.Debug",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolUtilsDebugTest::RunTest(const FString& Parameters)
{
    // 测试1: IsPoolHealthy - 健康的池
    {
        FObjectPoolStatsSimplified HealthyStats;
        HealthyStats.TotalCreated = 20;
        HealthyStats.CurrentActive = 15;
        HealthyStats.CurrentAvailable = 5;
        HealthyStats.PoolSize = 20;
        HealthyStats.HitRate = 0.8f; // 80%命中率
        HealthyStats.ActorClassName = TEXT("TestActor");

        bool bIsHealthy = FObjectPoolUtils::IsPoolHealthy(HealthyStats);
        TestTrue(TEXT("高命中率的池应该被认为是健康的"), bIsHealthy);

        AddInfo(TEXT("✅ 健康池检测测试通过"));
    }

    // 测试2: IsPoolHealthy - 不健康的池（低命中率）
    {
        FObjectPoolStatsSimplified UnhealthyStats;
        UnhealthyStats.TotalCreated = 50;
        UnhealthyStats.CurrentActive = 10;
        UnhealthyStats.CurrentAvailable = 40;
        UnhealthyStats.PoolSize = 50;
        UnhealthyStats.HitRate = 0.2f; // 20%命中率，过低
        UnhealthyStats.ActorClassName = TEXT("TestActor");

        bool bIsHealthy = FObjectPoolUtils::IsPoolHealthy(UnhealthyStats);
        TestFalse(TEXT("低命中率的池应该被认为是不健康的"), bIsHealthy);

        AddInfo(TEXT("✅ 不健康池检测测试通过"));
    }

    // 测试3: GetPerformanceSuggestions
    {
        FObjectPoolStatsSimplified Stats;
        Stats.TotalCreated = 30;
        Stats.CurrentActive = 5;
        Stats.CurrentAvailable = 25; // 大量未使用对象
        Stats.PoolSize = 30;
        Stats.HitRate = 0.4f; // 中等命中率
        Stats.ActorClassName = TEXT("TestActor");

        TArray<FString> Suggestions = FObjectPoolUtils::GetPerformanceSuggestions(Stats);
        TestTrue(TEXT("应该有性能建议"), Suggestions.Num() > 0);

        // 检查是否包含预期的建议
        bool bFoundHitRateSuggestion = false;
        bool bFoundUnusedObjectSuggestion = false;

        for (const FString& Suggestion : Suggestions)
        {
            if (Suggestion.Contains(TEXT("命中率")))
            {
                bFoundHitRateSuggestion = true;
            }
            if (Suggestion.Contains(TEXT("未使用")))
            {
                bFoundUnusedObjectSuggestion = true;
            }
        }

        TestTrue(TEXT("应该包含命中率相关建议"), bFoundHitRateSuggestion);
        TestTrue(TEXT("应该包含未使用对象相关建议"), bFoundUnusedObjectSuggestion);

        AddInfo(TEXT("✅ 性能建议测试通过"));
    }

    // 测试4: GetDebugInfo
    {
        FObjectPoolStatsSimplified Stats;
        Stats.TotalCreated = 10;
        Stats.CurrentActive = 8;
        Stats.CurrentAvailable = 2;
        Stats.PoolSize = 10;
        Stats.HitRate = 0.9f; // 高命中率
        Stats.ActorClassName = TEXT("TestActor");

        FObjectPoolDebugInfoSimplified DebugInfo = FObjectPoolUtils::GetDebugInfo(Stats, TEXT("TestPool"));

        TestEqual(TEXT("池名称应该正确"), DebugInfo.PoolName, TEXT("TestPool"));
        TestTrue(TEXT("高命中率的池应该是健康的"), DebugInfo.bIsHealthy);
        TestEqual(TEXT("统计信息应该匹配"), DebugInfo.Stats.TotalCreated, Stats.TotalCreated);

        AddInfo(TEXT("✅ 调试信息获取测试通过"));
    }

    // 测试5: FormatStatsString
    {
        FObjectPoolStatsSimplified Stats;
        Stats.TotalCreated = 15;
        Stats.CurrentActive = 10;
        Stats.CurrentAvailable = 5;
        Stats.PoolSize = 15;
        Stats.HitRate = 0.75f;
        Stats.ActorClassName = TEXT("TestActor");

        // 测试简单格式
        FString SimpleFormat = FObjectPoolUtils::FormatStatsString(Stats, false);
        TestTrue(TEXT("简单格式应该包含活跃数"), SimpleFormat.Contains(TEXT("活跃=10")));
        TestTrue(TEXT("简单格式应该包含可用数"), SimpleFormat.Contains(TEXT("可用=5")));
        TestTrue(TEXT("简单格式应该包含命中率"), SimpleFormat.Contains(TEXT("75.0%")));

        // 测试详细格式
        FString DetailedFormat = FObjectPoolUtils::FormatStatsString(Stats, true);
        TestTrue(TEXT("详细格式应该包含总创建数"), DetailedFormat.Contains(TEXT("总创建=15")));
        TestTrue(TEXT("详细格式应该包含类型"), DetailedFormat.Contains(TEXT("TestActor")));

        AddInfo(TEXT("✅ 统计信息格式化测试通过"));
    }

    return true;
}

/**
 * FObjectPoolUtils 性能分析功能测试
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolUtilsPerformanceTest,
    "ObjectPool.Utils.Performance",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolUtilsPerformanceTest::RunTest(const FString& Parameters)
{
    // 测试1: EstimateMemoryUsage
    {
        int64 ActorMemory = FObjectPoolUtils::EstimateMemoryUsage(AActor::StaticClass(), 10);
        TestTrue(TEXT("Actor内存估算应该大于0"), ActorMemory > 0);

        int64 CharacterMemory = FObjectPoolUtils::EstimateMemoryUsage(ACharacter::StaticClass(), 10);
        TestTrue(TEXT("Character内存估算应该大于Actor"), CharacterMemory > ActorMemory);

        // 测试无效输入
        int64 InvalidMemory = FObjectPoolUtils::EstimateMemoryUsage(nullptr, 10);
        TestEqual(TEXT("无效Actor类应该返回0"), InvalidMemory, static_cast<int64>(0));

        InvalidMemory = FObjectPoolUtils::EstimateMemoryUsage(AActor::StaticClass(), 0);
        TestEqual(TEXT("池大小为0应该返回0"), InvalidMemory, static_cast<int64>(0));

        AddInfo(TEXT("✅ 内存使用估算测试通过"));
    }

    // 测试2: AnalyzeUsagePattern
    {
        // 高效使用模式
        FObjectPoolStatsSimplified HighEfficiencyStats;
        HighEfficiencyStats.TotalCreated = 20;
        HighEfficiencyStats.HitRate = 0.9f;

        FString Pattern = FObjectPoolUtils::AnalyzeUsagePattern(HighEfficiencyStats);
        TestTrue(TEXT("高命中率应该被识别为高效使用"), Pattern.Contains(TEXT("高效")));

        // 低效使用模式
        FObjectPoolStatsSimplified LowEfficiencyStats;
        LowEfficiencyStats.TotalCreated = 20;
        LowEfficiencyStats.CurrentActive = 2;
        LowEfficiencyStats.HitRate = 0.3f;

        Pattern = FObjectPoolUtils::AnalyzeUsagePattern(LowEfficiencyStats);
        TestTrue(TEXT("低命中率应该被识别为低效使用"), Pattern.Contains(TEXT("低效")));

        // 无数据情况
        FObjectPoolStatsSimplified NoDataStats;
        NoDataStats.TotalCreated = 0;

        Pattern = FObjectPoolUtils::AnalyzeUsagePattern(NoDataStats);
        TestTrue(TEXT("无数据应该返回相应提示"), Pattern.Contains(TEXT("无使用数据")));

        AddInfo(TEXT("✅ 使用模式分析测试通过"));
    }

    // 测试3: GetOptimizationSuggestions
    {
        FObjectPoolConfigSimplified Config;
        Config.ActorClass = AActor::StaticClass();
        Config.InitialSize = 50;
        Config.HardLimit = 100;

        FObjectPoolStatsSimplified Stats;
        Stats.TotalCreated = 20; // 远小于初始大小
        Stats.CurrentActive = 15;
        Stats.CurrentAvailable = 5;
        Stats.HitRate = 0.8f;

        TArray<FString> Suggestions = FObjectPoolUtils::GetOptimizationSuggestions(Config, Stats);

        // 应该建议减少初始大小
        bool bFoundSizeSuggestion = false;
        for (const FString& Suggestion : Suggestions)
        {
            if (Suggestion.Contains(TEXT("初始大小")) && Suggestion.Contains(TEXT("过大")))
            {
                bFoundSizeSuggestion = true;
                break;
            }
        }
        TestTrue(TEXT("应该建议减少过大的初始大小"), bFoundSizeSuggestion);

        AddInfo(TEXT("✅ 优化建议测试通过"));
    }

    return true;
}

/**
 * FObjectPoolUtils 实用工具功能测试
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolUtilsUtilityTest,
    "ObjectPool.Utils.Utility",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolUtilsUtilityTest::RunTest(const FString& Parameters)
{
    // 测试1: IsActorSuitableForPooling
    {
        bool bSuitable = FObjectPoolUtils::IsActorSuitableForPooling(AActor::StaticClass());
        TestTrue(TEXT("普通Actor应该适合池化"), bSuitable);

        bSuitable = FObjectPoolUtils::IsActorSuitableForPooling(ACharacter::StaticClass());
        TestTrue(TEXT("Character应该适合池化"), bSuitable);

        bSuitable = FObjectPoolUtils::IsActorSuitableForPooling(APawn::StaticClass());
        TestTrue(TEXT("Pawn应该适合池化"), bSuitable);

        bSuitable = FObjectPoolUtils::IsActorSuitableForPooling(nullptr);
        TestFalse(TEXT("空Actor类不应该适合池化"), bSuitable);

        AddInfo(TEXT("✅ Actor池化适用性测试通过"));
    }

    // 测试2: GeneratePoolId
    {
        FString PoolId1 = FObjectPoolUtils::GeneratePoolId(AActor::StaticClass());
        FString PoolId2 = FObjectPoolUtils::GeneratePoolId(ACharacter::StaticClass());

        TestTrue(TEXT("池ID应该包含Pool前缀"), PoolId1.StartsWith(TEXT("Pool_")));
        TestTrue(TEXT("池ID应该包含Actor类名"), PoolId1.Contains(TEXT("Actor")));
        TestNotEqual(TEXT("不同Actor类应该生成不同的池ID"), PoolId1, PoolId2);

        FString InvalidPoolId = FObjectPoolUtils::GeneratePoolId(nullptr);
        TestEqual(TEXT("无效Actor类应该返回InvalidPool"), InvalidPoolId, TEXT("InvalidPool"));

        AddInfo(TEXT("✅ 池ID生成测试通过"));
    }

    // 测试3: SafeCallLifecycleInterface（需要实际的Actor实例）
    {
        UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
        if (TestWorld)
        {
            ATestUtilsActor* TestActor = TestWorld->SpawnActor<ATestUtilsActor>();
            if (TestActor)
            {
                TestActor->ResetTestState();

                // 测试安全调用生命周期接口
                FObjectPoolUtils::SafeCallLifecycleInterface(TestActor, TEXT("Created"));
                TestTrue(TEXT("应该成功调用OnPoolActorCreated"), TestActor->bWasCreated);

                FObjectPoolUtils::SafeCallLifecycleInterface(TestActor, TEXT("Activated"));
                TestTrue(TEXT("应该成功调用OnPoolActorActivated"), TestActor->bWasActivated);

                FObjectPoolUtils::SafeCallLifecycleInterface(TestActor, TEXT("ReturnedToPool"));
                TestTrue(TEXT("应该成功调用OnReturnToPool"), TestActor->bWasReset);

                // 测试无效事件类型（应该不会崩溃）
                FObjectPoolUtils::SafeCallLifecycleInterface(TestActor, TEXT("InvalidEvent"));

                // 测试空Actor（应该不会崩溃）
                FObjectPoolUtils::SafeCallLifecycleInterface(nullptr, TEXT("Created"));

                AddInfo(TEXT("✅ 生命周期接口安全调用测试通过"));
            }

            TestWorld->DestroyWorld(false);
        }
    }

    return true;
}

/**
 * FObjectPoolUtils 线程安全性测试
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolUtilsThreadSafetyTest,
    "ObjectPool.Utils.ThreadSafety",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolUtilsThreadSafetyTest::RunTest(const FString& Parameters)
{
    // 测试静态方法的线程安全性
    // 由于所有方法都是静态的且无状态，应该是线程安全的

    // 测试1: 并发配置验证
    {
        const int32 ThreadCount = 4;
        const int32 IterationsPerThread = 100;

        TArray<TFuture<bool>> Futures;

        for (int32 ThreadIndex = 0; ThreadIndex < ThreadCount; ThreadIndex++)
        {
            TFuture<bool> Future = Async(EAsyncExecution::Thread, [IterationsPerThread]()
            {
                for (int32 i = 0; i < IterationsPerThread; i++)
                {
                    FObjectPoolConfigSimplified Config;
                    Config.ActorClass = AActor::StaticClass();
                    Config.InitialSize = 10 + (i % 20);
                    Config.HardLimit = 50 + (i % 50);

                    FString ErrorMessage;
                    bool bResult = FObjectPoolUtils::ValidateConfig(Config, ErrorMessage);
                    if (!bResult)
                    {
                        return false; // 验证失败
                    }
                }
                return true;
            });

            Futures.Add(MoveTemp(Future));
        }

        // 等待所有线程完成
        bool bAllSucceeded = true;
        for (auto& Future : Futures)
        {
            if (!Future.Get())
            {
                bAllSucceeded = false;
                break;
            }
        }

        TestTrue(TEXT("并发配置验证应该全部成功"), bAllSucceeded);
        AddInfo(TEXT("✅ 并发配置验证测试通过"));
    }

    // 测试2: 并发内存估算
    {
        const int32 ThreadCount = 4;
        const int32 IterationsPerThread = 50;

        TArray<TFuture<bool>> Futures;

        for (int32 ThreadIndex = 0; ThreadIndex < ThreadCount; ThreadIndex++)
        {
            TFuture<bool> Future = Async(EAsyncExecution::Thread, [IterationsPerThread]()
            {
                for (int32 i = 0; i < IterationsPerThread; i++)
                {
                    int64 Memory = FObjectPoolUtils::EstimateMemoryUsage(AActor::StaticClass(), 10 + (i % 20));
                    if (Memory <= 0)
                    {
                        return false; // 内存估算失败
                    }
                }
                return true;
            });

            Futures.Add(MoveTemp(Future));
        }

        // 等待所有线程完成
        bool bAllSucceeded = true;
        for (auto& Future : Futures)
        {
            if (!Future.Get())
            {
                bAllSucceeded = false;
                break;
            }
        }

        TestTrue(TEXT("并发内存估算应该全部成功"), bAllSucceeded);
        AddInfo(TEXT("✅ 并发内存估算测试通过"));
    }

    return true;
}

#endif // WITH_OBJECTPOOL_TESTS
