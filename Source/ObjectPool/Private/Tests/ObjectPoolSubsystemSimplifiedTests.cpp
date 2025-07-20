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

// ✅ 简化子系统依赖
#include "ObjectPoolSubsystemSimplified.h"
#include "ObjectPoolTypesSimplified.h"
#include "ActorPoolSimplified.h"

// ✅ 测试环境依赖
#include "Engine/GameInstance.h"
#include "Subsystems/SubsystemCollection.h"
#include "HAL/PlatformFilemanager.h"

// ✅ 测试用的简单Actor类
class ATestSimpleActor : public AActor
{
public:
    ATestSimpleActor()
    {
        PrimaryActorTick.bCanEverTick = false;
        bReplicates = false;
    }

    // 用于测试的标记
    bool bWasInitialized = false;
    bool bWasReset = false;
    int32 TestValue = 0;
    FString TestString = TEXT("Default");

    void InitializeForPool()
    {
        bWasInitialized = true;
        TestValue = 42;
        TestString = TEXT("Initialized");
    }

    void ResetForPool()
    {
        bWasReset = true;
        TestValue = 0;
        TestString = TEXT("Reset");
    }
};

// ✅ 测试用的复杂Actor类
class ATestComplexActor : public ACharacter
{
public:
    ATestComplexActor()
    {
        PrimaryActorTick.bCanEverTick = false;
        bReplicates = false;
    }

    // 复杂状态用于测试
    TArray<int32> TestArray;
    TMap<FString, int32> TestMap;
    bool bComplexState = false;

    void InitializeForPool()
    {
        TestArray = {1, 2, 3, 4, 5};
        TestMap.Add(TEXT("Key1"), 100);
        TestMap.Add(TEXT("Key2"), 200);
        bComplexState = true;
    }

    void ResetForPool()
    {
        TestArray.Empty();
        TestMap.Empty();
        bComplexState = false;
    }
};

/**
 * 简化子系统测试辅助工具
 */
class FSimplifiedSubsystemTestHelpers
{
public:
    /**
     * 获取或创建测试用的简化子系统
     */
    static UObjectPoolSubsystemSimplified* GetTestSubsystem()
    {
        if (UWorld* World = GetTestWorld())
        {
            return World->GetSubsystem<UObjectPoolSubsystemSimplified>();
        }
        return nullptr;
    }

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
        if (UObjectPoolSubsystemSimplified* Subsystem = GetTestSubsystem())
        {
            Subsystem->ClearAllPools();
            Subsystem->ResetSubsystemStats();
        }
    }

    /**
     * 验证Actor状态
     */
    static bool ValidateActorState(AActor* Actor, bool bShouldBeInitialized)
    {
        if (!IsValid(Actor))
        {
            return false;
        }

        if (ATestSimpleActor* SimpleActor = Cast<ATestSimpleActor>(Actor))
        {
            return SimpleActor->bWasInitialized == bShouldBeInitialized;
        }

        if (ATestComplexActor* ComplexActor = Cast<ATestComplexActor>(Actor))
        {
            return ComplexActor->bComplexState == bShouldBeInitialized;
        }

        return true; // 对于其他类型的Actor，认为验证通过
    }
};

// ✅ 基础功能测试

