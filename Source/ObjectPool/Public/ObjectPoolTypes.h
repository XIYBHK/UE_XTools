// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// ⚠️⚠️⚠️ 严重废弃警告：此文件已被标记为废弃，将在下一个版本中移除 ⚠️⚠️⚠️
//
// 📋 迁移指南：
// - 新代码请使用 ObjectPoolTypesSimplified.h 中的简化类型
// - 现有代码已在任务 7.1 中迁移到新类型
// - 此文件仅为向后兼容性保留，不应在新代码中使用
//
// 🔄 替代方案：
// - FObjectPoolStats → FObjectPoolStatsSimplified
// - FObjectPoolConfig → FObjectPoolConfigSimplified
// - EObjectPoolState → EObjectPoolStateSimplified
//
// 📞 如需帮助，请参考迁移文档或联系开发团队

#ifdef _MSC_VER
#pragma message("警告: ObjectPoolTypes.h 已废弃，请使用 ObjectPoolTypesSimplified.h")
#endif

#if defined(__GNUC__) || defined(__clang__)
#warning "ObjectPoolTypes.h 已废弃，请使用 ObjectPoolTypesSimplified.h"
#endif

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/Engine.h"

// ✅ 生成的头文件必须放在最后
#include "ObjectPoolTypes.generated.h"

/**
 * 对象池统计信息
 * 用于监控和调试对象池的使用情况
 */
USTRUCT(BlueprintType, meta = (
    DisplayName = "对象池统计信息",
    ToolTip = "包含对象池的详细使用统计信息，用于性能监控、调试和优化。提供创建数量、命中率、内存使用等关键指标。",
    Keywords = "统计,监控,性能,调试,对象池,指标",
    Category = "XTools|统计",
    ShortToolTip = "对象池使用统计"))
struct OBJECTPOOL_API FObjectPoolStats
{
    GENERATED_BODY()

    /** 池中总共创建的Actor数量 */
    UPROPERTY(BlueprintReadOnly, Category = "统计", meta = (
        DisplayName = "总创建数",
        ToolTip = "自池创建以来总共创建的Actor数量"))
    int32 TotalCreated = 0;

    /** 当前活跃的Actor数量 */
    UPROPERTY(BlueprintReadOnly, Category = "统计", meta = (
        DisplayName = "当前活跃数",
        ToolTip = "当前正在使用中的Actor数量"))
    int32 CurrentActive = 0;

    /** 当前可用的Actor数量 */
    UPROPERTY(BlueprintReadOnly, Category = "统计", meta = (
        DisplayName = "当前可用数",
        ToolTip = "当前在池中可用的Actor数量"))
    int32 CurrentAvailable = 0;

    /** 池的最大容量 */
    UPROPERTY(BlueprintReadOnly, Category = "统计", meta = (
        DisplayName = "池大小",
        ToolTip = "对象池的最大容量"))
    int32 PoolSize = 0;

    /** 池命中率 (0.0 - 1.0) */
    UPROPERTY(BlueprintReadOnly, Category = "统计", meta = (
        DisplayName = "命中率",
        ToolTip = "池的命中率，范围0.0-1.0"))
    float HitRate = 0.0f;

    /** Actor类名 */
    UPROPERTY(BlueprintReadOnly, Category = "统计", meta = (
        DisplayName = "Actor类名",
        ToolTip = "池化的Actor类名称"))
    FString ActorClassName;

    /** 池创建时间 */
    UPROPERTY(BlueprintReadOnly, Category = "统计", meta = (
        DisplayName = "创建时间",
        ToolTip = "对象池的创建时间"))
    FDateTime CreationTime;

    /** 最后一次使用时间 */
    UPROPERTY(BlueprintReadOnly, Category = "统计", meta = (
        DisplayName = "最后使用时间",
        ToolTip = "对象池最后一次被使用的时间"))
    FDateTime LastUsedTime;

    /** 默认构造函数 */
    FObjectPoolStats()
    {
        CreationTime = FDateTime::Now();
        LastUsedTime = CreationTime;
    }

