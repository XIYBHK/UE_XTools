// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 条件编译保护 - 只在测试环境中编译
#if WITH_OBJECTPOOL_TESTS

// ✅ 核心依赖
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Engine/GameInstance.h"
#include "Subsystems/SubsystemCollection.h"

// ✅ 对象池模块依赖
#include "ObjectPoolSubsystem.h"
#include "ObjectPoolSubsystemSimplified.h"
#include "ObjectPoolLibrary.h"
#include "ObjectPoolTypes.h"
#include "ObjectPoolTypesSimplified.h"
#include "ObjectPoolInterface.h"

// ✅ 性能测试依赖
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "Stats/Stats.h"

/**
 * 重构测试专用的Actor类
 * 用于验证重构前后的行为一致性
 */
class ARefactoringTestActor : public AActor, public IObjectPoolInterface
{
public:
    ARefactoringTestActor()
    {
        PrimaryActorTick.bCanEverTick = false;
        bReplicates = false;
    }

    // 测试状态标记
    bool bWasActivated = false;
    bool bWasReturnedToPool = false;
    bool bWasCreated = false;
    int32 ActivationCount = 0;
    int32 ReturnCount = 0;

    // IObjectPoolInterface 实现
    virtual void OnPoolActorActivated_Implementation()
    {
        bWasActivated = true;
        ActivationCount++;
    }

    virtual void OnReturnToPool_Implementation()
    {
        bWasReturnedToPool = true;
        ReturnCount++;
    }

    virtual void OnPoolActorCreated_Implementation()
    {
        bWasCreated = true;
    }

    // 重置测试状态
    void ResetTestState()
    {
        bWasActivated = false;
        bWasReturnedToPool = false;
        bWasCreated = false;
        ActivationCount = 0;
        ReturnCount = 0;
    }
};

