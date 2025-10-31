/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#include "ObjectPoolManager.h"
#include "ActorPool.h"
#include "ObjectPool.h"

//  UE核心依赖
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformFilemanager.h"

//  日志和统计
DEFINE_LOG_CATEGORY(LogObjectPoolManager);

#if STATS
DEFINE_STAT(STAT_PoolManager_PerformMaintenance);
DEFINE_STAT(STAT_PoolManager_AutoResize);
DEFINE_STAT(STAT_PoolManager_SmartPreallocation);
#endif

//  构造函数和析构函数

FObjectPoolManager::FObjectPoolManager(EManagementStrategy InStrategy)
    : CurrentStrategy(InStrategy)
    , bAutoManagementEnabled(true)
{
    POOL_MANAGER_LOG(Log, TEXT("池管理器已创建: 策略=%s"), *GetStrategyName(InStrategy));
}

FObjectPoolManager::~FObjectPoolManager()
{
    POOL_MANAGER_LOG(Log, TEXT("池管理器已销毁: 总维护次数=%d"), Stats.TotalMaintenanceCount);
}

//  移动语义实现

FObjectPoolManager::FObjectPoolManager(FObjectPoolManager&& Other) noexcept
    : CurrentStrategy(Other.CurrentStrategy)
    , bAutoManagementEnabled(Other.bAutoManagementEnabled)
    , Stats(MoveTemp(Other.Stats))
    , UsageHistory(MoveTemp(Other.UsageHistory))
{
    Other.CurrentStrategy = EManagementStrategy::Manual;
    Other.bAutoManagementEnabled = false;
}

FObjectPoolManager& FObjectPoolManager::operator=(FObjectPoolManager&& Other) noexcept
{
    if (this != &Other)
    {
        CurrentStrategy = Other.CurrentStrategy;
        bAutoManagementEnabled = Other.bAutoManagementEnabled;
        Stats = MoveTemp(Other.Stats);
        UsageHistory = MoveTemp(Other.UsageHistory);
        
        Other.CurrentStrategy = EManagementStrategy::Manual;
        Other.bAutoManagementEnabled = false;
    }
    return *this;
}

//  池生命周期管理实现

void FObjectPoolManager::OnPoolCreated(UClass* ActorClass, TSharedPtr<FActorPool> Pool)
{
    if (!IsValid(ActorClass) || !Pool.IsValid())
    {
        return;
    }

    FScopeLock Lock(&ManagementLock);
    
    ++Stats.ManagedPoolCount;
    
    // 初始化使用历史
    UsageHistory.Add(ActorClass, TArray<float>());

    POOL_MANAGER_LOG(Log, TEXT("池已创建: %s, 管理池数量=%d"), 
        *ActorClass->GetName(), Stats.ManagedPoolCount);
}

void FObjectPoolManager::OnPoolDestroying(UClass* ActorClass)
{
    if (!IsValid(ActorClass))
    {
        return;
    }

    FScopeLock Lock(&ManagementLock);
    
    --Stats.ManagedPoolCount;
    
    // 清理使用历史
    UsageHistory.Remove(ActorClass);

    POOL_MANAGER_LOG(Log, TEXT("池即将销毁: %s, 剩余管理池数量=%d"), 
        *ActorClass->GetName(), Stats.ManagedPoolCount);
}

bool FObjectPoolManager::ShouldCreatePool(UClass* ActorClass) const
{
    if (!IsValid(ActorClass) || !bAutoManagementEnabled)
    {
        return false;
    }

    // 根据策略决定是否自动创建池
    switch (CurrentStrategy)
    {
    case EManagementStrategy::Aggressive:
        return true; // 激进策略：总是创建池

    case EManagementStrategy::Adaptive:
        // 自适应策略：根据Actor类型决定
        return ActorClass->IsChildOf<AActor>();

    case EManagementStrategy::Conservative:
        return false; // 保守策略：不自动创建

    case EManagementStrategy::Manual:
    default:
        return false; // 手动策略：不自动创建
    }
}

bool FObjectPoolManager::ShouldDestroyPool(UClass* ActorClass, const FActorPool& Pool) const
{
    if (!IsValid(ActorClass) || !bAutoManagementEnabled)
    {
        return false;
    }

    // 检查池是否长期未使用
    FObjectPoolStats PoolStats = Pool.GetStats();
    
    // 如果池为空且没有请求，考虑销毁
    if (PoolStats.PoolSize == 0 && PoolStats.TotalCreated == 0)
    {
        return CurrentStrategy == EManagementStrategy::Conservative;
    }

    return false;
}

//  智能管理功能实现