    /** 
     * 构造函数
     * @param InActorClassName Actor类名
     * @param InPoolSize 池大小
     */
    FObjectPoolStats(const FString& InActorClassName, int32 InPoolSize)
        : PoolSize(InPoolSize)
        , ActorClassName(InActorClassName)
    {
        CreationTime = FDateTime::Now();
        LastUsedTime = CreationTime;
    }

    /** 更新命中率 */
    void UpdateHitRate(int32 Hits, int32 TotalRequests)
    {
        if (TotalRequests > 0)
        {
            HitRate = static_cast<float>(Hits) / static_cast<float>(TotalRequests);
        }
        LastUsedTime = FDateTime::Now();
    }

    /** 获取格式化的统计信息字符串 */
    FString ToString() const
    {
        return FString::Printf(TEXT("对象池[%s]: 池大小=%d, 活跃数=%d, 可用数=%d, 总创建=%d, 命中率=%.2f%%"),
            *ActorClassName, PoolSize, CurrentActive, CurrentAvailable, TotalCreated, HitRate * 100.0f);
    }
};

/**
 * Actor池统计信息类型别名
 * 为了保持API一致性，FActorPoolStats是FObjectPoolStats的别名
 */
using FActorPoolStats = FObjectPoolStats;

/**
 * 对象池配置信息
 * 用于配置对象池的行为参数
 */
USTRUCT(BlueprintType)
struct OBJECTPOOL_API FObjectPoolConfig
{
    GENERATED_BODY()

    /** 要池化的Actor类 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置")
    TSubclassOf<AActor> ActorClass;

    /** 初始池大小 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (ClampMin = "1", ClampMax = "1000"))
    int32 InitialSize = 10;

    /** 池的硬限制 (0表示无限制) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (ClampMin = "0", ClampMax = "10000"))
    int32 HardLimit = 0;

    /** 是否启用预热 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置")
    bool bEnablePrewarm = true;

    /** 是否启用自动扩展 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置")
    bool bAutoExpand = true;

    /** 是否启用自动收缩 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置")
    bool bAutoShrink = false;

    /** 默认构造函数 */
    FObjectPoolConfig()
    {
        ActorClass = nullptr;
    }

    /** 
     * 构造函数
     * @param InActorClass Actor类
     * @param InInitialSize 初始大小
     * @param InHardLimit 硬限制
     */
    FObjectPoolConfig(TSubclassOf<AActor> InActorClass, int32 InInitialSize, int32 InHardLimit = 0)
        : ActorClass(InActorClass)
        , InitialSize(InInitialSize)
        , HardLimit(InHardLimit)
    {
    }

    /** 验证配置是否有效 */
    bool IsValid() const
    {
        return ActorClass != nullptr && InitialSize > 0;
    }
};

/**
 * 对象池事件类型
 * 用于统计和调试
 */
UENUM(BlueprintType)
enum class EObjectPoolEvent : uint8
{
    /** Actor从池中获取 */
    ActorAcquired       UMETA(DisplayName = "Actor获取"),
    
    /** Actor归还到池 */
    ActorReturned       UMETA(DisplayName = "Actor归还"),
    
    /** 新Actor创建 */
    ActorCreated        UMETA(DisplayName = "Actor创建"),
    
    /** Actor销毁 */
    ActorDestroyed      UMETA(DisplayName = "Actor销毁"),
    
    /** 池扩展 */
    PoolExpanded        UMETA(DisplayName = "池扩展"),
    
    /** 池收缩 */
    PoolShrunk          UMETA(DisplayName = "池收缩"),
    
    /** 池清空 */
    PoolCleared         UMETA(DisplayName = "池清空")
};

/**
 * 对象池错误类型
 * 用于错误处理和调试
 */
UENUM(BlueprintType)
enum class EObjectPoolError : uint8
{
    /** 无错误 */
    None                UMETA(DisplayName = "无错误"),

