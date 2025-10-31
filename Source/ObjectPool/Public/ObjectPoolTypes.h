#pragma once

//  遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/Engine.h"
#include "ObjectPoolTypes.generated.h"

/**
 * 对象池生命周期事件类型
 * 用于跟踪Actor在对象池中的状态变化
 */
UENUM(BlueprintType, meta = (
    DisplayName = "对象池生命周期事件",
    ToolTip = "对象池中Actor的生命周期事件类型"))
enum class EObjectPoolLifecycleEvent : uint8
{
    /** Actor被创建时触发 */
    Created UMETA(DisplayName = "创建"),

    /** Actor从池中被激活时触发 */
    Activated UMETA(DisplayName = "激活"),

    /** Actor被归还到池中时触发 */
    ReturnedToPool UMETA(DisplayName = "归还"),

    /** Actor被销毁时触发 */
    Destroyed UMETA(DisplayName = "销毁")
};

/**
 * 预分配策略枚举
 * 控制对象池的预分配行为
 */
UENUM(BlueprintType, meta = (
    DisplayName = "预分配策略",
    ToolTip = "对象池的预分配策略类型"))
enum class EObjectPoolPreallocationStrategy : uint8
{
    /** 立即分配 - 一次性创建所有对象 */
    Immediate UMETA(DisplayName = "立即分配"),

    /** 渐进分配 - 分批次创建对象 */
    Progressive UMETA(DisplayName = "渐进分配"),

    /** 预测分配 - 基于使用模式预测 */
    Predictive UMETA(DisplayName = "预测分配"),

    /** 自适应分配 - 根据运行时状态调整 */
    Adaptive UMETA(DisplayName = "自适应分配")
};

/**
 * 对象池回退策略枚举
 * 当池用尽时的处理策略
 */
UENUM(BlueprintType, meta = (
    DisplayName = "回退策略",
    ToolTip = "对象池用尽时的回退策略"))
enum class EObjectPoolFallbackStrategy : uint8
{
    /** 拒绝请求 */
    Reject UMETA(DisplayName = "拒绝请求"),

    /** 动态创建 */
    CreateNew UMETA(DisplayName = "动态创建"),

    /** 等待回收 */
    WaitForReturn UMETA(DisplayName = "等待回收"),

    /** 强制回收最旧的对象 */
    ForceRecycleOldest UMETA(DisplayName = "强制回收")
};

/**
 * 批量操作失败策略
 */
UENUM(BlueprintType, meta = (
    DisplayName = "批量失败策略",
    ToolTip = "批量操作的失败处理策略"))
enum class EBatchFailurePolicy : uint8
{
    /** 全或无：任意一步失败则视为失败（调用方可据此回滚） */
    AllOrNothing UMETA(DisplayName = "全或无"),

    /** 尽力而为：尽可能完成，允许部分失败 */
    BestEffort UMETA(DisplayName = "尽力而为")
};

/**
 * 池操作结果
 */
UENUM(BlueprintType, meta = (
    DisplayName = "池操作结果",
    ToolTip = "用于标识对象池相关操作的结果"))
enum class EPoolOpResult : uint8
{
    Success UMETA(DisplayName = "成功"),
    FallbackSpawned UMETA(DisplayName = "回退生成"),
    NotPooled UMETA(DisplayName = "非池对象"),
    InvalidArgs UMETA(DisplayName = "参数无效")
};

/**
 * 对象池配置
 * 包含最核心的配置选项，简洁高效的设计
 */
USTRUCT(BlueprintType, meta = (
    DisplayName = "对象池配置",
    ToolTip = "对象池配置，包含核心的池化参数"))
struct OBJECTPOOL_API FObjectPoolConfig
{
    GENERATED_BODY()

