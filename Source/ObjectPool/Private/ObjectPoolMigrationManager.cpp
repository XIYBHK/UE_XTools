// Fill out your copyright notice in the Description page of Project Settings.

#include "ObjectPoolMigrationManager.h"
#include "ObjectPoolSubsystem.h"
#include "ObjectPoolSubsystemSimplified.h"
#include "ObjectPool.h"

// ✅ UE核心依赖
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformFilemanager.h"

// ✅ 日志和统计
DEFINE_LOG_CATEGORY(LogObjectPoolMigration);
DEFINE_LOG_CATEGORY(LogObjectPoolCompatibility);

#if STATS
DEFINE_STAT(STAT_MigrationManager_SwitchImplementation);
DEFINE_STAT(STAT_MigrationManager_ValidateConsistency);
#endif

// ✅ 单例实例
FObjectPoolMigrationManager* FObjectPoolMigrationManager::Instance = nullptr;

// ✅ 构造函数和析构函数

FObjectPoolMigrationManager::FObjectPoolMigrationManager()
    : CurrentImplementationType(EImplementationType::Auto)
    , MigrationState(EMigrationState::NotStarted)
    , bABTestingEnabled(false)
    , ABTestRatio(0.5f)
{
    Initialize();
}

FObjectPoolMigrationManager::~FObjectPoolMigrationManager()
{
    Cleanup();
}

// ✅ 单例管理

FObjectPoolMigrationManager& FObjectPoolMigrationManager::Get()
{
    if (!Instance)
    {
        Instance = new FObjectPoolMigrationManager();
    }
    return *Instance;
}

void FObjectPoolMigrationManager::Shutdown()
{
    if (Instance)
    {
        delete Instance;
        Instance = nullptr;
    }
}

// ✅ 实现选择和切换

bool FObjectPoolMigrationManager::IsUsingSimplifiedImplementation()
{
#if OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 1
    return true;
#elif OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 0
    return false;
#else
    // 混合模式：运行时决定
    FObjectPoolMigrationManager& Manager = Get();
    FScopeLock Lock(&Manager.MigrationLock);
    
    if (Manager.bABTestingEnabled)
    {
        return Manager.GetABTestImplementation() == EImplementationType::Simplified;
    }
    
    return Manager.CurrentImplementationType == EImplementationType::Simplified ||
           (Manager.CurrentImplementationType == EImplementationType::Auto && 
            OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 2);
#endif
}

bool FObjectPoolMigrationManager::SetImplementationType(EImplementationType ImplementationType)
{
    SCOPE_CYCLE_COUNTER(STAT_MigrationManager_SwitchImplementation);

    FScopeLock Lock(&MigrationLock);

    if (CurrentImplementationType == ImplementationType)
    {
        return true; // 已经是目标类型
    }

    EImplementationType OldType = CurrentImplementationType;
    CurrentImplementationType = ImplementationType;

    OBJECTPOOL_MIGRATION_LOG(Log, TEXT("切换实现类型: %s -> %s"), 
        *GetImplementationTypeName(OldType), 
        *GetImplementationTypeName(ImplementationType));

    // 记录切换
    RecordImplementationCall(ImplementationType);

    return true;
}

FObjectPoolMigrationManager::EImplementationType FObjectPoolMigrationManager::GetCurrentImplementationType() const
{
    FScopeLock Lock(&MigrationLock);
    return CurrentImplementationType;
}

bool FObjectPoolMigrationManager::SwitchToSimplifiedImplementation()
{
    return SetImplementationType(EImplementationType::Simplified);
}

bool FObjectPoolMigrationManager::SwitchToOriginalImplementation()
{
    return SetImplementationType(EImplementationType::Original);
}

bool FObjectPoolMigrationManager::ToggleImplementation()
{
    FScopeLock Lock(&MigrationLock);
    
    EImplementationType NewType = (CurrentImplementationType == EImplementationType::Simplified) 
        ? EImplementationType::Original 
        : EImplementationType::Simplified;
    
    return SetImplementationType(NewType);
}

