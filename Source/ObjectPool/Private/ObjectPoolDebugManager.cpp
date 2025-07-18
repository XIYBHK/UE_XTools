// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 遵循IWYU原则的头文件包含
#include "ObjectPoolDebugManager.h"

// ✅ UE核心依赖
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Engine/Console.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"

// ✅ 对象池模块依赖
#include "ObjectPool.h"
#include "ObjectPoolSubsystem.h"

FObjectPoolDebugManager::FObjectPoolDebugManager()
{
    bIsInitialized = false;
    CurrentDebugMode = EObjectPoolDebugMode::None;
    LastUpdateTime = FDateTime::Now();
}

FObjectPoolDebugManager::~FObjectPoolDebugManager()
{
    Shutdown();
}

void FObjectPoolDebugManager::Initialize()
{
    if (bIsInitialized)
    {
        return;
    }

    OBJECTPOOL_LOG(Log, TEXT("调试管理器初始化"));

    // ✅ 注册控制台命令
    RegisterConsoleCommands();

    bIsInitialized = true;
    OBJECTPOOL_LOG(Log, TEXT("调试管理器初始化完成"));
}

void FObjectPoolDebugManager::Shutdown()
{
    if (!bIsInitialized)
    {
        return;
    }

    OBJECTPOOL_LOG(Log, TEXT("调试管理器关闭"));

    // ✅ 注销控制台命令
    UnregisterConsoleCommands();

    bIsInitialized = false;
}

void FObjectPoolDebugManager::UpdateDebugData(UObjectPoolSubsystem* Subsystem)
{
    if (!IsValid(Subsystem))
    {
        return;
    }

    // ✅ 利用子系统的已有API获取统计数据
    CachedSnapshot = FObjectPoolDebugSnapshot();
    CachedSnapshot.SnapshotTime = FDateTime::Now();

    // ✅ 获取所有池统计 - 不重复造轮子，使用已有API
    CachedSnapshot.AllPoolStats = Subsystem->GetAllPoolStats();
    CachedSnapshot.TotalPoolCount = CachedSnapshot.AllPoolStats.Num();

    // ✅ 计算聚合数据
    int32 TotalActors = 0;
    int32 ActiveActors = 0;
    float TotalHitRate = 0.0f;
    float TotalMemoryMB = 0.0f;

    for (const FObjectPoolStats& PoolStats : CachedSnapshot.AllPoolStats)
    {
        TotalActors += PoolStats.CurrentActive + PoolStats.CurrentAvailable;
        ActiveActors += PoolStats.CurrentActive;
        TotalHitRate += PoolStats.HitRate;
        // 估算内存使用（简化计算）
        TotalMemoryMB += (PoolStats.CurrentActive + PoolStats.CurrentAvailable) * 0.1f; // 假设每个Actor约100KB
    }

    CachedSnapshot.TotalActorCount = TotalActors;
    CachedSnapshot.ActiveActorCount = ActiveActors;
    CachedSnapshot.AverageHitRate = CachedSnapshot.TotalPoolCount > 0 ? TotalHitRate / CachedSnapshot.TotalPoolCount : 0.0f;
    CachedSnapshot.TotalMemoryUsageMB = TotalMemoryMB;

    // ✅ 检测热点 - 基于已有统计数据
    CachedSnapshot.DetectedHotspots = DetectHotspots(Subsystem);

    LastUpdateTime = FDateTime::Now();
}

FObjectPoolDebugSnapshot FObjectPoolDebugManager::GetDebugSnapshot() const
{
    return CachedSnapshot;
}

TArray<FObjectPoolDebugHotspot> FObjectPoolDebugManager::DetectHotspots(UObjectPoolSubsystem* Subsystem)
{
    TArray<FObjectPoolDebugHotspot> Hotspots;

    if (!IsValid(Subsystem))
    {
        return Hotspots;
    }

    // ✅ 基于已有统计数据分析热点
    TArray<FObjectPoolStats> AllStats = Subsystem->GetAllPoolStats();
    
    for (const FObjectPoolStats& PoolStats : AllStats)
    {
        AnalyzePoolHotspots(PoolStats, Hotspots);
    }

    // ✅ 分析重置性能热点
    FActorResetStats ResetStats = Subsystem->GetActorResetStats();
    AnalyzeResetHotspots(ResetStats, Hotspots);

    return Hotspots;
}

