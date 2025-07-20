// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Array.h"
#include "Templates/SharedPointer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// ✅ 对象池模块依赖
#include "ObjectPoolTypesSimplified.h"
#include "ObjectPoolUtils.h"

// 前向声明
class FActorPoolSimplified;

/**
 * Actor池内存优化器
 * 
 * 设计原则：
 * - 单一职责：专注于内存优化和监控
 * - 外部工具：不侵入FActorPool的核心逻辑
 * - 可选使用：池可以选择是否使用优化器
 * - 策略模式：支持不同的优化策略
 * 
 * 功能：
 * - 内存使用统计和监控
 * - 智能预分配策略
 * - 内存压缩和碎片整理
 * - 性能分析和建议
 */
class OBJECTPOOL_API FActorPoolMemoryOptimizer
{
public:
    /**
     * 内存使用统计结构
     */
    struct FMemoryStats
    {
        /** 当前内存使用（字节） */
        int64 CurrentMemoryUsage;

        /** 峰值内存使用（字节） */
        int64 PeakMemoryUsage;

        /** 平均Actor内存大小（字节） */
        int64 AverageActorSize;

        /** 内存碎片化程度（0.0-1.0） */
        float FragmentationRatio;

        /** 上次内存检查时间 */
        double LastMemoryCheckTime;

        /** 内存增长趋势 */
        float GrowthTrend;

        FMemoryStats()
            : CurrentMemoryUsage(0)
            , PeakMemoryUsage(0)
            , AverageActorSize(0)
            , FragmentationRatio(0.0f)
            , LastMemoryCheckTime(0.0)
            , GrowthTrend(0.0f)
        {
        }
    };

    /**
     * 预分配策略配置
     */
    struct FPreallocationConfig
    {
        /** 预分配增长因子 */
        float GrowthFactor;

        /** 最小预分配数量 */
        int32 MinPreallocCount;

        /** 最大预分配数量 */
        int32 MaxPreallocCount;

        /** 预分配触发阈值 */
        float TriggerThreshold;

        /** 是否启用智能预分配 */
        bool bEnableSmartPreallocation;

        FPreallocationConfig()
            : GrowthFactor(1.5f)
            , MinPreallocCount(5)
            , MaxPreallocCount(50)
            , TriggerThreshold(0.8f)
            , bEnableSmartPreallocation(true)
        {
        }
    };

    /**
     * 优化策略枚举
     */
    enum class EOptimizationStrategy : uint8
    {
        Conservative,   // 保守策略：最小化内存使用
        Balanced,       // 平衡策略：平衡性能和内存
        Aggressive,     // 激进策略：最大化性能
        Custom          // 自定义策略
    };

public:
    /**
     * 构造函数
     * @param InStrategy 优化策略
     */
    explicit FActorPoolMemoryOptimizer(EOptimizationStrategy InStrategy = EOptimizationStrategy::Balanced);

    /** 析构函数 */
    ~FActorPoolMemoryOptimizer();

    // ✅ 禁用拷贝构造和赋值
    FActorPoolMemoryOptimizer(const FActorPoolMemoryOptimizer&) = delete;
    FActorPoolMemoryOptimizer& operator=(const FActorPoolMemoryOptimizer&) = delete;

    // ✅ 支持移动语义
    FActorPoolMemoryOptimizer(FActorPoolMemoryOptimizer&& Other) noexcept;
    FActorPoolMemoryOptimizer& operator=(FActorPoolMemoryOptimizer&& Other) noexcept;

public:
    // ✅ 内存监控功能

    /**
     * 分析池的内存使用情况
     * @param Pool 要分析的池
     * @return 内存统计信息
     */
    FMemoryStats AnalyzeMemoryUsage(const FActorPoolSimplified& Pool) const;

    /**
     * 获取内存使用建议
     * @param Pool 要分析的池
     * @return 优化建议列表
     */
    TArray<FString> GetMemoryOptimizationSuggestions(const FActorPoolSimplified& Pool) const;

    /**
     * 检查是否需要内存优化
     * @param Pool 要检查的池
     * @return 是否需要优化
     */
    bool ShouldOptimizeMemory(const FActorPoolSimplified& Pool) const;

    // ✅ 预分配优化功能

    /**
     * 检查是否应该进行预分配
     * @param Pool 要检查的池
     * @return 是否应该预分配
     */
    bool ShouldPreallocate(const FActorPoolSimplified& Pool) const;

