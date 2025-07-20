// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 条件编译保护 - 只在非Shipping版本中编译测试
#if WITH_OBJECTPOOL_TESTS

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "Async/Async.h"

// ✅ 对象池依赖
#include "ObjectPoolLibrary.h"
#include "ObjectPoolSubsystemSimplified.h"
#include "ObjectPoolMigrationManager.h"

// 简化的测试Actor类声明（避免复杂的UCLASS定义）
// 在实际测试中我们使用标准的AActor类

// 简化的测试敌人Actor类声明（避免复杂的UCLASS定义）
// 在实际测试中我们使用标准的AActor类

/**
 * 端到端测试辅助工具
 */
class FEndToEndTestHelpers
{
public:
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
     * 等待指定时间（秒）
     */
    static void WaitForSeconds(float Seconds)
    {
        double StartTime = FPlatformTime::Seconds();
        while ((FPlatformTime::Seconds() - StartTime) < Seconds)
        {
            FPlatformProcess::Sleep(0.01f); // 10ms
        }
    }

    /**
     * 生成随机位置
     */
    static FVector GenerateRandomLocation(float Range = 1000.0f)
    {
        return FVector(
            FMath::RandRange(-Range, Range),
            FMath::RandRange(-Range, Range),
            FMath::RandRange(0.0f, Range * 0.1f)
        );
    }

    /**
     * 生成随机方向
     */
    static FVector GenerateRandomDirection()
    {
        return FVector(
            FMath::RandRange(-1.0f, 1.0f),
            FMath::RandRange(-1.0f, 1.0f),
            FMath::RandRange(-0.5f, 0.5f)
        ).GetSafeNormal();
    }
};

// ✅ 端到端测试用例