void FObjectPoolManager::PerformMaintenance(const TMap<UClass*, TSharedPtr<FActorPool>>& AllPools, EMaintenanceType MaintenanceType)
{
    SCOPE_CYCLE_COUNTER(STAT_PoolManager_PerformMaintenance);

    if (!bAutoManagementEnabled)
    {
        return;
    }

    FScopeLock Lock(&ManagementLock);
    
    double StartTime = FPlatformTime::Seconds();
    
    for (const auto& PoolPair : AllPools)
    {
        UClass* ActorClass = PoolPair.Key;
        TSharedPtr<FActorPool> Pool = PoolPair.Value;
        
        if (!IsValid(ActorClass) || !Pool.IsValid())
        {
            continue;
        }

        // 更新使用历史
        FObjectPoolStats PoolStats = Pool->GetStats();
        float UsageRatio = PoolStats.PoolSize > 0 ? 
            static_cast<float>(PoolStats.CurrentActive) / static_cast<float>(PoolStats.PoolSize) : 0.0f;
        UpdateUsageHistory(ActorClass, UsageRatio);

        // 执行不同类型的维护
        if (MaintenanceType == EMaintenanceType::All || MaintenanceType == EMaintenanceType::Cleanup)
        {
            if (ShouldPerformCleanup(*Pool))
            {
                // 这里可以触发池的清理操作
                ++Stats.CleanupCount;
            }
        }

        if (MaintenanceType == EMaintenanceType::All || MaintenanceType == EMaintenanceType::Resize)
        {
            if (AutoResizePool(ActorClass, *Pool))
            {
                ++Stats.AutoResizeCount;
            }
        }

        if (MaintenanceType == EMaintenanceType::All || MaintenanceType == EMaintenanceType::Preallocation)
        {
            if (ShouldPerformPreallocation(*Pool))
            {
                // 预分配需要World上下文，这里只记录需求
                ++Stats.PreallocationCount;
            }
        }
    }

    double EndTime = FPlatformTime::Seconds();
    Stats.TotalManagementTime += (EndTime - StartTime);
    Stats.LastMaintenanceTime = EndTime;
    ++Stats.TotalMaintenanceCount;

    POOL_MANAGER_LOG(VeryVerbose, TEXT("维护完成: 类型=%s, 耗时=%.4f秒"), 
        *GetMaintenanceTypeName(MaintenanceType), EndTime - StartTime);
}

TArray<FString> FObjectPoolManager::AnalyzePoolUsage(UClass* ActorClass, const FActorPool& Pool) const
{
    TArray<FString> Suggestions;

    if (!IsValid(ActorClass))
    {
        return Suggestions;
    }

    FObjectPoolStats PoolStats = Pool.GetStats();
    float UsageRatio = PoolStats.PoolSize > 0 ? 
        static_cast<float>(PoolStats.CurrentActive) / static_cast<float>(PoolStats.PoolSize) : 0.0f;

    // 分析使用率
    if (UsageRatio > AUTO_RESIZE_THRESHOLD)
    {
        Suggestions.Add(FString::Printf(TEXT("使用率过高(%.1f%%)，建议增加池大小"), UsageRatio * 100.0f));
    }
    else if (UsageRatio < CLEANUP_THRESHOLD)
    {
        Suggestions.Add(FString::Printf(TEXT("使用率较低(%.1f%%)，建议减少池大小"), UsageRatio * 100.0f));
    }

    // 分析命中率
    if (PoolStats.HitRate < 0.7f)
    {
        Suggestions.Add(FString::Printf(TEXT("命中率较低(%.1f%%)，建议增加预分配"), PoolStats.HitRate * 100.0f));
    }

    // 分析趋势
    float Trend = AnalyzeUsageTrend(ActorClass, Pool);
    if (Trend > 0.1f)
    {
        Suggestions.Add(TEXT("使用量呈上升趋势，建议提前扩容"));
    }
    else if (Trend < -0.1f)
    {
        Suggestions.Add(TEXT("使用量呈下降趋势，建议适当缩容"));
    }

    return Suggestions;
}

bool FObjectPoolManager::AutoResizePool(UClass* ActorClass, FActorPool& Pool) const
{
    SCOPE_CYCLE_COUNTER(STAT_PoolManager_AutoResize);

    if (!IsValid(ActorClass) || CurrentStrategy == EManagementStrategy::Manual)
    {
        return false;
    }

    int32 RecommendedSize = CalculateRecommendedSize(ActorClass, Pool);
    int32 CurrentMaxSize = Pool.GetMaxSize();

    if (RecommendedSize != CurrentMaxSize && RecommendedSize > 0)
    {
        Pool.SetMaxSize(RecommendedSize);
        
        POOL_MANAGER_LOG(Log, TEXT("自动调整池大小: %s, %d -> %d"), 
            *ActorClass->GetName(), CurrentMaxSize, RecommendedSize);
        
        return true;
    }

    return false;
}

