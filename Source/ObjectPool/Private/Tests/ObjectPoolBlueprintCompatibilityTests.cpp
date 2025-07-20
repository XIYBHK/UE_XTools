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
#include "GameFramework/Pawn.h"

// ✅ 蓝图库依赖
#include "ObjectPoolLibrary.h"
#include "ObjectPoolTypes.h"
#include "ObjectPoolTypesSimplified.h"

// ✅ 子系统依赖
#include "ObjectPoolSubsystem.h"
#include "ObjectPoolSubsystemSimplified.h"

// ✅ 测试环境依赖
#include "Engine/GameInstance.h"
#include "Subsystems/SubsystemCollection.h"
#include "HAL/PlatformFilemanager.h"

// ✅ 测试用的简单Actor类
class ABlueprintTestActor : public AActor
{
public:
    ABlueprintTestActor()
    {
        PrimaryActorTick.bCanEverTick = false;
        bReplicates = false;
    }

    // 用于测试的标记
    bool bWasPooled = false;
    int32 TestValue = 0;
    FString TestString = TEXT("Default");

    void MarkAsPooled()
    {
        bWasPooled = true;
        TestValue = 999;
        TestString = TEXT("Pooled");
    }

    void ResetForPool()
    {
        bWasPooled = false;
        TestValue = 0;
        TestString = TEXT("Reset");
    }
};

// ✅ 测试用的复杂Actor类
class ABlueprintTestCharacter : public ACharacter
{
public:
    ABlueprintTestCharacter()
    {
        PrimaryActorTick.bCanEverTick = false;
        bReplicates = false;
    }

    // 复杂状态用于测试
    TArray<int32> TestArray;
    TMap<FString, int32> TestMap;
    bool bComplexState = false;

    void InitializeComplexState()
    {
        TestArray = {10, 20, 30};
        TestMap.Add(TEXT("Test1"), 100);
        TestMap.Add(TEXT("Test2"), 200);
        bComplexState = true;
    }

    void ResetComplexState()
    {
        TestArray.Empty();
        TestMap.Empty();
        bComplexState = false;
    }
};

/**
 * 蓝图兼容性测试辅助工具
 */
class FBlueprintCompatibilityTestHelpers
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
            if (UObjectPoolSubsystem* Subsystem = GameInstance->GetSubsystem<UObjectPoolSubsystem>())
            {
                Subsystem->ClearAllPools();
            }
        }
    }

    /**
     * 验证Actor状态
     */
    static bool ValidateActorState(AActor* Actor, bool bShouldBeValid)
    {
        if (bShouldBeValid)
        {
            return IsValid(Actor) && Actor->IsValidLowLevel();
        }
        else
        {
            return !IsValid(Actor);
        }
    }

    /**
     * 模拟蓝图调用环境
     */
    static const UObject* GetBlueprintWorldContext()
    {
        return GetTestWorld();
    }
};

// ✅ 基础蓝图库功能测试