void FObjectPoolDebugManager::AnalyzePoolHotspots(const FObjectPoolStats& PoolStats, TArray<FObjectPoolDebugHotspot>& OutHotspots)
{
    // ✅ 检测低命中率
    if (PoolStats.HitRate < HotspotThresholds.LowHitRateThreshold)
    {
        FObjectPoolDebugHotspot Hotspot;
        Hotspot.HotspotType = TEXT("低命中率");
        Hotspot.ActorClassName = PoolStats.ActorClassName;
        Hotspot.Severity = 1.0f - PoolStats.HitRate; // 命中率越低，严重程度越高
        Hotspot.Description = FString::Printf(TEXT("池 %s 的命中率仅为 %.1f%%"), 
            *PoolStats.ActorClassName, PoolStats.HitRate * 100.0f);
        Hotspot.Suggestion = TEXT("考虑增加初始池大小或启用预分配");
        OutHotspots.Add(Hotspot);
    }

    // ✅ 检测大池
    if (PoolStats.PoolSize > HotspotThresholds.LargePoolThreshold)
    {
        FObjectPoolDebugHotspot Hotspot;
        Hotspot.HotspotType = TEXT("大池");
        Hotspot.ActorClassName = PoolStats.ActorClassName;
        Hotspot.Severity = FMath::Min(1.0f, (float)PoolStats.PoolSize / (HotspotThresholds.LargePoolThreshold * 2));
        Hotspot.Description = FString::Printf(TEXT("池 %s 的大小为 %d，可能占用过多内存"), 
            *PoolStats.ActorClassName, PoolStats.PoolSize);
        Hotspot.Suggestion = TEXT("考虑启用自动收缩或降低硬限制");
        OutHotspots.Add(Hotspot);
    }

    // ✅ 检测空闲池
    if (PoolStats.CurrentActive == 0 && PoolStats.CurrentAvailable > 0)
    {
        FObjectPoolDebugHotspot Hotspot;
        Hotspot.HotspotType = TEXT("空闲池");
        Hotspot.ActorClassName = PoolStats.ActorClassName;
        Hotspot.Severity = 0.3f; // 低严重程度
        Hotspot.Description = FString::Printf(TEXT("池 %s 有 %d 个可用Actor但无活跃使用"), 
            *PoolStats.ActorClassName, PoolStats.CurrentAvailable);
        Hotspot.Suggestion = TEXT("考虑启用自动收缩以释放内存");
        OutHotspots.Add(Hotspot);
    }
}

void FObjectPoolDebugManager::AnalyzeResetHotspots(const FActorResetStats& ResetStats, TArray<FObjectPoolDebugHotspot>& OutHotspots)
{
    // ✅ 检测慢重置
    if (ResetStats.AverageResetTimeMs > HotspotThresholds.SlowResetThreshold)
    {
        FObjectPoolDebugHotspot Hotspot;
        Hotspot.HotspotType = TEXT("慢重置");
        Hotspot.ActorClassName = TEXT("全局");
        Hotspot.Severity = FMath::Min(1.0f, ResetStats.AverageResetTimeMs / (HotspotThresholds.SlowResetThreshold * 2));
        Hotspot.Description = FString::Printf(TEXT("Actor重置平均耗时 %.2fms，可能影响性能"), 
            ResetStats.AverageResetTimeMs);
        Hotspot.Suggestion = TEXT("检查重置配置，禁用不必要的重置选项");
        OutHotspots.Add(Hotspot);
    }

    // ✅ 检测重置失败率
    if (ResetStats.ResetSuccessRate < 0.95f)
    {
        FObjectPoolDebugHotspot Hotspot;
        Hotspot.HotspotType = TEXT("重置失败");
        Hotspot.ActorClassName = TEXT("全局");
        Hotspot.Severity = 1.0f - ResetStats.ResetSuccessRate;
        Hotspot.Description = FString::Printf(TEXT("重置成功率仅为 %.1f%%"), 
            ResetStats.ResetSuccessRate * 100.0f);
        Hotspot.Suggestion = TEXT("检查Actor状态重置逻辑，确保兼容性");
        OutHotspots.Add(Hotspot);
    }
}

void FObjectPoolDebugManager::SetDebugMode(EObjectPoolDebugMode NewMode)
{
    if (CurrentDebugMode != NewMode)
    {
        CurrentDebugMode = NewMode;
        OBJECTPOOL_LOG(Log, TEXT("调试模式已切换到: %d"), (int32)NewMode);
    }
}