int32 FObjectPoolManager::PerformSmartPreallocation(UClass* ActorClass, FActorPool& Pool, UWorld* World) const
{
    SCOPE_CYCLE_COUNTER(STAT_PoolManager_SmartPreallocation);

    if (!IsValid(ActorClass) || !IsValid(World) || !ShouldPerformPreallocation(Pool))
    {
        return 0;
    }

    FObjectPoolStats PoolStats = Pool.GetStats();
    
    // 计算预分配数量
    int32 PreallocCount = FMath::Max(1, PoolStats.CurrentActive / 2);
    
    // 限制预分配数量
    PreallocCount = FMath::Min(PreallocCount, 10);

    //  智能预分配机制 - 组件自动激活问题已解决
    Pool.PrewarmPool(World, PreallocCount);

            POOL_MANAGER_LOG(Log, TEXT("预分配完成: %s, 预分配数量=%d"), 
        *ActorClass->GetName(), PreallocCount);

    return PreallocCount;
}

//  策略管理实现

void FObjectPoolManager::SetManagementStrategy(EManagementStrategy NewStrategy)
{
    if (CurrentStrategy != NewStrategy)
    {
        EManagementStrategy OldStrategy = CurrentStrategy;
        CurrentStrategy = NewStrategy;

        POOL_MANAGER_LOG(Log, TEXT("管理策略变更: %s -> %s"),
            *GetStrategyName(OldStrategy), *GetStrategyName(NewStrategy));
    }
}

void FObjectPoolManager::SetAutoManagementEnabled(bool bEnable)
{
    if (bAutoManagementEnabled != bEnable)
    {
        bAutoManagementEnabled = bEnable;

        POOL_MANAGER_LOG(Log, TEXT("%s自动管理"), bEnable ? TEXT("启用") : TEXT("禁用"));
    }
}

//  统计和监控实现

FObjectPoolManager::FManagementStats FObjectPoolManager::GetManagementStats() const
{
    FScopeLock Lock(&ManagementLock);
    return Stats;
}

FString FObjectPoolManager::GenerateManagementReport(const TMap<UClass*, TSharedPtr<FActorPool>>& AllPools) const
{
    FScopeLock Lock(&ManagementLock);

    FString Report = FString::Printf(TEXT(
        "=== 池管理器报告 ===\n"
        "管理策略: %s\n"
        "自动管理: %s\n"
        "管理池数量: %d\n"
        "\n"
        "=== 维护统计 ===\n"
        "总维护次数: %d\n"
        "自动调整次数: %d\n"
        "预分配次数: %d\n"
        "清理次数: %d\n"
        "总管理时间: %.2f 秒\n"
        "\n"
        "=== 池状态分析 ===\n"
    ),
        *GetStrategyName(CurrentStrategy),
        bAutoManagementEnabled ? TEXT("启用") : TEXT("禁用"),
        Stats.ManagedPoolCount,
        Stats.TotalMaintenanceCount,
        Stats.AutoResizeCount,
        Stats.PreallocationCount,
        Stats.CleanupCount,
        Stats.TotalManagementTime
    );

    // 分析每个池的状态
    for (const auto& PoolPair : AllPools)
    {
        UClass* ActorClass = PoolPair.Key;
        TSharedPtr<FActorPool> Pool = PoolPair.Value;

        if (!IsValid(ActorClass) || !Pool.IsValid())
        {
            continue;
        }

        FObjectPoolStats PoolStats = Pool->GetStats();
        float UsageRatio = PoolStats.PoolSize > 0 ?
            static_cast<float>(PoolStats.CurrentActive) / static_cast<float>(PoolStats.PoolSize) : 0.0f;

        Report += FString::Printf(TEXT("- %s: 大小=%d, 使用率=%.1f%%, 命中率=%.1f%%\n"),
            *ActorClass->GetName(),
            PoolStats.PoolSize,
            UsageRatio * 100.0f,
            PoolStats.HitRate * 100.0f
        );
    }

    return Report;
}

void FObjectPoolManager::ResetStats()
{
    FScopeLock Lock(&ManagementLock);

    Stats = FManagementStats();
    UsageHistory.Empty();

    POOL_MANAGER_LOG(Log, TEXT("管理统计已重置"));
}

//  内部辅助方法实现

float FObjectPoolManager::AnalyzeUsageTrend(UClass* ActorClass, const FActorPool& Pool) const
{
    // 注意：调用者应该持有锁

    const TArray<float>* History = UsageHistory.Find(ActorClass);
    if (!History || History->Num() < 3)
    {
        return 0.0f; // 数据不足，无法分析趋势
    }

    // 简单的线性趋势分析
    float Sum = 0.0f;
    float WeightedSum = 0.0f;
    float WeightSum = 0.0f;

    for (int32 i = 0; i < History->Num(); ++i)
    {
        float Weight = static_cast<float>(i + 1);
        WeightedSum += (*History)[i] * Weight;
        WeightSum += Weight;
        Sum += (*History)[i];
    }

    float Average = Sum / History->Num();
    float WeightedAverage = WeightedSum / WeightSum;

    // 返回趋势值（正数表示上升，负数表示下降）
    return WeightedAverage - Average;
}