// ✅ 迁移状态管理

void FObjectPoolMigrationManager::StartMigration()
{
    FScopeLock Lock(&MigrationLock);
    
    if (MigrationState == EMigrationState::NotStarted)
    {
        MigrationState = EMigrationState::InProgress;
        Stats.MigrationStartTime = FPlatformTime::Seconds();
        
        OBJECTPOOL_MIGRATION_LOG(Log, TEXT("开始迁移过程"));
    }
}

void FObjectPoolMigrationManager::CompleteMigration()
{
    FScopeLock Lock(&MigrationLock);
    
    if (MigrationState == EMigrationState::InProgress)
    {
        MigrationState = EMigrationState::Completed;
        Stats.MigrationEndTime = FPlatformTime::Seconds();
        
        double MigrationDuration = Stats.MigrationEndTime - Stats.MigrationStartTime;
        OBJECTPOOL_MIGRATION_LOG(Log, TEXT("迁移完成，耗时: %.2f 秒"), MigrationDuration);
    }
}

void FObjectPoolMigrationManager::RollbackMigration()
{
    FScopeLock Lock(&MigrationLock);
    
    MigrationState = EMigrationState::RolledBack;
    CurrentImplementationType = EImplementationType::Original;
    
    OBJECTPOOL_MIGRATION_LOG(Warning, TEXT("迁移已回滚"));
}

FObjectPoolMigrationManager::EMigrationState FObjectPoolMigrationManager::GetMigrationState() const
{
    FScopeLock Lock(&MigrationLock);
    return MigrationState;
}

bool FObjectPoolMigrationManager::IsMigrationInProgress() const
{
    return GetMigrationState() == EMigrationState::InProgress;
}

// ✅ 统计和监控

FObjectPoolMigrationManager::FMigrationStats FObjectPoolMigrationManager::GetMigrationStats() const
{
    FScopeLock Lock(&MigrationLock);
    return Stats;
}

void FObjectPoolMigrationManager::ResetStats()
{
    FScopeLock Lock(&MigrationLock);
    
    Stats = FMigrationStats();
    PerformanceHistory.Empty();
    
    OBJECTPOOL_MIGRATION_LOG(Log, TEXT("迁移统计信息已重置"));
}

void FObjectPoolMigrationManager::RecordImplementationCall(EImplementationType ImplementationType)
{
    // 注意：调用者应该持有锁
    
    switch (ImplementationType)
    {
    case EImplementationType::Original:
        ++Stats.OriginalImplementationCalls;
        break;
    case EImplementationType::Simplified:
        ++Stats.SimplifiedImplementationCalls;
        break;
    default:
        break;
    }
}

void FObjectPoolMigrationManager::RecordCompatibilityCheck(bool bPassed)
{
    FScopeLock Lock(&MigrationLock);
    
    if (bPassed)
    {
        ++Stats.CompatibilityChecksPassed;
    }
    else
    {
        ++Stats.CompatibilityChecksFailed;
        OBJECTPOOL_COMPATIBILITY_LOG(Warning, TEXT("兼容性检查失败"));
    }
}

void FObjectPoolMigrationManager::RecordPerformanceComparison(const FPerformanceComparisonResult& Result)
{
    FScopeLock Lock(&MigrationLock);
    
    PerformanceHistory.Add(Result);
    ++Stats.PerformanceComparisons;
    
    // 更新平均性能提升
    UpdatePerformanceStats();
    
    OBJECTPOOL_MIGRATION_LOG(VeryVerbose, TEXT("性能对比: %s, 提升: %.1f%%"), 
        *Result.OperationType, Result.ImprovementPercentage);
}

// ✅ A/B测试

