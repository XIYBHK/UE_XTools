// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 条件编译保护 - 只在非Shipping版本中编译测试
#if WITH_OBJECTPOOL_TESTS

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/DateTime.h"

// ✅ 对象池依赖
#include "ObjectPoolLibrary.h"
#include "ObjectPoolSubsystem.h"
#include "ObjectPoolSubsystemSimplified.h"
#include "ObjectPoolMigrationManager.h"
#include "ObjectPoolMigrationConfig.h"

// ✅ 测试环境依赖
#include "Engine/GameInstance.h"
#include "Subsystems/SubsystemCollection.h"

/**
 * 性能测试用的轻量级Actor类
 */
class APerformanceTestActor : public AActor
{
public:
    APerformanceTestActor()
    {
        PrimaryActorTick.bCanEverTick = false;
        bReplicates = false;
        
        // 最小化组件以减少创建开销
        RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    }

    // 简单的测试数据
    int32 TestID = 0;
    float TestValue = 0.0f;
    bool bIsActive = false;

    void InitializeForTest(int32 ID)
    {
        TestID = ID;
        TestValue = FMath::RandRange(0.0f, 100.0f);
        bIsActive = true;
    }

    void ResetForPool()
    {
        TestID = 0;
        TestValue = 0.0f;
        bIsActive = false;
    }
};

/**
 * 性能测试用的复杂Actor类
 */
class AComplexPerformanceTestActor : public ACharacter
{
public:
    AComplexPerformanceTestActor()
    {
        PrimaryActorTick.bCanEverTick = false;
        bReplicates = false;
    }

    // 复杂的测试数据
    TArray<FVector> TestPositions;
    TMap<FString, float> TestProperties;
    FString TestDescription;

    void InitializeComplexData()
    {
        TestPositions.Empty();
        for (int32 i = 0; i < 10; ++i)
        {
            TestPositions.Add(FVector(FMath::RandRange(-1000.0f, 1000.0f), 
                                    FMath::RandRange(-1000.0f, 1000.0f), 
                                    FMath::RandRange(0.0f, 500.0f)));
        }

        TestProperties.Empty();
        TestProperties.Add(TEXT("Speed"), FMath::RandRange(100.0f, 500.0f));
        TestProperties.Add(TEXT("Health"), FMath::RandRange(50.0f, 100.0f));
        TestProperties.Add(TEXT("Damage"), FMath::RandRange(10.0f, 50.0f));

        TestDescription = FString::Printf(TEXT("ComplexActor_%d_%s"), 
            FMath::RandRange(1000, 9999), 
            *FDateTime::Now().ToString());
    }

    void ResetComplexData()
    {
        TestPositions.Empty();
        TestProperties.Empty();
        TestDescription.Empty();
    }
};

/**
 * 性能测试辅助工具
 */
class FPerformanceTestHelpers
{
public:
    /**
     * 高精度计时器
     */
    struct FHighPrecisionTimer
    {
        double StartTime;
        double EndTime;

        FHighPrecisionTimer()
        {
            StartTime = FPlatformTime::Seconds();
            EndTime = 0.0;
        }

        void Stop()
        {
            EndTime = FPlatformTime::Seconds();
        }

        double GetElapsedTime() const
        {
            return (EndTime > 0.0) ? (EndTime - StartTime) : (FPlatformTime::Seconds() - StartTime);
        }

        double GetElapsedTimeMs() const
        {
            return GetElapsedTime() * 1000.0;
        }
    };

    /**
     * 性能测试结果
     */
    struct FPerformanceTestResult
    {
        FString TestName;
        FString Implementation;
        int32 IterationCount;
        double TotalTime;
        double AverageTime;
        double MinTime;
        double MaxTime;
        double MemoryUsageMB;
        int32 SuccessCount;
        int32 FailureCount;

        FPerformanceTestResult()
            : IterationCount(0)
            , TotalTime(0.0)
            , AverageTime(0.0)
            , MinTime(DBL_MAX)
            , MaxTime(0.0)
            , MemoryUsageMB(0.0)
            , SuccessCount(0)
            , FailureCount(0)
        {
        }

        void AddSample(double SampleTime, bool bSuccess = true)
        {
            ++IterationCount;
            TotalTime += SampleTime;
            MinTime = FMath::Min(MinTime, SampleTime);
            MaxTime = FMath::Max(MaxTime, SampleTime);
            
            if (bSuccess)
            {
                ++SuccessCount;
            }
            else
            {
                ++FailureCount;
            }

            AverageTime = TotalTime / IterationCount;
        }

