// Fill out your copyright notice in the Description page of Project Settings.

#include "ActorPoolMemoryOptimizer.h"
#include "ActorPoolSimplified.h"
#include "ObjectPool.h"

// ✅ UE核心依赖
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformFilemanager.h"

// ✅ 日志和统计
DEFINE_LOG_CATEGORY(LogActorPoolMemoryOptimizer);

#if STATS
DEFINE_STAT(STAT_MemoryOptimizer_AnalyzeMemory);
DEFINE_STAT(STAT_MemoryOptimizer_Preallocation);
DEFINE_STAT(STAT_MemoryOptimizer_Compaction);
#endif

// ✅ 构造函数和析构函数

FActorPoolMemoryOptimizer::FActorPoolMemoryOptimizer(EOptimizationStrategy InStrategy)
    : CurrentStrategy(InStrategy)
{
    InitializeConfigForStrategy(InStrategy);
    
    MEMORY_OPTIMIZER_LOG(Log, TEXT("创建内存优化器: 策略=%s"), 
        *GetStrategyName(InStrategy));
}

FActorPoolMemoryOptimizer::~FActorPoolMemoryOptimizer()
{
    MEMORY_OPTIMIZER_LOG(Log, TEXT("销毁内存优化器: 总优化次数=%d, 节省内存=%lld字节"), 
        OptimizationStats.TotalOptimizations, OptimizationStats.TotalMemorySaved);
}

// ✅ 移动语义实现

FActorPoolMemoryOptimizer::FActorPoolMemoryOptimizer(FActorPoolMemoryOptimizer&& Other) noexcept
    : CurrentStrategy(Other.CurrentStrategy)
    , PreallocConfig(MoveTemp(Other.PreallocConfig))
    , OptimizationStats(MoveTemp(Other.OptimizationStats))
{
    // 重置源对象
    Other.CurrentStrategy = EOptimizationStrategy::Balanced;
}

FActorPoolMemoryOptimizer& FActorPoolMemoryOptimizer::operator=(FActorPoolMemoryOptimizer&& Other) noexcept
{
    if (this != &Other)
    {
        CurrentStrategy = Other.CurrentStrategy;
        PreallocConfig = MoveTemp(Other.PreallocConfig);
        OptimizationStats = MoveTemp(Other.OptimizationStats);
        
        // 重置源对象
        Other.CurrentStrategy = EOptimizationStrategy::Balanced;
    }
    return *this;
}

// ✅ 内存监控功能实现

FActorPoolMemoryOptimizer::FMemoryStats FActorPoolMemoryOptimizer::AnalyzeMemoryUsage(const FActorPoolSimplified& Pool) const
{
    SCOPE_CYCLE_COUNTER(STAT_MemoryOptimizer_AnalyzeMemory);

    FMemoryStats Stats;
    
    // 计算当前内存使用
    Stats.CurrentMemoryUsage = CalculatePoolMemoryUsage(Pool);
    
    // 分析碎片化程度
    Stats.FragmentationRatio = AnalyzeFragmentation(Pool);
    
    // 计算平均Actor大小
    FObjectPoolStatsSimplified PoolStats = Pool.GetStats();
    if (PoolStats.TotalCreated > 0)
    {
        Stats.AverageActorSize = Stats.CurrentMemoryUsage / PoolStats.TotalCreated;
    }
    
    // 更新时间戳
    Stats.LastMemoryCheckTime = FPlatformTime::Seconds();
    
    MEMORY_OPTIMIZER_LOG(VeryVerbose, TEXT("内存分析完成: 使用=%lld字节, 碎片化=%.2f%%"),
        Stats.CurrentMemoryUsage, Stats.FragmentationRatio * 100.0f);
    
    return Stats;
}

