// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/World.h"

// ✅ 对象池模块依赖
#include "ObjectPoolTypes.h"

#include "ObjectPoolDebugManager.generated.h"

// ✅ 前向声明
class UObjectPoolSubsystem;
class UCanvas;
class AHUD;

/**
 * 调试显示模式
 * 控制调试信息的显示方式
 */
UENUM(BlueprintType)
enum class EObjectPoolDebugMode : uint8
{
    /** 关闭调试显示 */
    None            UMETA(DisplayName = "关闭"),
    
    /** 简单模式：只显示基本信息 */
    Simple          UMETA(DisplayName = "简单模式"),
    
    /** 详细模式：显示详细统计 */
    Detailed        UMETA(DisplayName = "详细模式"),
    
    /** 性能模式：专注性能指标 */
    Performance     UMETA(DisplayName = "性能模式"),
    
    /** 内存模式：专注内存使用 */
    Memory          UMETA(DisplayName = "内存模式")
};

/**
 * 调试热点信息
 * 记录性能热点和问题区域
 */
USTRUCT(BlueprintType, meta = (
    DisplayName = "调试热点信息",
    ToolTip = "记录对象池使用中的性能热点和潜在问题"))
struct OBJECTPOOL_API FObjectPoolDebugHotspot
{
    GENERATED_BODY()

    /** 热点类型 */
    UPROPERTY(BlueprintReadOnly, Category = "热点信息")
    FString HotspotType;

    /** Actor类名 */
    UPROPERTY(BlueprintReadOnly, Category = "热点信息")
    FString ActorClassName;

    /** 严重程度 (0.0-1.0) */
    UPROPERTY(BlueprintReadOnly, Category = "热点信息")
    float Severity = 0.0f;

    /** 描述信息 */
    UPROPERTY(BlueprintReadOnly, Category = "热点信息")
    FString Description;

    /** 建议解决方案 */
    UPROPERTY(BlueprintReadOnly, Category = "热点信息")
    FString Suggestion;

    /** 检测时间 */
    UPROPERTY(BlueprintReadOnly, Category = "热点信息")
    FDateTime DetectionTime;

    FObjectPoolDebugHotspot()
    {
        Severity = 0.0f;
        DetectionTime = FDateTime::Now();
    }
};

/**
 * 实时调试数据
 * 聚合所有调试信息的快照
 */
USTRUCT(BlueprintType, meta = (
    DisplayName = "实时调试数据",
    ToolTip = "对象池系统的实时调试信息快照"))
struct OBJECTPOOL_API FObjectPoolDebugSnapshot
{
    GENERATED_BODY()

    /** 快照时间 */
    UPROPERTY(BlueprintReadOnly, Category = "基本信息")
    FDateTime SnapshotTime;

    /** 总池数量 */
    UPROPERTY(BlueprintReadOnly, Category = "基本信息")
    int32 TotalPoolCount = 0;

    /** 总Actor数量 */
    UPROPERTY(BlueprintReadOnly, Category = "基本信息")
    int32 TotalActorCount = 0;

    /** 活跃Actor数量 */
    UPROPERTY(BlueprintReadOnly, Category = "基本信息")
    int32 ActiveActorCount = 0;

    /** 总内存使用（MB） */
    UPROPERTY(BlueprintReadOnly, Category = "内存信息")
    float TotalMemoryUsageMB = 0.0f;

    /** 平均命中率 */
    UPROPERTY(BlueprintReadOnly, Category = "性能信息")
    float AverageHitRate = 0.0f;

    /** 检测到的热点 */
    UPROPERTY(BlueprintReadOnly, Category = "热点信息")
    TArray<FObjectPoolDebugHotspot> DetectedHotspots;

    /** 所有池的统计信息 */
    UPROPERTY(BlueprintReadOnly, Category = "详细统计")
    TArray<FObjectPoolStats> AllPoolStats;

    FObjectPoolDebugSnapshot()
    {
        SnapshotTime = FDateTime::Now();
    }
};

/**
 * 对象池调试管理器
 * 单一职责：专门负责调试信息的收集、分析和显示
 * 利用子系统优势：通过子系统获取统计数据
 * 不重复造轮子：基于已有的统计功能构建
 */