        FString ToString() const
        {
            return FString::Printf(TEXT(
                "%s (%s):\n"
                "  迭代次数: %d\n"
                "  总时间: %.4f ms\n"
                "  平均时间: %.4f ms\n"
                "  最小时间: %.4f ms\n"
                "  最大时间: %.4f ms\n"
                "  内存使用: %.2f MB\n"
                "  成功率: %.1f%% (%d/%d)\n"
            ),
                *TestName, *Implementation,
                IterationCount,
                TotalTime * 1000.0,
                AverageTime * 1000.0,
                MinTime * 1000.0,
                MaxTime * 1000.0,
                MemoryUsageMB,
                IterationCount > 0 ? (float(SuccessCount) / float(IterationCount)) * 100.0f : 0.0f,
                SuccessCount, IterationCount
            );
        }
    };

    /**
     * 获取测试用的World
     */
    static UWorld* GetTestWorld()
    {
        if (GEngine && GEngine->GetWorldContexts().Num() > 0)
        {
            return GEngine->GetWorldContexts()[0].World();
        }
        return nullptr;
    }

    /**
     * 清理测试环境
     */
    static void CleanupTestEnvironment()
    {
        UWorld* World = GetTestWorld();
        if (!World)
        {
            return;
        }

        // 清理简化子系统
        if (UObjectPoolSubsystemSimplified* SimplifiedSubsystem = World->GetSubsystem<UObjectPoolSubsystemSimplified>())
        {
            SimplifiedSubsystem->ClearAllPools();
            SimplifiedSubsystem->ResetSubsystemStats();
        }

        // 清理原始子系统（如果存在）
        if (UGameInstance* GameInstance = World->GetGameInstance())
        {
            if (UObjectPoolSubsystem* OriginalSubsystem = GameInstance->GetSubsystem<UObjectPoolSubsystem>())
            {
                OriginalSubsystem->ClearAllPools();
            }
        }

        // 强制垃圾回收
        CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    }

    /**
     * 获取当前内存使用量（MB）
     */
    static double GetCurrentMemoryUsageMB()
    {
        FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
        return static_cast<double>(MemStats.UsedPhysical) / (1024.0 * 1024.0);
    }

    /**
     * 预热系统（减少首次调用的开销）
     */
    static void WarmupSystem(UClass* ActorClass, int32 WarmupCount = 10)
    {
        UWorld* World = GetTestWorld();
        if (!World || !ActorClass)
        {
            return;
        }

        // 预热简化实现
        FObjectPoolMigrationManager& MigrationManager = FObjectPoolMigrationManager::Get();
        MigrationManager.SwitchToSimplifiedImplementation();

        UObjectPoolLibrary::RegisterActorClass(World, ActorClass, WarmupCount, WarmupCount * 2);
        
        TArray<AActor*> WarmupActors;
        for (int32 i = 0; i < WarmupCount; ++i)
        {
            AActor* Actor = UObjectPoolLibrary::SpawnActorFromPool(World, ActorClass, FTransform::Identity);
            if (Actor)
            {
                WarmupActors.Add(Actor);
            }
        }

        for (AActor* Actor : WarmupActors)
        {
            UObjectPoolLibrary::ReturnActorToPool(World, Actor);
        }

        CleanupTestEnvironment();
    }

    /**
     * 运行性能测试
     */
    static FPerformanceTestResult RunPerformanceTest(
        const FString& TestName,
        const FString& Implementation,
        TFunction<bool()> TestFunction,
        int32 IterationCount = 1000)
    {
        FPerformanceTestResult Result;
        Result.TestName = TestName;
        Result.Implementation = Implementation;

        double InitialMemory = GetCurrentMemoryUsageMB();

        for (int32 i = 0; i < IterationCount; ++i)
        {
            FHighPrecisionTimer Timer;
            bool bSuccess = TestFunction();
            Timer.Stop();

            Result.AddSample(Timer.GetElapsedTime(), bSuccess);
        }

        Result.MemoryUsageMB = GetCurrentMemoryUsageMB() - InitialMemory;

        return Result;
    }

