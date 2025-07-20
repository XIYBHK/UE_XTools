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
 * 迁移测试用的简单Actor类
 */
class AMigrationTestActor : public AActor
{
public:
    AMigrationTestActor()
    {
        PrimaryActorTick.bCanEverTick = false;
        bReplicates = false;
    }

    // 测试属性
    int32 TestValue = 0;
    FString TestString = TEXT("Default");
    bool bTestFlag = false;
    TArray<int32> TestArray;

    void InitializeTestData()
    {
        TestValue = 42;
        TestString = TEXT("Initialized");
        bTestFlag = true;
        TestArray = {1, 2, 3, 4, 5};
    }

    void ResetTestData()
    {
        TestValue = 0;
        TestString = TEXT("Reset");
        bTestFlag = false;
        TestArray.Empty();
    }

    bool ValidateTestData() const
    {
        return TestValue == 42 && 
               TestString == TEXT("Initialized") && 
               bTestFlag && 
               TestArray.Num() == 5;
    }
};

/**
 * 迁移验证测试辅助工具
 */
class FMigrationTestHelpers
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

        // 清理原始子系统（如果存在）
        if (UGameInstance* GameInstance = World->GetGameInstance())
        {
            if (UObjectPoolSubsystem* OriginalSubsystem = GameInstance->GetSubsystem<UObjectPoolSubsystem>())
            {
                OriginalSubsystem->ClearAllPools();
            }
        }
    }

    /**
     * 比较两个Actor的状态
     */
    static bool CompareActorStates(AActor* Actor1, AActor* Actor2)
    {
        if (!IsValid(Actor1) || !IsValid(Actor2))
        {
            return false;
        }

        // 比较基本属性
        if (Actor1->GetClass() != Actor2->GetClass())
        {
            return false;
        }

        // 比较位置
        FVector Location1 = Actor1->GetActorLocation();
        FVector Location2 = Actor2->GetActorLocation();
        if (!Location1.Equals(Location2, 0.1f))
        {
            return false;
        }

        // 比较旋转
        FRotator Rotation1 = Actor1->GetActorRotation();
        FRotator Rotation2 = Actor2->GetActorRotation();
        if (!Rotation1.Equals(Rotation2, 0.1f))
        {
            return false;
        }

        // 如果是测试Actor，比较测试数据
        if (AMigrationTestActor* TestActor1 = Cast<AMigrationTestActor>(Actor1))
        {
            if (AMigrationTestActor* TestActor2 = Cast<AMigrationTestActor>(Actor2))
            {
                return TestActor1->TestValue == TestActor2->TestValue &&
                       TestActor1->TestString == TestActor2->TestString &&
                       TestActor1->bTestFlag == TestActor2->bTestFlag &&
                       TestActor1->TestArray == TestActor2->TestArray;
            }
        }

        return true;
    }

    /**
     * 测试性能差异
     */
    static double MeasureOperationTime(TFunction<void()> Operation)
    {
        double StartTime = FPlatformTime::Seconds();
        Operation();
        double EndTime = FPlatformTime::Seconds();
        return EndTime - StartTime;
    }

    /**
     * 比较性能结果
     */
    static bool ComparePerformance(double OriginalTime, double SimplifiedTime, float TolerancePercentage = 10.0f)
    {
        if (OriginalTime <= 0.0 || SimplifiedTime <= 0.0)
        {
            return false;
        }

        float DifferencePercentage = FMath::Abs((float)((SimplifiedTime - OriginalTime) / OriginalTime)) * 100.0f;
        return DifferencePercentage <= TolerancePercentage;
    }

    /**
     * 生成测试Transform数组
     */
    static TArray<FTransform> GenerateTestTransforms(int32 Count)
    {
        TArray<FTransform> Transforms;
        for (int32 i = 0; i < Count; ++i)
        {
            FTransform Transform = FTransform::Identity;
            Transform.SetLocation(FVector(i * 100.0f, 0.0f, 0.0f));
            Transform.SetRotation(FQuat::MakeFromEuler(FVector(0.0f, i * 10.0f, 0.0f)));
            Transforms.Add(Transform);
        }
        return Transforms;
    }
};