    /** 池中对象的类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基础配置")
    TSubclassOf<AActor> ActorClass;

    /** 初始池大小 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基础配置", meta = (ClampMin = "0"))
    int32 InitialSize = 10;

    /** 池的硬性上限，0表示无限制 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基础配置", meta = (ClampMin = "0"))
    int32 HardLimit = 0;

    /** 池用尽时的回退策略 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基础配置")
    EObjectPoolFallbackStrategy FallbackStrategy = EObjectPoolFallbackStrategy::CreateNew;

    /** 预分配策略 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "高级配置")
    EObjectPoolPreallocationStrategy PreallocationStrategy = EObjectPoolPreallocationStrategy::Progressive;

    /** 是否在游戏开始时预热 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "高级配置")
    bool bPrewarmOnStart = true;

    /** 是否启用自动清理 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "高级配置")
    bool bAutoCleanup = true;

    /** 自动清理间隔(秒) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "高级配置", meta = (ClampMin = "1.0"))
    float AutoCleanupInterval = 60.0f;

    /** 预分配数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "预分配配置", meta = (ClampMin = "0"))
    int32 PreallocationCount = 0;

    /** 预分配策略 (用于预分配器内部) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "预分配配置")
    EObjectPoolPreallocationStrategy Strategy = EObjectPoolPreallocationStrategy::Progressive;

    /** 是否启用预热 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "预分配配置")
    bool bEnablePrewarm = true;

    /** 预分配延迟（毫秒） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "预分配配置", meta = (ClampMin = "0"))
    float PreallocationDelay = 0.0f;

    /** 是否启用内存预算 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "内存管理")
    bool bEnableMemoryBudget = false;

    /** 最大内存预算（MB） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "内存管理", meta = (ClampMin = "1"))
    int32 MaxMemoryBudgetMB = 100;

    /** 每帧最大分配数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "性能控制", meta = (ClampMin = "1"))
    int32 MaxAllocationsPerFrame = 10;

    /** 默认构造函数 */
    FObjectPoolConfig()
    {
        ActorClass = nullptr;
        InitialSize = 10;
        HardLimit = 0;
        FallbackStrategy = EObjectPoolFallbackStrategy::CreateNew;
        PreallocationStrategy = EObjectPoolPreallocationStrategy::Progressive;
        bPrewarmOnStart = true;
        bAutoCleanup = true;
        AutoCleanupInterval = 60.0f;
        PreallocationCount = 0;
        Strategy = EObjectPoolPreallocationStrategy::Progressive;
        bEnablePrewarm = true;
        PreallocationDelay = 0.0f;
        bEnableMemoryBudget = false;
        MaxMemoryBudgetMB = 100;
        MaxAllocationsPerFrame = 10;
    }
};

/**
 * 对象池统计信息
 * 用于监控对象池的使用情况和性能
 */
USTRUCT(BlueprintType, meta = (
    DisplayName = "对象池统计",
    ToolTip = "对象池的统计信息"))
struct OBJECTPOOL_API FObjectPoolStats
{
    GENERATED_BODY()

    /** 累计创建的对象数量 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    int32 TotalCreated = 0;

    /** 当前活跃对象数量 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    int32 CurrentActive = 0;

    /** 当前可用对象数量 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    int32 CurrentAvailable = 0;

    /** 池的大小 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    int32 PoolSize = 0;

    /** Actor类名称 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    FString ActorClassName;

    /** 累计获取次数 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    int32 TotalAcquired = 0;

    /** 累计归还次数 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    int32 TotalReleased = 0;

    /** 池命中率(0.0-1.0) */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    float HitRate = 0.0f;

    /** 创建时间 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    FDateTime CreationTime;

    /** 最后使用时间 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    FDateTime LastUsedTime;

    /** 默认构造函数 */
    FObjectPoolStats()
    {
        TotalCreated = 0;
        CurrentActive = 0;
        CurrentAvailable = 0;
        PoolSize = 0;
        ActorClassName = TEXT("");
        TotalAcquired = 0;
        TotalReleased = 0;
        HitRate = 0.0f;
        CreationTime = FDateTime::Now();
        LastUsedTime = FDateTime::Now();
    }

    /** 转换为字符串 */
    FString ToString() const
    {
        return FString::Printf(TEXT("池[%s]: 活跃=%d, 可用=%d, 总计=%d, 命中率=%.2f%%"), 
            *ActorClassName, CurrentActive, CurrentAvailable, TotalCreated, HitRate * 100.0f);
    }
};

/**
 * Actor重置配置
 * 用于控制Actor状态重置的行为
 */
USTRUCT(BlueprintType, meta = (
    DisplayName = "Actor重置配置",
    ToolTip = "控制Actor归还对象池时的状态重置行为"))
struct OBJECTPOOL_API FActorResetConfig
{
    GENERATED_BODY()

    /** 是否重置Transform */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基础重置")
    bool bResetTransform = true;

    /** 是否重置物理状态 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基础重置")
    bool bResetPhysics = true;

    /** 是否重置AI状态 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基础重置")
    bool bResetAI = true;

    /** 是否重置动画状态 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基础重置")
    bool bResetAnimation = true;

    /** 是否清理定时器 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基础重置")
    bool bClearTimers = true;

    /** 是否重置音频状态 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基础重置")
    bool bResetAudio = true;

    /** 是否重置粒子系统 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基础重置")
    bool bResetParticles = true;

    /** 是否重置网络状态 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基础重置")
    bool bResetNetwork = false;

    /** 默认构造函数 */
    FActorResetConfig() = default;
};

/**
 * Actor重置统计信息
 * 用于跟踪Actor状态重置的性能和效果
 */
USTRUCT(BlueprintType, meta = (
    DisplayName = "Actor重置统计",
    ToolTip = "Actor状态重置的统计信息"))
struct OBJECTPOOL_API FActorResetStats
{
    GENERATED_BODY()
public:
    /** 总重置次数 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    int32 TotalResets = 0;

    /** 成功重置次数 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    int32 SuccessfulResets = 0;

    /** 失败重置次数 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    int32 FailedResets = 0;

    /** 平均重置时间（毫秒） */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    float AverageResetTimeMs = 0.0f;