    /** 无效的Actor类 */
    InvalidActorClass   UMETA(DisplayName = "无效Actor类"),

    /** 池已满 */
    PoolFull            UMETA(DisplayName = "池已满"),

    /** 池为空 */
    PoolEmpty           UMETA(DisplayName = "池为空"),

    /** Actor创建失败 */
    ActorCreationFailed UMETA(DisplayName = "Actor创建失败"),

    /** 内存不足 */
    OutOfMemory         UMETA(DisplayName = "内存不足"),

    /** 线程安全错误 */
    ThreadSafetyError   UMETA(DisplayName = "线程安全错误")
};

/**
 * 自动回退策略类型
 * 定义当对象池操作失败时的回退行为
 */
UENUM(BlueprintType)
enum class EObjectPoolFallbackStrategy : uint8
{
    /** 永不失败：尝试所有可能的回退策略 */
    NeverFail           UMETA(DisplayName = "永不失败"),

    /** 严格模式：只使用指定的Actor类，失败时返回null */
    StrictMode          UMETA(DisplayName = "严格模式"),

    /** 类型回退：失败时回退到父类或默认Actor */
    TypeFallback        UMETA(DisplayName = "类型回退"),

    /** 池优先：优先使用池，失败时直接创建 */
    PoolFirst           UMETA(DisplayName = "池优先"),

    /** 直接创建：跳过池，直接创建新Actor */
    DirectCreate        UMETA(DisplayName = "直接创建")
};

/**
 * 对象池回退配置
 * 用于配置自动回退机制的行为
 */
USTRUCT(BlueprintType)
struct OBJECTPOOL_API FObjectPoolFallbackConfig
{
    GENERATED_BODY()

    /** 回退策略 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "回退配置")
    EObjectPoolFallbackStrategy Strategy = EObjectPoolFallbackStrategy::NeverFail;

    /** 是否允许回退到默认Actor类 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "回退配置")
    bool bAllowDefaultActorFallback = true;

    /** 是否在回退时记录警告日志 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "回退配置")
    bool bLogFallbackWarnings = true;

    /** 最大回退尝试次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "回退配置", meta = (ClampMin = "1", ClampMax = "10"))
    int32 MaxFallbackAttempts = 3;

    /** 默认构造函数 */
    FObjectPoolFallbackConfig()
    {
        Strategy = EObjectPoolFallbackStrategy::NeverFail;
        bAllowDefaultActorFallback = true;
        bLogFallbackWarnings = true;
        MaxFallbackAttempts = 3;
    }
};

/**
 * 智能预分配策略类型
 * 定义对象池的预分配行为模式
 */
UENUM(BlueprintType, meta = (
    DisplayName = "预分配策略",
    ToolTip = "定义对象池的智能预分配策略，控制预热时机和分配算法。",
    Keywords = "预分配,预热,策略,性能优化"))
enum class EObjectPoolPreallocationStrategy : uint8
{
    /** 禁用预分配 */
    Disabled            UMETA(DisplayName = "禁用"),

    /** 立即预分配：游戏启动时立即分配 */
    Immediate           UMETA(DisplayName = "立即预分配"),

    /** 延迟预分配：首次使用时分配 */
    Lazy                UMETA(DisplayName = "延迟预分配"),

    /** 渐进式预分配：分帧逐步分配 */
    Progressive         UMETA(DisplayName = "渐进式预分配"),

    /** 预测性预分配：基于使用模式预测 */
    Predictive          UMETA(DisplayName = "预测性预分配"),

    /** 自适应预分配：根据性能动态调整 */
    Adaptive            UMETA(DisplayName = "自适应预分配")
};

/**
 * 预分配配置
 * 控制智能预分配机制的详细参数
 */
USTRUCT(BlueprintType, meta = (
    DisplayName = "预分配配置",
    ToolTip = "控制对象池智能预分配机制的详细配置参数，包括策略、时机、性能限制等。",
    Keywords = "预分配,配置,性能,内存管理"))
