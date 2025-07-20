// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/Engine.h"

// ✅ 生成的头文件必须放在最后
#include "ObjectPoolTypesSimplified.generated.h"

/**
 * 简化的对象池配置
 * 只包含最核心的配置选项，移除过度设计的功能
 */
USTRUCT(BlueprintType, meta = (
    DisplayName = "对象池配置",
    ToolTip = "简化的对象池配置，包含核心的池化参数"))
struct OBJECTPOOL_API FObjectPoolConfigSimplified
{
    GENERATED_BODY()

    /** 要池化的Actor类 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基本配置", meta = (
        DisplayName = "Actor类",
        ToolTip = "要进行池化的Actor类型"))
    TSubclassOf<AActor> ActorClass;

    /** 初始池大小 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基本配置", meta = (
        DisplayName = "初始大小",
        ToolTip = "池的初始大小，建议设置为常用数量",
        ClampMin = "1", ClampMax = "1000"))
    int32 InitialSize = 10;

    /** 池的最大限制 (0表示无限制) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基本配置", meta = (
        DisplayName = "最大限制",
        ToolTip = "池的最大大小限制，0表示无限制",
        ClampMin = "0", ClampMax = "10000"))
    int32 HardLimit = 0;

    /** 是否启用预热 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基本配置", meta = (
        DisplayName = "启用预热",
        ToolTip = "是否在注册时预先创建Actor"))
    bool bEnablePrewarm = true;

    /** 默认构造函数 */
    FObjectPoolConfigSimplified()
        : ActorClass(nullptr)
        , InitialSize(10)
        , HardLimit(0)
        , bEnablePrewarm(true)
    {
    }

    /** 
     * 构造函数
     * @param InActorClass Actor类
     * @param InInitialSize 初始大小
     * @param InHardLimit 硬限制
     */
    FObjectPoolConfigSimplified(TSubclassOf<AActor> InActorClass, int32 InInitialSize, int32 InHardLimit = 0)
        : ActorClass(InActorClass)
        , InitialSize(InInitialSize)
        , HardLimit(InHardLimit)
        , bEnablePrewarm(true)
    {
    }

    /** 验证配置是否有效 */
    bool IsValid() const
    {
        return ActorClass != nullptr && InitialSize > 0 && (HardLimit == 0 || HardLimit >= InitialSize);
    }
};

/**
 * 简化的对象池统计信息
 * 只保留最重要的统计数据，移除复杂的分析功能
 */
USTRUCT(BlueprintType, meta = (
    DisplayName = "对象池统计",
    ToolTip = "对象池的基本统计信息"))
struct OBJECTPOOL_API FObjectPoolStatsSimplified
{
    GENERATED_BODY()

    /** 总共创建的Actor数量 */
    UPROPERTY(BlueprintReadOnly, Category = "统计信息", meta = (
        DisplayName = "总创建数",
        ToolTip = "自池创建以来总共创建的Actor数量"))
    int32 TotalCreated = 0;

    /** 当前活跃的Actor数量 */
    UPROPERTY(BlueprintReadOnly, Category = "统计信息", meta = (
        DisplayName = "当前活跃",
        ToolTip = "当前正在使用中的Actor数量"))
    int32 CurrentActive = 0;

    /** 当前可用的Actor数量 */
    UPROPERTY(BlueprintReadOnly, Category = "统计信息", meta = (
        DisplayName = "当前可用",
        ToolTip = "当前在池中可用的Actor数量"))
    int32 CurrentAvailable = 0;

    /** 池的总大小 */
    UPROPERTY(BlueprintReadOnly, Category = "统计信息", meta = (
        DisplayName = "池大小",
        ToolTip = "池的总大小（活跃+可用）"))
    int32 PoolSize = 0;

    /** 池命中率 (0.0-1.0) */
    UPROPERTY(BlueprintReadOnly, Category = "统计信息", meta = (
        DisplayName = "命中率",
        ToolTip = "从池中成功获取Actor的比率"))
    float HitRate = 0.0f;

    /** Actor类名称 */
    UPROPERTY(BlueprintReadOnly, Category = "统计信息", meta = (
        DisplayName = "Actor类名",
        ToolTip = "池化的Actor类名称"))
    FString ActorClassName;

    /** 默认构造函数 */
    FObjectPoolStatsSimplified()
        : TotalCreated(0)
        , CurrentActive(0)
        , CurrentAvailable(0)
        , PoolSize(0)
        , HitRate(0.0f)
        , ActorClassName(TEXT("Unknown"))
    {
    }

    /** 
     * 构造函数
     * @param InActorClass Actor类
     */
    FObjectPoolStatsSimplified(UClass* InActorClass)
        : TotalCreated(0)
        , CurrentActive(0)
        , CurrentAvailable(0)
        , PoolSize(0)
        , HitRate(0.0f)
        , ActorClassName(InActorClass ? InActorClass->GetName() : TEXT("Unknown"))
    {
    }