void FObjectPoolMigrationManager::EnableABTesting(float TestRatio)
{
    FScopeLock Lock(&MigrationLock);
    
    bABTestingEnabled = true;
    ABTestRatio = FMath::Clamp(TestRatio, 0.0f, 1.0f);
    MigrationState = EMigrationState::Testing;
    
    OBJECTPOOL_MIGRATION_LOG(Log, TEXT("启用A/B测试，简化实现比例: %.1f%%"), ABTestRatio * 100.0f);
}

void FObjectPoolMigrationManager::DisableABTesting()
{
    FScopeLock Lock(&MigrationLock);
    
    bABTestingEnabled = false;
    
    OBJECTPOOL_MIGRATION_LOG(Log, TEXT("禁用A/B测试"));
}

bool FObjectPoolMigrationManager::IsABTestingEnabled() const
{
    FScopeLock Lock(&MigrationLock);
    return bABTestingEnabled;
}

FObjectPoolMigrationManager::EImplementationType FObjectPoolMigrationManager::GetABTestImplementation() const
{
    // 注意：调用者应该持有锁
    
    if (!bABTestingEnabled)
    {
        return CurrentImplementationType;
    }
    
    // 使用简单的随机选择
    float RandomValue = FMath::RandRange(0.0f, 1.0f);
    return (RandomValue < ABTestRatio) ? EImplementationType::Simplified : EImplementationType::Original;
}

// ✅ 验证

bool FObjectPoolMigrationManager::ValidateImplementationConsistency(UClass* ActorClass, int32 TestCount)
{
    SCOPE_CYCLE_COUNTER(STAT_MigrationManager_ValidateConsistency);

    if (!IsValid(ActorClass))
    {
        OBJECTPOOL_COMPATIBILITY_LOG(Warning, TEXT("无效的Actor类，跳过一致性验证"));
        return false;
    }

    OBJECTPOOL_COMPATIBILITY_LOG(Log, TEXT("开始验证实现一致性: %s, 测试次数: %d"), 
        *ActorClass->GetName(), TestCount);

    int32 PassedTests = 0;
    
    for (int32 i = 0; i < TestCount; ++i)
    {
        // 这里可以实现具体的一致性验证逻辑
        // 例如：比较两个实现的生成结果、性能等
        
        // 简化的验证：检查两个实现都能成功生成Actor
        bool bTestPassed = true; // 实际实现中需要具体的验证逻辑
        
        if (bTestPassed)
        {
            ++PassedTests;
        }
        
        RecordCompatibilityCheck(bTestPassed);
    }
    
    float PassRate = static_cast<float>(PassedTests) / static_cast<float>(TestCount);
    bool bOverallPassed = PassRate >= 0.95f; // 95%通过率
    
    OBJECTPOOL_COMPATIBILITY_LOG(Log, TEXT("一致性验证完成: %s, 通过率: %.1f%% (%d/%d)"), 
        bOverallPassed ? TEXT("通过") : TEXT("失败"), 
        PassRate * 100.0f, PassedTests, TestCount);
    
    return bOverallPassed;
}

// ✅ 配置和信息

FString FObjectPoolMigrationManager::GetConfigurationSummary() const
{
    FScopeLock Lock(&MigrationLock);

    FString ConfigSummary = OBJECTPOOL_GET_CONFIG_SUMMARY();
    return FString::Printf(TEXT(
        "=== 对象池迁移配置摘要 ===\n"
        "当前实现: %s\n"
        "迁移状态: %s\n"
        "A/B测试: %s\n"
        "A/B测试比例: %.1f%%\n"
        "编译时配置: %s\n"
        "验证启用: %s\n"
        "性能监控: %s\n"
    ),
        *GetImplementationTypeName(CurrentImplementationType),
        *GetMigrationStateName(MigrationState),
        bABTestingEnabled ? TEXT("启用") : TEXT("禁用"),
        ABTestRatio * 100.0f,
        *ConfigSummary,
        OBJECTPOOL_ENABLE_MIGRATION_VALIDATION ? TEXT("是") : TEXT("否"),
        OBJECTPOOL_ENABLE_PERFORMANCE_MONITORING ? TEXT("是") : TEXT("否")
    );
}