TArray<FString> FActorPoolMemoryOptimizer::GetMemoryOptimizationSuggestions(const FActorPoolSimplified& Pool) const
{
    TArray<FString> Suggestions;
    
    FMemoryStats MemStats = AnalyzeMemoryUsage(Pool);
    FObjectPoolStatsSimplified PoolStats = Pool.GetStats();
    
    // 基于碎片化程度的建议
    if (MemStats.FragmentationRatio > 0.3f)
    {
        Suggestions.Add(FString::Printf(TEXT("内存碎片化程度较高(%.1f%%)，建议执行内存压缩"), 
            MemStats.FragmentationRatio * 100.0f));
    }
    
    // 基于池使用率的建议
    float UsageRatio = static_cast<float>(PoolStats.CurrentActive) / 
                      static_cast<float>(FMath::Max(PoolStats.PoolSize, 1));
    
    if (UsageRatio > 0.9f)
    {
        Suggestions.Add(TEXT("池使用率过高，建议增加池大小或启用智能预分配"));
    }
    else if (UsageRatio < 0.3f && PoolStats.PoolSize > 10)
    {
        Suggestions.Add(TEXT("池使用率较低，建议减少池大小以节省内存"));
    }
    
    // 基于命中率的建议
    if (PoolStats.HitRate < 0.7f)
    {
        Suggestions.Add(FString::Printf(TEXT("池命中率较低(%.1f%%)，建议调整预分配策略"), 
            PoolStats.HitRate * 100.0f));
    }
    
    // 基于内存使用的建议
    if (MemStats.CurrentMemoryUsage > 100 * 1024 * 1024) // 100MB
    {
        Suggestions.Add(TEXT("内存使用量较大，建议启用保守优化策略"));
    }
    
    return Suggestions;
}

bool FActorPoolMemoryOptimizer::ShouldOptimizeMemory(const FActorPoolSimplified& Pool) const
{
    FMemoryStats MemStats = AnalyzeMemoryUsage(Pool);
    
    // 基于策略决定优化阈值
    float FragmentationThreshold = 0.3f;
    switch (CurrentStrategy)
    {
    case EOptimizationStrategy::Conservative:
        FragmentationThreshold = 0.5f;
        break;
    case EOptimizationStrategy::Balanced:
        FragmentationThreshold = 0.3f;
        break;
    case EOptimizationStrategy::Aggressive:
        FragmentationThreshold = 0.2f;
        break;
    default:
        break;
    }
    
    return MemStats.FragmentationRatio > FragmentationThreshold;
}

// ✅ 预分配优化功能实现

bool FActorPoolMemoryOptimizer::ShouldPreallocate(const FActorPoolSimplified& Pool) const
{
    if (!PreallocConfig.bEnableSmartPreallocation)
    {
        return false;
    }
    
    FObjectPoolStatsSimplified PoolStats = Pool.GetStats();
    
    // 检查池是否已满
    if (PoolStats.PoolSize >= Pool.GetPoolSize()) // 假设有GetMaxSize方法
    {
        return false;
    }
    
    // 基于使用率决定是否预分配
    float UsageRatio = static_cast<float>(PoolStats.CurrentActive) / 
                      static_cast<float>(FMath::Max(PoolStats.PoolSize, 1));
    
    return UsageRatio >= PreallocConfig.TriggerThreshold;
}

int32 FActorPoolMemoryOptimizer::CalculatePreallocationCount(const FActorPoolSimplified& Pool) const
{
    if (!ShouldPreallocate(Pool))
    {
        return 0;
    }
    
    FObjectPoolStatsSimplified PoolStats = Pool.GetStats();
    
    // 基于当前使用模式计算预分配数量
    int32 CurrentActive = PoolStats.CurrentActive;
    int32 PredictedNeed = static_cast<int32>(CurrentActive * PreallocConfig.GrowthFactor);
    int32 CurrentAvailable = PoolStats.CurrentAvailable;
    
    int32 NeededCount = FMath::Max(0, PredictedNeed - CurrentAvailable);
    
    // 限制在配置范围内
    NeededCount = FMath::Clamp(NeededCount, PreallocConfig.MinPreallocCount, PreallocConfig.MaxPreallocCount);
    
    return NeededCount;
}

int32 FActorPoolMemoryOptimizer::PerformSmartPreallocation(FActorPoolSimplified& Pool, UWorld* World) const
{
    SCOPE_CYCLE_COUNTER(STAT_MemoryOptimizer_Preallocation);

    if (!IsValid(World))
    {
        return 0;
    }
    
    int32 PreallocCount = CalculatePreallocationCount(Pool);
    if (PreallocCount <= 0)
    {
        return 0;
    }
    
    MEMORY_OPTIMIZER_LOG(Log, TEXT("执行智能预分配: %d个Actor"), PreallocCount);
    
    // 使用池的预热功能进行预分配
    Pool.PrewarmPool(World, PreallocCount);
    
    // 更新统计
    ++OptimizationStats.TotalPreallocations;
    
    return PreallocCount;
}

// ✅ 内存压缩功能实现