struct OBJECTPOOL_API FObjectPoolPreallocationConfig
{
    GENERATED_BODY()

    /** 预分配策略 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "预分配策略")
    EObjectPoolPreallocationStrategy Strategy = EObjectPoolPreallocationStrategy::Progressive;

    /** 预分配的Actor数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "预分配参数", meta = (ClampMin = "0", ClampMax = "1000"))
    int32 PreallocationCount = 10;

    /** 每帧最大分配数量（渐进式预分配） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "预分配参数", meta = (ClampMin = "1", ClampMax = "50"))
    int32 MaxAllocationsPerFrame = 5;

    /** 预分配延迟时间（秒） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "预分配参数", meta = (ClampMin = "0.0", ClampMax = "60.0"))
    float PreallocationDelay = 1.0f;

    /** 是否启用内存预算限制 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "内存管理")
    bool bEnableMemoryBudget = true;

    /** 最大内存预算（MB） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "内存管理", meta = (ClampMin = "1", ClampMax = "1024"))
    int32 MaxMemoryBudgetMB = 64;

    /** 是否启用动态调整 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "动态调整")
    bool bEnableDynamicAdjustment = true;

    /** 使用率阈值：超过此值时扩容 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "动态调整", meta = (ClampMin = "0.1", ClampMax = "1.0"))
    float ExpandThreshold = 0.8f;

    /** 使用率阈值：低于此值时缩容 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "动态调整", meta = (ClampMin = "0.0", ClampMax = "0.9"))
    float ShrinkThreshold = 0.2f;

    /** 扩容倍数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "动态调整", meta = (ClampMin = "1.1", ClampMax = "3.0"))
    float ExpandMultiplier = 1.5f;

    /** 缩容倍数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "动态调整", meta = (ClampMin = "0.3", ClampMax = "0.9"))
    float ShrinkMultiplier = 0.7f;

    /** 默认构造函数 */
    FObjectPoolPreallocationConfig()
    {
        Strategy = EObjectPoolPreallocationStrategy::Progressive;
        PreallocationCount = 10;
        MaxAllocationsPerFrame = 5;
        PreallocationDelay = 1.0f;
        bEnableMemoryBudget = true;
        MaxMemoryBudgetMB = 64;
        bEnableDynamicAdjustment = true;
        ExpandThreshold = 0.8f;
        ShrinkThreshold = 0.2f;
        ExpandMultiplier = 1.5f;
        ShrinkMultiplier = 0.7f;
    }
};

/**
 * 预分配统计信息
 * 记录预分配过程的详细统计数据
 */
USTRUCT(BlueprintType, meta = (
    DisplayName = "预分配统计",
    ToolTip = "记录对象池预分配过程的详细统计信息，用于性能分析和优化。",
    Keywords = "预分配,统计,性能,监控"))
struct OBJECTPOOL_API FObjectPoolPreallocationStats
{
    GENERATED_BODY()

    /** 预分配开始时间 */
    UPROPERTY(BlueprintReadOnly, Category = "时间统计")
    FDateTime PreallocationStartTime;

    /** 预分配完成时间 */
    UPROPERTY(BlueprintReadOnly, Category = "时间统计")
    FDateTime PreallocationEndTime;

    /** 预分配总耗时（毫秒） */
    UPROPERTY(BlueprintReadOnly, Category = "时间统计")
    float TotalPreallocationTimeMs = 0.0f;

    /** 已预分配的Actor数量 */
    UPROPERTY(BlueprintReadOnly, Category = "数量统计")
    int32 PreallocatedCount = 0;

    /** 目标预分配数量 */
    UPROPERTY(BlueprintReadOnly, Category = "数量统计")
    int32 TargetCount = 0;

    /** 预分配成功率 */
    UPROPERTY(BlueprintReadOnly, Category = "数量统计")
    float SuccessRate = 0.0f;

    /** 使用的内存大小（字节） */
    UPROPERTY(BlueprintReadOnly, Category = "内存统计")
    int64 MemoryUsedBytes = 0;