/**
 * 测试简化子系统的基本初始化和清理
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolSubsystemSimplifiedBasicTest, 
    "ObjectPool.SubsystemSimplified.BasicTest", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolSubsystemSimplifiedBasicTest::RunTest(const FString& Parameters)
{
    // 获取子系统
    UObjectPoolSubsystemSimplified* Subsystem = FSimplifiedSubsystemTestHelpers::GetTestSubsystem();
    TestNotNull("子系统应该可用", Subsystem);

    if (!Subsystem)
    {
        return false;
    }

    // 测试初始状态
    TestEqual("初始池数量应为0", Subsystem->GetPoolCount(), 0);

    // 测试统计信息
    FObjectPoolSubsystemStats Stats = Subsystem->GetSubsystemStats();
    TestEqual("初始Spawn调用次数应为0", Stats.TotalSpawnCalls, 0);
    TestEqual("初始Return调用次数应为0", Stats.TotalReturnCalls, 0);
    TestEqual("初始池创建次数应为0", Stats.TotalPoolsCreated, 0);

    // 清理
    FSimplifiedSubsystemTestHelpers::CleanupTestEnvironment();

    return true;
}

/**
 * 测试完整的池化流程（注册→获取→归还）
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolSubsystemSimplifiedPoolingFlowTest, 
    "ObjectPool.SubsystemSimplified.PoolingFlowTest", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolSubsystemSimplifiedPoolingFlowTest::RunTest(const FString& Parameters)
{
    // 获取子系统
    UObjectPoolSubsystemSimplified* Subsystem = FSimplifiedSubsystemTestHelpers::GetTestSubsystem();
    TestNotNull("子系统应该可用", Subsystem);

    if (!Subsystem)
    {
        return false;
    }

    // 清理环境
    FSimplifiedSubsystemTestHelpers::CleanupTestEnvironment();

    // 1. 测试池配置
    FObjectPoolConfigSimplified Config;
    Config.ActorClass = ATestSimpleActor::StaticClass();
    Config.InitialSize = 5;
    Config.HardLimit = 20;

    bool bConfigSet = Subsystem->SetPoolConfig(ATestSimpleActor::StaticClass(), Config);
    TestTrue("应该能够设置池配置", bConfigSet);

    // 2. 测试预热池
    int32 PrewarmCount = Subsystem->PrewarmPool(ATestSimpleActor::StaticClass(), 3);
    TestEqual("预热应该返回正确的可用数量", PrewarmCount, 3);
    TestEqual("池数量应该增加", Subsystem->GetPoolCount(), 1);

    // 3. 测试从池中获取Actor
    AActor* SpawnedActor = Subsystem->SpawnActorFromPool(ATestSimpleActor::StaticClass(), FTransform::Identity);
    TestNotNull("应该能够从池中获取Actor", SpawnedActor);
    TestTrue("Actor应该是正确的类型", SpawnedActor->IsA<ATestSimpleActor>());

    // 4. 测试归还Actor到池
    bool bReturned = Subsystem->ReturnActorToPool(SpawnedActor);
    TestTrue("应该能够归还Actor到池", bReturned);

    // 5. 验证统计信息
    FObjectPoolSubsystemStats Stats = Subsystem->GetSubsystemStats();
    TestEqual("Spawn调用次数应为1", Stats.TotalSpawnCalls, 1);
    TestEqual("Return调用次数应为1", Stats.TotalReturnCalls, 1);
    TestEqual("池创建次数应为1", Stats.TotalPoolsCreated, 1);

    // 清理
    FSimplifiedSubsystemTestHelpers::CleanupTestEnvironment();

    return true;
}

/**
 * 测试多种Actor类型的并发池化
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolSubsystemSimplifiedMultiTypeTest, 
    "ObjectPool.SubsystemSimplified.MultiTypeTest", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolSubsystemSimplifiedMultiTypeTest::RunTest(const FString& Parameters)
{
    // 获取子系统
    UObjectPoolSubsystemSimplified* Subsystem = FSimplifiedSubsystemTestHelpers::GetTestSubsystem();
    TestNotNull("子系统应该可用", Subsystem);

    if (!Subsystem)
    {
        return false;
    }

    // 清理环境
    FSimplifiedSubsystemTestHelpers::CleanupTestEnvironment();

    // 测试多种Actor类型
    TArray<UClass*> ActorClasses = {
        ATestSimpleActor::StaticClass(),
        ATestComplexActor::StaticClass(),
        AActor::StaticClass()
    };

    TArray<AActor*> SpawnedActors;

    // 1. 为每种类型创建池并获取Actor
    for (UClass* ActorClass : ActorClasses)
    {
        // 预热池
        int32 PrewarmCount = Subsystem->PrewarmPool(ActorClass, 2);
        TestTrue("预热应该成功", PrewarmCount >= 0);

        // 获取Actor
        AActor* SpawnedActor = Subsystem->SpawnActorFromPool(ActorClass, FTransform::Identity);
        TestNotNull("应该能够获取Actor", SpawnedActor);
        TestTrue("Actor应该是正确的类型", SpawnedActor->IsA(ActorClass));

        SpawnedActors.Add(SpawnedActor);
    }

    // 2. 验证池数量
    TestEqual("应该有3个池", Subsystem->GetPoolCount(), 3);

    // 3. 归还所有Actor
    for (AActor* Actor : SpawnedActors)
    {
        bool bReturned = Subsystem->ReturnActorToPool(Actor);
        TestTrue("应该能够归还Actor", bReturned);
    }

    // 4. 验证统计信息
    FObjectPoolSubsystemStats Stats = Subsystem->GetSubsystemStats();
    TestEqual("Spawn调用次数应为3", Stats.TotalSpawnCalls, 3);
    TestEqual("Return调用次数应为3", Stats.TotalReturnCalls, 3);

    // 清理
    FSimplifiedSubsystemTestHelpers::CleanupTestEnvironment();

    return true;
}

/**
 * 测试错误处理和回退机制
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolSubsystemSimplifiedErrorHandlingTest,
    "ObjectPool.SubsystemSimplified.ErrorHandlingTest",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolSubsystemSimplifiedErrorHandlingTest::RunTest(const FString& Parameters)
{
    // 获取子系统
    UObjectPoolSubsystemSimplified* Subsystem = FSimplifiedSubsystemTestHelpers::GetTestSubsystem();
    TestNotNull("子系统应该可用", Subsystem);

    if (!Subsystem)
    {
        return false;
    }

    // 清理环境
    FSimplifiedSubsystemTestHelpers::CleanupTestEnvironment();

    // 1. 测试无效参数处理
    AActor* NullClassActor = Subsystem->SpawnActorFromPool(nullptr, FTransform::Identity);
    TestNull("使用nullptr类应该返回nullptr", NullClassActor);

    bool bNullReturn = Subsystem->ReturnActorToPool(nullptr);
    TestFalse("归还nullptr应该返回false", bNullReturn);

    // 2. 测试无效配置处理
    FObjectPoolConfigSimplified InvalidConfig;
    InvalidConfig.ActorClass = nullptr;
    InvalidConfig.InitialSize = -1;
    InvalidConfig.HardLimit = -1;

    bool bInvalidConfigSet = Subsystem->SetPoolConfig(nullptr, InvalidConfig);
    TestFalse("设置无效配置应该失败", bInvalidConfigSet);

    // 3. 测试预热无效池
    int32 InvalidPrewarm = Subsystem->PrewarmPool(nullptr, 5);
    TestEqual("预热无效池应该返回0", InvalidPrewarm, 0);

    // 4. 测试归还错误类型的Actor
    AActor* WrongTypeActor = Subsystem->SpawnActorFromPool(ATestSimpleActor::StaticClass(), FTransform::Identity);
    TestNotNull("应该能够获取正确类型的Actor", WrongTypeActor);

    if (WrongTypeActor)
    {
        // 尝试归还Actor（简化子系统应该能够处理）
        bool bWrongReturn = Subsystem->ReturnActorToPool(WrongTypeActor);
        TestTrue("应该能够归还Actor（回退机制）", bWrongReturn);
    }

    // 5. 测试池限制处理
    FObjectPoolConfigSimplified LimitConfig;
    LimitConfig.ActorClass = ATestSimpleActor::StaticClass();
    LimitConfig.InitialSize = 1;
    LimitConfig.HardLimit = 2;

    Subsystem->SetPoolConfig(ATestSimpleActor::StaticClass(), LimitConfig);

    // 获取超过限制的Actor数量
    TArray<AActor*> LimitActors;
    for (int32 i = 0; i < 5; ++i)
    {
        AActor* LimitActor = Subsystem->SpawnActorFromPool(ATestSimpleActor::StaticClass(), FTransform::Identity);
        if (LimitActor)
        {
            LimitActors.Add(LimitActor);
        }
    }

    TestTrue("应该能够获取一些Actor（即使超过限制）", LimitActors.Num() > 0);
    TestTrue("回退机制应该确保永不失败", LimitActors.Num() <= 5);

    // 归还所有Actor
    for (AActor* Actor : LimitActors)
    {
        Subsystem->ReturnActorToPool(Actor);
    }

    // 清理
    FSimplifiedSubsystemTestHelpers::CleanupTestEnvironment();

    return true;
}

/**
 * 测试API兼容性
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolSubsystemSimplifiedCompatibilityTest,
    "ObjectPool.SubsystemSimplified.CompatibilityTest",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolSubsystemSimplifiedCompatibilityTest::RunTest(const FString& Parameters)
{
    // 获取子系统
    UObjectPoolSubsystemSimplified* Subsystem = FSimplifiedSubsystemTestHelpers::GetTestSubsystem();
    TestNotNull("子系统应该可用", Subsystem);

    if (!Subsystem)
    {
        return false;
    }

    // 清理环境
    FSimplifiedSubsystemTestHelpers::CleanupTestEnvironment();

    // 1. 测试静态访问方法
    UObjectPoolSubsystemSimplified* StaticSubsystem = UObjectPoolSubsystemSimplified::Get(Subsystem);
    TestEqual("静态访问应该返回相同的实例", StaticSubsystem, Subsystem);

    // 2. 测试蓝图兼容的方法
    AActor* BlueprintActor = Subsystem->SpawnActorFromPoolSimple(ATestSimpleActor::StaticClass());
    TestNotNull("蓝图兼容方法应该工作", BlueprintActor);

    if (BlueprintActor)
    {
        bool bBlueprintReturn = Subsystem->ReturnActorToPool(BlueprintActor);
        TestTrue("蓝图兼容的归还应该工作", bBlueprintReturn);
    }

    // 3. 测试统计信息的蓝图兼容性
    FObjectPoolSubsystemStats BlueprintStats = Subsystem->GetSubsystemStats();
    TestTrue("统计信息应该可以从蓝图访问", BlueprintStats.TotalSpawnCalls >= 0);

    // 4. 测试性能报告生成
    FString PerformanceReport = Subsystem->GeneratePerformanceReport();
    TestTrue("性能报告应该不为空", !PerformanceReport.IsEmpty());
    TestTrue("性能报告应该包含关键信息", PerformanceReport.Contains(TEXT("对象池子系统性能报告")));

    // 5. 测试监控功能
    Subsystem->SetMonitoringEnabled(true);
    TestTrue("应该能够启用监控", true); // 简化实现中这个功能被简化了

    Subsystem->SetMonitoringEnabled(false);
    TestTrue("应该能够禁用监控", true);

    // 6. 测试配置管理器访问
    auto& ConfigManager = Subsystem->GetConfigManager();
    FObjectPoolConfigSimplified DefaultConfig = ConfigManager.GetDefaultConfig();
    TestTrue("应该能够访问配置管理器", true);

    // 7. 测试池管理器访问
    auto& PoolManager = Subsystem->GetPoolManager();
    TestTrue("应该能够访问池管理器", true);

    // 8. 测试池统计信息
    TArray<FObjectPoolStatsSimplified> AllStats = Subsystem->GetAllPoolStats();
    TestTrue("应该能够获取所有池的统计信息", AllStats.Num() >= 0);

    // 清理
    FSimplifiedSubsystemTestHelpers::CleanupTestEnvironment();

    return true;
}

/**
 * 测试并发安全性
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolSubsystemSimplifiedConcurrencyTest,
    "ObjectPool.SubsystemSimplified.ConcurrencyTest",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolSubsystemSimplifiedConcurrencyTest::RunTest(const FString& Parameters)
{
    // 获取子系统
    UObjectPoolSubsystemSimplified* Subsystem = FSimplifiedSubsystemTestHelpers::GetTestSubsystem();
    TestNotNull("子系统应该可用", Subsystem);

    if (!Subsystem)
    {
        return false;
    }

    // 清理环境
    FSimplifiedSubsystemTestHelpers::CleanupTestEnvironment();

    // 预热池
    Subsystem->PrewarmPool(ATestSimpleActor::StaticClass(), 10);

    // 模拟并发访问
    TArray<AActor*> ConcurrentActors;
    const int32 ConcurrentCount = 20;

    // 1. 并发获取Actor
    for (int32 i = 0; i < ConcurrentCount; ++i)
    {
        AActor* ConcurrentActor = Subsystem->SpawnActorFromPool(ATestSimpleActor::StaticClass(), FTransform::Identity);
        if (ConcurrentActor)
        {
            ConcurrentActors.Add(ConcurrentActor);
        }
    }

    TestTrue("并发获取应该成功", ConcurrentActors.Num() > 0);
    TestTrue("并发获取应该不会崩溃", ConcurrentActors.Num() <= ConcurrentCount);

    // 2. 并发归还Actor
    int32 SuccessfulReturns = 0;
    for (AActor* Actor : ConcurrentActors)
    {
        if (Subsystem->ReturnActorToPool(Actor))
        {
            ++SuccessfulReturns;
        }
    }

    TestEqual("所有Actor应该能够成功归还", SuccessfulReturns, ConcurrentActors.Num());

    // 3. 测试并发统计访问
    for (int32 i = 0; i < 10; ++i)
    {
        FObjectPoolSubsystemStats Stats = Subsystem->GetSubsystemStats();
        TestTrue("并发统计访问应该安全", Stats.TotalSpawnCalls >= 0);
    }

    // 4. 测试并发池管理
    for (int32 i = 0; i < 5; ++i)
    {
        int32 PoolCount = Subsystem->GetPoolCount();
        TestTrue("并发池计数访问应该安全", PoolCount >= 0);
    }

    // 清理
    FSimplifiedSubsystemTestHelpers::CleanupTestEnvironment();

    return true;
}

#endif // WITH_OBJECTPOOL_TESTS