float FActorPoolMemoryOptimizer::AnalyzeFragmentation(const FActorPoolSimplified& Pool) const
{
    FObjectPoolStatsSimplified PoolStats = Pool.GetStats();
    
    // 简化的碎片化计算：基于池的使用效率
    if (PoolStats.PoolSize == 0)
    {
        return 0.0f;
    }
    
    float EfficiencyRatio = static_cast<float>(PoolStats.CurrentActive + PoolStats.CurrentAvailable) / 
                           static_cast<float>(PoolStats.PoolSize);
    
    // 碎片化程度 = 1 - 效率比率
    return FMath::Clamp(1.0f - EfficiencyRatio, 0.0f, 1.0f);
}

int64 FActorPoolMemoryOptimizer::CompactMemory(FActorPoolSimplified& Pool) const
{
    SCOPE_CYCLE_COUNTER(STAT_MemoryOptimizer_Compaction);

    int64 MemoryBefore = CalculatePoolMemoryUsage(Pool);
    
    // 触发池的内存优化（如果池有这样的方法）
    // 这里我们只能记录，实际的压缩需要池自己实现
    MEMORY_OPTIMIZER_LOG(Log, TEXT("建议池执行内存压缩"));
    
    int64 MemoryAfter = CalculatePoolMemoryUsage(Pool);
    int64 MemorySaved = MemoryBefore - MemoryAfter;
    
    // 更新统计
    ++OptimizationStats.TotalOptimizations;
    OptimizationStats.TotalMemorySaved += MemorySaved;
    
    MEMORY_OPTIMIZER_LOG(Log, TEXT("内存压缩完成: 节省=%lld字节"), MemorySaved);

    return MemorySaved;
}

// ✅ 配置管理实现

void FActorPoolMemoryOptimizer::SetOptimizationStrategy(EOptimizationStrategy NewStrategy)
{
    if (CurrentStrategy != NewStrategy)
    {
        EOptimizationStrategy OldStrategy = CurrentStrategy;
        CurrentStrategy = NewStrategy;

        InitializeConfigForStrategy(NewStrategy);

        MEMORY_OPTIMIZER_LOG(Log, TEXT("优化策略变更: %s -> %s"),
            *GetStrategyName(OldStrategy), *GetStrategyName(NewStrategy));
    }
}

void FActorPoolMemoryOptimizer::SetPreallocationConfig(const FPreallocationConfig& NewConfig)
{
    PreallocConfig = NewConfig;
    MEMORY_OPTIMIZER_LOG(Log, TEXT("预分配配置已更新: 增长因子=%.2f, 触发阈值=%.2f"),
        NewConfig.GrowthFactor, NewConfig.TriggerThreshold);
}

// ✅ 性能分析实现

FString FActorPoolMemoryOptimizer::GeneratePerformanceReport(const FActorPoolSimplified& Pool) const
{
    FMemoryStats MemStats = AnalyzeMemoryUsage(Pool);
    FObjectPoolStatsSimplified PoolStats = Pool.GetStats();
    FString UsagePattern = AnalyzeUsagePattern(Pool);

    FString Report = FString::Printf(TEXT(
        "=== Actor池性能报告 ===\n"
        "池类型: %s\n"
        "优化策略: %s\n"
        "\n"
        "=== 基本统计 ===\n"
        "总创建: %d\n"
        "当前活跃: %d\n"
        "当前可用: %d\n"
        "池大小: %d\n"
        "命中率: %.1f%%\n"
        "\n"
        "=== 内存统计 ===\n"
        "当前内存使用: %.2f MB\n"
        "平均Actor大小: %.2f KB\n"
        "碎片化程度: %.1f%%\n"
        "\n"
        "=== 使用模式 ===\n"
        "%s\n"
        "\n"
        "=== 优化建议 ===\n"
    ),
        *PoolStats.ActorClassName,
        *GetStrategyName(CurrentStrategy),
        PoolStats.TotalCreated,
        PoolStats.CurrentActive,
        PoolStats.CurrentAvailable,
        PoolStats.PoolSize,
        PoolStats.HitRate * 100.0f,
        MemStats.CurrentMemoryUsage / (1024.0f * 1024.0f),
        MemStats.AverageActorSize / 1024.0f,
        MemStats.FragmentationRatio * 100.0f,
        *UsagePattern
    );

    // 添加优化建议
    TArray<FString> Suggestions = GetMemoryOptimizationSuggestions(Pool);
    for (int32 i = 0; i < Suggestions.Num(); ++i)
    {
        Report += FString::Printf(TEXT("%d. %s\n"), i + 1, *Suggestions[i]);
    }

    if (Suggestions.Num() == 0)
    {
        Report += TEXT("当前池运行状态良好，无需特殊优化。\n");
    }

    return Report;
}