    /** 平均每个Actor的内存大小（字节） */
    UPROPERTY(BlueprintReadOnly, Category = "内存统计")
    int32 AverageActorSizeBytes = 0;

    /** 动态调整次数 */
    UPROPERTY(BlueprintReadOnly, Category = "动态调整统计")
    int32 DynamicAdjustmentCount = 0;

    /** 扩容次数 */
    UPROPERTY(BlueprintReadOnly, Category = "动态调整统计")
    int32 ExpandCount = 0;

    /** 缩容次数 */
    UPROPERTY(BlueprintReadOnly, Category = "动态调整统计")
    int32 ShrinkCount = 0;

    /** 默认构造函数 */
    FObjectPoolPreallocationStats()
    {
        PreallocationStartTime = FDateTime::Now();
        PreallocationEndTime = FDateTime::Now();
    }

    /** 计算预分配完成度 */
    float GetCompletionPercentage() const
    {
        if (TargetCount <= 0) return 100.0f;
        return (float)PreallocatedCount / TargetCount * 100.0f;
    }

    /** 更新统计信息 */
    void UpdateStats(int32 NewPreallocatedCount, int32 NewTargetCount, int64 NewMemoryUsed)
    {
        PreallocatedCount = NewPreallocatedCount;
        TargetCount = NewTargetCount;
        MemoryUsedBytes = NewMemoryUsed;

        if (PreallocatedCount > 0)
        {
            SuccessRate = (float)PreallocatedCount / FMath::Max(TargetCount, 1) * 100.0f;
            AverageActorSizeBytes = MemoryUsedBytes / PreallocatedCount;
        }

        PreallocationEndTime = FDateTime::Now();
        TotalPreallocationTimeMs = (PreallocationEndTime - PreallocationStartTime).GetTotalMilliseconds();
    }
};

/**
 * 生命周期事件类型
 * 定义对象池中Actor的生命周期事件
 */
UENUM(BlueprintType, meta = (
    DisplayName = "对象池生命周期事件",
    ToolTip = "定义对象池中Actor的生命周期事件类型，用于生命周期管理和事件调用。",
    Keywords = "生命周期,事件,对象池,Actor,状态管理"))
enum class EObjectPoolLifecycleEvent : uint8
{
    /** Actor在池中首次创建 */
    Created             UMETA(DisplayName = "创建"),

    /** Actor从池中激活 */
    Activated           UMETA(DisplayName = "激活"),

    /** Actor归还到池 */
    ReturnedToPool      UMETA(DisplayName = "归还到池"),

    /** Actor从池中销毁 */
    Destroyed           UMETA(DisplayName = "销毁"),

    /** Actor状态重置 */
    StateReset          UMETA(DisplayName = "状态重置"),

    /** Actor验证失败 */
    ValidationFailed    UMETA(DisplayName = "验证失败")
};

/**
 * 生命周期事件配置
 * 控制生命周期事件的调用行为
 */
USTRUCT(BlueprintType)
struct OBJECTPOOL_API FObjectPoolLifecycleConfig
{
    GENERATED_BODY()

    /** 是否启用生命周期事件 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "生命周期配置")
    bool bEnableLifecycleEvents = true;

    /** 是否在事件调用失败时记录错误 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "生命周期配置")
    bool bLogEventErrors = true;

    /** 是否异步调用生命周期事件 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "生命周期配置")
    bool bAsyncEventCalls = false;

    /** 事件调用超时时间（毫秒） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "生命周期配置", meta = (ClampMin = "1", ClampMax = "10000"))
    int32 EventTimeoutMs = 1000;

    /** 是否缓存接口检查结果 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "生命周期配置")
    bool bCacheInterfaceChecks = true;

    /** 默认构造函数 */
    FObjectPoolLifecycleConfig()
    {
        bEnableLifecycleEvents = true;
        bLogEventErrors = true;
        bAsyncEventCalls = false;
        EventTimeoutMs = 1000;
        bCacheInterfaceChecks = true;
    }
};

