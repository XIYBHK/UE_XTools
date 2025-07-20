// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 条件编译保护 - 只在非Shipping版本中编译测试
#if WITH_OBJECTPOOL_TESTS

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// ✅ 简化子系统依赖
#include "ObjectPoolSubsystemSimplified.h"
#include "ObjectPoolTypesSimplified.h"

/**
 * 简化子系统测试验证函数
 * 用于验证简化子系统的基础功能是否正常工作
 */
void RunObjectPoolSubsystemSimplifiedBasicTests()
{
    UE_LOG(LogTemp, Warning, TEXT("=== 开始ObjectPoolSubsystemSimplified基础测试 ==="));

    // 获取测试World
    UWorld* TestWorld = nullptr;
    if (GEngine && GEngine->GetWorldContexts().Num() > 0)
    {
        TestWorld = GEngine->GetWorldContexts()[0].World();
    }

    if (!TestWorld)
    {
        UE_LOG(LogTemp, Error, TEXT("无法获取测试World，跳过测试"));
        return;
    }

    // 获取简化子系统
    UObjectPoolSubsystemSimplified* Subsystem = TestWorld->GetSubsystem<UObjectPoolSubsystemSimplified>();
    if (!Subsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("无法获取简化子系统，跳过测试"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("✅ 成功获取简化子系统"));

    // ✅ 测试基础功能
    {
        // 清理环境
        Subsystem->ClearAllPools();
        Subsystem->ResetSubsystemStats();

        // 测试初始状态
        int32 InitialPoolCount = Subsystem->GetPoolCount();
        UE_LOG(LogTemp, Log, TEXT("初始池数量: %d"), InitialPoolCount);

        // 测试统计信息
        FObjectPoolSubsystemStats InitialStats = Subsystem->GetSubsystemStats();
        UE_LOG(LogTemp, Log, TEXT("初始统计 - Spawn: %d, Return: %d, 池创建: %d"), 
            InitialStats.TotalSpawnCalls, InitialStats.TotalReturnCalls, InitialStats.TotalPoolsCreated);

        bool bBasicTest = (InitialPoolCount == 0 && InitialStats.TotalSpawnCalls == 0);
        UE_LOG(LogTemp, Warning, TEXT("基础状态测试: %s"), bBasicTest ? TEXT("通过") : TEXT("失败"));
    }

    // ✅ 测试配置功能
    {
        FObjectPoolConfigSimplified TestConfig;
        TestConfig.ActorClass = AActor::StaticClass();
        TestConfig.InitialSize = 5;
        TestConfig.HardLimit = 20;

        bool bConfigSet = Subsystem->SetPoolConfig(AActor::StaticClass(), TestConfig);
        UE_LOG(LogTemp, Warning, TEXT("配置设置测试: %s"), bConfigSet ? TEXT("通过") : TEXT("失败"));

        if (bConfigSet)
        {
            FObjectPoolConfigSimplified RetrievedConfig = Subsystem->GetPoolConfig(AActor::StaticClass());
            bool bConfigMatch = (RetrievedConfig.InitialSize == TestConfig.InitialSize && 
                               RetrievedConfig.HardLimit == TestConfig.HardLimit);
            UE_LOG(LogTemp, Warning, TEXT("配置检索测试: %s"), bConfigMatch ? TEXT("通过") : TEXT("失败"));
        }
    }

    // ✅ 测试池操作
    {
        // 预热池
        int32 PrewarmCount = Subsystem->PrewarmPool(AActor::StaticClass(), 3);
        UE_LOG(LogTemp, Log, TEXT("预热池返回数量: %d"), PrewarmCount);

        // 检查池数量
        int32 PoolCount = Subsystem->GetPoolCount();
        UE_LOG(LogTemp, Warning, TEXT("池创建测试: %s"), (PoolCount > 0) ? TEXT("通过") : TEXT("失败"));

        // 获取Actor
        AActor* SpawnedActor = Subsystem->SpawnActorFromPool(AActor::StaticClass(), FTransform::Identity);
        bool bSpawnSuccess = IsValid(SpawnedActor);
        UE_LOG(LogTemp, Warning, TEXT("Actor生成测试: %s"), bSpawnSuccess ? TEXT("通过") : TEXT("失败"));

        if (bSpawnSuccess)
        {
            // 归还Actor
            bool bReturnSuccess = Subsystem->ReturnActorToPool(SpawnedActor);
            UE_LOG(LogTemp, Warning, TEXT("Actor归还测试: %s"), bReturnSuccess ? TEXT("通过") : TEXT("失败"));
        }
    }

    // ✅ 测试统计功能
    {
        FObjectPoolSubsystemStats FinalStats = Subsystem->GetSubsystemStats();
        UE_LOG(LogTemp, Log, TEXT("最终统计 - Spawn: %d, Return: %d, 池创建: %d"), 
            FinalStats.TotalSpawnCalls, FinalStats.TotalReturnCalls, FinalStats.TotalPoolsCreated);

        bool bStatsValid = (FinalStats.TotalSpawnCalls > 0 && FinalStats.TotalPoolsCreated > 0);
        UE_LOG(LogTemp, Warning, TEXT("统计信息测试: %s"), bStatsValid ? TEXT("通过") : TEXT("失败"));

        // 测试性能报告
        FString PerformanceReport = Subsystem->GeneratePerformanceReport();
        bool bReportValid = !PerformanceReport.IsEmpty() && PerformanceReport.Contains(TEXT("对象池子系统性能报告"));
        UE_LOG(LogTemp, Warning, TEXT("性能报告测试: %s"), bReportValid ? TEXT("通过") : TEXT("失败"));
    }

    // ✅ 测试错误处理
    {
        // 测试无效参数
        AActor* NullActor = Subsystem->SpawnActorFromPool(nullptr, FTransform::Identity);
        bool bNullHandling = (NullActor == nullptr);
        UE_LOG(LogTemp, Warning, TEXT("空指针处理测试: %s"), bNullHandling ? TEXT("通过") : TEXT("失败"));

        bool bNullReturn = Subsystem->ReturnActorToPool(nullptr);
        bool bNullReturnHandling = !bNullReturn;
        UE_LOG(LogTemp, Warning, TEXT("空指针归还测试: %s"), bNullReturnHandling ? TEXT("通过") : TEXT("失败"));
    }

    // ✅ 测试API兼容性
    {
        // 测试静态访问
        UObjectPoolSubsystemSimplified* StaticSubsystem = UObjectPoolSubsystemSimplified::Get(Subsystem);
        bool bStaticAccess = (StaticSubsystem == Subsystem);
        UE_LOG(LogTemp, Warning, TEXT("静态访问测试: %s"), bStaticAccess ? TEXT("通过") : TEXT("失败"));

        // 测试蓝图兼容方法
        AActor* BlueprintActor = Subsystem->SpawnActorFromPoolSimple(AActor::StaticClass());
        bool bBlueprintCompat = IsValid(BlueprintActor);
        UE_LOG(LogTemp, Warning, TEXT("蓝图兼容性测试: %s"), bBlueprintCompat ? TEXT("通过") : TEXT("失败"));

        if (BlueprintActor)
        {
            Subsystem->ReturnActorToPool(BlueprintActor);
        }
    }

    // 清理测试环境
    Subsystem->ClearAllPools();
    UE_LOG(LogTemp, Warning, TEXT("=== ObjectPoolSubsystemSimplified基础测试完成 ==="));
}

/**
 * 测试配置管理器功能
 */
void RunObjectPoolConfigManagerSimplifiedTests()
{
    UE_LOG(LogTemp, Warning, TEXT("=== 开始ConfigManagerSimplified测试 ==="));

    // 获取测试World和子系统
    UWorld* TestWorld = nullptr;
    if (GEngine && GEngine->GetWorldContexts().Num() > 0)
    {
        TestWorld = GEngine->GetWorldContexts()[0].World();
    }

    if (!TestWorld)
    {
        UE_LOG(LogTemp, Error, TEXT("无法获取测试World，跳过配置管理器测试"));
        return;
    }

    UObjectPoolSubsystemSimplified* Subsystem = TestWorld->GetSubsystem<UObjectPoolSubsystemSimplified>();
    if (!Subsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("无法获取简化子系统，跳过配置管理器测试"));
        return;
    }

    // 获取配置管理器
    auto& ConfigManager = Subsystem->GetConfigManager();

    // ✅ 测试默认配置
    {
        FObjectPoolConfigSimplified DefaultConfig = ConfigManager.GetDefaultConfig();
        bool bDefaultValid = IsValid(DefaultConfig.ActorClass) && DefaultConfig.InitialSize > 0;
        UE_LOG(LogTemp, Warning, TEXT("默认配置测试: %s"), bDefaultValid ? TEXT("通过") : TEXT("失败"));
    }

    // ✅ 测试配置设置和获取
    {
        FObjectPoolConfigSimplified TestConfig;
        TestConfig.ActorClass = AActor::StaticClass();
        TestConfig.InitialSize = 8;
        TestConfig.HardLimit = 40;

        bool bSetSuccess = ConfigManager.SetConfig(AActor::StaticClass(), TestConfig);
        UE_LOG(LogTemp, Warning, TEXT("配置设置测试: %s"), bSetSuccess ? TEXT("通过") : TEXT("失败"));

        if (bSetSuccess)
        {
            FObjectPoolConfigSimplified RetrievedConfig = ConfigManager.GetConfig(AActor::StaticClass());
            bool bGetSuccess = (RetrievedConfig.InitialSize == TestConfig.InitialSize);
            UE_LOG(LogTemp, Warning, TEXT("配置获取测试: %s"), bGetSuccess ? TEXT("通过") : TEXT("失败"));
        }
    }

    // ✅ 测试配置验证
    {
        FObjectPoolConfigSimplified InvalidConfig;
        InvalidConfig.ActorClass = nullptr;
        InvalidConfig.InitialSize = -1;
        InvalidConfig.HardLimit = -1;

        FString ErrorMessage;
        bool bValidationResult = ConfigManager.ValidateConfig(InvalidConfig, ErrorMessage);
        bool bValidationTest = !bValidationResult && !ErrorMessage.IsEmpty();
        UE_LOG(LogTemp, Warning, TEXT("配置验证测试: %s"), bValidationTest ? TEXT("通过") : TEXT("失败"));

        if (!ErrorMessage.IsEmpty())
        {
            UE_LOG(LogTemp, Log, TEXT("验证错误信息: %s"), *ErrorMessage);
        }
    }

    // ✅ 测试推荐配置生成
    {
        FObjectPoolConfigSimplified RecommendedConfig = ConfigManager.GenerateRecommendedConfig(AActor::StaticClass());
        bool bRecommendedValid = IsValid(RecommendedConfig.ActorClass) && RecommendedConfig.InitialSize > 0;
        UE_LOG(LogTemp, Warning, TEXT("推荐配置测试: %s"), bRecommendedValid ? TEXT("通过") : TEXT("失败"));
    }

    UE_LOG(LogTemp, Warning, TEXT("=== ConfigManagerSimplified测试完成 ==="));
}

/**
 * 运行所有简化子系统测试
 */
void RunAllObjectPoolSubsystemSimplifiedTests()
{
    UE_LOG(LogTemp, Warning, TEXT("========================================"));
    UE_LOG(LogTemp, Warning, TEXT("开始运行所有简化子系统测试"));
    UE_LOG(LogTemp, Warning, TEXT("========================================"));

    RunObjectPoolSubsystemSimplifiedBasicTests();
    RunObjectPoolConfigManagerSimplifiedTests();

    UE_LOG(LogTemp, Warning, TEXT("========================================"));
    UE_LOG(LogTemp, Warning, TEXT("所有简化子系统测试完成"));
    UE_LOG(LogTemp, Warning, TEXT("========================================"));
}

#endif // WITH_OBJECTPOOL_TESTS