/**
 * 测试新旧实现的基本行为一致性
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolMigrationBasicConsistencyTest, 
    "ObjectPool.Migration.BasicConsistencyTest", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolMigrationBasicConsistencyTest::RunTest(const FString& Parameters)
{
    // 获取测试环境
    UWorld* World = FMigrationTestHelpers::GetTestWorld();
    TestNotNull("测试World应该可用", World);

    if (!World)
    {
        return false;
    }

    // 清理环境
    FMigrationTestHelpers::CleanupTestEnvironment();

    // 获取迁移管理器
    FObjectPoolMigrationManager& MigrationManager = FObjectPoolMigrationManager::Get();

    // 1. 测试注册行为一致性
    bool bOriginalRegistered = false;
    bool bSimplifiedRegistered = false;

    // 切换到原始实现并测试
    if (MigrationManager.SwitchToOriginalImplementation())
    {
        bOriginalRegistered = UObjectPoolLibrary::RegisterActorClass(
            World, 
            AMigrationTestActor::StaticClass(), 
            5, 
            20
        );
    }

    // 切换到简化实现并测试
    if (MigrationManager.SwitchToSimplifiedImplementation())
    {
        bSimplifiedRegistered = UObjectPoolLibrary::RegisterActorClass(
            World, 
            AMigrationTestActor::StaticClass(), 
            5, 
            20
        );
    }

    TestEqual("注册行为应该一致", bOriginalRegistered, bSimplifiedRegistered);

    // 2. 测试生成行为一致性
    AActor* OriginalActor = nullptr;
    AActor* SimplifiedActor = nullptr;

    FTransform TestTransform = FTransform::Identity;
    TestTransform.SetLocation(FVector(100.0f, 200.0f, 300.0f));

    // 从原始实现生成
    MigrationManager.SwitchToOriginalImplementation();
    OriginalActor = UObjectPoolLibrary::SpawnActorFromPool(
        World, 
        AMigrationTestActor::StaticClass(), 
        TestTransform
    );

    // 从简化实现生成
    MigrationManager.SwitchToSimplifiedImplementation();
    SimplifiedActor = UObjectPoolLibrary::SpawnActorFromPool(
        World, 
        AMigrationTestActor::StaticClass(), 
        TestTransform
    );

    TestNotNull("原始实现应该能生成Actor", OriginalActor);
    TestNotNull("简化实现应该能生成Actor", SimplifiedActor);

    if (OriginalActor && SimplifiedActor)
    {
        TestTrue("生成的Actor类型应该一致", OriginalActor->GetClass() == SimplifiedActor->GetClass());
        TestTrue("生成的Actor位置应该一致", 
            OriginalActor->GetActorLocation().Equals(SimplifiedActor->GetActorLocation(), 0.1f));
    }

    // 3. 测试归还行为一致性
    if (OriginalActor)
    {
        MigrationManager.SwitchToOriginalImplementation();
        UObjectPoolLibrary::ReturnActorToPool(World, OriginalActor);
    }

    if (SimplifiedActor)
    {
        MigrationManager.SwitchToSimplifiedImplementation();
        UObjectPoolLibrary::ReturnActorToPool(World, SimplifiedActor);
    }

    // 清理
    FMigrationTestHelpers::CleanupTestEnvironment();

    return true;
}

/**
 * 测试性能特征的一致性
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolMigrationPerformanceConsistencyTest,
    "ObjectPool.Migration.PerformanceConsistencyTest",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolMigrationPerformanceConsistencyTest::RunTest(const FString& Parameters)
{
    // 获取测试环境
    UWorld* World = FMigrationTestHelpers::GetTestWorld();
    TestNotNull("测试World应该可用", World);

    if (!World)
    {
        return false;
    }

    // 清理环境
    FMigrationTestHelpers::CleanupTestEnvironment();

    // 获取迁移管理器
    FObjectPoolMigrationManager& MigrationManager = FObjectPoolMigrationManager::Get();

    // 测试参数
    const int32 TestIterations = 100;
    const int32 BatchSize = 10;

    // 1. 测试注册性能
    double OriginalRegisterTime = 0.0;
    double SimplifiedRegisterTime = 0.0;

    // 原始实现注册性能
    MigrationManager.SwitchToOriginalImplementation();
    OriginalRegisterTime = FMigrationTestHelpers::MeasureOperationTime([&]()
    {
        for (int32 i = 0; i < TestIterations; ++i)
        {
            UObjectPoolLibrary::RegisterActorClass(
                World,
                AMigrationTestActor::StaticClass(),
                5,
                20
            );
        }
    });

    // 简化实现注册性能
    MigrationManager.SwitchToSimplifiedImplementation();
    SimplifiedRegisterTime = FMigrationTestHelpers::MeasureOperationTime([&]()
    {
        for (int32 i = 0; i < TestIterations; ++i)
        {
            UObjectPoolLibrary::RegisterActorClass(
                World,
                AMigrationTestActor::StaticClass(),
                5,
                20
            );
        }
    });

    TestTrue("注册性能应该在合理范围内",
        FMigrationTestHelpers::ComparePerformance(OriginalRegisterTime, SimplifiedRegisterTime, 50.0f));

    // 2. 测试生成性能
    double OriginalSpawnTime = 0.0;
    double SimplifiedSpawnTime = 0.0;

    // 原始实现生成性能
    MigrationManager.SwitchToOriginalImplementation();
    UObjectPoolLibrary::RegisterActorClass(World, AMigrationTestActor::StaticClass(), 50, 200);

    OriginalSpawnTime = FMigrationTestHelpers::MeasureOperationTime([&]()
    {
        TArray<AActor*> Actors;
        for (int32 i = 0; i < TestIterations; ++i)
        {
            AActor* Actor = UObjectPoolLibrary::SpawnActorFromPool(
                World,
                AMigrationTestActor::StaticClass(),
                FTransform::Identity
            );
            if (Actor)
            {
                Actors.Add(Actor);
            }
        }

        // 归还所有Actor
        for (AActor* Actor : Actors)
        {
            UObjectPoolLibrary::ReturnActorToPool(World, Actor);
        }
    });

    // 简化实现生成性能
    MigrationManager.SwitchToSimplifiedImplementation();
    UObjectPoolLibrary::RegisterActorClass(World, AMigrationTestActor::StaticClass(), 50, 200);

    SimplifiedSpawnTime = FMigrationTestHelpers::MeasureOperationTime([&]()
    {
        TArray<AActor*> Actors;
        for (int32 i = 0; i < TestIterations; ++i)
        {
            AActor* Actor = UObjectPoolLibrary::SpawnActorFromPool(
                World,
                AMigrationTestActor::StaticClass(),
                FTransform::Identity
            );
            if (Actor)
            {
                Actors.Add(Actor);
            }
        }

        // 归还所有Actor
        for (AActor* Actor : Actors)
        {
            UObjectPoolLibrary::ReturnActorToPool(World, Actor);
        }
    });

    // 验证性能特征
    TestTrue("生成性能应该在合理范围内",
        FMigrationTestHelpers::ComparePerformance(OriginalSpawnTime, SimplifiedSpawnTime, 30.0f));

    // 记录性能对比结果
    if (SimplifiedSpawnTime > 0.0 && OriginalSpawnTime > 0.0)
    {
        float ImprovementPercentage = ((OriginalSpawnTime - SimplifiedSpawnTime) / OriginalSpawnTime) * 100.0f;

        FObjectPoolMigrationManager::FPerformanceComparisonResult Result;
        Result.OriginalTime = OriginalSpawnTime;
        Result.SimplifiedTime = SimplifiedSpawnTime;
        Result.ImprovementPercentage = ImprovementPercentage;
        Result.OperationType = TEXT("SpawnActorFromPool");
        Result.TestTime = FPlatformTime::Seconds();

        MigrationManager.RecordPerformanceComparison(Result);

        AddInfo(FString::Printf(TEXT("生成性能对比 - 原始: %.4fms, 简化: %.4fms, 提升: %.1f%%"),
            OriginalSpawnTime * 1000.0, SimplifiedSpawnTime * 1000.0, ImprovementPercentage));
    }

    // 3. 测试批量操作性能
    double OriginalBatchTime = 0.0;
    double SimplifiedBatchTime = 0.0;

    TArray<FTransform> BatchTransforms = FMigrationTestHelpers::GenerateTestTransforms(BatchSize);

    // 原始实现批量性能
    MigrationManager.SwitchToOriginalImplementation();
    OriginalBatchTime = FMigrationTestHelpers::MeasureOperationTime([&]()
    {
        for (int32 i = 0; i < TestIterations / 10; ++i) // 减少迭代次数，因为批量操作更耗时
        {
            TArray<AActor*> OutActors;
            UObjectPoolLibrary::BatchSpawnActors(
                World,
                AMigrationTestActor::StaticClass(),
                BatchTransforms,
                OutActors
            );

            UObjectPoolLibrary::BatchReturnActors(World, OutActors);
        }
    });

    // 简化实现批量性能
    MigrationManager.SwitchToSimplifiedImplementation();
    SimplifiedBatchTime = FMigrationTestHelpers::MeasureOperationTime([&]()
    {
        for (int32 i = 0; i < TestIterations / 10; ++i)
        {
            TArray<AActor*> OutActors;
            UObjectPoolLibrary::BatchSpawnActors(
                World,
                AMigrationTestActor::StaticClass(),
                BatchTransforms,
                OutActors
            );

            UObjectPoolLibrary::BatchReturnActors(World, OutActors);
        }
    });

    TestTrue("批量操作性能应该在合理范围内",
        FMigrationTestHelpers::ComparePerformance(OriginalBatchTime, SimplifiedBatchTime, 40.0f));

    // 清理
    FMigrationTestHelpers::CleanupTestEnvironment();

    return true;
}

/**
 * 测试A/B测试功能
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolMigrationABTestingTest,
    "ObjectPool.Migration.ABTestingTest",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolMigrationABTestingTest::RunTest(const FString& Parameters)
{
    // 获取测试环境
    UWorld* World = FMigrationTestHelpers::GetTestWorld();
    TestNotNull("测试World应该可用", World);

    if (!World)
    {
        return false;
    }

    // 清理环境
    FMigrationTestHelpers::CleanupTestEnvironment();

    // 获取迁移管理器
    FObjectPoolMigrationManager& MigrationManager = FObjectPoolMigrationManager::Get();

    // 1. 测试A/B测试启用/禁用
    TestFalse("A/B测试初始应该是禁用的", MigrationManager.IsABTestingEnabled());

    MigrationManager.EnableABTesting(0.5f);
    TestTrue("应该能够启用A/B测试", MigrationManager.IsABTestingEnabled());

    MigrationManager.DisableABTesting();
    TestFalse("应该能够禁用A/B测试", MigrationManager.IsABTestingEnabled());

    // 2. 测试A/B测试比例
    MigrationManager.EnableABTesting(0.3f); // 30%使用简化实现

    // 注册Actor类
    UObjectPoolLibrary::RegisterActorClass(World, AMigrationTestActor::StaticClass(), 20, 100);

    // 进行多次测试以验证比例
    const int32 TestCount = 100;
    int32 SimplifiedCount = 0;
    int32 OriginalCount = 0;

    for (int32 i = 0; i < TestCount; ++i)
    {
        // 检查当前使用的实现
        if (UObjectPoolLibrary::IsUsingSimplifiedImplementation())
        {
            ++SimplifiedCount;
        }
        else
        {
            ++OriginalCount;
        }

        // 生成一个Actor来触发实现选择
        AActor* Actor = UObjectPoolLibrary::SpawnActorFromPool(
            World,
            AMigrationTestActor::StaticClass(),
            FTransform::Identity
        );

        if (Actor)
        {
            UObjectPoolLibrary::ReturnActorToPool(World, Actor);
        }
    }

    // 验证比例（允许一定的误差）
    float SimplifiedRatio = static_cast<float>(SimplifiedCount) / static_cast<float>(TestCount);
    TestTrue("A/B测试比例应该接近设定值", FMath::Abs(SimplifiedRatio - 0.3f) < 0.2f); // 允许20%的误差

    AddInfo(FString::Printf(TEXT("A/B测试结果 - 简化实现: %d/%d (%.1f%%), 原始实现: %d/%d (%.1f%%)"),
        SimplifiedCount, TestCount, SimplifiedRatio * 100.0f,
        OriginalCount, TestCount, (1.0f - SimplifiedRatio) * 100.0f));

    // 3. 测试迁移状态管理
    MigrationManager.StartMigration();
    TestTrue("应该能够开始迁移", MigrationManager.IsMigrationInProgress());

    MigrationManager.CompleteMigration();
    TestFalse("完成迁移后应该不再是进行中状态", MigrationManager.IsMigrationInProgress());

    // 4. 测试统计信息收集
    FObjectPoolMigrationManager::FMigrationStats Stats = MigrationManager.GetMigrationStats();
    TestTrue("应该收集到统计信息", Stats.OriginalImplementationCalls > 0 || Stats.SimplifiedImplementationCalls > 0);

    // 清理
    MigrationManager.DisableABTesting();
    FMigrationTestHelpers::CleanupTestEnvironment();

    return true;
}

/**
 * 测试迁移管理器的完整功能
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolMigrationManagerTest,
    "ObjectPool.Migration.MigrationManagerTest",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolMigrationManagerTest::RunTest(const FString& Parameters)
{
    // 获取迁移管理器
    FObjectPoolMigrationManager& MigrationManager = FObjectPoolMigrationManager::Get();

    // 1. 测试实现类型切换
    auto OriginalType = MigrationManager.GetCurrentImplementationType();

    bool bSwitchSuccess = MigrationManager.SwitchToSimplifiedImplementation();
    TestTrue("应该能够切换到简化实现", bSwitchSuccess);
    TestEqual("当前实现类型应该是简化实现",
        MigrationManager.GetCurrentImplementationType(),
        FObjectPoolMigrationManager::EImplementationType::Simplified);

    bSwitchSuccess = MigrationManager.SwitchToOriginalImplementation();
    TestTrue("应该能够切换到原始实现", bSwitchSuccess);
    TestEqual("当前实现类型应该是原始实现",
        MigrationManager.GetCurrentImplementationType(),
        FObjectPoolMigrationManager::EImplementationType::Original);

    bSwitchSuccess = MigrationManager.ToggleImplementation();
    TestTrue("应该能够切换实现类型", bSwitchSuccess);
    TestNotEqual("切换后实现类型应该不同",
        MigrationManager.GetCurrentImplementationType(),
        FObjectPoolMigrationManager::EImplementationType::Original);

    // 2. 测试迁移状态管理
    TestEqual("初始迁移状态应该是未开始",
        MigrationManager.GetMigrationState(),
        FObjectPoolMigrationManager::EMigrationState::NotStarted);

    MigrationManager.StartMigration();
    TestEqual("开始迁移后状态应该是进行中",
        MigrationManager.GetMigrationState(),
        FObjectPoolMigrationManager::EMigrationState::InProgress);

    MigrationManager.CompleteMigration();
    TestEqual("完成迁移后状态应该是已完成",
        MigrationManager.GetMigrationState(),
        FObjectPoolMigrationManager::EMigrationState::Completed);

    // 测试回滚
    MigrationManager.RollbackMigration();
    TestEqual("回滚后状态应该是已回滚",
        MigrationManager.GetMigrationState(),
        FObjectPoolMigrationManager::EMigrationState::RolledBack);
    TestEqual("回滚后应该使用原始实现",
        MigrationManager.GetCurrentImplementationType(),
        FObjectPoolMigrationManager::EImplementationType::Original);

    // 3. 测试统计信息
    MigrationManager.ResetStats();
    FObjectPoolMigrationManager::FMigrationStats InitialStats = MigrationManager.GetMigrationStats();
    TestEqual("重置后原始实现调用次数应该为0", InitialStats.OriginalImplementationCalls, 0);
    TestEqual("重置后简化实现调用次数应该为0", InitialStats.SimplifiedImplementationCalls, 0);

    // 记录一些调用
    MigrationManager.RecordImplementationCall(FObjectPoolMigrationManager::EImplementationType::Original);
    MigrationManager.RecordImplementationCall(FObjectPoolMigrationManager::EImplementationType::Simplified);
    MigrationManager.RecordCompatibilityCheck(true);
    MigrationManager.RecordCompatibilityCheck(false);

    FObjectPoolMigrationManager::FMigrationStats UpdatedStats = MigrationManager.GetMigrationStats();
    TestEqual("原始实现调用次数应该增加", UpdatedStats.OriginalImplementationCalls, 1);
    TestEqual("简化实现调用次数应该增加", UpdatedStats.SimplifiedImplementationCalls, 1);
    TestEqual("兼容性检查通过次数应该为1", UpdatedStats.CompatibilityChecksPassed, 1);
    TestEqual("兼容性检查失败次数应该为1", UpdatedStats.CompatibilityChecksFailed, 1);

    // 4. 测试配置验证
    TestTrue("配置应该是有效的", MigrationManager.IsConfigurationValid());

    // 5. 测试报告生成
    FString ConfigSummary = MigrationManager.GetConfigurationSummary();
    TestTrue("配置摘要应该不为空", !ConfigSummary.IsEmpty());
    TestTrue("配置摘要应该包含关键信息", ConfigSummary.Contains(TEXT("迁移配置摘要")));

    FString MigrationReport = MigrationManager.GenerateMigrationReport();
    TestTrue("迁移报告应该不为空", !MigrationReport.IsEmpty());
    TestTrue("迁移报告应该包含统计信息", MigrationReport.Contains(TEXT("使用统计")));

    // 6. 测试性能对比记录
    FObjectPoolMigrationManager::FPerformanceComparisonResult PerfResult;
    PerfResult.OriginalTime = 0.01;
    PerfResult.SimplifiedTime = 0.008;
    PerfResult.ImprovementPercentage = 20.0f;
    PerfResult.OperationType = TEXT("TestOperation");
    PerfResult.TestTime = FPlatformTime::Seconds();

    MigrationManager.RecordPerformanceComparison(PerfResult);

    FString PerformanceReport = MigrationManager.GeneratePerformanceReport();
    TestTrue("性能报告应该不为空", !PerformanceReport.IsEmpty());
    TestTrue("性能报告应该包含测试操作", PerformanceReport.Contains(TEXT("TestOperation")));

    return true;
}

/**
 * 测试实现一致性验证
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolMigrationConsistencyValidationTest,
    "ObjectPool.Migration.ConsistencyValidationTest",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolMigrationConsistencyValidationTest::RunTest(const FString& Parameters)
{
    // 获取测试环境
    UWorld* World = FMigrationTestHelpers::GetTestWorld();
    TestNotNull("测试World应该可用", World);

    if (!World)
    {
        return false;
    }

    // 清理环境
    FMigrationTestHelpers::CleanupTestEnvironment();

    // 获取迁移管理器
    FObjectPoolMigrationManager& MigrationManager = FObjectPoolMigrationManager::Get();

    // 1. 测试实现一致性验证
    bool bValidationResult = MigrationManager.ValidateImplementationConsistency(
        AMigrationTestActor::StaticClass(),
        10
    );
    TestTrue("实现一致性验证应该通过", bValidationResult);

    // 2. 测试无效类的处理
    bool bInvalidValidation = MigrationManager.ValidateImplementationConsistency(nullptr, 5);
    TestFalse("无效类的验证应该失败", bInvalidValidation);

    // 3. 测试大量验证
    bool bLargeValidation = MigrationManager.ValidateImplementationConsistency(
        AMigrationTestActor::StaticClass(),
        100
    );
    TestTrue("大量验证应该能够处理", bLargeValidation);

    // 4. 验证统计信息更新
    FObjectPoolMigrationManager::FMigrationStats Stats = MigrationManager.GetMigrationStats();
    TestTrue("验证后应该有兼容性检查记录",
        Stats.CompatibilityChecksPassed > 0 || Stats.CompatibilityChecksFailed > 0);

    // 清理
    FMigrationTestHelpers::CleanupTestEnvironment();

    return true;
}

/**
 * 测试边界条件和错误处理
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolMigrationEdgeCasesTest,
    "ObjectPool.Migration.EdgeCasesTest",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolMigrationEdgeCasesTest::RunTest(const FString& Parameters)
{
    // 获取测试环境
    UWorld* World = FMigrationTestHelpers::GetTestWorld();
    TestNotNull("测试World应该可用", World);

    if (!World)
    {
        return false;
    }

    // 清理环境
    FMigrationTestHelpers::CleanupTestEnvironment();

    // 获取迁移管理器
    FObjectPoolMigrationManager& MigrationManager = FObjectPoolMigrationManager::Get();

    // 1. 测试重复切换
    auto InitialType = MigrationManager.GetCurrentImplementationType();

    for (int32 i = 0; i < 10; ++i)
    {
        MigrationManager.ToggleImplementation();
    }

    // 应该能够正常处理重复切换
    TestTrue("重复切换后管理器应该仍然有效", MigrationManager.IsConfigurationValid());

    // 2. 测试重复迁移操作
    MigrationManager.StartMigration();
    MigrationManager.StartMigration(); // 重复开始
    TestEqual("重复开始迁移应该保持进行中状态",
        MigrationManager.GetMigrationState(),
        FObjectPoolMigrationManager::EMigrationState::InProgress);

    MigrationManager.CompleteMigration();
    MigrationManager.CompleteMigration(); // 重复完成
    TestEqual("重复完成迁移应该保持完成状态",
        MigrationManager.GetMigrationState(),
        FObjectPoolMigrationManager::EMigrationState::Completed);

    // 3. 测试A/B测试边界值
    MigrationManager.EnableABTesting(-0.5f); // 负值
    TestTrue("负值A/B测试比例应该被修正", MigrationManager.IsABTestingEnabled());

    MigrationManager.EnableABTesting(1.5f); // 超过1.0
    TestTrue("超过1.0的A/B测试比例应该被修正", MigrationManager.IsABTestingEnabled());

    MigrationManager.EnableABTesting(0.0f); // 边界值
    TestTrue("0.0的A/B测试比例应该有效", MigrationManager.IsABTestingEnabled());

    MigrationManager.EnableABTesting(1.0f); // 边界值
    TestTrue("1.0的A/B测试比例应该有效", MigrationManager.IsABTestingEnabled());

    // 4. 测试大量统计记录
    MigrationManager.ResetStats();

    for (int32 i = 0; i < 1000; ++i)
    {
        MigrationManager.RecordImplementationCall(
            (i % 2 == 0) ? FObjectPoolMigrationManager::EImplementationType::Original
                         : FObjectPoolMigrationManager::EImplementationType::Simplified
        );
        MigrationManager.RecordCompatibilityCheck(i % 3 != 0); // 大部分通过
    }

    FObjectPoolMigrationManager::FMigrationStats LargeStats = MigrationManager.GetMigrationStats();
    TestEqual("大量记录后原始实现调用次数应该正确", LargeStats.OriginalImplementationCalls, 500);
    TestEqual("大量记录后简化实现调用次数应该正确", LargeStats.SimplifiedImplementationCalls, 500);
    TestTrue("大量记录后兼容性检查应该有结果",
        LargeStats.CompatibilityChecksPassed > 0 && LargeStats.CompatibilityChecksFailed > 0);

    // 5. 测试报告生成的稳定性
    for (int32 i = 0; i < 10; ++i)
    {
        FString Report = MigrationManager.GenerateMigrationReport();
        TestTrue("多次生成报告应该稳定", !Report.IsEmpty());

        FString PerfReport = MigrationManager.GeneratePerformanceReport();
        TestTrue("多次生成性能报告应该稳定", !PerfReport.IsEmpty());
    }

    // 清理
    MigrationManager.DisableABTesting();
    FMigrationTestHelpers::CleanupTestEnvironment();

    return true;
}

#endif // WITH_OBJECTPOOL_TESTS

/**
 * 测试迁移过程中的数据完整性
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolMigrationDataIntegrityTest, 
    "ObjectPool.Migration.DataIntegrityTest", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolMigrationDataIntegrityTest::RunTest(const FString& Parameters)
{
    // 获取测试环境
    UWorld* World = FMigrationTestHelpers::GetTestWorld();
    TestNotNull("测试World应该可用", World);

    if (!World)
    {
        return false;
    }

    // 清理环境
    FMigrationTestHelpers::CleanupTestEnvironment();

    // 获取迁移管理器
    FObjectPoolMigrationManager& MigrationManager = FObjectPoolMigrationManager::Get();

    // 1. 在原始实现中创建和初始化数据
    MigrationManager.SwitchToOriginalImplementation();
    
    bool bRegistered = UObjectPoolLibrary::RegisterActorClass(
        World, 
        AMigrationTestActor::StaticClass(), 
        10, 
        50
    );
    TestTrue("应该能够注册Actor类", bRegistered);

    // 创建多个Actor并初始化数据
    TArray<AActor*> TestActors;
    TArray<FTransform> TestTransforms = FMigrationTestHelpers::GenerateTestTransforms(5);

    for (const FTransform& Transform : TestTransforms)
    {
        AActor* Actor = UObjectPoolLibrary::SpawnActorFromPool(
            World, 
            AMigrationTestActor::StaticClass(), 
            Transform
        );
        
        if (AMigrationTestActor* TestActor = Cast<AMigrationTestActor>(Actor))
        {
            TestActor->InitializeTestData();
            TestActors.Add(Actor);
        }
    }

    TestEqual("应该创建正确数量的Actor", TestActors.Num(), 5);

    // 验证初始数据
    for (AActor* Actor : TestActors)
    {
        if (AMigrationTestActor* TestActor = Cast<AMigrationTestActor>(Actor))
        {
            TestTrue("Actor数据应该正确初始化", TestActor->ValidateTestData());
        }
    }

    // 2. 切换到简化实现
    MigrationManager.SwitchToSimplifiedImplementation();

    // 验证数据在切换后仍然完整
    for (AActor* Actor : TestActors)
    {
        if (AMigrationTestActor* TestActor = Cast<AMigrationTestActor>(Actor))
        {
            TestTrue("切换实现后数据应该保持完整", TestActor->ValidateTestData());
        }
    }

    // 3. 在简化实现中继续操作
    AActor* NewActor = UObjectPoolLibrary::SpawnActorFromPool(
        World, 
        AMigrationTestActor::StaticClass(), 
        FTransform::Identity
    );
    TestNotNull("简化实现应该能够继续生成Actor", NewActor);

    // 4. 归还所有Actor
    for (AActor* Actor : TestActors)
    {
        UObjectPoolLibrary::ReturnActorToPool(World, Actor);
    }

    if (NewActor)
    {
        UObjectPoolLibrary::ReturnActorToPool(World, NewActor);
    }

    // 清理
    FMigrationTestHelpers::CleanupTestEnvironment();

    return true;
}