    /**
     * 比较两个性能测试结果
     */
    static FString CompareResults(const FPerformanceTestResult& OriginalResult, const FPerformanceTestResult& SimplifiedResult)
    {
        double TimeImprovement = 0.0;
        if (OriginalResult.AverageTime > 0.0)
        {
            TimeImprovement = ((OriginalResult.AverageTime - SimplifiedResult.AverageTime) / OriginalResult.AverageTime) * 100.0;
        }

        double MemoryImprovement = OriginalResult.MemoryUsageMB - SimplifiedResult.MemoryUsageMB;

        return FString::Printf(TEXT(
            "=== 性能对比: %s ===\n"
            "时间性能:\n"
            "  原始实现: %.4f ms (平均)\n"
            "  简化实现: %.4f ms (平均)\n"
            "  性能提升: %.1f%%\n"
            "\n"
            "内存使用:\n"
            "  原始实现: %.2f MB\n"
            "  简化实现: %.2f MB\n"
            "  内存节省: %.2f MB\n"
            "\n"
            "成功率:\n"
            "  原始实现: %.1f%%\n"
            "  简化实现: %.1f%%\n"
        ),
            *OriginalResult.TestName,
            OriginalResult.AverageTime * 1000.0,
            SimplifiedResult.AverageTime * 1000.0,
            TimeImprovement,
            OriginalResult.MemoryUsageMB,
            SimplifiedResult.MemoryUsageMB,
            MemoryImprovement,
            OriginalResult.IterationCount > 0 ? (float(OriginalResult.SuccessCount) / float(OriginalResult.IterationCount)) * 100.0f : 0.0f,
            SimplifiedResult.IterationCount > 0 ? (float(SimplifiedResult.SuccessCount) / float(SimplifiedResult.IterationCount)) * 100.0f : 0.0f
        );
    }
};

// ✅ 基础性能测试