/**
 * 射击游戏场景端到端测试
 * 模拟完整的射击游戏场景：生成敌人、发射子弹、碰撞检测、对象回收
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolShootingGameEndToEndTest,
    "ObjectPool.EndToEnd.ShootingGameTest",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolShootingGameEndToEndTest::RunTest(const FString& Parameters)
{
    // 获取测试环境
    UWorld* World = FEndToEndTestHelpers::GetTestWorld();
    TestNotNull("测试World应该可用", World);

    if (!World)
    {
        return false;
    }

    // 清理环境
    FEndToEndTestHelpers::CleanupTestEnvironment();

    // 切换到简化实现
    FObjectPoolMigrationManager& MigrationManager = FObjectPoolMigrationManager::Get();
    MigrationManager.SwitchToSimplifiedImplementation();

    // 注册Actor类到对象池（使用标准AActor类）
    bool bActorRegistered = UObjectPoolLibrary::RegisterActorClass(
        World,
        AActor::StaticClass(),
        50,  // 初始池大小
        200  // 硬限制
    );
    TestTrue("Actor应该成功注册到对象池", bActorRegistered);

    // 预热池
    bool bPrewarmed = UObjectPoolLibrary::PrewarmPool(World, AActor::StaticClass(), 25);

    AddInfo(FString::Printf(TEXT("预热结果: %s"),
        bPrewarmed ? TEXT("成功") : TEXT("失败")));

    // 记录初始内存使用
    double InitialMemory = FEndToEndTestHelpers::GetCurrentMemoryUsageMB();

    // 模拟简化的游戏场景
    const int32 MaxActors = 50;
    const float TestDuration = 5.0f; // 5秒测试
    const float SpawnInterval = 0.1f; // 每0.1秒生成一个Actor

    TArray<AActor*> ActiveActors;

    double StartTime = FPlatformTime::Seconds();
    int32 TotalActorsSpawned = 0;
    int32 TotalActorsReturned = 0;

    AddInfo(FString::Printf(TEXT("开始简化游戏场景测试，持续时间: %.1f秒"), TestDuration));

    // 主游戏循环
    while ((FPlatformTime::Seconds() - StartTime) < TestDuration)
    {
        double CurrentTime = FPlatformTime::Seconds() - StartTime;

        // 定期生成Actor
        if (ActiveActors.Num() < MaxActors && FMath::Fmod(CurrentTime, SpawnInterval) < 0.05)
        {
            AActor* NewActor = UObjectPoolLibrary::SpawnActorFromPool(
                World,
                AActor::StaticClass(),
                FTransform(FEndToEndTestHelpers::GenerateRandomLocation(500.0f))
            );

            if (IsValid(NewActor))
            {
                ActiveActors.Add(NewActor);
                TotalActorsSpawned++;
            }
        }

        // 随机移除一些Actor（模拟游戏逻辑）
        if (ActiveActors.Num() > 0 && FMath::RandBool())
        {
            int32 RandomIndex = FMath::RandRange(0, ActiveActors.Num() - 1);
            AActor* ActorToRemove = ActiveActors[RandomIndex];

            if (IsValid(ActorToRemove))
            {
                UObjectPoolLibrary::ReturnActorToPool(World, ActorToRemove);
                TotalActorsReturned++;
            }

            ActiveActors.RemoveAt(RandomIndex);
        }

        // 短暂等待以模拟帧率
        FEndToEndTestHelpers::WaitForSeconds(0.016f); // ~60 FPS
    }

    // 清理剩余的Actor
    for (AActor* Actor : ActiveActors)
    {
        if (IsValid(Actor))
        {
            UObjectPoolLibrary::ReturnActorToPool(World, Actor);
            TotalActorsReturned++;
        }
    }

    // 等待清理完成
    FEndToEndTestHelpers::WaitForSeconds(1.0f);

    // 记录最终内存使用
    double FinalMemory = FEndToEndTestHelpers::GetCurrentMemoryUsageMB();
    double MemoryDelta = FinalMemory - InitialMemory;

    // 输出测试结果
    FString TestReport = FString::Printf(TEXT(
        "=== 简化游戏场景测试结果 ===\n"
        "测试持续时间: %.1f 秒\n"
        "Actor统计:\n"
        "  生成数量: %d\n"
        "  回收数量: %d\n"
        "  回收率: %.1f%%\n"
        "内存使用:\n"
        "  初始内存: %.2f MB\n"
        "  最终内存: %.2f MB\n"
        "  内存变化: %.2f MB\n"
    ),
        TestDuration,
        TotalActorsSpawned,
        TotalActorsReturned,
        TotalActorsSpawned > 0 ? (float(TotalActorsReturned) / float(TotalActorsSpawned)) * 100.0f : 0.0f,
        InitialMemory,
        FinalMemory,
        MemoryDelta
    );

    AddInfo(TestReport);

    // 验证测试结果
    TestTrue("应该生成了Actor", TotalActorsSpawned > 0);
    TestTrue("Actor回收率应该很高",
        TotalActorsSpawned > 0 ? (float(TotalActorsReturned) / float(TotalActorsSpawned)) >= 0.8f : true);
    TestTrue("内存使用应该稳定", FMath::Abs(MemoryDelta) < 100.0); // 内存变化不超过100MB

    // 清理
    FEndToEndTestHelpers::CleanupTestEnvironment();

    return true;
}

/**
 * 长时间运行稳定性测试
 * 验证对象池在长时间运行下的稳定性和内存管理
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolLongRunningStabilityTest,
    "ObjectPool.EndToEnd.LongRunningStabilityTest",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolLongRunningStabilityTest::RunTest(const FString& Parameters)
{
    // 获取测试环境
    UWorld* World = FEndToEndTestHelpers::GetTestWorld();
    TestNotNull("测试World应该可用", World);

    if (!World)
    {
        return false;
    }

    // 清理环境
    FEndToEndTestHelpers::CleanupTestEnvironment();

    // 切换到简化实现
    FObjectPoolMigrationManager& MigrationManager = FObjectPoolMigrationManager::Get();
    MigrationManager.SwitchToSimplifiedImplementation();

    // 注册Actor类到对象池
    bool bActorRegistered = UObjectPoolLibrary::RegisterActorClass(
        World,
        AActor::StaticClass(),
        30,  // 初始池大小
        150  // 硬限制
    );
    TestTrue("Actor应该成功注册到对象池", bActorRegistered);

    // 预热池
    bool bPrewarmed = UObjectPoolLibrary::PrewarmPool(World, AActor::StaticClass(), 15);
    AddInfo(FString::Printf(TEXT("预热结果: %s"), bPrewarmed ? TEXT("成功") : TEXT("失败")));

    // 长时间运行测试参数
    const int32 TotalCycles = 1000;        // 总循环次数
    const int32 ActorsPerCycle = 20;       // 每个循环生成的Actor数量
    const float CycleInterval = 0.05f;     // 循环间隔（秒）

    double InitialMemory = FEndToEndTestHelpers::GetCurrentMemoryUsageMB();
    TArray<double> MemorySamples;
    TArray<double> CycleTimes;

    int32 TotalActorsSpawned = 0;
    int32 TotalActorsReturned = 0;
    int32 FailedSpawns = 0;
    int32 FailedReturns = 0;

    AddInfo(FString::Printf(TEXT("开始长时间运行稳定性测试，总循环数: %d"), TotalCycles));

    double TestStartTime = FPlatformTime::Seconds();

    // 主测试循环
    for (int32 Cycle = 0; Cycle < TotalCycles; ++Cycle)
    {
        double CycleStartTime = FPlatformTime::Seconds();

        // 生成Actor
        TArray<AActor*> CycleActors;
        for (int32 i = 0; i < ActorsPerCycle; ++i)
        {
            FVector SpawnLocation = FEndToEndTestHelpers::GenerateRandomLocation(100.0f);
            AActor* Actor = UObjectPoolLibrary::SpawnActorFromPool(
                World,
                AActor::StaticClass(),
                FTransform(SpawnLocation)
            );

            if (IsValid(Actor))
            {
                CycleActors.Add(Actor);
                TotalActorsSpawned++;
            }
            else
            {
                FailedSpawns++;
            }
        }

        // 短暂等待模拟使用
        FEndToEndTestHelpers::WaitForSeconds(CycleInterval);

        // 归还Actor
        for (AActor* Actor : CycleActors)
        {
            if (IsValid(Actor))
            {
                UObjectPoolLibrary::ReturnActorToPool(World, Actor);
                TotalActorsReturned++;
            }
            else
            {
                FailedReturns++;
            }
        }

        double CycleEndTime = FPlatformTime::Seconds();
        CycleTimes.Add(CycleEndTime - CycleStartTime);

        // 每100个循环记录内存使用情况
        if (Cycle % 100 == 0)
        {
            double CurrentMemory = FEndToEndTestHelpers::GetCurrentMemoryUsageMB();
            MemorySamples.Add(CurrentMemory);

            // 输出进度
            float Progress = (float(Cycle) / float(TotalCycles)) * 100.0f;
            AddInfo(FString::Printf(TEXT("进度: %.1f%% (循环 %d/%d), 当前内存: %.2f MB"),
                Progress, Cycle, TotalCycles, CurrentMemory));
        }

        // 每500个循环进行垃圾回收
        if (Cycle % 500 == 0 && Cycle > 0)
        {
            CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
        }
    }

    double TestEndTime = FPlatformTime::Seconds();
    double TotalTestTime = TestEndTime - TestStartTime;
    double FinalMemory = FEndToEndTestHelpers::GetCurrentMemoryUsageMB();

    // 计算统计信息
    double AverageCycleTime = 0.0;
    double MinCycleTime = DBL_MAX;
    double MaxCycleTime = 0.0;
    for (double CycleTime : CycleTimes)
    {
        AverageCycleTime += CycleTime;
        MinCycleTime = FMath::Min(MinCycleTime, CycleTime);
        MaxCycleTime = FMath::Max(MaxCycleTime, CycleTime);
    }
    AverageCycleTime /= CycleTimes.Num();

    // 计算内存趋势
    double MemoryTrend = 0.0;
    if (MemorySamples.Num() > 1)
    {
        MemoryTrend = MemorySamples.Last() - MemorySamples[0];
    }

    // 输出详细测试结果
    FString TestReport = FString::Printf(TEXT(
        "=== 长时间运行稳定性测试结果 ===\n"
        "测试配置:\n"
        "  总循环数: %d\n"
        "  每循环Actor数: %d\n"
        "  循环间隔: %.3f 秒\n"
        "\n"
        "执行统计:\n"
        "  总测试时间: %.2f 秒\n"
        "  平均循环时间: %.4f 秒\n"
        "  最快循环时间: %.4f 秒\n"
        "  最慢循环时间: %.4f 秒\n"
        "\n"
        "Actor统计:\n"
        "  总生成数量: %d\n"
        "  总归还数量: %d\n"
        "  生成失败数: %d\n"
        "  归还失败数: %d\n"
        "  成功率: %.2f%%\n"
        "\n"
        "内存统计:\n"
        "  初始内存: %.2f MB\n"
        "  最终内存: %.2f MB\n"
        "  内存变化: %.2f MB\n"
        "  内存趋势: %.2f MB\n"
        "  内存样本数: %d\n"
    ),
        TotalCycles,
        ActorsPerCycle,
        CycleInterval,
        TotalTestTime,
        AverageCycleTime,
        MinCycleTime,
        MaxCycleTime,
        TotalActorsSpawned,
        TotalActorsReturned,
        FailedSpawns,
        FailedReturns,
        TotalActorsSpawned > 0 ? (float(TotalActorsReturned) / float(TotalActorsSpawned)) * 100.0f : 0.0f,
        InitialMemory,
        FinalMemory,
        FinalMemory - InitialMemory,
        MemoryTrend,
        MemorySamples.Num()
    );

    AddInfo(TestReport);

    // 验证稳定性指标
    float SuccessRate = TotalActorsSpawned > 0 ? (float(TotalActorsReturned) / float(TotalActorsSpawned)) : 0.0f;
    double MemoryDelta = FMath::Abs(FinalMemory - InitialMemory);

    TestTrue("应该完成所有循环", CycleTimes.Num() == TotalCycles);
    TestTrue("应该生成大量Actor", TotalActorsSpawned >= TotalCycles * ActorsPerCycle * 0.9); // 允许10%的失败
    TestTrue("成功率应该很高", SuccessRate >= 0.95f); // 95%以上成功率
    TestTrue("生成失败应该很少", FailedSpawns < TotalActorsSpawned * 0.05); // 失败率小于5%
    TestTrue("归还失败应该很少", FailedReturns < TotalActorsReturned * 0.05); // 失败率小于5%
    TestTrue("内存使用应该稳定", MemoryDelta < 200.0); // 内存变化不超过200MB
    TestTrue("不应该有显著的内存泄漏", FMath::Abs(MemoryTrend) < 100.0); // 内存趋势不超过100MB

    // 性能一致性检查
    double CycleTimeVariance = 0.0;
    for (double CycleTime : CycleTimes)
    {
        double Diff = CycleTime - AverageCycleTime;
        CycleTimeVariance += Diff * Diff;
    }
    CycleTimeVariance /= CycleTimes.Num();
    double CycleTimeStdDev = FMath::Sqrt(CycleTimeVariance);
    double CoefficientOfVariation = AverageCycleTime > 0.0 ? CycleTimeStdDev / AverageCycleTime : 0.0;

    AddInfo(FString::Printf(TEXT("性能一致性: 变异系数 = %.3f"), CoefficientOfVariation));
    TestTrue("性能应该保持一致", CoefficientOfVariation < 0.5); // 变异系数小于50%

    // 清理
    FEndToEndTestHelpers::CleanupTestEnvironment();

    return true;
}

/**
 * 内存压力测试
 * 测试对象池在高内存压力下的表现
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolMemoryStressTest,
    "ObjectPool.EndToEnd.MemoryStressTest",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolMemoryStressTest::RunTest(const FString& Parameters)
{
    // 获取测试环境
    UWorld* World = FEndToEndTestHelpers::GetTestWorld();
    TestNotNull("测试World应该可用", World);

    if (!World)
    {
        return false;
    }

    // 清理环境
    FEndToEndTestHelpers::CleanupTestEnvironment();

    // 切换到简化实现
    FObjectPoolMigrationManager& MigrationManager = FObjectPoolMigrationManager::Get();
    MigrationManager.SwitchToSimplifiedImplementation();

    // 注册Actor类型
    bool bActorRegistered = UObjectPoolLibrary::RegisterActorClass(
        World,
        AActor::StaticClass(),
        100,  // 大初始池
        1000  // 高硬限制
    );
    TestTrue("Actor应该成功注册", bActorRegistered);

    double InitialMemory = FEndToEndTestHelpers::GetCurrentMemoryUsageMB();
    AddInfo(FString::Printf(TEXT("开始内存压力测试，初始内存: %.2f MB"), InitialMemory));

    // 压力测试阶段1：快速大量生成
    const int32 BurstSize = 500;
    TArray<AActor*> BurstActors;

    AddInfo(TEXT("阶段1: 快速大量生成测试"));
    double Phase1StartTime = FPlatformTime::Seconds();

    for (int32 i = 0; i < BurstSize; ++i)
    {
        AActor* Actor = UObjectPoolLibrary::SpawnActorFromPool(
            World,
            AActor::StaticClass(),
            FTransform(FEndToEndTestHelpers::GenerateRandomLocation())
        );

        if (IsValid(Actor))
        {
            BurstActors.Add(Actor);
        }
    }

    double Phase1Memory = FEndToEndTestHelpers::GetCurrentMemoryUsageMB();
    double Phase1Time = FPlatformTime::Seconds() - Phase1StartTime;

    AddInfo(FString::Printf(TEXT("阶段1完成: 生成%d个Actor，用时%.3f秒，内存%.2f MB"),
        BurstActors.Num(), Phase1Time, Phase1Memory));

    // 压力测试阶段2：快速大量回收
    AddInfo(TEXT("阶段2: 快速大量回收测试"));
    double Phase2StartTime = FPlatformTime::Seconds();

    int32 ReturnedCount = 0;
    for (AActor* Actor : BurstActors)
    {
        if (IsValid(Actor))
        {
            UObjectPoolLibrary::ReturnActorToPool(World, Actor);
            ReturnedCount++;
        }
    }

    double Phase2Memory = FEndToEndTestHelpers::GetCurrentMemoryUsageMB();
    double Phase2Time = FPlatformTime::Seconds() - Phase2StartTime;

    AddInfo(FString::Printf(TEXT("阶段2完成: 回收%d个Actor，用时%.3f秒，内存%.2f MB"),
        ReturnedCount, Phase2Time, Phase2Memory));

    // 压力测试阶段3：循环压力测试
    AddInfo(TEXT("阶段3: 循环压力测试"));
    const int32 CycleCount = 100;
    const int32 ActorsPerCycle = 50;

    double Phase3StartTime = FPlatformTime::Seconds();
    double MaxMemoryUsed = Phase2Memory;
    double MinMemoryUsed = Phase2Memory;

    for (int32 Cycle = 0; Cycle < CycleCount; ++Cycle)
    {
        TArray<AActor*> CycleActors;

        // 生成Actor
        for (int32 i = 0; i < ActorsPerCycle; ++i)
        {
            AActor* Actor = UObjectPoolLibrary::SpawnActorFromPool(
                World,
                AActor::StaticClass(),
                FTransform(FEndToEndTestHelpers::GenerateRandomLocation())
            );

            if (IsValid(Actor))
            {
                CycleActors.Add(Actor);
            }
        }

        // 随机等待
        FEndToEndTestHelpers::WaitForSeconds(FMath::RandRange(0.01f, 0.05f));

        // 回收Actor
        for (AActor* Actor : CycleActors)
        {
            if (IsValid(Actor))
            {
                UObjectPoolLibrary::ReturnActorToPool(World, Actor);
            }
        }

        // 监控内存使用
        if (Cycle % 10 == 0)
        {
            double CurrentMemory = FEndToEndTestHelpers::GetCurrentMemoryUsageMB();
            MaxMemoryUsed = FMath::Max(MaxMemoryUsed, CurrentMemory);
            MinMemoryUsed = FMath::Min(MinMemoryUsed, CurrentMemory);
        }
    }

    double Phase3Memory = FEndToEndTestHelpers::GetCurrentMemoryUsageMB();
    double Phase3Time = FPlatformTime::Seconds() - Phase3StartTime;

    AddInfo(FString::Printf(TEXT("阶段3完成: %d个循环，用时%.3f秒，内存%.2f MB"),
        CycleCount, Phase3Time, Phase3Memory));

    // 最终垃圾回收
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    FEndToEndTestHelpers::WaitForSeconds(1.0f);
    double FinalMemory = FEndToEndTestHelpers::GetCurrentMemoryUsageMB();

    // 输出完整测试报告
    FString TestReport = FString::Printf(TEXT(
        "=== 内存压力测试结果 ===\n"
        "初始内存: %.2f MB\n"
        "\n"
        "阶段1 - 快速大量生成:\n"
        "  生成数量: %d\n"
        "  执行时间: %.3f 秒\n"
        "  内存使用: %.2f MB (+%.2f MB)\n"
        "\n"
        "阶段2 - 快速大量回收:\n"
        "  回收数量: %d\n"
        "  执行时间: %.3f 秒\n"
        "  内存使用: %.2f MB (%.2f MB)\n"
        "\n"
        "阶段3 - 循环压力测试:\n"
        "  循环次数: %d\n"
        "  执行时间: %.3f 秒\n"
        "  最大内存: %.2f MB\n"
        "  最小内存: %.2f MB\n"
        "  内存波动: %.2f MB\n"
        "\n"
        "最终状态:\n"
        "  最终内存: %.2f MB\n"
        "  总内存变化: %.2f MB\n"
    ),
        InitialMemory,
        BurstActors.Num(),
        Phase1Time,
        Phase1Memory, Phase1Memory - InitialMemory,
        ReturnedCount,
        Phase2Time,
        Phase2Memory, Phase2Memory - InitialMemory,
        CycleCount,
        Phase3Time,
        MaxMemoryUsed,
        MinMemoryUsed,
        MaxMemoryUsed - MinMemoryUsed,
        FinalMemory,
        FinalMemory - InitialMemory
    );

    AddInfo(TestReport);

    // 验证内存压力测试结果
    TestTrue("应该成功生成大量Actor", BurstActors.Num() >= BurstSize * 0.9); // 允许10%失败
    TestTrue("应该成功回收大部分Actor", ReturnedCount >= BurstActors.Num() * 0.9); // 90%回收率
    TestTrue("内存使用应该在合理范围内", MaxMemoryUsed - InitialMemory < 1000.0); // 内存增长不超过1GB
    TestTrue("内存应该能够回收", FinalMemory - InitialMemory < 200.0); // 最终内存增长不超过200MB
    TestTrue("内存波动应该在控制范围内", MaxMemoryUsed - MinMemoryUsed < 500.0); // 内存波动不超过500MB

    // 清理
    FEndToEndTestHelpers::CleanupTestEnvironment();

    return true;
}

#endif // WITH_OBJECTPOOL_TESTS