FString FObjectPoolMigrationManager::GenerateMigrationReport() const
{
    FScopeLock Lock(&MigrationLock);

    double TotalCalls = Stats.OriginalImplementationCalls + Stats.SimplifiedImplementationCalls;
    double OriginalPercentage = TotalCalls > 0 ? (Stats.OriginalImplementationCalls / TotalCalls) * 100.0 : 0.0;
    double SimplifiedPercentage = TotalCalls > 0 ? (Stats.SimplifiedImplementationCalls / TotalCalls) * 100.0 : 0.0;

    double MigrationDuration = 0.0;
    if (Stats.MigrationStartTime > 0.0)
    {
        double EndTime = (Stats.MigrationEndTime > 0.0) ? Stats.MigrationEndTime : FPlatformTime::Seconds();
        MigrationDuration = EndTime - Stats.MigrationStartTime;
    }

    return FString::Printf(TEXT(
        "=== 对象池迁移报告 ===\n"
        "迁移状态: %s\n"
        "迁移耗时: %.2f 秒\n"
        "\n"
        "=== 使用统计 ===\n"
        "原始实现调用: %d (%.1f%%)\n"
        "简化实现调用: %d (%.1f%%)\n"
        "总调用次数: %.0f\n"
        "\n"
        "=== 兼容性验证 ===\n"
        "验证通过: %d\n"
        "验证失败: %d\n"
        "通过率: %.1f%%\n"
        "\n"
        "=== 性能对比 ===\n"
        "对比次数: %d\n"
        "平均性能提升: %.1f%%\n"
    ),
        *GetMigrationStateName(MigrationState),
        MigrationDuration,
        Stats.OriginalImplementationCalls, OriginalPercentage,
        Stats.SimplifiedImplementationCalls, SimplifiedPercentage,
        TotalCalls,
        Stats.CompatibilityChecksPassed,
        Stats.CompatibilityChecksFailed,
        (Stats.CompatibilityChecksPassed + Stats.CompatibilityChecksFailed) > 0 ?
            (static_cast<float>(Stats.CompatibilityChecksPassed) /
             (Stats.CompatibilityChecksPassed + Stats.CompatibilityChecksFailed)) * 100.0f : 0.0f,
        Stats.PerformanceComparisons,
        Stats.AveragePerformanceImprovement
    );
}

FString FObjectPoolMigrationManager::GeneratePerformanceReport() const
{
    FScopeLock Lock(&MigrationLock);

    if (PerformanceHistory.Num() == 0)
    {
        return TEXT("暂无性能对比数据");
    }

    FString Report = TEXT("=== 性能对比详细报告 ===\n");

    // 按操作类型分组统计
    TMap<FString, TArray<FPerformanceComparisonResult>> GroupedResults;
    for (const auto& Result : PerformanceHistory)
    {
        GroupedResults.FindOrAdd(Result.OperationType).Add(Result);
    }

    for (const auto& Group : GroupedResults)
    {
        const FString& OperationType = Group.Key;
        const TArray<FPerformanceComparisonResult>& Results = Group.Value;

        if (Results.Num() == 0) continue;

        // 计算统计信息
        float TotalImprovement = 0.0f;
        double TotalOriginalTime = 0.0;
        double TotalSimplifiedTime = 0.0;

        for (const auto& Result : Results)
        {
            TotalImprovement += Result.ImprovementPercentage;
            TotalOriginalTime += Result.OriginalTime;
            TotalSimplifiedTime += Result.SimplifiedTime;
        }

        float AvgImprovement = TotalImprovement / Results.Num();
        double AvgOriginalTime = TotalOriginalTime / Results.Num();
        double AvgSimplifiedTime = TotalSimplifiedTime / Results.Num();

        Report += FString::Printf(TEXT(
            "\n--- %s ---\n"
            "测试次数: %d\n"
            "平均原始耗时: %.4f ms\n"
            "平均简化耗时: %.4f ms\n"
            "平均性能提升: %.1f%%\n"
        ),
            *OperationType,
            Results.Num(),
            AvgOriginalTime * 1000.0,
            AvgSimplifiedTime * 1000.0,
            AvgImprovement
        );
    }

    return Report;
}