    /** 更新统计信息 */
    void UpdateStats(int32 InTotalCreated, int32 InCurrentActive, int32 InCurrentAvailable, int32 TotalRequests)
    {
        TotalCreated = InTotalCreated;
        CurrentActive = InCurrentActive;
        CurrentAvailable = InCurrentAvailable;
        PoolSize = CurrentActive + CurrentAvailable;
        
        // 计算命中率
        if (TotalRequests > 0)
        {
            int32 PoolHits = TotalRequests - TotalCreated;
            HitRate = FMath::Clamp(static_cast<float>(PoolHits) / static_cast<float>(TotalRequests), 0.0f, 1.0f);
        }
        else
        {
            HitRate = 0.0f;
        }
    }

    /** 获取格式化的统计字符串 */
    FString ToString() const
    {
        return FString::Printf(TEXT("对象池[%s]: 总创建=%d, 活跃数=%d, 可用数=%d, 命中率=%.1f%%"),
            *ActorClassName, TotalCreated, CurrentActive, CurrentAvailable, HitRate * 100.0f);
    }
};

/**
 * 简化的回退策略枚举
 * 只保留最常用的回退策略
 */
UENUM(BlueprintType, meta = (
    DisplayName = "回退策略",
    ToolTip = "当对象池操作失败时的回退策略"))
enum class EObjectPoolFallbackStrategySimplified : uint8
{
    /** 永不失败：自动回退到直接创建 */
    NeverFail       UMETA(DisplayName = "永不失败"),

    /** 严格模式：失败时返回null */
    StrictMode      UMETA(DisplayName = "严格模式"),

    /** 池优先：优先使用池，失败时直接创建 */
    PoolFirst       UMETA(DisplayName = "池优先")
};

/**
 * 简化类型的别名定义
 * 使用不同的命名避免与现有类型冲突
 */

// 简化配置类型别名
using FSimplePoolConfig = FObjectPoolConfigSimplified;
using FSimpleActorPoolConfig = FObjectPoolConfigSimplified;

// 简化统计类型别名
using FSimplePoolStats = FObjectPoolStatsSimplified;
using FSimpleActorPoolStats = FObjectPoolStatsSimplified;

// 简化回退策略类型别名
using ESimplePoolFallbackStrategy = EObjectPoolFallbackStrategySimplified;

/**
 * 类型转换工具
 * 用于在简化类型和原始类型之间进行转换
 */
namespace ObjectPoolTypeConversion
{
    // 前向声明，避免循环包含
    struct FObjectPoolConfigOriginal;
    struct FObjectPoolStatsOriginal;

    /**
     * 从原始配置转换为简化配置
     * @param OriginalConfig 原始的配置结构
     * @return 简化的配置结构
     */
    OBJECTPOOL_API FObjectPoolConfigSimplified ConvertFromOriginal(const FObjectPoolConfigOriginal& OriginalConfig);

    /**
     * 从简化配置转换为原始配置
     * @param SimpleConfig 简化的配置结构
     * @return 原始的配置结构
     */
    OBJECTPOOL_API FObjectPoolConfigOriginal ConvertToOriginal(const FObjectPoolConfigSimplified& SimpleConfig);

    /**
     * 从原始统计转换为简化统计
     * @param OriginalStats 原始的统计结构
     * @return 简化的统计结构
     */
    OBJECTPOOL_API FObjectPoolStatsSimplified ConvertStatsFromOriginal(const FObjectPoolStatsOriginal& OriginalStats);
}

/**
 * 简化的调试信息结构
 * 用于基本的调试和监控
 */
USTRUCT(BlueprintType, meta = (
    DisplayName = "调试信息",
    ToolTip = "对象池的基本调试信息"))
struct OBJECTPOOL_API FObjectPoolDebugInfoSimplified
{
    GENERATED_BODY()

    /** 池的名称 */
    UPROPERTY(BlueprintReadOnly, Category = "调试")
    FString PoolName;

    /** 是否健康 */
    UPROPERTY(BlueprintReadOnly, Category = "调试")
    bool bIsHealthy = true;

    /** 警告消息 */
    UPROPERTY(BlueprintReadOnly, Category = "调试")
    TArray<FString> Warnings;

    /** 性能指标 */
    UPROPERTY(BlueprintReadOnly, Category = "调试")
    FObjectPoolStatsSimplified Stats;

    /** 默认构造函数 */
    FObjectPoolDebugInfoSimplified()
        : PoolName(TEXT("Unknown"))
        , bIsHealthy(true)
    {
    }

    /** 添加警告 */
    void AddWarning(const FString& Warning)
    {
        Warnings.Add(Warning);
        bIsHealthy = false;
    }

    /** 清除警告 */
    void ClearWarnings()
    {
        Warnings.Empty();
        bIsHealthy = true;
    }
};