/**
 * 生命周期事件统计
 * 记录生命周期事件的调用统计
 */
USTRUCT(BlueprintType)
struct OBJECTPOOL_API FObjectPoolLifecycleStats
{
    GENERATED_BODY()

    /** 创建事件调用次数 */
    UPROPERTY(BlueprintReadOnly, Category = "生命周期统计")
    int32 CreatedEventCalls = 0;

    /** 激活事件调用次数 */
    UPROPERTY(BlueprintReadOnly, Category = "生命周期统计")
    int32 ActivatedEventCalls = 0;

    /** 归还事件调用次数 */
    UPROPERTY(BlueprintReadOnly, Category = "生命周期统计")
    int32 ReturnedEventCalls = 0;

    /** 事件调用失败次数 */
    UPROPERTY(BlueprintReadOnly, Category = "生命周期统计")
    int32 FailedEventCalls = 0;

    /** 平均事件调用时间（微秒） */
    UPROPERTY(BlueprintReadOnly, Category = "生命周期统计")
    float AverageEventTimeUs = 0.0f;

    /** 最后一次事件调用时间 */
    UPROPERTY(BlueprintReadOnly, Category = "生命周期统计")
    FDateTime LastEventTime;

    /** 默认构造函数 */
    FObjectPoolLifecycleStats()
    {
        LastEventTime = FDateTime::Now();
    }

    /** 更新统计信息 */
    void UpdateStats(EObjectPoolLifecycleEvent EventType, bool bSuccess, float ExecutionTimeUs)
    {
        LastEventTime = FDateTime::Now();

        if (bSuccess)
        {
            switch (EventType)
            {
            case EObjectPoolLifecycleEvent::Created:
                ++CreatedEventCalls;
                break;
            case EObjectPoolLifecycleEvent::Activated:
                ++ActivatedEventCalls;
                break;
            case EObjectPoolLifecycleEvent::ReturnedToPool:
                ++ReturnedEventCalls;
                break;
            default:
                break;
            }

            // 更新平均执行时间
            AverageEventTimeUs = (AverageEventTimeUs + ExecutionTimeUs) * 0.5f;
        }
        else
        {
            ++FailedEventCalls;
        }
    }
};

/**
 * Actor重置配置
 */
USTRUCT(BlueprintType, meta = (
    DisplayName = "Actor重置配置",
    ToolTip = "配置Actor状态重置的详细选项，控制哪些组件和属性需要重置。"))
struct OBJECTPOOL_API FActorResetConfig
{
    GENERATED_BODY()

    /** 是否重置Transform */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基础重置", meta = (
        DisplayName = "重置Transform",
        ToolTip = "是否重置Actor的位置、旋转和缩放"))
    bool bResetTransform = true;

    /** 是否重置物理状态 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "物理重置", meta = (
        DisplayName = "重置物理状态",
        ToolTip = "是否重置速度、角速度等物理属性"))
    bool bResetPhysics = true;

    /** 是否重置AI状态 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI重置", meta = (
        DisplayName = "重置AI状态",
        ToolTip = "是否重置AI控制器、行为树等AI相关状态"))
    bool bResetAI = true;

    /** 是否重置动画状态 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "动画重置", meta = (
        DisplayName = "重置动画状态",
        ToolTip = "是否重置动画蒙太奇、状态机等动画状态"))
    bool bResetAnimation = true;

    /** 是否清理定时器 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "事件重置", meta = (
        DisplayName = "清理定时器",
        ToolTip = "是否清理所有定时器和延迟调用"))
    bool bClearTimers = true;

    /** 是否重置音频状态 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "音频重置", meta = (
        DisplayName = "重置音频状态",
        ToolTip = "是否停止音频播放并重置音频组件"))
    bool bResetAudio = true;

    /** 是否重置粒子系统 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "特效重置", meta = (
        DisplayName = "重置粒子系统",
        ToolTip = "是否重置粒子系统和特效组件"))
    bool bResetParticles = true;

    /** 是否重置网络状态 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网络重置", meta = (
        DisplayName = "重置网络状态",
        ToolTip = "是否重置网络复制相关状态"))
    bool bResetNetwork = false;

    /** 自定义重置标记 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "自定义重置", meta = (
        DisplayName = "自定义重置标记",
        ToolTip = "用户自定义的重置选项标记"))
    TMap<FString, bool> CustomResetFlags;

    FActorResetConfig()
    {
        bResetTransform = true;
        bResetPhysics = true;
        bResetAI = true;
        bResetAnimation = true;
        bClearTimers = true;
        bResetAudio = true;
        bResetParticles = true;
        bResetNetwork = false;
    }
};

/**
 * Actor重置统计信息
 */