class OBJECTPOOL_API FObjectPoolDebugManager
{
public:
    FObjectPoolDebugManager();
    ~FObjectPoolDebugManager();

    /** 初始化调试管理器 */
    void Initialize();

    /** 清理调试管理器 */
    void Shutdown();

    /** 更新调试数据 - 利用子系统的已有API */
    void UpdateDebugData(UObjectPoolSubsystem* Subsystem);

    /** 获取实时调试快照 */
    FObjectPoolDebugSnapshot GetDebugSnapshot() const;

    /** 检测性能热点 - 基于已有统计数据 */
    TArray<FObjectPoolDebugHotspot> DetectHotspots(UObjectPoolSubsystem* Subsystem);

    /** 设置调试显示模式 */
    void SetDebugMode(EObjectPoolDebugMode NewMode);

    /** 获取当前调试模式 */
    EObjectPoolDebugMode GetDebugMode() const { return CurrentDebugMode; }

    /** 屏幕调试显示 - 利用UE5内置的Canvas系统 */
    void DrawDebugInfo(UCanvas* Canvas, UObjectPoolSubsystem* Subsystem);

    /** 控制台命令支持 */
    void RegisterConsoleCommands();
    void UnregisterConsoleCommands();

    /** 获取调试统计摘要 */
    FString GetDebugSummary(UObjectPoolSubsystem* Subsystem) const;

    /** 导出调试报告 */
    bool ExportDebugReport(const FString& FilePath, UObjectPoolSubsystem* Subsystem);

private:
    /** 当前调试模式 */
    EObjectPoolDebugMode CurrentDebugMode = EObjectPoolDebugMode::None;

    /** 是否已初始化 */
    bool bIsInitialized = false;

    /** 最后更新时间 */
    FDateTime LastUpdateTime;

    /** 缓存的调试快照 */
    FObjectPoolDebugSnapshot CachedSnapshot;

    /** 注册的控制台命令 */
    TArray<struct IConsoleCommand*> RegisteredCommands;

    /** 热点检测阈值 */
    struct FHotspotThresholds
    {
        float LowHitRateThreshold = 0.5f;      // 低命中率阈值
        float HighMemoryThreshold = 100.0f;    // 高内存使用阈值(MB)
        float SlowResetThreshold = 10.0f;      // 慢重置阈值(ms)
        int32 LargePoolThreshold = 100;        // 大池阈值
    } HotspotThresholds;

    /** 分析单个池的热点 */
    void AnalyzePoolHotspots(const FObjectPoolStats& PoolStats, TArray<FObjectPoolDebugHotspot>& OutHotspots);

    /** 分析重置性能热点 */
    void AnalyzeResetHotspots(const FActorResetStats& ResetStats, TArray<FObjectPoolDebugHotspot>& OutHotspots);

    /** 格式化内存大小 */
    FString FormatMemorySize(int64 Bytes) const;

    /** 格式化时间 */
    FString FormatTime(float TimeMs) const;

    /** 获取严重程度颜色 */
    FLinearColor GetSeverityColor(float Severity) const;

    /** 绘制简单模式信息 */
    void DrawSimpleDebugInfo(UCanvas* Canvas, const FObjectPoolDebugSnapshot& Snapshot);

    /** 绘制详细模式信息 */
    void DrawDetailedDebugInfo(UCanvas* Canvas, const FObjectPoolDebugSnapshot& Snapshot);

    /** 绘制性能模式信息 */
    void DrawPerformanceDebugInfo(UCanvas* Canvas, const FObjectPoolDebugSnapshot& Snapshot);

    /** 绘制内存模式信息 */
    void DrawMemoryDebugInfo(UCanvas* Canvas, const FObjectPoolDebugSnapshot& Snapshot);

    /** 控制台命令回调 */
    void OnConsoleCommand_SetDebugMode(const TArray<FString>& Args);
    void OnConsoleCommand_ShowStats(const TArray<FString>& Args);
    void OnConsoleCommand_DetectHotspots(const TArray<FString>& Args);
    void OnConsoleCommand_ExportReport(const TArray<FString>& Args);
};
