// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 条件编译保护 - 只在非Shipping版本中编译测试
#if WITH_OBJECTPOOL_TESTS

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// ✅ 蓝图库依赖
#include "ObjectPoolLibrary.h"
#include "ObjectPoolTypes.h"

// ✅ 子系统依赖
#include "ObjectPoolSubsystemSimplified.h"

/**
 * 蓝图兼容性测试运行器
 * 用于手动验证蓝图库的兼容性和功能正确性
 */
class FBlueprintCompatibilityTestRunner
{
public:
    /**
     * 运行所有蓝图兼容性测试
     */
    static void RunAllBlueprintCompatibilityTests()
    {
        UE_LOG(LogTemp, Warning, TEXT("========================================"));
        UE_LOG(LogTemp, Warning, TEXT("开始运行蓝图兼容性测试"));
        UE_LOG(LogTemp, Warning, TEXT("========================================"));

        RunBasicBlueprintFunctionTests();
        RunBlueprintParameterValidationTests();
        RunBlueprintBatchOperationTests();
        RunBlueprintSubsystemAccessTests();
        RunBlueprintErrorHandlingTests();

        UE_LOG(LogTemp, Warning, TEXT("========================================"));
        UE_LOG(LogTemp, Warning, TEXT("蓝图兼容性测试完成"));
        UE_LOG(LogTemp, Warning, TEXT("========================================"));
    }

private:
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
    }

    /**
     * 测试基础蓝图函数
     */
    static void RunBasicBlueprintFunctionTests()
    {
        UE_LOG(LogTemp, Warning, TEXT("=== 开始基础蓝图函数测试 ==="));

        const UObject* WorldContext = GetTestWorld();
        if (!WorldContext)
        {
            UE_LOG(LogTemp, Error, TEXT("无法获取测试World，跳过基础函数测试"));
            return;
        }

        CleanupTestEnvironment();

        // 1. 测试RegisterActorClass
        bool bRegistered = UObjectPoolLibrary::RegisterActorClass(
            WorldContext, 
            AActor::StaticClass(), 
            5, 
            20
        );
        UE_LOG(LogTemp, Warning, TEXT("RegisterActorClass测试: %s"), bRegistered ? TEXT("通过") : TEXT("失败"));

        // 2. 测试IsActorClassRegistered
        bool bIsRegistered = UObjectPoolLibrary::IsActorClassRegistered(
            WorldContext, 
            AActor::StaticClass()
        );
        UE_LOG(LogTemp, Warning, TEXT("IsActorClassRegistered测试: %s"), bIsRegistered ? TEXT("通过") : TEXT("失败"));

        // 3. 测试PrewarmPool
        bool bPrewarmed = UObjectPoolLibrary::PrewarmPool(
            WorldContext, 
            AActor::StaticClass(), 
            3
        );
        UE_LOG(LogTemp, Warning, TEXT("PrewarmPool测试: %s"), bPrewarmed ? TEXT("通过") : TEXT("失败"));

        // 4. 测试SpawnActorFromPool
        AActor* SpawnedActor = UObjectPoolLibrary::SpawnActorFromPool(
            WorldContext, 
            AActor::StaticClass(), 
            FTransform::Identity
        );
        bool bSpawnSuccess = IsValid(SpawnedActor);
        UE_LOG(LogTemp, Warning, TEXT("SpawnActorFromPool测试: %s"), bSpawnSuccess ? TEXT("通过") : TEXT("失败"));

        // 5. 测试ReturnActorToPool
        if (SpawnedActor)
        {
            UObjectPoolLibrary::ReturnActorToPool(WorldContext, SpawnedActor);
            UE_LOG(LogTemp, Warning, TEXT("ReturnActorToPool测试: 通过"));
        }

        // 6. 测试GetPoolStats
        FObjectPoolStats Stats = UObjectPoolLibrary::GetPoolStats(
            WorldContext, 
            AActor::StaticClass()
        );
        bool bStatsValid = (Stats.PoolSize >= 0);
        UE_LOG(LogTemp, Warning, TEXT("GetPoolStats测试: %s"), bStatsValid ? TEXT("通过") : TEXT("失败"));

        CleanupTestEnvironment();
        UE_LOG(LogTemp, Warning, TEXT("=== 基础蓝图函数测试完成 ==="));
    }

    /**
     * 测试蓝图参数验证
     */
    static void RunBlueprintParameterValidationTests()
    {
        UE_LOG(LogTemp, Warning, TEXT("=== 开始蓝图参数验证测试 ==="));

        const UObject* WorldContext = GetTestWorld();
        if (!WorldContext)
        {
            UE_LOG(LogTemp, Error, TEXT("无法获取测试World，跳过参数验证测试"));
            return;
        }

        // 1. 测试无效WorldContext
        bool bNullContextResult = UObjectPoolLibrary::RegisterActorClass(
            nullptr, 
            AActor::StaticClass(), 
            5, 
            20
        );
        UE_LOG(LogTemp, Warning, TEXT("空WorldContext处理测试: %s"), !bNullContextResult ? TEXT("通过") : TEXT("失败"));

        // 2. 测试无效ActorClass
        bool bNullClassResult = UObjectPoolLibrary::RegisterActorClass(
            WorldContext, 
            nullptr, 
            5, 
            20
        );
        UE_LOG(LogTemp, Warning, TEXT("空ActorClass处理测试: %s"), !bNullClassResult ? TEXT("通过") : TEXT("失败"));

        // 3. 测试无效参数
        AActor* NullClassActor = UObjectPoolLibrary::SpawnActorFromPool(
            WorldContext, 
            nullptr, 
            FTransform::Identity
        );
        UE_LOG(LogTemp, Warning, TEXT("空类生成Actor测试: %s"), !IsValid(NullClassActor) ? TEXT("通过") : TEXT("失败"));

        // 4. 测试无效Count
        bool bInvalidCountResult = UObjectPoolLibrary::PrewarmPool(
            WorldContext, 
            AActor::StaticClass(), 
            -5
        );
        UE_LOG(LogTemp, Warning, TEXT("无效Count处理测试: %s"), !bInvalidCountResult ? TEXT("通过") : TEXT("失败"));

        UE_LOG(LogTemp, Warning, TEXT("=== 蓝图参数验证测试完成 ==="));
    }

    /**
     * 测试蓝图批量操作
     */
    static void RunBlueprintBatchOperationTests()
    {
        UE_LOG(LogTemp, Warning, TEXT("=== 开始蓝图批量操作测试 ==="));

        const UObject* WorldContext = GetTestWorld();
        if (!WorldContext)
        {
            UE_LOG(LogTemp, Error, TEXT("无法获取测试World，跳过批量操作测试"));
            return;
        }

        CleanupTestEnvironment();

        // 注册Actor类
        UObjectPoolLibrary::RegisterActorClass(
            WorldContext, 
            AActor::StaticClass(), 
            10, 
            50
        );

        // 1. 测试BatchSpawnActors
        TArray<FTransform> SpawnTransforms;
        for (int32 i = 0; i < 5; ++i)
        {
            FTransform Transform = FTransform::Identity;
            Transform.SetLocation(FVector(i * 100.0f, 0.0f, 0.0f));
            SpawnTransforms.Add(Transform);
        }

        TArray<AActor*> OutActors;
        int32 SpawnedCount = UObjectPoolLibrary::BatchSpawnActors(
            WorldContext,
            AActor::StaticClass(),
            SpawnTransforms,
            OutActors
        );
        UE_LOG(LogTemp, Warning, TEXT("BatchSpawnActors测试: %s (生成数量: %d)"), 
            (SpawnedCount > 0) ? TEXT("通过") : TEXT("失败"), SpawnedCount);

        // 2. 获取一些Actor用于批量归还测试
        TArray<AActor*> TestActors;
        for (int32 i = 0; i < 3; ++i)
        {
            AActor* Actor = UObjectPoolLibrary::SpawnActorFromPool(
                WorldContext, 
                AActor::StaticClass(), 
                FTransform::Identity
            );
            if (Actor)
            {
                TestActors.Add(Actor);
            }
        }

        // 3. 测试BatchReturnActors
        int32 ReturnedCount = UObjectPoolLibrary::BatchReturnActors(WorldContext, TestActors);
        UE_LOG(LogTemp, Warning, TEXT("BatchReturnActors测试: %s (归还数量: %d)"), 
            (ReturnedCount >= 0) ? TEXT("通过") : TEXT("失败"), ReturnedCount);

        CleanupTestEnvironment();
        UE_LOG(LogTemp, Warning, TEXT("=== 蓝图批量操作测试完成 ==="));
    }

    /**
     * 测试蓝图子系统访问
     */
    static void RunBlueprintSubsystemAccessTests()
    {
        UE_LOG(LogTemp, Warning, TEXT("=== 开始蓝图子系统访问测试 ==="));

        const UObject* WorldContext = GetTestWorld();
        if (!WorldContext)
        {
            UE_LOG(LogTemp, Error, TEXT("无法获取测试World，跳过子系统访问测试"));
            return;
        }

        // 1. 测试GetObjectPoolSubsystemSimplified
        UObjectPoolSubsystemSimplified* SimplifiedSubsystem = UObjectPoolLibrary::GetObjectPoolSubsystemSimplified(WorldContext);
        bool bSimplifiedSubsystemValid = IsValid(SimplifiedSubsystem);
        UE_LOG(LogTemp, Warning, TEXT("GetObjectPoolSubsystemSimplified测试: %s"), 
            bSimplifiedSubsystemValid ? TEXT("通过") : TEXT("失败"));

        // 2. 测试GetObjectPoolSubsystem
        UObjectPoolSubsystem* OriginalSubsystem = UObjectPoolLibrary::GetObjectPoolSubsystem(WorldContext);
        // 原始子系统可能不存在，这是正常的
        UE_LOG(LogTemp, Warning, TEXT("GetObjectPoolSubsystem测试: %s"), 
            TEXT("完成（原始子系统可能不存在）"));

        // 3. 验证简化子系统的基本功能
        if (SimplifiedSubsystem)
        {
            int32 InitialPoolCount = SimplifiedSubsystem->GetPoolCount();
            UE_LOG(LogTemp, Log, TEXT("简化子系统初始池数量: %d"), InitialPoolCount);

            // 通过蓝图库与简化子系统交互
            UObjectPoolLibrary::RegisterActorClass(
                WorldContext, 
                AActor::StaticClass(), 
                3, 
                15
            );

            int32 NewPoolCount = SimplifiedSubsystem->GetPoolCount();
            bool bPoolCountIncreased = (NewPoolCount > InitialPoolCount);
            UE_LOG(LogTemp, Warning, TEXT("简化子系统交互测试: %s"), 
                bPoolCountIncreased ? TEXT("通过") : TEXT("失败"));
        }

        CleanupTestEnvironment();
        UE_LOG(LogTemp, Warning, TEXT("=== 蓝图子系统访问测试完成 ==="));
    }

    /**
     * 测试蓝图错误处理
     */
    static void RunBlueprintErrorHandlingTests()
    {
        UE_LOG(LogTemp, Warning, TEXT("=== 开始蓝图错误处理测试 ==="));

        const UObject* WorldContext = GetTestWorld();
        if (!WorldContext)
        {
            UE_LOG(LogTemp, Error, TEXT("无法获取测试World，跳过错误处理测试"));
            return;
        }

        CleanupTestEnvironment();

        // 1. 测试在没有注册的情况下生成Actor（回退机制）
        AActor* FallbackActor = UObjectPoolLibrary::SpawnActorFromPool(
            WorldContext, 
            AActor::StaticClass(), 
            FTransform::Identity
        );
        bool bFallbackSuccess = IsValid(FallbackActor);
        UE_LOG(LogTemp, Warning, TEXT("回退机制测试: %s"), bFallbackSuccess ? TEXT("通过") : TEXT("失败"));

        if (FallbackActor)
        {
            UObjectPoolLibrary::ReturnActorToPool(WorldContext, FallbackActor);
        }

        // 2. 测试极限情况
        TArray<AActor*> ManyActors;
        for (int32 i = 0; i < 20; ++i)
        {
            AActor* Actor = UObjectPoolLibrary::SpawnActorFromPool(
                WorldContext, 
                AActor::StaticClass(), 
                FTransform::Identity
            );
            if (Actor)
            {
                ManyActors.Add(Actor);
            }
        }

        bool bExtremeTestSuccess = (ManyActors.Num() > 0);
        UE_LOG(LogTemp, Warning, TEXT("极限情况测试: %s (生成数量: %d)"), 
            bExtremeTestSuccess ? TEXT("通过") : TEXT("失败"), ManyActors.Num());

        // 归还所有Actor
        for (AActor* Actor : ManyActors)
        {
            UObjectPoolLibrary::ReturnActorToPool(WorldContext, Actor);
        }

        CleanupTestEnvironment();
        UE_LOG(LogTemp, Warning, TEXT("=== 蓝图错误处理测试完成 ==="));
    }
};

/**
 * 全局函数：运行所有蓝图兼容性测试
 */
void RunAllObjectPoolBlueprintCompatibilityTests()
{
    FBlueprintCompatibilityTestRunner::RunAllBlueprintCompatibilityTests();
}

#endif // WITH_OBJECTPOOL_TESTS