USTRUCT(BlueprintType, meta = (
    DisplayName = "Actor重置统计",
    ToolTip = "Actor状态重置操作的统计信息，包括成功率、耗时等性能指标。"))
struct OBJECTPOOL_API FActorResetStats
{
    GENERATED_BODY()

    /** 总重置次数 */
    UPROPERTY(BlueprintReadOnly, Category = "重置统计", meta = (
        DisplayName = "总重置次数",
        ToolTip = "Actor状态重置的总次数"))
    int32 TotalResetCount = 0;

    /** 成功重置次数 */
    UPROPERTY(BlueprintReadOnly, Category = "重置统计", meta = (
        DisplayName = "成功重置次数",
        ToolTip = "成功完成重置的次数"))
    int32 SuccessfulResetCount = 0;

    /** 重置成功率 */
    UPROPERTY(BlueprintReadOnly, Category = "重置统计", meta = (
        DisplayName = "重置成功率",
        ToolTip = "重置操作的成功率（0.0-1.0）"))
    float ResetSuccessRate = 1.0f;

    /** 平均重置时间（毫秒） */
    UPROPERTY(BlueprintReadOnly, Category = "重置统计", meta = (
        DisplayName = "平均重置时间",
        ToolTip = "单次重置操作的平均耗时（毫秒）"))
    float AverageResetTimeMs = 0.0f;

    /** 最大重置时间（毫秒） */
    UPROPERTY(BlueprintReadOnly, Category = "重置统计", meta = (
        DisplayName = "最大重置时间",
        ToolTip = "单次重置操作的最大耗时（毫秒）"))
    float MaxResetTimeMs = 0.0f;

    /** 最小重置时间（毫秒） */
    UPROPERTY(BlueprintReadOnly, Category = "重置统计", meta = (
        DisplayName = "最小重置时间",
        ToolTip = "单次重置操作的最小耗时（毫秒）"))
    float MinResetTimeMs = 0.0f;

    FActorResetStats()
    {
        TotalResetCount = 0;
        SuccessfulResetCount = 0;
        ResetSuccessRate = 1.0f;
        AverageResetTimeMs = 0.0f;
        MaxResetTimeMs = 0.0f;
        MinResetTimeMs = 0.0f;
    }

    /** 更新统计信息 */
    void UpdateStats(bool bSuccess, float ResetTimeMs)
    {
        TotalResetCount++;
        if (bSuccess)
        {
            SuccessfulResetCount++;
        }

        ResetSuccessRate = TotalResetCount > 0 ? (float)SuccessfulResetCount / TotalResetCount : 1.0f;

        if (ResetTimeMs > 0.0f)
        {
            if (MaxResetTimeMs == 0.0f || ResetTimeMs > MaxResetTimeMs)
            {
                MaxResetTimeMs = ResetTimeMs;
            }
            if (MinResetTimeMs == 0.0f || ResetTimeMs < MinResetTimeMs)
            {
                MinResetTimeMs = ResetTimeMs;
            }

            // 更新平均时间
            static float TotalTime = 0.0f;
            static int32 TimeCount = 0;
            TotalTime += ResetTimeMs;
            TimeCount++;
            AverageResetTimeMs = TotalTime / TimeCount;
        }
    }
};