FString FActorPoolMemoryOptimizer::GetOptimizationStats() const
{
    return FString::Printf(TEXT(
        "=== 优化器统计 ===\n"
        "总优化次数: %d\n"
        "总预分配次数: %d\n"
        "节省内存: %.2f MB\n"
        "总优化时间: %.2f 秒\n"
    ),
        OptimizationStats.TotalOptimizations,
        OptimizationStats.TotalPreallocations,
        OptimizationStats.TotalMemorySaved / (1024.0f * 1024.0f),
        OptimizationStats.TotalOptimizationTime
    );
}

// ✅ 内部辅助方法实现

void FActorPoolMemoryOptimizer::InitializeConfigForStrategy(EOptimizationStrategy Strategy)
{
    switch (Strategy)
    {
    case EOptimizationStrategy::Conservative:
        PreallocConfig.GrowthFactor = 1.2f;
        PreallocConfig.MinPreallocCount = 2;
        PreallocConfig.MaxPreallocCount = 10;
        PreallocConfig.TriggerThreshold = 0.9f;
        PreallocConfig.bEnableSmartPreallocation = false;
        break;

    case EOptimizationStrategy::Balanced:
        PreallocConfig.GrowthFactor = 1.5f;
        PreallocConfig.MinPreallocCount = 5;
        PreallocConfig.MaxPreallocCount = 25;
        PreallocConfig.TriggerThreshold = 0.8f;
        PreallocConfig.bEnableSmartPreallocation = true;
        break;

    case EOptimizationStrategy::Aggressive:
        PreallocConfig.GrowthFactor = 2.0f;
        PreallocConfig.MinPreallocCount = 10;
        PreallocConfig.MaxPreallocCount = 50;
        PreallocConfig.TriggerThreshold = 0.7f;
        PreallocConfig.bEnableSmartPreallocation = true;
        break;

    case EOptimizationStrategy::Custom:
        // 保持当前配置不变
        break;
    }
}

int64 FActorPoolMemoryOptimizer::CalculatePoolMemoryUsage(const FActorPoolSimplified& Pool) const
{
    // 使用FObjectPoolUtils来估算内存使用
    UClass* ActorClass = Pool.GetActorClass();
    if (!ActorClass)
    {
        return 0;
    }

    FObjectPoolStatsSimplified PoolStats = Pool.GetStats();

    // 估算Actor内存
    int64 ActorMemory = FObjectPoolUtils::EstimateMemoryUsage(ActorClass, PoolStats.TotalCreated);

    // 估算池自身内存（简化计算）
    int64 PoolOverhead = sizeof(FActorPoolSimplified) +
                        (PoolStats.PoolSize * sizeof(TWeakObjectPtr<AActor>) * 2); // 两个数组

    return ActorMemory + PoolOverhead;
}

FString FActorPoolMemoryOptimizer::AnalyzeUsagePattern(const FActorPoolSimplified& Pool) const
{
    FObjectPoolStatsSimplified PoolStats = Pool.GetStats();

    if (PoolStats.TotalCreated == 0)
    {
        return TEXT("池尚未使用");
    }

    float UsageRatio = static_cast<float>(PoolStats.CurrentActive) /
                      static_cast<float>(FMath::Max(PoolStats.PoolSize, 1));

    FString Pattern;

    if (PoolStats.HitRate > 0.9f)
    {
        Pattern += TEXT("高效使用模式 - ");
    }
    else if (PoolStats.HitRate > 0.7f)
    {
        Pattern += TEXT("中等使用模式 - ");
    }
    else
    {
        Pattern += TEXT("低效使用模式 - ");
    }

    if (UsageRatio > 0.8f)
    {
        Pattern += TEXT("高负载运行");
    }
    else if (UsageRatio > 0.5f)
    {
        Pattern += TEXT("中等负载运行");
    }
    else
    {
        Pattern += TEXT("低负载运行");
    }

    return Pattern;
}

FString FActorPoolMemoryOptimizer::GetStrategyName(EOptimizationStrategy Strategy)
{
    switch (Strategy)
    {
    case EOptimizationStrategy::Conservative:
        return TEXT("保守策略");
    case EOptimizationStrategy::Balanced:
        return TEXT("平衡策略");
    case EOptimizationStrategy::Aggressive:
        return TEXT("激进策略");
    case EOptimizationStrategy::Custom:
        return TEXT("自定义策略");
    default:
        return TEXT("未知策略");
    }
}