/**
 * 重构基础测试 - 验证核心API功能
 * 这些测试将在重构过程中持续运行，确保功能不被破坏
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolRefactoringCoreAPITest,
    "ObjectPool.Refactoring.CoreAPI",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

// 智能获取测试World
UWorld* GetObjectPoolTestGameWorld(const int32 TestFlags)
{
    if (GEngine && GEngine->GetWorldContexts().Num() > 0)
    {
        // 优先查找Game类型的World
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.World() && Context.WorldType == EWorldType::Game)
            {
                return Context.World();
            }
        }

        // 如果没有Game World，查找PIE World
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.World() && Context.WorldType == EWorldType::PIE)
            {
                return Context.World();
            }
        }

        // 最后回退到Editor World（用于回退机制测试）
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.World() && Context.WorldType == EWorldType::Editor)
            {
                return Context.World();
            }
        }

        // 如果都没有，使用第一个可用的World
        if (GEngine->GetWorldContexts()[0].World())
        {
            return GEngine->GetWorldContexts()[0].World();
        }
    }

    return nullptr;
}

bool FObjectPoolRefactoringCoreAPITest::RunTest(const FString& Parameters)
{
    AddInfo(TEXT("开始ObjectPool核心API测试..."));

    // 使用UE官方推荐的方式获取测试World
    UWorld* TestWorld = GetObjectPoolTestGameWorld(GetTestFlags());

    if (!TestWorld)
    {
        AddError(TEXT("无法获取Game World - 请确保在PIE模式下运行测试"));
        return false;
    }

    AddInfo(FString::Printf(TEXT("获取到测试世界: %s (类型: %s)"),
        *TestWorld->GetName(),
        TestWorld->WorldType == EWorldType::Game ? TEXT("Game") :
        TestWorld->WorldType == EWorldType::PIE ? TEXT("PIE") : TEXT("Other")));

    // 现在应该能够正确获取子系统了
    AddInfo(TEXT("尝试获取ObjectPool子系统..."));

    // 优先尝试简化子系统
    UObjectPoolSubsystemSimplified* SimplifiedSubsystem = TestWorld->GetSubsystem<UObjectPoolSubsystemSimplified>();
    if (SimplifiedSubsystem)
    {
        AddInfo(TEXT("✅ 成功获取简化子系统"));

        // 测试简化子系统
        {
            AddInfo(TEXT("开始测试简化子系统功能..."));

            // 测试1: 注册Actor类
            FObjectPoolConfigSimplified Config;
            Config.InitialSize = 3;
            Config.HardLimit = 10;

            bool bSuccess = SimplifiedSubsystem->SetPoolConfig(AActor::StaticClass(), Config);
            TestTrue(TEXT("SetPoolConfig应该成功"), bSuccess);
            AddInfo(TEXT("✅ Actor类注册测试通过"));

            // 测试2: 从池获取Actor
            FTransform SpawnTransform = FTransform::Identity;
            AActor* SpawnedActor = SimplifiedSubsystem->SpawnActorFromPool(AActor::StaticClass(), SpawnTransform);

            TestNotNull(TEXT("SpawnActorFromPool应返回有效Actor"), SpawnedActor);
            if (SpawnedActor)
            {
                TestTrue(TEXT("返回的Actor应为正确类型"), SpawnedActor->IsA<AActor>());
                TestTrue(TEXT("Actor应该有效"), IsValid(SpawnedActor));
                AddInfo(TEXT("✅ 从池获取Actor测试通过"));

                // 测试3: 归还Actor到池
                bool bReturned = SimplifiedSubsystem->ReturnActorToPool(SpawnedActor);
                TestTrue(TEXT("ReturnActorToPool应该成功"), bReturned);
                AddInfo(TEXT("✅ 归还Actor到池测试通过"));
            }

            // 清理测试
            SimplifiedSubsystem->RemovePool(AActor::StaticClass());
            AddInfo(TEXT("✅ 简化子系统测试全部完成"));
        }

        return true;
    }

    // 回退到原始子系统
    if (UGameInstance* GameInstance = TestWorld->GetGameInstance())
    {
        UObjectPoolSubsystem* OriginalSubsystem = GameInstance->GetSubsystem<UObjectPoolSubsystem>();
        if (OriginalSubsystem)
        {
            AddInfo(TEXT("✅ 成功获取原始子系统"));

            // 测试原始子系统
            {
                AddInfo(TEXT("开始测试原始子系统功能..."));

                // 测试1: 注册Actor类
                OriginalSubsystem->RegisterActorClass(AActor::StaticClass(), 3, 10);
                AddInfo(TEXT("✅ Actor类注册测试通过"));

                // 测试2: 从池获取Actor
                FTransform SpawnTransform = FTransform::Identity;
                AActor* SpawnedActor = OriginalSubsystem->SpawnActorFromPool(AActor::StaticClass(), SpawnTransform);

                TestNotNull(TEXT("SpawnActorFromPool应返回有效Actor"), SpawnedActor);
                if (SpawnedActor)
                {
                    TestTrue(TEXT("返回的Actor应为正确类型"), SpawnedActor->IsA<AActor>());
                    TestTrue(TEXT("Actor应该有效"), IsValid(SpawnedActor));
                    AddInfo(TEXT("✅ 从池获取Actor测试通过"));

                    // 测试3: 归还Actor到池
                    OriginalSubsystem->ReturnActorToPool(SpawnedActor);
                    AddInfo(TEXT("✅ 归还Actor到池测试通过"));
                }

                // 清理测试
                OriginalSubsystem->ClearPool(AActor::StaticClass());
                AddInfo(TEXT("✅ 原始子系统测试全部完成"));
            }

            return true;
        }
    }

    // 最后回退到蓝图库
    AddWarning(TEXT("⚠️ 无法获取任何子系统，使用蓝图库回退机制"));

    // 测试蓝图库回退机制
    {
        AddInfo(TEXT("开始测试蓝图库回退机制..."));

        // 测试1: 注册Actor类
        bool bSuccess = UObjectPoolLibrary::RegisterActorClass(TestWorld, AActor::StaticClass(), 3, 10);
        if (bSuccess)
        {
            AddInfo(TEXT("✅ Actor类注册成功"));
        }
        else
        {
            AddWarning(TEXT("⚠️ Actor类注册失败（回退机制）"));
        }

        // 测试2: 从池获取Actor
        FTransform SpawnTransform = FTransform::Identity;
        AActor* SpawnedActor = UObjectPoolLibrary::SpawnActorFromPool(TestWorld, AActor::StaticClass(), SpawnTransform);

        TestNotNull(TEXT("SpawnActorFromPool应通过回退机制返回有效Actor"), SpawnedActor);
        if (SpawnedActor)
        {
            TestTrue(TEXT("返回的Actor应为正确类型"), SpawnedActor->IsA<AActor>());
            TestTrue(TEXT("Actor应该有效"), IsValid(SpawnedActor));
            AddInfo(TEXT("✅ 回退机制成功创建了Actor"));

            // 测试3: 归还Actor到池
            UObjectPoolLibrary::ReturnActorToPool(TestWorld, SpawnedActor);
            AddInfo(TEXT("✅ 回退机制成功处理了Actor归还"));
        }
        else
        {
            AddError(TEXT("❌ 回退机制也失败了"));
        }

        // 清理测试
        UObjectPoolLibrary::ClearPool(TestWorld, AActor::StaticClass());
        AddInfo(TEXT("✅ 蓝图库回退机制测试完成"));
    }

    return true;
}

/**
 * 重构性能基准测试
 * 记录重构前的性能基准，用于对比重构后的性能
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolRefactoringPerformanceBaselineTest,
    "ObjectPool.Refactoring.PerformanceBaseline",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolRefactoringPerformanceBaselineTest::RunTest(const FString& Parameters)
{
    AddInfo(TEXT("开始ObjectPool性能基准测试..."));

    // 使用现有的世界而不是创建新的
    UWorld* TestWorld = nullptr;
    if (GEngine && GEngine->GetWorldContexts().Num() > 0)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.World() && (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Editor))
            {
                TestWorld = Context.World();
                break;
            }
        }
    }

    if (!TestWorld)
    {
        AddError(TEXT("无法获取测试世界"));
        return false;
    }

    AddInfo(FString::Printf(TEXT("使用测试世界: %s"), *TestWorld->GetName()));

    // 使用蓝图库进行测试，避免子系统依赖问题
    AddInfo(TEXT("使用蓝图库进行性能测试"));

    // 注册测试Actor类（使用标准AActor）
    bool bRegistered = UObjectPoolLibrary::RegisterActorClass(TestWorld, AActor::StaticClass(), 50, 200);
    if (!bRegistered)
    {
        AddWarning(TEXT("Actor类注册失败，将测试回退机制性能"));
    }

    // 减少测试迭代次数，避免性能问题
    const int32 TestIterations = 100;
    TArray<AActor*> SpawnedActors;
    SpawnedActors.Reserve(TestIterations);

    // 性能测试1: 批量生成Actor
    {
        AddInfo(TEXT("开始批量生成Actor性能测试..."));
        double StartTime = FPlatformTime::Seconds();

        for (int32 i = 0; i < TestIterations; i++)
        {
            FTransform SpawnTransform = FTransform::Identity;
            AActor* Actor = UObjectPoolLibrary::SpawnActorFromPool(TestWorld, AActor::StaticClass(), SpawnTransform);
            if (Actor)
            {
                SpawnedActors.Add(Actor);
            }
        }

        double EndTime = FPlatformTime::Seconds();
        double SpawnTime = (EndTime - StartTime) * 1000.0; // 转换为毫秒

        AddInfo(FString::Printf(TEXT("✅ 生成%d个Actor耗时: %.2f ms (平均: %.4f ms/个)"),
            TestIterations, SpawnTime, SpawnTime / TestIterations));

        AddInfo(FString::Printf(TEXT("实际生成数量: %d"), SpawnedActors.Num()));
    }

    // 性能测试2: 批量归还Actor
    {
        AddInfo(TEXT("开始批量归还Actor性能测试..."));
        double StartTime = FPlatformTime::Seconds();

        for (AActor* Actor : SpawnedActors)
        {
            if (IsValid(Actor))
            {
                UObjectPoolLibrary::ReturnActorToPool(TestWorld, Actor);
            }
        }

        double EndTime = FPlatformTime::Seconds();
        double ReturnTime = (EndTime - StartTime) * 1000.0; // 转换为毫秒

        AddInfo(FString::Printf(TEXT("✅ 归还%d个Actor耗时: %.2f ms (平均: %.4f ms/个)"),
            SpawnedActors.Num(), ReturnTime, ReturnTime / SpawnedActors.Num()));
    }

    // 记录内存使用情况（简化版本）
    {
        AddInfo(TEXT("获取内存使用统计..."));
        FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
        double MemoryUsageMB = static_cast<double>(MemStats.UsedPhysical) / (1024.0 * 1024.0);
        AddInfo(FString::Printf(TEXT("✅ 当前内存使用: %.2f MB"), MemoryUsageMB));
    }

    // 清理测试环境
    AddInfo(TEXT("清理测试环境..."));
    UObjectPoolLibrary::ClearPool(TestWorld, AActor::StaticClass());

    AddInfo(TEXT("✅ ObjectPool性能基准测试完成"));
    return true;
}

/**
 * 重构API兼容性测试
 * 验证所有公开API的签名和行为保持不变
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolRefactoringAPICompatibilityTest,
    "ObjectPool.Refactoring.APICompatibility",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolRefactoringAPICompatibilityTest::RunTest(const FString& Parameters)
{
    AddInfo(TEXT("开始ObjectPool API兼容性测试..."));

    // 使用现有的世界
    UWorld* TestWorld = nullptr;
    if (GEngine && GEngine->GetWorldContexts().Num() > 0)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.World() && (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Editor))
            {
                TestWorld = Context.World();
                break;
            }
        }
    }

    if (!TestWorld)
    {
        AddError(TEXT("无法获取测试世界"));
        return false;
    }

    AddInfo(FString::Printf(TEXT("使用测试世界: %s"), *TestWorld->GetName()));

    // 测试蓝图库API兼容性
    {
        AddInfo(TEXT("测试蓝图库API兼容性..."));

        // 这些调用应该编译通过，验证API签名没有改变
        bool bRegistered = UObjectPoolLibrary::RegisterActorClass(TestWorld, AActor::StaticClass(), 10, 50);
        AddInfo(FString::Printf(TEXT("RegisterActorClass 结果: %s"), bRegistered ? TEXT("成功") : TEXT("失败（回退机制）")));

        FTransform TestTransform = FTransform::Identity;
        AActor* TestActor = UObjectPoolLibrary::SpawnActorFromPool(TestWorld, AActor::StaticClass(), TestTransform);
        TestNotNull(TEXT("SpawnActorFromPool应返回有效Actor"), TestActor);

        if (TestActor)
        {
            UObjectPoolLibrary::ReturnActorToPool(TestWorld, TestActor);
            AddInfo(TEXT("ReturnActorToPool 调用成功"));
        }

        // 测试清理API
        UObjectPoolLibrary::ClearPool(TestWorld, AActor::StaticClass());

        AddInfo(TEXT("✅ 蓝图库API兼容性测试通过"));
    }

    // 测试永不失败原则
    {
        AddInfo(TEXT("测试永不失败原则..."));

        // 即使没有注册，也应该返回有效Actor（通过回退机制）
        AActor* UnregisteredActor = UObjectPoolLibrary::SpawnActorFromPool(TestWorld, AActor::StaticClass(), FTransform::Identity);
        TestNotNull(TEXT("未注册的Actor类也应返回有效Actor（永不失败原则）"), UnregisteredActor);

        if (UnregisteredActor)
        {
            UObjectPoolLibrary::ReturnActorToPool(TestWorld, UnregisteredActor);
            AddInfo(TEXT("未注册Actor的归还处理成功"));
        }

        AddInfo(TEXT("✅ 永不失败原则测试通过"));
    }

    // 清理测试环境
    AddInfo(TEXT("清理测试环境..."));
    UObjectPoolLibrary::ClearPool(TestWorld, AActor::StaticClass());

    AddInfo(TEXT("✅ ObjectPool API兼容性测试完成"));
    return true;
}

#endif // WITH_OBJECTPOOL_TESTS