    /** 最后重置时间 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    FDateTime LastResetTime;

    /** 默认构造函数 */
    FActorResetStats() = default;

    /** 更新统计信息 */
    void UpdateStats(bool bSuccess, float TimeMs)
    {
        TotalResets++;
        if (bSuccess)
        {
            SuccessfulResets++;
        }
        else
        {
            FailedResets++;
        }
        
        // 更新平均时间
        AverageResetTimeMs = (AverageResetTimeMs * (TotalResets - 1) + TimeMs) / TotalResets;
        LastResetTime = FDateTime::Now();
    }
};

/**
 * 预分配统计信息
 * 用于跟踪对象池预分配的性能和状态
 */
USTRUCT(BlueprintType, meta = (
    DisplayName = "预分配统计",
    ToolTip = "对象池预分配操作的统计信息"))
struct OBJECTPOOL_API FObjectPoolPreallocationStats
{
    GENERATED_BODY()

    /** 已预分配的对象数量 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    int32 PreallocatedCount = 0;

    /** 预分配操作次数 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    int32 PreallocationOperations = 0;

    /** 预分配失败次数 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    int32 FailedPreallocations = 0;

    /** 预分配总耗时(毫秒) */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    float TotalPreallocationTimeMs = 0.0f;

    /** 平均预分配耗时(毫秒) */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    float AveragePreallocationTimeMs = 0.0f;

    /** 内存使用量(字节) */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    int64 MemoryUsageBytes = 0;

    /** 目标预分配数量 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    int32 TargetCount = 0;

    /** 预分配开始时间 */
    UPROPERTY(BlueprintReadOnly, Category = "时间统计")
    FDateTime PreallocationStartTime;

    /** 预分配完成时间 */
    UPROPERTY(BlueprintReadOnly, Category = "时间统计")
    FDateTime PreallocationEndTime;

    /** 默认构造函数 */
    FObjectPoolPreallocationStats() = default;

    /** 获取完成百分比 */
    float GetCompletionPercentage() const
    {
        if (TargetCount <= 0) return 100.0f;
        return FMath::Clamp((float)PreallocatedCount / TargetCount * 100.0f, 0.0f, 100.0f);
    }

    /** 更新统计信息 */
    void UpdateStats(int32 AllocatedCount, float TimeMs, int64 MemoryBytes)
    {
        PreallocatedCount += AllocatedCount;
        TotalPreallocationTimeMs += TimeMs;
        PreallocationOperations++;
        MemoryUsageBytes += MemoryBytes;
        
        if (PreallocationOperations > 0)
        {
            AveragePreallocationTimeMs = TotalPreallocationTimeMs / PreallocationOperations;
        }
    }
};

/**
 * 生命周期事件统计信息
 * 用于跟踪每个Actor的生命周期事件调用情况
 */
USTRUCT(BlueprintType, meta = (
    DisplayName = "生命周期统计",
    ToolTip = "Actor的生命周期事件统计信息"))
struct OBJECTPOOL_API FObjectPoolLifecycleStats
{
    GENERATED_BODY()

    /** 创建事件调用次数 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    int32 CreatedCount = 0;

    /** 激活事件调用次数 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    int32 ActivatedCount = 0;

    /** 归还事件调用次数 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    int32 ReturnedCount = 0;

    /** 销毁事件调用次数 */
    UPROPERTY(BlueprintReadOnly, Category = "统计")
    int32 DestroyedCount = 0;

    /** 默认构造函数 */
    FObjectPoolLifecycleStats() = default;
};

/**
 * 对象池调试信息
 * 用于调试和分析对象池状态
 */
USTRUCT(BlueprintType, meta = (
    DisplayName = "对象池调试信息",
    ToolTip = "对象池的调试信息"))
struct OBJECTPOOL_API FObjectPoolDebugInfo
{
    GENERATED_BODY()

    /** 池名称 */
    UPROPERTY(BlueprintReadOnly, Category = "调试")
    FString PoolName;

    /** 是否健康 */
    UPROPERTY(BlueprintReadOnly, Category = "调试")
    bool bIsHealthy = true;

    /** 使用率 */
    UPROPERTY(BlueprintReadOnly, Category = "调试")
    float UsageRate = 0.0f;

    /** 效率评分 */
    UPROPERTY(BlueprintReadOnly, Category = "调试")
    float EfficiencyScore = 0.0f;

    /** 建议信息 */
    UPROPERTY(BlueprintReadOnly, Category = "调试")
    TArray<FString> Suggestions;

    /** 警告信息 */
    UPROPERTY(BlueprintReadOnly, Category = "调试")
    TArray<FString> Warnings;

    /** 默认构造函数 */
    FObjectPoolDebugInfo() = default;

    /** 添加警告信息 */
    void AddWarning(const FString& Warning)
    {
        Warnings.Add(Warning);
        bIsHealthy = false;
    }
};