/**
 * 测试Actor注册性能
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolPerformanceRegistrationTest,
    "ObjectPool.Performance.RegistrationTest",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolPerformanceRegistrationTest::RunTest(const FString& Parameters)
{
    // 获取测试环境
    UWorld* World = FPerformanceTestHelpers::GetTestWorld();
    TestNotNull("测试World应该可用", World);

    if (!World)
    {
        return false;
    }

    // 清理环境
    FPerformanceTestHelpers::CleanupTestEnvironment();

    // 获取迁移管理器
    FObjectPoolMigrationManager& MigrationManager = FObjectPoolMigrationManager::Get();

    const int32 TestIterations = 500;
    const int32 PoolSize = 10;
    const int32 HardLimit = 50;

    // 测试原始实现的注册性能
    FPerformanceTestHelpers::FPerformanceTestResult OriginalResult =
        FPerformanceTestHelpers::RunPerformanceTest(
            TEXT("Actor注册性能"),
            TEXT("原始实现"),
            [&]() -> bool
            {
                MigrationManager.SwitchToOriginalImplementation();
                bool bResult = UObjectPoolLibrary::RegisterActorClass(
                    World,
                    APerformanceTestActor::StaticClass(),
                    PoolSize,
                    HardLimit
                );
                FPerformanceTestHelpers::CleanupTestEnvironment();
                return bResult;
            },
            TestIterations
        );

    // 测试简化实现的注册性能
    FPerformanceTestHelpers::FPerformanceTestResult SimplifiedResult =
        FPerformanceTestHelpers::RunPerformanceTest(
            TEXT("Actor注册性能"),
            TEXT("简化实现"),
            [&]() -> bool
            {
                MigrationManager.SwitchToSimplifiedImplementation();
                bool bResult = UObjectPoolLibrary::RegisterActorClass(
                    World,
                    APerformanceTestActor::StaticClass(),
                    PoolSize,
                    HardLimit
                );
                FPerformanceTestHelpers::CleanupTestEnvironment();
                return bResult;
            },
            TestIterations
        );

    // 记录性能对比结果
    FObjectPoolMigrationManager::FPerformanceComparisonResult ComparisonResult;
    ComparisonResult.OriginalTime = OriginalResult.AverageTime;
    ComparisonResult.SimplifiedTime = SimplifiedResult.AverageTime;
    ComparisonResult.ImprovementPercentage = OriginalResult.AverageTime > 0.0 ?
        ((OriginalResult.AverageTime - SimplifiedResult.AverageTime) / OriginalResult.AverageTime) * 100.0f : 0.0f;
    ComparisonResult.OperationType = TEXT("RegisterActorClass");
    ComparisonResult.TestTime = FPlatformTime::Seconds();

    MigrationManager.RecordPerformanceComparison(ComparisonResult);

    // 输出对比结果
    FString ComparisonReport = FPerformanceTestHelpers::CompareResults(OriginalResult, SimplifiedResult);
    AddInfo(ComparisonReport);

    // 验证性能改善
    TestTrue("简化实现的注册性能应该不差于原始实现",
        SimplifiedResult.AverageTime <= OriginalResult.AverageTime * 1.1); // 允许10%的误差

    // 清理
    FPerformanceTestHelpers::CleanupTestEnvironment();

    return true;
}

/**
 * 测试Actor生成性能
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolPerformanceSpawnTest,
    "ObjectPool.Performance.SpawnTest",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolPerformanceSpawnTest::RunTest(const FString& Parameters)
{
    // 获取测试环境
    UWorld* World = FPerformanceTestHelpers::GetTestWorld();
    TestNotNull("测试World应该可用", World);

    if (!World)
    {
        return false;
    }

    // 预热系统
    FPerformanceTestHelpers::WarmupSystem(APerformanceTestActor::StaticClass());

    // 获取迁移管理器
    FObjectPoolMigrationManager& MigrationManager = FObjectPoolMigrationManager::Get();

    const int32 TestIterations = 1000;
    const int32 PoolSize = 50;
    const int32 HardLimit = 200;

    // 准备测试环境
    auto PrepareTest = [&](bool bUseSimplified)
    {
        FPerformanceTestHelpers::CleanupTestEnvironment();
        if (bUseSimplified)
        {
            MigrationManager.SwitchToSimplifiedImplementation();
        }
        else
        {
            MigrationManager.SwitchToOriginalImplementation();
        }
        UObjectPoolLibrary::RegisterActorClass(World, APerformanceTestActor::StaticClass(), PoolSize, HardLimit);
        UObjectPoolLibrary::PrewarmPool(World, APerformanceTestActor::StaticClass(), PoolSize / 2);
    };

    // 测试原始实现的生成性能
    PrepareTest(false);
    FPerformanceTestHelpers::FPerformanceTestResult OriginalResult =
        FPerformanceTestHelpers::RunPerformanceTest(
            TEXT("Actor生成性能"),
            TEXT("原始实现"),
            [&]() -> bool
            {
                AActor* Actor = UObjectPoolLibrary::SpawnActorFromPool(
                    World,
                    APerformanceTestActor::StaticClass(),
                    FTransform::Identity
                );
                bool bSuccess = IsValid(Actor);
                if (bSuccess)
                {
                    UObjectPoolLibrary::ReturnActorToPool(World, Actor);
                }
                return bSuccess;
            },
            TestIterations
        );

    // 测试简化实现的生成性能
    PrepareTest(true);
    FPerformanceTestHelpers::FPerformanceTestResult SimplifiedResult =
        FPerformanceTestHelpers::RunPerformanceTest(
            TEXT("Actor生成性能"),
            TEXT("简化实现"),
            [&]() -> bool
            {
                AActor* Actor = UObjectPoolLibrary::SpawnActorFromPool(
                    World,
                    APerformanceTestActor::StaticClass(),
                    FTransform::Identity
                );
                bool bSuccess = IsValid(Actor);
                if (bSuccess)
                {
                    UObjectPoolLibrary::ReturnActorToPool(World, Actor);
                }
                return bSuccess;
            },
            TestIterations
        );

    // 记录性能对比结果
    FObjectPoolMigrationManager::FPerformanceComparisonResult ComparisonResult;
    ComparisonResult.OriginalTime = OriginalResult.AverageTime;
    ComparisonResult.SimplifiedTime = SimplifiedResult.AverageTime;
    ComparisonResult.ImprovementPercentage = OriginalResult.AverageTime > 0.0 ?
        ((OriginalResult.AverageTime - SimplifiedResult.AverageTime) / OriginalResult.AverageTime) * 100.0f : 0.0f;
    ComparisonResult.OperationType = TEXT("SpawnActorFromPool");
    ComparisonResult.TestTime = FPlatformTime::Seconds();

    MigrationManager.RecordPerformanceComparison(ComparisonResult);

    // 输出对比结果
    FString ComparisonReport = FPerformanceTestHelpers::CompareResults(OriginalResult, SimplifiedResult);
    AddInfo(ComparisonReport);

    // 验证性能改善
    TestTrue("简化实现的生成性能应该优于原始实现",
        SimplifiedResult.AverageTime <= OriginalResult.AverageTime);

    // 清理
    FPerformanceTestHelpers::CleanupTestEnvironment();

    return true;
}

#endif // WITH_OBJECTPOOL_TESTS