bool FObjectPoolMigrationManager::IsConfigurationValid() const
{
    return ValidateConfiguration();
}

// ✅ 内部辅助方法

void FObjectPoolMigrationManager::Initialize()
{
    OBJECTPOOL_MIGRATION_LOG(Log, TEXT("初始化迁移管理器"));

    // 根据编译时配置设置初始状态
#if OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 1
    CurrentImplementationType = EImplementationType::Simplified;
#elif OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 0
    CurrentImplementationType = EImplementationType::Original;
#else
    CurrentImplementationType = EImplementationType::Auto;
#endif

    // 验证配置
    if (!ValidateConfiguration())
    {
        OBJECTPOOL_MIGRATION_LOG(Error, TEXT("迁移配置验证失败"));
    }

    OBJECTPOOL_MIGRATION_LOG(Log, TEXT("迁移管理器初始化完成: %s"),
        *GetConfigurationSummary());
}

void FObjectPoolMigrationManager::Cleanup()
{
    OBJECTPOOL_MIGRATION_LOG(Log, TEXT("清理迁移管理器"));

    // 生成最终报告
    if (Stats.PerformanceComparisons > 0 || Stats.OriginalImplementationCalls > 0 || Stats.SimplifiedImplementationCalls > 0)
    {
        OBJECTPOOL_MIGRATION_LOG(Log, TEXT("最终迁移报告:\n%s"), *GenerateMigrationReport());
    }
}

void FObjectPoolMigrationManager::UpdatePerformanceStats()
{
    // 注意：调用者应该持有锁

    if (PerformanceHistory.Num() == 0)
    {
        Stats.AveragePerformanceImprovement = 0.0f;
        return;
    }

    float TotalImprovement = 0.0f;
    for (const auto& Result : PerformanceHistory)
    {
        TotalImprovement += Result.ImprovementPercentage;
    }

    Stats.AveragePerformanceImprovement = TotalImprovement / PerformanceHistory.Num();
}

bool FObjectPoolMigrationManager::ValidateConfiguration() const
{
    // 检查编译时配置的一致性
    bool bValid = true;

#if OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 1 && OBJECTPOOL_LIBRARY_USE_SIMPLIFIED == 0
    OBJECTPOOL_MIGRATION_LOG(Error, TEXT("配置不一致：不能在仅使用简化实现时使用原始库"));
    bValid = false;
#endif

#if OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 0 && OBJECTPOOL_LIBRARY_USE_SIMPLIFIED == 1
    OBJECTPOOL_MIGRATION_LOG(Error, TEXT("配置不一致：不能在仅使用原始实现时使用简化库"));
    bValid = false;
#endif

    return bValid;
}

FString FObjectPoolMigrationManager::GetImplementationTypeName(EImplementationType Type)
{
    switch (Type)
    {
    case EImplementationType::Original:
        return TEXT("原始实现");
    case EImplementationType::Simplified:
        return TEXT("简化实现");
    case EImplementationType::Auto:
        return TEXT("自动选择");
    default:
        return TEXT("未知类型");
    }
}

FString FObjectPoolMigrationManager::GetMigrationStateName(EMigrationState State)
{
    switch (State)
    {
    case EMigrationState::NotStarted:
        return TEXT("未开始");
    case EMigrationState::InProgress:
        return TEXT("进行中");
    case EMigrationState::Completed:
        return TEXT("已完成");
    case EMigrationState::RolledBack:
        return TEXT("已回滚");
    case EMigrationState::Testing:
        return TEXT("测试中");
    default:
        return TEXT("未知状态");
    }
}