int32 FObjectPoolManager::CalculateRecommendedSize(UClass* ActorClass, const FActorPool& Pool) const
{
    FObjectPoolStats PoolStats = Pool.GetStats();

    if (PoolStats.PoolSize == 0)
    {
        return 10; // 默认推荐大小
    }

    float UsageRatio = static_cast<float>(PoolStats.CurrentActive) / static_cast<float>(PoolStats.PoolSize);
    float Trend = AnalyzeUsageTrend(ActorClass, Pool);

    int32 RecommendedSize = PoolStats.PoolSize;

    // 根据策略调整推荐大小
    switch (CurrentStrategy)
    {
    case EManagementStrategy::Aggressive:
        if (UsageRatio > 0.6f || Trend > 0.05f)
        {
            RecommendedSize = static_cast<int32>(PoolStats.PoolSize * 1.5f);
        }
        break;

    case EManagementStrategy::Adaptive:
        if (UsageRatio > AUTO_RESIZE_THRESHOLD)
        {
            RecommendedSize = static_cast<int32>(PoolStats.PoolSize * 1.2f);
        }
        else if (UsageRatio < CLEANUP_THRESHOLD)
        {
            RecommendedSize = static_cast<int32>(PoolStats.PoolSize * 0.8f);
        }
        break;

    case EManagementStrategy::Conservative:
        if (UsageRatio > 0.9f)
        {
            RecommendedSize = static_cast<int32>(PoolStats.PoolSize * 1.1f);
        }
        break;

    case EManagementStrategy::Manual:
    default:
        // 手动策略不自动调整
        break;
    }

    // 限制推荐大小的范围
    RecommendedSize = FMath::Clamp(RecommendedSize, 1, 1000);

    return RecommendedSize;
}

bool FObjectPoolManager::ShouldPerformCleanup(const FActorPool& Pool) const
{
    FObjectPoolStats PoolStats = Pool.GetStats();

    if (PoolStats.PoolSize == 0)
    {
        return false;
    }

    float UsageRatio = static_cast<float>(PoolStats.CurrentActive) / static_cast<float>(PoolStats.PoolSize);
    return UsageRatio < CLEANUP_THRESHOLD;
}

bool FObjectPoolManager::ShouldPerformPreallocation(const FActorPool& Pool) const
{
    FObjectPoolStats PoolStats = Pool.GetStats();

    if (PoolStats.PoolSize == 0)
    {
        return false;
    }

    float UsageRatio = static_cast<float>(PoolStats.CurrentActive) / static_cast<float>(PoolStats.PoolSize);
    return UsageRatio > PREALLOCATION_THRESHOLD && PoolStats.CurrentAvailable < 3;
}

void FObjectPoolManager::UpdateUsageHistory(UClass* ActorClass, float UsageRatio) const
{
    // 注意：调用者应该持有锁
    // 使用const_cast来修改mutable数据

    TMap<UClass*, TArray<float>>& MutableUsageHistory = const_cast<TMap<UClass*, TArray<float>>&>(UsageHistory);

    TArray<float>* History = MutableUsageHistory.Find(ActorClass);
    if (!History)
    {
        MutableUsageHistory.Add(ActorClass, TArray<float>());
        History = MutableUsageHistory.Find(ActorClass);
    }

    History->Add(UsageRatio);

    // 限制历史记录长度
    if (History->Num() > MAX_USAGE_HISTORY)
    {
        History->RemoveAt(0);
    }
}

FString FObjectPoolManager::GetStrategyName(EManagementStrategy Strategy)
{
    switch (Strategy)
    {
    case EManagementStrategy::Conservative:
        return TEXT("保守");
    case EManagementStrategy::Adaptive:
        return TEXT("自适应");
    case EManagementStrategy::Aggressive:
        return TEXT("激进");
    case EManagementStrategy::Manual:
        return TEXT("手动");
    default:
        return TEXT("未知");
    }
}

FString FObjectPoolManager::GetMaintenanceTypeName(EMaintenanceType MaintenanceType)
{
    switch (MaintenanceType)
    {
    case EMaintenanceType::Cleanup:
        return TEXT("清理");
    case EMaintenanceType::Resize:
        return TEXT("调整大小");
    case EMaintenanceType::Preallocation:
        return TEXT("预分配");
    case EMaintenanceType::Optimization:
        return TEXT("优化");
    case EMaintenanceType::All:
        return TEXT("全部维护");
    default:
        return TEXT("未知类型");
    }
}