    /**
     * 计算建议的预分配数量
     * @param Pool 要分析的池
     * @return 建议的预分配数量
     */
    int32 CalculatePreallocationCount(const FActorPoolSimplified& Pool) const;

    /**
     * 执行智能预分配
     * @param Pool 目标池
     * @param World 世界上下文
     * @return 实际预分配的数量
     */
    int32 PerformSmartPreallocation(FActorPoolSimplified& Pool, UWorld* World) const;

    // ✅ 内存压缩功能

    /**
     * 分析内存碎片化程度
     * @param Pool 要分析的池
     * @return 碎片化程度（0.0-1.0）
     */
    float AnalyzeFragmentation(const FActorPoolSimplified& Pool) const;

    /**
     * 执行内存压缩
     * @param Pool 目标池
     * @return 压缩后释放的内存量（字节）
     */
    int64 CompactMemory(FActorPoolSimplified& Pool) const;

    // ✅ 配置管理

    /**
     * 设置优化策略
     * @param NewStrategy 新的优化策略
     */
    void SetOptimizationStrategy(EOptimizationStrategy NewStrategy);

    /**
     * 获取当前优化策略
     * @return 当前优化策略
     */
    EOptimizationStrategy GetOptimizationStrategy() const { return CurrentStrategy; }

    /**
     * 设置预分配配置
     * @param NewConfig 新的预分配配置
     */
    void SetPreallocationConfig(const FPreallocationConfig& NewConfig);

    /**
     * 获取预分配配置
     * @return 当前预分配配置
     */
    const FPreallocationConfig& GetPreallocationConfig() const { return PreallocConfig; }

    // ✅ 性能分析

    /**
     * 生成性能报告
     * @param Pool 要分析的池
     * @return 格式化的性能报告
     */
    FString GeneratePerformanceReport(const FActorPoolSimplified& Pool) const;

    /**
     * 获取优化效果统计
     * @return 优化效果统计信息
     */
    FString GetOptimizationStats() const;

private:
    // ✅ 内部数据成员

    /** 当前优化策略 */
    EOptimizationStrategy CurrentStrategy;

    /** 预分配配置 */
    FPreallocationConfig PreallocConfig;

    /** 优化统计 */
    mutable struct FOptimizationStats
    {
        int32 TotalOptimizations;
        int64 TotalMemorySaved;
        int32 TotalPreallocations;
        double TotalOptimizationTime;

        FOptimizationStats()
            : TotalOptimizations(0)
            , TotalMemorySaved(0)
            , TotalPreallocations(0)
            , TotalOptimizationTime(0.0)
        {
        }
    } OptimizationStats;

private:
    // ✅ 内部辅助方法

    /**
     * 根据策略初始化配置
     * @param Strategy 优化策略
     */
    void InitializeConfigForStrategy(EOptimizationStrategy Strategy);

    /**
     * 计算池的内存使用量
     * @param Pool 目标池
     * @return 内存使用量（字节）
     */
    int64 CalculatePoolMemoryUsage(const FActorPoolSimplified& Pool) const;

    /**
     * 分析使用模式
     * @param Pool 目标池
     * @return 使用模式描述
     */
    FString AnalyzeUsagePattern(const FActorPoolSimplified& Pool) const;

    /**
     * 获取策略名称
     * @param Strategy 策略枚举
     * @return 策略名称字符串
     */
    static FString GetStrategyName(EOptimizationStrategy Strategy);
};

/**
 * 内存优化器的日志宏
 */
DECLARE_LOG_CATEGORY_EXTERN(LogActorPoolMemoryOptimizer, Log, All);

#define MEMORY_OPTIMIZER_LOG(Verbosity, Format, ...) \
    UE_LOG(LogActorPoolMemoryOptimizer, Verbosity, Format, ##__VA_ARGS__)

/**
 * 内存优化器的性能统计宏
 */
#if STATS
DECLARE_CYCLE_STAT_EXTERN(TEXT("MemoryOptimizer_AnalyzeMemory"), STAT_MemoryOptimizer_AnalyzeMemory, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("MemoryOptimizer_Preallocation"), STAT_MemoryOptimizer_Preallocation, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("MemoryOptimizer_Compaction"), STAT_MemoryOptimizer_Compaction, STATGROUP_ObjectPoolUtils, OBJECTPOOL_API);
#endif