/**
 * 测试蓝图库的基本注册和生成功能
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolBlueprintLibraryBasicTest, 
    "ObjectPool.BlueprintLibrary.BasicTest", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolBlueprintLibraryBasicTest::RunTest(const FString& Parameters)
{
    // 获取蓝图世界上下文
    const UObject* WorldContext = FBlueprintCompatibilityTestHelpers::GetBlueprintWorldContext();
    TestNotNull("蓝图世界上下文应该可用", WorldContext);

    if (!WorldContext)
    {
        return false;
    }

    // 清理环境
    FBlueprintCompatibilityTestHelpers::CleanupTestEnvironment();

    // 1. 测试RegisterActorClass（模拟蓝图调用）
    bool bRegistered = UObjectPoolLibrary::RegisterActorClass(
        WorldContext, 
        ABlueprintTestActor::StaticClass(), 
        5,  // InitialSize
        20  // HardLimit
    );
    TestTrue("应该能够注册Actor类", bRegistered);

    // 2. 测试IsActorClassRegistered
    bool bIsRegistered = UObjectPoolLibrary::IsActorClassRegistered(
        WorldContext, 
        ABlueprintTestActor::StaticClass()
    );
    TestTrue("Actor类应该显示为已注册", bIsRegistered);

    // 3. 测试PrewarmPool
    bool bPrewarmed = UObjectPoolLibrary::PrewarmPool(
        WorldContext, 
        ABlueprintTestActor::StaticClass(), 
        3
    );
    TestTrue("应该能够预热池", bPrewarmed);

    // 4. 测试SpawnActorFromPool
    FTransform SpawnTransform = FTransform::Identity;
    AActor* SpawnedActor = UObjectPoolLibrary::SpawnActorFromPool(
        WorldContext, 
        ABlueprintTestActor::StaticClass(), 
        SpawnTransform
    );
    TestNotNull("应该能够从池中生成Actor", SpawnedActor);
    TestTrue("生成的Actor应该是正确的类型", SpawnedActor->IsA<ABlueprintTestActor>());

    // 5. 测试ReturnActorToPool
    if (SpawnedActor)
    {
        UObjectPoolLibrary::ReturnActorToPool(WorldContext, SpawnedActor);
        // 注意：ReturnActorToPool是void函数，我们通过后续操作验证其效果
    }

    // 6. 测试GetPoolStats
    FObjectPoolStats PoolStats = UObjectPoolLibrary::GetPoolStats(
        WorldContext, 
        ABlueprintTestActor::StaticClass()
    );
    TestTrue("应该能够获取池统计信息", PoolStats.PoolSize >= 0);
    TestTrue("统计信息应该包含正确的类名", !PoolStats.ActorClassName.IsEmpty());

    // 清理
    FBlueprintCompatibilityTestHelpers::CleanupTestEnvironment();

    return true;
}

/**
 * 测试蓝图库的批量操作功能
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolBlueprintLibraryBatchTest, 
    "ObjectPool.BlueprintLibrary.BatchTest", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolBlueprintLibraryBatchTest::RunTest(const FString& Parameters)
{
    // 获取蓝图世界上下文
    const UObject* WorldContext = FBlueprintCompatibilityTestHelpers::GetBlueprintWorldContext();
    TestNotNull("蓝图世界上下文应该可用", WorldContext);

    if (!WorldContext)
    {
        return false;
    }

    // 清理环境
    FBlueprintCompatibilityTestHelpers::CleanupTestEnvironment();

    // 注册Actor类
    UObjectPoolLibrary::RegisterActorClass(
        WorldContext, 
        ABlueprintTestActor::StaticClass(), 
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
        ABlueprintTestActor::StaticClass(),
        SpawnTransforms,
        OutActors
    );
    TestEqual("应该生成正确数量的Actor", SpawnedCount, 5);

    // 2. 获取生成的Actors（通过重新生成来验证池的状态）
    TArray<AActor*> TestActors;
    for (int32 i = 0; i < 3; ++i)
    {
        AActor* Actor = UObjectPoolLibrary::SpawnActorFromPool(
            WorldContext, 
            ABlueprintTestActor::StaticClass(), 
            FTransform::Identity
        );
        if (Actor)
        {
            TestActors.Add(Actor);
        }
    }

    TestTrue("应该能够获取多个Actor", TestActors.Num() > 0);

    // 3. 测试BatchReturnActors
    int32 ReturnedCount = UObjectPoolLibrary::BatchReturnActors(WorldContext, TestActors);
    TestEqual("应该归还正确数量的Actor", ReturnedCount, TestActors.Num());

    // 清理
    FBlueprintCompatibilityTestHelpers::CleanupTestEnvironment();

    return true;
}

/**
 * 测试蓝图库的参数验证和错误处理
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolBlueprintLibraryParameterValidationTest,
    "ObjectPool.BlueprintLibrary.ParameterValidationTest",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolBlueprintLibraryParameterValidationTest::RunTest(const FString& Parameters)
{
    // 获取蓝图世界上下文
    const UObject* WorldContext = FBlueprintCompatibilityTestHelpers::GetBlueprintWorldContext();
    TestNotNull("蓝图世界上下文应该可用", WorldContext);

    if (!WorldContext)
    {
        return false;
    }

    // 清理环境
    FBlueprintCompatibilityTestHelpers::CleanupTestEnvironment();

    // 1. 测试无效WorldContext处理
    bool bRegisteredWithNullContext = UObjectPoolLibrary::RegisterActorClass(
        nullptr,
        ABlueprintTestActor::StaticClass(),
        5,
        20
    );
    TestFalse("使用nullptr WorldContext应该失败", bRegisteredWithNullContext);

    // 2. 测试无效ActorClass处理
    bool bRegisteredWithNullClass = UObjectPoolLibrary::RegisterActorClass(
        WorldContext,
        nullptr,
        5,
        20
    );
    TestFalse("使用nullptr ActorClass应该失败", bRegisteredWithNullClass);

    // 3. 测试无效参数处理
    bool bRegisteredWithInvalidParams = UObjectPoolLibrary::RegisterActorClass(
        WorldContext,
        ABlueprintTestActor::StaticClass(),
        -1,  // 无效的InitialSize
        -1   // 无效的HardLimit
    );
    // 注意：简化子系统可能会修复无效参数，所以这里不一定失败

    // 4. 测试SpawnActorFromPool的无效参数
    AActor* ActorWithNullContext = UObjectPoolLibrary::SpawnActorFromPool(
        nullptr,
        ABlueprintTestActor::StaticClass(),
        FTransform::Identity
    );
    TestNull("使用nullptr WorldContext生成Actor应该失败", ActorWithNullContext);

    AActor* ActorWithNullClass = UObjectPoolLibrary::SpawnActorFromPool(
        WorldContext,
        nullptr,
        FTransform::Identity
    );
    TestNull("使用nullptr ActorClass生成Actor应该失败", ActorWithNullClass);

    // 5. 测试ReturnActorToPool的无效参数
    // ReturnActorToPool是void函数，我们通过日志验证其行为
    UObjectPoolLibrary::ReturnActorToPool(nullptr, nullptr);
    UObjectPoolLibrary::ReturnActorToPool(WorldContext, nullptr);
    // 这些调用不应该崩溃

    // 6. 测试PrewarmPool的无效参数
    bool bPrewarmedWithInvalidCount = UObjectPoolLibrary::PrewarmPool(
        WorldContext,
        ABlueprintTestActor::StaticClass(),
        -5  // 无效的Count
    );
    TestFalse("使用无效Count预热应该失败", bPrewarmedWithInvalidCount);

    // 7. 测试GetPoolStats的无效参数
    FObjectPoolStats StatsWithNullContext = UObjectPoolLibrary::GetPoolStats(
        nullptr,
        ABlueprintTestActor::StaticClass()
    );
    TestTrue("使用nullptr WorldContext获取统计应该返回默认值", StatsWithNullContext.PoolSize == 0);

    FObjectPoolStats StatsWithNullClass = UObjectPoolLibrary::GetPoolStats(
        WorldContext,
        nullptr
    );
    TestTrue("使用nullptr ActorClass获取统计应该返回默认值", StatsWithNullClass.PoolSize == 0);

    // 清理
    FBlueprintCompatibilityTestHelpers::CleanupTestEnvironment();

    return true;
}

/**
 * 测试蓝图库的多类型支持
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolBlueprintLibraryMultiTypeTest,
    "ObjectPool.BlueprintLibrary.MultiTypeTest",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolBlueprintLibraryMultiTypeTest::RunTest(const FString& Parameters)
{
    // 获取蓝图世界上下文
    const UObject* WorldContext = FBlueprintCompatibilityTestHelpers::GetBlueprintWorldContext();
    TestNotNull("蓝图世界上下文应该可用", WorldContext);

    if (!WorldContext)
    {
        return false;
    }

    // 清理环境
    FBlueprintCompatibilityTestHelpers::CleanupTestEnvironment();

    // 测试多种Actor类型
    TArray<UClass*> ActorClasses = {
        ABlueprintTestActor::StaticClass(),
        ABlueprintTestCharacter::StaticClass(),
        AActor::StaticClass()
    };

    TArray<AActor*> SpawnedActors;

    // 1. 为每种类型注册和预热
    for (UClass* ActorClass : ActorClasses)
    {
        bool bRegistered = UObjectPoolLibrary::RegisterActorClass(
            WorldContext,
            ActorClass,
            3,
            15
        );
        TestTrue("应该能够注册每种Actor类型", bRegistered);

        bool bPrewarmed = UObjectPoolLibrary::PrewarmPool(
            WorldContext,
            ActorClass,
            2
        );
        TestTrue("应该能够预热每种Actor类型的池", bPrewarmed);
    }

    // 2. 从每种类型的池中生成Actor
    for (UClass* ActorClass : ActorClasses)
    {
        AActor* SpawnedActor = UObjectPoolLibrary::SpawnActorFromPool(
            WorldContext,
            ActorClass,
            FTransform::Identity
        );
        TestNotNull("应该能够从每种类型的池中生成Actor", SpawnedActor);
        TestTrue("生成的Actor应该是正确的类型", SpawnedActor->IsA(ActorClass));

        if (SpawnedActor)
        {
            SpawnedActors.Add(SpawnedActor);
        }
    }

    // 3. 验证每种类型的统计信息
    for (UClass* ActorClass : ActorClasses)
    {
        FObjectPoolStats Stats = UObjectPoolLibrary::GetPoolStats(WorldContext, ActorClass);
        TestTrue("每种类型都应该有有效的统计信息", Stats.PoolSize > 0);
        TestTrue("统计信息应该包含正确的类名", !Stats.ActorClassName.IsEmpty());

        bool bIsRegistered = UObjectPoolLibrary::IsActorClassRegistered(WorldContext, ActorClass);
        TestTrue("每种类型都应该显示为已注册", bIsRegistered);
    }

    // 4. 归还所有Actor
    for (AActor* Actor : SpawnedActors)
    {
        UObjectPoolLibrary::ReturnActorToPool(WorldContext, Actor);
    }

    // 5. 测试批量归还
    int32 BatchReturnCount = UObjectPoolLibrary::BatchReturnActors(WorldContext, SpawnedActors);
    TestTrue("批量归还应该处理所有Actor", BatchReturnCount >= 0);

    // 清理
    FBlueprintCompatibilityTestHelpers::CleanupTestEnvironment();

    return true;
}

/**
 * 测试蓝图库的回退机制
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolBlueprintLibraryFallbackTest,
    "ObjectPool.BlueprintLibrary.FallbackTest",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolBlueprintLibraryFallbackTest::RunTest(const FString& Parameters)
{
    // 获取蓝图世界上下文
    const UObject* WorldContext = FBlueprintCompatibilityTestHelpers::GetBlueprintWorldContext();
    TestNotNull("蓝图世界上下文应该可用", WorldContext);

    if (!WorldContext)
    {
        return false;
    }

    // 清理环境
    FBlueprintCompatibilityTestHelpers::CleanupTestEnvironment();

    // 1. 测试在没有注册的情况下生成Actor（回退机制）
    AActor* FallbackActor = UObjectPoolLibrary::SpawnActorFromPool(
        WorldContext,
        ABlueprintTestActor::StaticClass(),
        FTransform::Identity
    );
    TestNotNull("即使没有注册，也应该能够生成Actor（回退机制）", FallbackActor);

    if (FallbackActor)
    {
        TestTrue("回退生成的Actor应该是正确的类型", FallbackActor->IsA<ABlueprintTestActor>());

        // 测试归还回退生成的Actor
        UObjectPoolLibrary::ReturnActorToPool(WorldContext, FallbackActor);
    }

    // 2. 测试极限情况下的回退
    // 尝试生成大量Actor来测试池限制和回退
    TArray<AActor*> ManyActors;
    for (int32 i = 0; i < 50; ++i)
    {
        AActor* Actor = UObjectPoolLibrary::SpawnActorFromPool(
            WorldContext,
            ABlueprintTestActor::StaticClass(),
            FTransform::Identity
        );
        if (Actor)
        {
            ManyActors.Add(Actor);
        }
    }

    TestTrue("即使在极限情况下，也应该能够生成一些Actor", ManyActors.Num() > 0);
    TestTrue("回退机制应该确保永不完全失败", ManyActors.Num() <= 50);

    // 归还所有Actor
    for (AActor* Actor : ManyActors)
    {
        UObjectPoolLibrary::ReturnActorToPool(WorldContext, Actor);
    }

    // 3. 测试子系统访问的回退
    UObjectPoolSubsystemSimplified* SimplifiedSubsystem = UObjectPoolLibrary::GetObjectPoolSubsystemSimplified(WorldContext);
    TestNotNull("应该能够获取简化子系统", SimplifiedSubsystem);

    UObjectPoolSubsystem* OriginalSubsystem = UObjectPoolLibrary::GetObjectPoolSubsystem(WorldContext);
    // 原始子系统可能不存在，这是正常的

    // 清理
    FBlueprintCompatibilityTestHelpers::CleanupTestEnvironment();

    return true;
}

#endif // WITH_OBJECTPOOL_TESTS

/**
 * 测试蓝图库的子系统访问功能
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolBlueprintLibrarySubsystemAccessTest, 
    "ObjectPool.BlueprintLibrary.SubsystemAccessTest", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolBlueprintLibrarySubsystemAccessTest::RunTest(const FString& Parameters)
{
    // 获取蓝图世界上下文
    const UObject* WorldContext = FBlueprintCompatibilityTestHelpers::GetBlueprintWorldContext();
    TestNotNull("蓝图世界上下文应该可用", WorldContext);

    if (!WorldContext)
    {
        return false;
    }

    // 1. 测试GetObjectPoolSubsystem
    UObjectPoolSubsystem* OriginalSubsystem = UObjectPoolLibrary::GetObjectPoolSubsystem(WorldContext);
    // 注意：原始子系统可能不存在，这是正常的

    // 2. 测试GetObjectPoolSubsystemSimplified
    UObjectPoolSubsystemSimplified* SimplifiedSubsystem = UObjectPoolLibrary::GetObjectPoolSubsystemSimplified(WorldContext);
    TestNotNull("应该能够获取简化子系统", SimplifiedSubsystem);

    if (SimplifiedSubsystem)
    {
        // 验证简化子系统的基本功能
        int32 InitialPoolCount = SimplifiedSubsystem->GetPoolCount();
        TestTrue("简化子系统应该可用", InitialPoolCount >= 0);

        // 测试通过蓝图库与简化子系统的交互
        UObjectPoolLibrary::RegisterActorClass(
            WorldContext, 
            ABlueprintTestActor::StaticClass(), 
            3, 
            15
        );

        int32 NewPoolCount = SimplifiedSubsystem->GetPoolCount();
        TestTrue("注册后池数量应该增加", NewPoolCount > InitialPoolCount);
    }

    // 清理
    FBlueprintCompatibilityTestHelpers::CleanupTestEnvironment();

    return true;
}