void FObjectPoolDebugManager::DrawDebugInfo(UCanvas* Canvas, UObjectPoolSubsystem* Subsystem)
{
    if (!IsValid(Canvas) || !IsValid(Subsystem) || CurrentDebugMode == EObjectPoolDebugMode::None)
    {
        return;
    }

    // ✅ 更新调试数据
    UpdateDebugData(Subsystem);

    // ✅ 根据模式绘制不同信息
    switch (CurrentDebugMode)
    {
    case EObjectPoolDebugMode::Simple:
        DrawSimpleDebugInfo(Canvas, CachedSnapshot);
        break;
    case EObjectPoolDebugMode::Detailed:
        DrawDetailedDebugInfo(Canvas, CachedSnapshot);
        break;
    case EObjectPoolDebugMode::Performance:
        DrawPerformanceDebugInfo(Canvas, CachedSnapshot);
        break;
    case EObjectPoolDebugMode::Memory:
        DrawMemoryDebugInfo(Canvas, CachedSnapshot);
        break;
    default:
        break;
    }
}

FString FObjectPoolDebugManager::GetDebugSummary(UObjectPoolSubsystem* Subsystem) const
{
    if (!IsValid(Subsystem))
    {
        return TEXT("子系统无效");
    }

    // ✅ 利用已有API生成摘要
    FString Summary;
    Summary += FString::Printf(TEXT("=== 对象池调试摘要 ===\n"));
    Summary += FString::Printf(TEXT("总池数: %d\n"), CachedSnapshot.TotalPoolCount);
    Summary += FString::Printf(TEXT("总Actor数: %d\n"), CachedSnapshot.TotalActorCount);
    Summary += FString::Printf(TEXT("活跃Actor数: %d\n"), CachedSnapshot.ActiveActorCount);
    Summary += FString::Printf(TEXT("平均命中率: %.1f%%\n"), CachedSnapshot.AverageHitRate * 100.0f);
    Summary += FString::Printf(TEXT("内存使用: %.1f MB\n"), CachedSnapshot.TotalMemoryUsageMB);
    Summary += FString::Printf(TEXT("检测到热点: %d\n"), CachedSnapshot.DetectedHotspots.Num());

    return Summary;
}

FString FObjectPoolDebugManager::FormatMemorySize(int64 Bytes) const
{
    if (Bytes < 1024)
    {
        return FString::Printf(TEXT("%lld B"), Bytes);
    }
    else if (Bytes < 1024 * 1024)
    {
        return FString::Printf(TEXT("%.1f KB"), Bytes / 1024.0f);
    }
    else
    {
        return FString::Printf(TEXT("%.1f MB"), Bytes / (1024.0f * 1024.0f));
    }
}

FString FObjectPoolDebugManager::FormatTime(float TimeMs) const
{
    if (TimeMs < 1.0f)
    {
        return FString::Printf(TEXT("%.2f ms"), TimeMs);
    }
    else if (TimeMs < 1000.0f)
    {
        return FString::Printf(TEXT("%.1f ms"), TimeMs);
    }
    else
    {
        return FString::Printf(TEXT("%.2f s"), TimeMs / 1000.0f);
    }
}

FLinearColor FObjectPoolDebugManager::GetSeverityColor(float Severity) const
{
    if (Severity < 0.3f)
    {
        return FLinearColor::Green;
    }
    else if (Severity < 0.7f)
    {
        return FLinearColor::Yellow;
    }
    else
    {
        return FLinearColor::Red;
    }
}

// ✅ 简化的绘制方法实现，后续可以扩展
void FObjectPoolDebugManager::DrawSimpleDebugInfo(UCanvas* Canvas, const FObjectPoolDebugSnapshot& Snapshot) {}
void FObjectPoolDebugManager::DrawDetailedDebugInfo(UCanvas* Canvas, const FObjectPoolDebugSnapshot& Snapshot) {}
void FObjectPoolDebugManager::DrawPerformanceDebugInfo(UCanvas* Canvas, const FObjectPoolDebugSnapshot& Snapshot) {}
void FObjectPoolDebugManager::DrawMemoryDebugInfo(UCanvas* Canvas, const FObjectPoolDebugSnapshot& Snapshot) {}

// ✅ 控制台命令实现，后续可以扩展
void FObjectPoolDebugManager::RegisterConsoleCommands() {}
void FObjectPoolDebugManager::UnregisterConsoleCommands() {}
void FObjectPoolDebugManager::OnConsoleCommand_SetDebugMode(const TArray<FString>& Args) {}
void FObjectPoolDebugManager::OnConsoleCommand_ShowStats(const TArray<FString>& Args) {}
void FObjectPoolDebugManager::OnConsoleCommand_DetectHotspots(const TArray<FString>& Args) {}
void FObjectPoolDebugManager::OnConsoleCommand_ExportReport(const TArray<FString>& Args) {}
bool FObjectPoolDebugManager::ExportDebugReport(const FString& FilePath, UObjectPoolSubsystem* Subsystem) { return false; }
