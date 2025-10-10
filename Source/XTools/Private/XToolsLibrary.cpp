// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 遵循 IWYU 原则的头文件包含
#include "XToolsLibrary.h"

// ✅ 插件模块依赖
#include "XToolsDefines.h"
#include "RandomShuffleArrayLibrary.h"
#include "FormationSystem.h"
#include "FormationLibrary.h"

// ✅ UE 核心依赖
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "CollisionShape.h"
#include "Curves/CurveFloat.h"

// ✅ UObject 系统
#include "UObject/UObjectGlobals.h"

// ✅ 线程安全支持
#include "HAL/CriticalSection.h"
#include "HAL/PlatformMemory.h"

// ✅ 线程安全的 PRD 测试管理器
class FThreadSafePRDTester
{
public:
    static FThreadSafePRDTester& Get()
    {
        static FThreadSafePRDTester Instance;
        return Instance;
    }

    // 线程安全的 PRD 测试执行
    bool ExecutePRDTest(float BaseChance, int32& OutNextFailureCount, float& OutActualChance,
                       const FString& StateID, int32 CurrentFailureCount)
    {
        FScopeLock Lock(&CriticalSection);
        return URandomShuffleArrayLibrary::PseudoRandomBoolAdvanced(
            BaseChance, OutNextFailureCount, OutActualChance, StateID, CurrentFailureCount);
    }

private:
    FCriticalSection CriticalSection;
};

// ✅ 配置常量 - 消除魔法数字
namespace XToolsConfig
{
    // PRD 测试配置
    constexpr int32 PRD_MAX_FAILURE_COUNT = 12;
    constexpr int32 PRD_ARRAY_SIZE = PRD_MAX_FAILURE_COUNT + 1;
    constexpr int32 PRD_TARGET_SUCCESSES = 10000;

    // 性能测试配置
    constexpr int32 PERF_TEST_ARRAY_COUNT = 100;
    constexpr int32 PERF_TEST_ARRAY_SIZE = 1000;
    constexpr float PERF_TEST_RANGE_MIN = -100.0f;
    constexpr float PERF_TEST_RANGE_MAX = 100.0f;

    // 内存阈值配置 (字节)
    constexpr SIZE_T MEMORY_THRESHOLD_BYTES = 50 * 1024 * 1024; // 50MB

    // 百分比转换常量
    constexpr float PERCENTAGE_MULTIPLIER = 100.0f;
    constexpr double MILLISECONDS_MULTIPLIER = 1000.0;
    constexpr double MEGABYTES_DIVISOR = 1024.0 * 1024.0;

    // 默认容量预分配
    constexpr int32 DEFAULT_POINTS_RESERVE = 1000;

    // 评分阈值
    constexpr float EXCELLENT_SCORE_THRESHOLD = 9.0f;
    constexpr float GOOD_SCORE_THRESHOLD = 7.0f;
    constexpr float MAX_SCORE = 10.0f;
}

// ✅ 网格参数结构体
struct FGridParameters
{
    FTransform BoxTransform;
    FVector Scale3D;
    FVector ScaledBoxExtent;
    FVector UnscaledBoxExtent;
    FVector LocalGridStep;
    FVector GridStart;
    FVector GridEnd;
    int32 NumStepsX;
    int32 NumStepsY;
    int32 NumStepsZ;
    int32 TotalPoints;
    bool bIsValid;
    FString ErrorMessage;
};

// ✅ 智能缓存系统
struct FGridParametersKey
{
    FVector BoxExtent;
    FTransform BoxTransform;
    float GridSpacing;

    bool operator==(const FGridParametersKey& Other) const
    {
        return BoxExtent.Equals(Other.BoxExtent, 0.001f) &&
               BoxTransform.Equals(Other.BoxTransform, 0.001f) &&
               FMath::IsNearlyEqual(GridSpacing, Other.GridSpacing, 0.001f);
    }

    friend uint32 GetTypeHash(const FGridParametersKey& Key)
    {
        return HashCombine(
            HashCombine(GetTypeHash(Key.BoxExtent), GetTypeHash(Key.BoxTransform.GetLocation())),
            GetTypeHash(Key.GridSpacing)
        );
    }
};

class FGridParametersCache
{
public:
    static FGridParametersCache& Get()
    {
        static FGridParametersCache Instance;
        return Instance;
    }

    TOptional<FGridParameters> GetCachedParameters(const FGridParametersKey& Key)
    {
        FScopeLock Lock(&CacheLock);
        if (const FGridParameters* Found = Cache.Find(Key))
        {
            return *Found;
        }
        return {};
    }

    void CacheParameters(const FGridParametersKey& Key, const FGridParameters& Params)
    {
        FScopeLock Lock(&CacheLock);

        // 限制缓存大小，避免内存泄漏
        if (Cache.Num() >= 100)
        {
            Cache.Empty(50); // 清空一半
        }

        Cache.Add(Key, Params);
    }

    void ClearCache()
    {
        FScopeLock Lock(&CacheLock);
        Cache.Empty();
    }

private:
    FCriticalSection CacheLock;
    TMap<FGridParametersKey, FGridParameters> Cache;
};

// ============================================================================
// 泊松采样结果缓存系统（前置声明，用于清理函数）
// ============================================================================

    struct FPoissonCacheKey
    {
        FVector BoxExtent;
        FQuat Rotation;
        float Radius;
        int32 TargetPointCount;
        float JitterStrength;
        bool bIs2D;
        bool bWorldSpace;
        
        bool operator==(const FPoissonCacheKey& Other) const
        {
            return BoxExtent.Equals(Other.BoxExtent, 0.1f) &&
                   Rotation.Equals(Other.Rotation, 0.001f) &&
                   FMath::IsNearlyEqual(Radius, Other.Radius, 0.1f) &&
                   TargetPointCount == Other.TargetPointCount &&
                   FMath::IsNearlyEqual(JitterStrength, Other.JitterStrength, 0.01f) &&
                   bIs2D == Other.bIs2D &&
                   bWorldSpace == Other.bWorldSpace;
        }
        
        friend uint32 GetTypeHash(const FPoissonCacheKey& Key)
        {
            return HashCombine(
                HashCombine(
                    HashCombine(
                        HashCombine(
                            HashCombine(GetTypeHash(Key.BoxExtent), GetTypeHash(Key.Rotation)),
                            GetTypeHash(Key.Radius)
                        ),
                        GetTypeHash(Key.TargetPointCount)
                    ),
                    GetTypeHash(Key.JitterStrength)
                ),
                HashCombine(GetTypeHash(Key.bIs2D), GetTypeHash(Key.bWorldSpace))
            );
        }
    };

class FPoissonResultCache
{
public:
    static FPoissonResultCache& Get()
    {
        static FPoissonResultCache Instance;
        return Instance;
    }
    
    TOptional<TArray<FVector>> GetCached(const FPoissonCacheKey& Key)
    {
        FScopeLock Lock(&CacheLock);
        if (const TArray<FVector>* Found = Cache.Find(Key))
        {
            CacheHits++;
            return *Found;
        }
        CacheMisses++;
        return {};
    }
    
    void Store(const FPoissonCacheKey& Key, const TArray<FVector>& Points)
    {
        FScopeLock Lock(&CacheLock);
        
        // 限制缓存大小
        if (Cache.Num() >= 50)
        {
            Cache.Empty(25); // 清空一半
        }
        
        Cache.Add(Key, Points);
    }
    
    void ClearCache()
    {
        FScopeLock Lock(&CacheLock);
        Cache.Empty();
        CacheHits = 0;
        CacheMisses = 0;
    }
    
    void GetStats(int32& OutHits, int32& OutMisses)
    {
        FScopeLock Lock(&CacheLock);
        OutHits = CacheHits;
        OutMisses = CacheMisses;
    }
    
private:
    FCriticalSection CacheLock;
    TMap<FPoissonCacheKey, TArray<FVector>> Cache;
    int32 CacheHits = 0;
    int32 CacheMisses = 0;
};

// ✅ 平台安全的内存统计工具
class FPlatformSafeMemoryStats
{
public:
    static SIZE_T GetSafeMemoryUsage()
    {
        // Win64 平台专用优化
        #if PLATFORM_WINDOWS && PLATFORM_64BITS
            // 直接获取内存统计，不使用try-catch，因为UE的异常处理通常是针对特定情况的
            // 如果FPlatformMemory::GetStats()在非编辑器构建中不可用，它应该有自己的处理机制
            const FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
            return Stats.UsedPhysical;
        #else
            // 其他平台返回默认值
            // UE_LOG(LogXTools, Warning, TEXT("当前平台不支持内存统计")); // 移除日志，避免在非编辑器构建中引入不必要的依赖
            return 0;
        #endif
    }

    static bool IsMemoryStatsAvailable()
    {
        #if PLATFORM_WINDOWS && PLATFORM_64BITS
            return true;
        #else
            return false;
        #endif
    }
};

/**
 * 在组件层级中查找匹配指定类和标签的父Actor
 * 
 * @param Component 起始组件，必须是SceneComponent（因为需要访问GetAttachParent()来遍历组件层级）
 * @param ActorClass 要查找的Actor类（可选）
 * @param ActorTag 要匹配的标签（可选）
 * @return 找到的父Actor，未找到返回nullptr
 * 
 * 查找规则：
 * - 同时指定类和标签时，返回第一个同时匹配的父级
 * - 只指定类时，返回最高级匹配的父级
 * - 只指定标签时，返回第一个匹配的父级
 * - 都未指定时，返回最顶层的父级
 * 
 * 注意：最大查找深度为XTOOLS_MAX_PARENT_DEPTH（默认100层）
 */
AActor* UXToolsLibrary::GetTopmostAttachedActor(USceneComponent* StartComponent, TSubclassOf<AActor> ActorClass, FName ActorTag)
{
    if (!StartComponent)
    {
        UE_LOG(LogXTools, Warning, TEXT("GetTopmostAttachedActor: 提供的起始组件无效 (StartComponent is null)."));
        return nullptr;
    }

    AActor* HighestMatchingActor = nullptr;
    // 从起始组件的直接父级开始向上查找
    USceneComponent* CurrentComponent = StartComponent->GetAttachParent();
    int32 Iterations = 0;

    // 持续向上遍历，直到没有父级或达到最大深度
    while (CurrentComponent && Iterations < XTOOLS_MAX_PARENT_DEPTH)
    {
        AActor* OwnerActor = CurrentComponent->GetOwner();
        if (OwnerActor)
        {
            // 条件1: 检查类是否匹配 (如果ActorClass被指定)
            const bool bClassMatches = !ActorClass || OwnerActor->IsA(ActorClass);

            // 条件2: 检查标签是否匹配 (如果ActorTag被指定)
            const bool bTagMatches = ActorTag.IsNone() || OwnerActor->ActorHasTag(ActorTag);

            if (bClassMatches && bTagMatches)
            {
                // 这是一个有效的匹配项，记录下来。
                // 由于我们持续向上查找，这个变量会被任何更高层级的匹配项覆盖。
                HighestMatchingActor = OwnerActor;
            }
        }

        CurrentComponent = CurrentComponent->GetAttachParent();
        Iterations++;
    }

    return HighestMatchingActor;
}

FVector UXToolsLibrary::CalculateBezierPoint(const UObject* Context,const TArray<FVector>& Points, float Progress, bool bShowDebug, float Duration, FBezierDebugColors DebugColors, FBezierSpeedOptions SpeedOptions)
{
    UWorld* World = GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        return FVector::ZeroVector;
    }
    
    // 参数验证
    if (Points.Num() < 2)
    {
        return Points.Num() == 1 ? Points[0] : FVector::ZeroVector;
    }

    // 确保Progress在[0,1]范围内
    Progress = FMath::Clamp(Progress, 0.0f, 1.0f);

    FVector ResultPoint;
    TArray<FVector> WorkPoints;
    
    if (SpeedOptions.SpeedMode == EBezierSpeedMode::Constant)
    {
        // --- 优化后的匀速模式 ---

        // 在匀速模式下应用速率曲线
        if (SpeedOptions.SpeedCurve)
        {
            Progress = SpeedOptions.SpeedCurve->GetFloatValue(Progress);
        }

        // 1. 一次性采样曲线，计算总长度和各分段长度
        const int32 Segments = 100; // 采样分段数，可以根据精度需求调整
        TArray<float> SegmentLengths;
        SegmentLengths.Reserve(Segments);
        float TotalLength = 0.0f;
        
        FVector PrevPoint = CalculatePointAtParameter(Points, 0.0f, WorkPoints);

        for (int32 i = 1; i <= Segments; ++i)
        {
            const float t = static_cast<float>(i) / Segments;
            const FVector CurrentPoint = CalculatePointAtParameter(Points, t, WorkPoints);
            const float SegmentLength = FVector::Distance(PrevPoint, CurrentPoint);
            SegmentLengths.Add(SegmentLength);
            TotalLength += SegmentLength;

            // 【新增】在调试模式下，绘制构成曲线的采样线段
            if (bShowDebug)
            {
                DrawDebugLine(World, PrevPoint, CurrentPoint, DebugColors.IntermediateLineColor.ToFColor(true), false, Duration);
            }
            
            PrevPoint = CurrentPoint;
        }

        if (FMath::IsNearlyZero(TotalLength))
        {
            ResultPoint = Points[0];
        }
        else
        {
            // 2. 根据总长度和进度计算目标距离
            const float TargetDistance = TotalLength * Progress;
            float AccumulatedLength = 0.0f;
            float Parameter = 1.0f; // 默认参数为1

            // 3. 从预计算的长度表中查找对应的参数t
            for (int32 i = 0; i < Segments; ++i)
            {
                if (AccumulatedLength + SegmentLengths[i] >= TargetDistance)
                {
                    const float ExcessLength = (AccumulatedLength + SegmentLengths[i]) - TargetDistance;
                    // SegmentLengths[i]为0时可能导致除零，增加检查
                    const float SegmentProgress = (SegmentLengths[i] > KINDA_SMALL_NUMBER) ? 1.0f - (ExcessLength / SegmentLengths[i]) : 1.0f;
                    
                    const float PrevT = static_cast<float>(i) / Segments;
                    const float CurrentT = static_cast<float>(i + 1) / Segments;
                    Parameter = FMath::Lerp(PrevT, CurrentT, SegmentProgress);
                    break;
                }
                AccumulatedLength += SegmentLengths[i];
            }

            // 4. 计算最终点
            // 注意：如果需要调试绘制中间点，这里的WorkPoints需要重新计算
            // 但由于性能优化是首要目标，我们只计算最终结果
            ResultPoint = CalculatePointAtParameter(Points, Parameter, WorkPoints);
        }
    }
    else
    {
        // 默认模式（直接使用参数t，不应用速率曲线）
        ResultPoint = CalculatePointAtParameter(Points, Progress, WorkPoints);
    }

    // 如果开启调试，绘制控制点和连线
    if (bShowDebug)
    {
        // 绘制控制点
        for (const FVector& Point : Points)
        {
            DrawDebugSphere(World, Point, 8.0f, 8, DebugColors.ControlPointColor.ToFColor(true), false, Duration);
        }

        // 绘制控制点之间的连线
        for (int32 i = 0; i < Points.Num() - 1; ++i)
        {
            DrawDebugLine(World, Points[i], Points[i + 1], DebugColors.ControlLineColor.ToFColor(true), false, Duration);
        }

        // 绘制中间计算过程
        const int32 PointCount = Points.Num();
        int32 CurrentIndex = PointCount;
        for (int32 Level = 1; Level < PointCount; ++Level)
        {
            const int32 LevelPoints = PointCount - Level;
            for (int32 i = 0; i < LevelPoints; ++i)
            {
                const FVector& P1 = WorkPoints[CurrentIndex - LevelPoints - 1];
                const FVector& P2 = WorkPoints[CurrentIndex - LevelPoints];
                
                // 绘制中间点
                DrawDebugPoint(World, WorkPoints[CurrentIndex], 4.0f, 
                    DebugColors.IntermediatePointColor.ToFColor(true), false, Duration);
                // 绘制中间连线
                DrawDebugLine(World, P1, P2, 
                    DebugColors.IntermediateLineColor.ToFColor(true), false, Duration);
                
                CurrentIndex++;
            }
        }

        // 绘制结果点（显示时间更长）
        const float ResultPointDuration = Duration * 5.0f;
        DrawDebugPoint(World, ResultPoint, 20.0f, DebugColors.ResultPointColor.ToFColor(true), false, ResultPointDuration);
    }

    return ResultPoint;
}

FVector UXToolsLibrary::CalculatePointAtParameter(const TArray<FVector>& Points, float t, TArray<FVector>& OutWorkPoints)
{
    const int32 PointCount = Points.Num();

    // -- 优化：为最常见的二阶和三阶曲线提供快速计算路径 --
    if (PointCount == 3) // 二阶 (Quadratic)
    {
        OutWorkPoints.Reset(6);
        OutWorkPoints.Append(Points);
        
        const FVector P01 = FMath::Lerp(Points[0], Points[1], t);
        const FVector P12 = FMath::Lerp(Points[1], Points[2], t);
        const FVector Result = FMath::Lerp(P01, P12, t);

        OutWorkPoints.Add(P01);
        OutWorkPoints.Add(P12);
        OutWorkPoints.Add(Result);
        
        return Result;
    }
    if (PointCount == 4) // 三阶 (Cubic)
    {
        OutWorkPoints.Reset(10);
        OutWorkPoints.Append(Points);

        const FVector P01 = FMath::Lerp(Points[0], Points[1], t);
        const FVector P12 = FMath::Lerp(Points[1], Points[2], t);
        const FVector P23 = FMath::Lerp(Points[2], Points[3], t);
        const FVector P012 = FMath::Lerp(P01, P12, t);
        const FVector P123 = FMath::Lerp(P12, P23, t);
        const FVector Result = FMath::Lerp(P012, P123, t);

        OutWorkPoints.Add(P01);
        OutWorkPoints.Add(P12);
        OutWorkPoints.Add(P23);
        OutWorkPoints.Add(P012);
        OutWorkPoints.Add(P123);
        OutWorkPoints.Add(Result);

        return Result;
    }

    // -- 回退到通用算法，处理其他阶数的曲线 --
    const int32 TotalLevels = PointCount - 1;
    const int32 TotalPoints = (PointCount * (PointCount + 1)) / 2;

    OutWorkPoints.Reset(TotalPoints);
    OutWorkPoints.Append(Points);

    for (int32 i = PointCount; i < TotalPoints; ++i)
    {
        OutWorkPoints.Add(FVector::ZeroVector);
    }

    int32 CurrentIndex = PointCount;
    for (int32 Level = 1; Level <= TotalLevels; ++Level)
    {
        const int32 LevelPoints = PointCount - Level;
        for (int32 i = 0; i < LevelPoints; ++i)
        {
            const FVector& P1 = OutWorkPoints[CurrentIndex - LevelPoints - 1];
            const FVector& P2 = OutWorkPoints[CurrentIndex - LevelPoints];
            OutWorkPoints[CurrentIndex++] = FMath::Lerp(P1, P2, t);
        }
    }

    return OutWorkPoints[TotalPoints - 1];
}

TArray<int32> UXToolsLibrary::TestPRDDistribution(float BaseChance)
{
    using namespace XToolsConfig;

    // ✅ 输入验证 - 使用配置常量
    if (BaseChance <= 0.0f || BaseChance > 1.0f)
    {
        UE_LOG(LogXTools, Warning, TEXT("TestPRDDistribution: 基础概率必须在(0,1]范围内，当前值: %.3f"), BaseChance);
        TArray<int32> EmptyDistribution;
        EmptyDistribution.Init(0, PRD_ARRAY_SIZE);
        return EmptyDistribution;
    }

    // ✅ 预分配内存，提升性能
    TArray<int32> Distribution;
    Distribution.Init(0, PRD_ARRAY_SIZE);

    TArray<int32> FailureTests;
    FailureTests.Init(0, PRD_ARRAY_SIZE);

    // ✅ 使用局部变量减少函数调用开销
    int32 CurrentFailureCount = 0;
    float ActualChance = 0.0f;
    int32 TotalSuccesses = 0;
    int32 TotalTests = 0;

    // ✅ 获取线程安全的 PRD 测试器
    FThreadSafePRDTester& PRDTester = FThreadSafePRDTester::Get();

    // ✅ 优化的测试循环 - 使用配置常量和线程安全
    while (TotalSuccesses < PRD_TARGET_SUCCESSES)
    {
        ++TotalTests;

        // ✅ 使用线程安全的 PRD 测试器
        int32 NextFailureCount = 0;
        const bool bSuccess = PRDTester.ExecutePRDTest(
            BaseChance,
            NextFailureCount,
            ActualChance,
            TEXT("PRD_Test"),
            CurrentFailureCount);

        // ✅ 边界检查优化 - 使用配置常量
        if (CurrentFailureCount <= PRD_MAX_FAILURE_COUNT)
        {
            ++FailureTests[CurrentFailureCount];
            if (bSuccess)
            {
                ++Distribution[CurrentFailureCount];
                ++TotalSuccesses;
            }
        }

        CurrentFailureCount = NextFailureCount;
    }

    // ✅ 优化的日志输出 - 减少字符串操作
    UE_LOG(LogXTools, Log, TEXT("=== PRD 分布测试结果 ==="));
    UE_LOG(LogXTools, Log, TEXT("基础概率: %.3f | 总测试次数: %d | 总成功次数: %d"), BaseChance, TotalTests, TotalSuccesses);
    UE_LOG(LogXTools, Log, TEXT("失败次数 | 成功次数 | 实际成功率 | 理论成功率 | 测试次数"));
    UE_LOG(LogXTools, Log, TEXT("---------|----------|------------|------------|----------"));

    // ✅ 优化循环 - 使用配置常量和线程安全
    for (int32 i = 0; i <= PRD_MAX_FAILURE_COUNT; ++i)
    {
        // ✅ 使用线程安全的理论概率获取
        float TheoreticalChance = 0.0f;
        int32 TempFailureCount = 0;
        PRDTester.ExecutePRDTest(
            BaseChance,
            TempFailureCount,
            TheoreticalChance,
            TEXT("Theory"),
            i);

        // ✅ 避免除零，使用更安全的计算
        const float ActualSuccessRate = (FailureTests[i] > 0) ?
            static_cast<float>(Distribution[i]) / static_cast<float>(FailureTests[i]) : 0.0f;

        UE_LOG(LogXTools, Log, TEXT("%8d | %8d | %9.2f%% | %9.2f%% | %8d"),
            i,
            Distribution[i],
            ActualSuccessRate * PERCENTAGE_MULTIPLIER,
            TheoreticalChance * PERCENTAGE_MULTIPLIER,
            FailureTests[i]);
    }

    UE_LOG(LogXTools, Log, TEXT("=== 测试完成 ==="));

    return Distribution;
}



// ✅ 清理点阵生成缓存的 Blueprint 函数
FString UXToolsLibrary::ClearPointSamplingCache()
{
    using namespace XToolsConfig;

    TStringBuilder<256> ResultBuilder;

    // 清理网格参数缓存
    FGridParametersCache& GridCache = FGridParametersCache::Get();
    GridCache.ClearCache();

    // 清理泊松采样结果缓存
    FPoissonResultCache& PoissonCache = FPoissonResultCache::Get();
    int32 CacheHits = 0, CacheMisses = 0;
    PoissonCache.GetStats(CacheHits, CacheMisses);
    PoissonCache.ClearCache();

    ResultBuilder.Append(TEXT("✅ 点阵生成缓存清理完成\n"));
    ResultBuilder.Append(TEXT("- '在模型中生成点阵'功能缓存已清空\n"));
    ResultBuilder.Append(FString::Printf(TEXT("- 泊松采样缓存已清空 (命中:%d, 未命中:%d)\n"), CacheHits, CacheMisses));
    ResultBuilder.Append(TEXT("- 计算参数已重置\n"));
    ResultBuilder.Append(TEXT("- 内存已释放\n"));

    const FString Result = ResultBuilder.ToString();
    UE_LOG(LogXTools, Log, TEXT("点阵生成缓存清理: %s"), *Result);

    return Result;
}



// ✅ 内部错误处理结构
struct FXToolsSamplingResult
{
    bool bSuccess = false;
    FString ErrorMessage;
    TArray<FVector> Points;
    int32 TotalPointsChecked = 0;
    int32 CulledPoints = 0;

    static FXToolsSamplingResult MakeError(const FString& Error)
    {
        FXToolsSamplingResult Result;
        Result.bSuccess = false;
        Result.ErrorMessage = Error;
        return Result;
    }

    static FXToolsSamplingResult MakeSuccess(const TArray<FVector>& InPoints, int32 TotalChecked = 0, int32 Culled = 0)
    {
        FXToolsSamplingResult Result;
        Result.bSuccess = true;
        Result.Points = InPoints;
        Result.TotalPointsChecked = TotalChecked;
        Result.CulledPoints = Culled;
        return Result;
    }
};

// ✅ 内部实现函数 - 使用现代错误处理
static FXToolsSamplingResult SamplePointsInternal(
    UWorld* World,
    AActor* TargetActor,
    UBoxComponent* BoundingBox,
    EXToolsSamplingMethod Method,
    float GridSpacing,
    float Noise,
    float TraceRadius,
    bool bEnableDebugDraw,
    bool bDrawOnlySuccessfulHits,
    bool bEnableBoundsCulling,
    float DebugDrawDuration,
    bool bUseComplexCollision);

// ✅ 公共蓝图接口 - 保持兼容性
void UXToolsLibrary::SamplePointsInsideStaticMeshWithBoxOptimized(
    const UObject* WorldContextObject,
    AActor* TargetActor,
    UBoxComponent* BoundingBox,
    EXToolsSamplingMethod Method,
    float GridSpacing,
    float Noise,
    float TraceRadius,
    bool bEnableDebugDraw,
    bool bDrawOnlySuccessfulHits,
    bool bEnableBoundsCulling,
    float DebugDrawDuration,
    TArray<FVector>& OutPoints,
    bool& bSuccess,
    bool bUseComplexCollision)
{
    // ✅ 输入验证和错误处理
    OutPoints.Empty();
    bSuccess = false;

    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        UE_LOG(LogXTools, Error, TEXT("在模型中生成点阵: 无效的世界上下文对象"));
        return;
    }

    // ✅ 调用内部实现
    const FXToolsSamplingResult Result = SamplePointsInternal(
        World, TargetActor, BoundingBox, Method, GridSpacing, Noise, TraceRadius,
        bEnableDebugDraw, bDrawOnlySuccessfulHits, bEnableBoundsCulling, DebugDrawDuration, bUseComplexCollision);

    // ✅ 设置输出参数
    bSuccess = Result.bSuccess;
    if (Result.bSuccess)
    {
        OutPoints = Result.Points;

        // ✅ 改进的日志输出
        if (bEnableBoundsCulling)
        {
            UE_LOG(LogXTools, Log, TEXT("采样完成: 检测 %d 个点, 剔除 %d 个点, 在 %s 内生成 %d 个有效点"),
                Result.TotalPointsChecked, Result.CulledPoints, *TargetActor->GetName(), OutPoints.Num());
        }
        else
        {
            UE_LOG(LogXTools, Log, TEXT("采样完成: 检测 %d 个点, 在 %s 内生成 %d 个有效点"),
                Result.TotalPointsChecked, *TargetActor->GetName(), OutPoints.Num());
        }
    }
    else
    {
        UE_LOG(LogXTools, Error, TEXT("采样失败: %s"), *Result.ErrorMessage);
    }
}

// ✅ 输入验证辅助函数
static FXToolsSamplingResult ValidateInputs(AActor* TargetActor, UBoxComponent* BoundingBox, float GridSpacing)
{
    if (!TargetActor)
    {
        return FXToolsSamplingResult::MakeError(TEXT("目标Actor为空"));
    }

    if (!BoundingBox)
    {
        return FXToolsSamplingResult::MakeError(TEXT("边界框组件为空"));
    }

    if (GridSpacing <= 0.0f)
    {
        return FXToolsSamplingResult::MakeError(FString::Printf(TEXT("网格间距必须大于0，当前值: %.2f"), GridSpacing));
    }

    UStaticMeshComponent* TargetMeshComponent = TargetActor->FindComponentByClass<UStaticMeshComponent>();
    if (!TargetMeshComponent)
    {
        return FXToolsSamplingResult::MakeError(FString::Printf(TEXT("Actor '%s' 没有StaticMeshComponent"), *TargetActor->GetName()));
    }

    return FXToolsSamplingResult::MakeSuccess({});
}



// ✅ 支持缓存的网格参数计算
static FGridParameters CalculateGridParameters(UBoxComponent* BoundingBox, float GridSpacing)
{
    // ✅ 创建缓存键
    FGridParametersKey CacheKey;
    CacheKey.BoxExtent = BoundingBox->GetScaledBoxExtent();
    CacheKey.BoxTransform = BoundingBox->GetComponentTransform();
    CacheKey.GridSpacing = GridSpacing;

    // ✅ 尝试从缓存获取
    FGridParametersCache& Cache = FGridParametersCache::Get();
    if (TOptional<FGridParameters> CachedParams = Cache.GetCachedParameters(CacheKey))
    {
        return CachedParams.GetValue();
    }

    // ✅ 缓存未命中，计算新参数
    FGridParameters Params;
    Params.bIsValid = false;

    // 获取旋转后的盒体信息
    Params.BoxTransform = BoundingBox->GetComponentToWorld();
    Params.Scale3D = Params.BoxTransform.GetScale3D();
    Params.ScaledBoxExtent = BoundingBox->GetScaledBoxExtent();
    Params.UnscaledBoxExtent = BoundingBox->GetUnscaledBoxExtent();

    // 根据世界空间的GridSpacing和组件的缩放，计算局部空间的步长
    Params.LocalGridStep = FVector(
        FMath::Abs(Params.Scale3D.X) > KINDA_SMALL_NUMBER ? GridSpacing / FMath::Abs(Params.Scale3D.X) : 0.0f,
        FMath::Abs(Params.Scale3D.Y) > KINDA_SMALL_NUMBER ? GridSpacing / FMath::Abs(Params.Scale3D.Y) : 0.0f,
        FMath::Abs(Params.Scale3D.Z) > KINDA_SMALL_NUMBER ? GridSpacing / FMath::Abs(Params.Scale3D.Z) : 0.0f
    );

    // 验证步长有效性
    if (!FMath::IsFinite(Params.LocalGridStep.X) || !FMath::IsFinite(Params.LocalGridStep.Y) || !FMath::IsFinite(Params.LocalGridStep.Z))
    {
        Params.ErrorMessage = TEXT("BoundingBox的某个轴缩放接近于零导致计算出无效的步长");
        return Params;
    }

    // 计算网格范围和步数
    Params.GridStart = -Params.UnscaledBoxExtent;
    Params.GridEnd = Params.UnscaledBoxExtent;

    Params.NumStepsX = FMath::FloorToInt((Params.GridEnd.X - Params.GridStart.X) / Params.LocalGridStep.X);
    Params.NumStepsY = FMath::FloorToInt((Params.GridEnd.Y - Params.GridStart.Y) / Params.LocalGridStep.Y);
    Params.NumStepsZ = FMath::FloorToInt((Params.GridEnd.Z - Params.GridStart.Z) / Params.LocalGridStep.Z);
    Params.TotalPoints = (Params.NumStepsX + 1) * (Params.NumStepsY + 1) * (Params.NumStepsZ + 1);

    Params.bIsValid = true;

    // ✅ 将计算结果存入缓存
    Cache.CacheParameters(CacheKey, Params);

    return Params;
}

// ✅ 表面邻近度采样实现
static FXToolsSamplingResult PerformSurfaceProximitySampling(
    UWorld* World,
    UStaticMeshComponent* TargetMeshComponent,
    const FGridParameters& GridParams,
    float Noise,
    float TraceRadius,
    bool bEnableDebugDraw,
    bool bDrawOnlySuccessfulHits,
    bool bEnableBoundsCulling,
    float DebugDrawDuration,
    bool bUseComplexCollision,
    const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
    EDrawDebugTrace::Type DebugDrawType)
{
    TArray<FVector> ValidPoints;
    ValidPoints.Reserve(GridParams.TotalPoints / 4); // 预分配内存，估计25%的点有效

    int32 TotalPointsChecked = 0;
    int32 CulledPoints = 0;

    // 获取目标模型的AABB用于粗筛
    FBox TargetBounds(EForceInit::ForceInit);
    if (bEnableBoundsCulling)
    {
        TargetBounds = TargetMeshComponent->Bounds.GetBox();
        TargetBounds = TargetBounds.ExpandBy(TraceRadius);
    }

    // 绘制调试盒体
    if (bEnableDebugDraw)
    {
        DrawDebugBox(
            World,
            GridParams.BoxTransform.GetLocation(),
            GridParams.ScaledBoxExtent,
            GridParams.BoxTransform.GetRotation(),
            FColor::Green,
            false,
            DebugDrawDuration,
            0,
            2.0f);
    }

    // 核心采样循环 - 使用整数索引避免浮点累积误差
    for (int32 i = 0; i <= GridParams.NumStepsX; ++i)
    {
        const float X = GridParams.GridStart.X + i * GridParams.LocalGridStep.X;
        for (int32 j = 0; j <= GridParams.NumStepsY; ++j)
        {
            const float Y = GridParams.GridStart.Y + j * GridParams.LocalGridStep.Y;
            for (int32 k = 0; k <= GridParams.NumStepsZ; ++k)
            {
                const float Z = GridParams.GridStart.Z + k * GridParams.LocalGridStep.Z;

                ++TotalPointsChecked;
                FVector LocalPoint(X, Y, Z);

                // 应用噪点偏移
                if (Noise > 0.0f)
                {
                    const FVector RandomOffset(
                        FMath::FRandRange(-Noise, Noise),
                        FMath::FRandRange(-Noise, Noise),
                        FMath::FRandRange(-Noise, Noise)
                    );
                    LocalPoint += RandomOffset;
                }

                const FVector WorldPoint = GridParams.BoxTransform.TransformPosition(LocalPoint);

                // 粗筛阶段 - 包围盒剔除
                if (bEnableBoundsCulling && !TargetBounds.IsInsideOrOn(WorldPoint))
                {
                    ++CulledPoints;
                    continue;
                }

                // 精确碰撞检测
                FHitResult HitResult;
                const bool bHit = UKismetSystemLibrary::SphereTraceSingleForObjects(
                    World,
                    WorldPoint,
                    WorldPoint,
                    TraceRadius,
                    ObjectTypes,
                    bUseComplexCollision,
                    TArray<AActor*>(), // 空的忽略列表
                    DebugDrawType,
                    HitResult,
                    true,
                    FLinearColor::Red,
                    FLinearColor::Green,
                    DebugDrawDuration
                );

                if (bHit)
                {
                    ValidPoints.Add(WorldPoint);

                    // 只绘制成功命中的点
                    if (bEnableDebugDraw && bDrawOnlySuccessfulHits)
                    {
                        DrawDebugSphere(World, WorldPoint, TraceRadius, 12, FColor::Blue, false, DebugDrawDuration);
                    }
                }
            }
        }
    }

    return FXToolsSamplingResult::MakeSuccess(ValidPoints, TotalPointsChecked, CulledPoints);
}

// ✅ 主要的内部实现函数
static FXToolsSamplingResult SamplePointsInternal(
    UWorld* World,
    AActor* TargetActor,
    UBoxComponent* BoundingBox,
    EXToolsSamplingMethod Method,
    float GridSpacing,
    float Noise,
    float TraceRadius,
    bool bEnableDebugDraw,
    bool bDrawOnlySuccessfulHits,
    bool bEnableBoundsCulling,
    float DebugDrawDuration,
    bool bUseComplexCollision)
{
    // 步骤1：输入验证
    const FXToolsSamplingResult ValidationResult = ValidateInputs(TargetActor, BoundingBox, GridSpacing);
    if (!ValidationResult.bSuccess)
    {
        return ValidationResult;
    }

    // 步骤2：计算网格参数
    const FGridParameters GridParams = CalculateGridParameters(BoundingBox, GridSpacing);
    if (!GridParams.bIsValid)
    {
        return FXToolsSamplingResult::MakeError(GridParams.ErrorMessage);
    }

    // 步骤3：获取目标组件和设置追踪参数
    UStaticMeshComponent* TargetMeshComponent = TargetActor->FindComponentByClass<UStaticMeshComponent>();
    const TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = { UEngineTypes::ConvertToObjectType(TargetMeshComponent->GetCollisionObjectType()) };
    const EDrawDebugTrace::Type DebugDrawType = (bEnableDebugDraw && !bDrawOnlySuccessfulHits) ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;

    // 步骤4：执行采样
    switch (Method)
    {
        case EXToolsSamplingMethod::SurfaceProximity:
            return PerformSurfaceProximitySampling(World, TargetMeshComponent, GridParams, Noise, TraceRadius,
                bEnableDebugDraw, bDrawOnlySuccessfulHits, bEnableBoundsCulling, DebugDrawDuration, bUseComplexCollision, ObjectTypes, DebugDrawType);

        case EXToolsSamplingMethod::Voxelize:
            return FXToolsSamplingResult::MakeError(TEXT("实体填充采样(Voxelize)模式尚未实现"));

        default:
            return FXToolsSamplingResult::MakeError(TEXT("未知的采样模式"));
    }
}

// ============================================================================
// 泊松圆盘采样实现
// ============================================================================

namespace PoissonSamplingHelpers
{
    /**
     * 根据目标点数计算合适的Radius
     */
    static float CalculateRadiusFromTargetCount(int32 TargetPointCount, float Width, float Height, float Depth, bool bIs2DPlane)
    {
        if (TargetPointCount <= 0)
        {
            return -1.0f;
        }
        
        if (bIs2DPlane)
        {
            // 2D情况：根据面积估算Radius
            const float Area = Width * Height;
            // 泊松采样理论密度约为 0.8 * (1 / (pi * r^2))
            return FMath::Sqrt(0.8f * Area / (TargetPointCount * PI));
        }
        else
        {
            // 3D情况：根据体积估算Radius
            const float Volume = Width * Height * Depth;
            // 泊松采样理论密度约为 0.6 * (1 / (4/3 * pi * r^3))
            return FMath::Pow(0.45f * Volume / (TargetPointCount * PI), 1.0f / 3.0f);
        }
    }
    
    /**
     * 裁剪点数组到目标数量（使用Fisher-Yates洗牌）
     * @param Stream 可选的随机流，nullptr时使用全局随机
     */
    static void TrimToTargetCount(TArray<FVector>& Points, int32 TargetPointCount, FRandomStream* Stream = nullptr)
    {
        if (TargetPointCount > 0 && Points.Num() > TargetPointCount)
        {
            // 使用 Fisher-Yates 洗牌算法随机选择
            for (int32 i = Points.Num() - 1; i > TargetPointCount; --i)
            {
                const int32 j = Stream ? Stream->RandRange(0, i) : FMath::RandRange(0, i);
                Points.Swap(i, j);
            }
            
            // 裁剪到目标数量
            Points.SetNum(TargetPointCount);
        }
    }
    
    /**
     * 应用扰动噪波到点集
     * @param Points 要扰动的点数组
     * @param Radius 参考半径（扰动范围基于此值）
     * @param JitterStrength 扰动强度 0-1（0=无扰动，1=最大扰动）
     * @param Stream 可选的随机流，nullptr时使用全局随机
     */
    static void ApplyJitter(TArray<FVector>& Points, float Radius, float JitterStrength, FRandomStream* Stream = nullptr)
    {
        if (JitterStrength <= 0.0f || Points.Num() == 0)
        {
            return;
        }
        
        // 扰动范围：最大为半径的50%，由强度控制
        const float MaxJitter = Radius * FMath::Clamp(JitterStrength, 0.0f, 1.0f) * 0.5f;
        
        for (FVector& Point : Points)
        {
            if (Stream)
            {
                Point.X += Stream->FRandRange(-MaxJitter, MaxJitter);
                Point.Y += Stream->FRandRange(-MaxJitter, MaxJitter);
                Point.Z += Stream->FRandRange(-MaxJitter, MaxJitter);
            }
            else
            {
                Point.X += FMath::FRandRange(-MaxJitter, MaxJitter);
                Point.Y += FMath::FRandRange(-MaxJitter, MaxJitter);
                Point.Z += FMath::FRandRange(-MaxJitter, MaxJitter);
            }
        }
    }
    
    /**
     * 应用Transform变换（移除缩放，因为Extent已包含缩放）
     * @param bWorldSpace true=应用位置+旋转（世界坐标），false=仅应用旋转（局部坐标）
     */
    static void ApplyTransform(TArray<FVector>& Points, const FTransform& Transform, bool bWorldSpace)
    {
        if (bWorldSpace)
        {
            // 世界坐标：应用位置+旋转（不应用缩放，因为Extent已包含）
            FTransform TransformNoScale = Transform;
            TransformNoScale.SetScale3D(FVector::OneVector);
            
            for (FVector& Point : Points)
            {
                Point = TransformNoScale.TransformPosition(Point);
            }
        }
        else
        {
            // 局部坐标：仅应用旋转，保持局部位置
            const FQuat Rotation = Transform.GetRotation();
            
            for (FVector& Point : Points)
            {
                Point = Rotation.RotateVector(Point);
            }
        }
    }

    /**
     * 在给定点周围生成随机点（2D）
     * @param Stream 可选的随机流，nullptr时使用全局随机
     */
    static FVector2D GenerateRandomPointAround2D(const FVector2D& Point, float MinDist, float MaxDist, FRandomStream* Stream = nullptr)
    {
        const float Angle = Stream ? Stream->FRandRange(0.0f, 2.0f * PI) : FMath::FRandRange(0.0f, 2.0f * PI);
        const float Distance = Stream ? Stream->FRandRange(MinDist, MaxDist) : FMath::FRandRange(MinDist, MaxDist);
        return FVector2D(
            Point.X + Distance * FMath::Cos(Angle),
            Point.Y + Distance * FMath::Sin(Angle)
        );
    }

    /**
     * 在给定点周围生成随机点（3D）
     * @param Stream 可选的随机流，nullptr时使用全局随机
     */
    static FVector GenerateRandomPointAround3D(const FVector& Point, float MinDist, float MaxDist, FRandomStream* Stream = nullptr)
    {
        // 在球面上均匀采样
        const float Theta = Stream ? Stream->FRandRange(0.0f, 2.0f * PI) : FMath::FRandRange(0.0f, 2.0f * PI);
        const float Phi = FMath::Acos(Stream ? Stream->FRandRange(-1.0f, 1.0f) : FMath::FRandRange(-1.0f, 1.0f));
        const float Distance = Stream ? Stream->FRandRange(MinDist, MaxDist) : FMath::FRandRange(MinDist, MaxDist);
        
        return FVector(
            Point.X + Distance * FMath::Sin(Phi) * FMath::Cos(Theta),
            Point.Y + Distance * FMath::Sin(Phi) * FMath::Sin(Theta),
            Point.Z + Distance * FMath::Cos(Phi)
        );
    }

    /**
     * 检查点是否有效（2D）- 优化版：使用平方距离避免开方
     */
    static bool IsValidPoint2D(
        const FVector2D& Point,
        float Radius,
        float Width,
        float Height,
        const TArray<FVector2D>& Grid,
        int32 GridWidth,
        int32 GridHeight,
        float CellSize)
    {
        // 边界检查
        if (Point.X < 0 || Point.X >= Width || Point.Y < 0 || Point.Y >= Height)
        {
            return false;
        }

        // 计算网格坐标
        const int32 CellX = FMath::FloorToInt(Point.X / CellSize);
        const int32 CellY = FMath::FloorToInt(Point.Y / CellSize);

        // 检查周围的网格单元
        const int32 SearchStartX = FMath::Max(CellX - 2, 0);
        const int32 SearchStartY = FMath::Max(CellY - 2, 0);
        const int32 SearchEndX = FMath::Min(CellX + 2, GridWidth - 1);
        const int32 SearchEndY = FMath::Min(CellY + 2, GridHeight - 1);

        // 使用平方距离比较，避免开方运算
        const float RadiusSquared = Radius * Radius;

        for (int32 x = SearchStartX; x <= SearchEndX; ++x)
        {
            for (int32 y = SearchStartY; y <= SearchEndY; ++y)
            {
                const FVector2D& Neighbor = Grid[y * GridWidth + x];
                if (Neighbor != FVector2D::ZeroVector)
                {
                    const float DistSquared = FVector2D::DistSquared(Point, Neighbor);
                    if (DistSquared < RadiusSquared)
                    {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    /**
     * 检查点是否有效（3D）- 优化版：使用平方距离避免开方
     */
    static bool IsValidPoint3D(
        const FVector& Point,
        float Radius,
        float Width,
        float Height,
        float Depth,
        const TArray<FVector>& Grid,
        int32 GridWidth,
        int32 GridHeight,
        int32 GridDepth,
        float CellSize)
    {
        // 边界检查
        if (Point.X < 0 || Point.X >= Width ||
            Point.Y < 0 || Point.Y >= Height ||
            Point.Z < 0 || Point.Z >= Depth)
        {
            return false;
        }

        // 计算网格坐标
        const int32 CellX = FMath::FloorToInt(Point.X / CellSize);
        const int32 CellY = FMath::FloorToInt(Point.Y / CellSize);
        const int32 CellZ = FMath::FloorToInt(Point.Z / CellSize);

        // 检查周围的网格单元
        const int32 SearchStartX = FMath::Max(CellX - 2, 0);
        const int32 SearchStartY = FMath::Max(CellY - 2, 0);
        const int32 SearchStartZ = FMath::Max(CellZ - 2, 0);
        const int32 SearchEndX = FMath::Min(CellX + 2, GridWidth - 1);
        const int32 SearchEndY = FMath::Min(CellY + 2, GridHeight - 1);
        const int32 SearchEndZ = FMath::Min(CellZ + 2, GridDepth - 1);

        // 使用平方距离比较，避免开方运算
        const float RadiusSquared = Radius * Radius;

        for (int32 x = SearchStartX; x <= SearchEndX; ++x)
        {
            for (int32 y = SearchStartY; y <= SearchEndY; ++y)
            {
                for (int32 z = SearchStartZ; z <= SearchEndZ; ++z)
                {
                    const int32 Index = z * (GridWidth * GridHeight) + y * GridWidth + x;
                    const FVector& Neighbor = Grid[Index];
                    if (Neighbor != FVector::ZeroVector)
                    {
                        const float DistSquared = FVector::DistSquared(Point, Neighbor);
                        if (DistSquared < RadiusSquared)
                        {
                            return false;
                        }
                    }
                }
            }
        }

        return true;
    }
}

TArray<FVector2D> UXToolsLibrary::GeneratePoissonPoints2D(float Width, float Height, float Radius, int32 MaxAttempts)
{
    using namespace PoissonSamplingHelpers;

    // 输入验证
    if (Width <= 0.0f || Height <= 0.0f || Radius <= 0.0f || MaxAttempts <= 0)
    {
        UE_LOG(LogXTools, Warning, TEXT("GeneratePoissonPoints2D: 无效的输入参数"));
        return TArray<FVector2D>();
    }

    TArray<FVector2D> ActivePoints;
    TArray<FVector2D> Points;

    // 计算网格参数
    const float CellSize = Radius / FMath::Sqrt(2.0f);
    const int32 GridWidth = FMath::CeilToInt(Width / CellSize);
    const int32 GridHeight = FMath::CeilToInt(Height / CellSize);

    // 初始化网格
    TArray<FVector2D> Grid;
    Grid.SetNumZeroed(GridWidth * GridHeight);

    // Lambda：获取网格坐标
    auto GetCellCoords = [CellSize](const FVector2D& Point) -> FIntPoint
    {
        return FIntPoint(
            FMath::FloorToInt(Point.X / CellSize),
            FMath::FloorToInt(Point.Y / CellSize)
        );
    };

    // 生成初始点
    const FVector2D InitialPoint(FMath::FRandRange(0.0f, Width), FMath::FRandRange(0.0f, Height));
    ActivePoints.Add(InitialPoint);
    Points.Add(InitialPoint);
    
    const FIntPoint InitialCell = GetCellCoords(InitialPoint);
    Grid[InitialCell.Y * GridWidth + InitialCell.X] = InitialPoint;

    // 主循环
    while (ActivePoints.Num() > 0)
    {
        const int32 Index = FMath::RandRange(0, ActivePoints.Num() - 1);
        const FVector2D Point = ActivePoints[Index];
        bool bFound = false;

        // 尝试在当前点周围生成新点
        for (int32 i = 0; i < MaxAttempts; ++i)
        {
            const FVector2D NewPoint = GenerateRandomPointAround2D(Point, Radius, 2.0f * Radius);
            
            if (IsValidPoint2D(NewPoint, Radius, Width, Height, Grid, GridWidth, GridHeight, CellSize))
            {
                ActivePoints.Add(NewPoint);
                Points.Add(NewPoint);
                
                const FIntPoint NewCell = GetCellCoords(NewPoint);
                Grid[NewCell.Y * GridWidth + NewCell.X] = NewPoint;
                
                bFound = true;
                break;
            }
        }

        // 如果未找到有效点，将当前点标记为不活跃
        if (!bFound)
        {
            ActivePoints.RemoveAt(Index);
        }
    }

    UE_LOG(LogXTools, Log, TEXT("GeneratePoissonPoints2D: 生成了 %d 个点 (区域: %.1fx%.1f, 半径: %.1f)"),
        Points.Num(), Width, Height, Radius);

    return Points;
}

TArray<FVector> UXToolsLibrary::GeneratePoissonPoints3D(
    float Width, float Height, float Depth, float Radius, int32 MaxAttempts)
{
    using namespace PoissonSamplingHelpers;

    // 输入验证
    if (Width <= 0.0f || Height <= 0.0f || Depth <= 0.0f || Radius <= 0.0f || MaxAttempts <= 0)
    {
        UE_LOG(LogXTools, Warning, TEXT("GeneratePoissonPoints3D: 无效的输入参数"));
        return TArray<FVector>();
    }

    TArray<FVector> ActivePoints;
    TArray<FVector> Points;

    // 计算网格参数（3D中使用sqrt(3)而不是sqrt(2)）
    const float CellSize = Radius / FMath::Sqrt(3.0f);
    const int32 GridWidth = FMath::CeilToInt(Width / CellSize);
    const int32 GridHeight = FMath::CeilToInt(Height / CellSize);
    const int32 GridDepth = FMath::CeilToInt(Depth / CellSize);

    // 初始化网格
    TArray<FVector> Grid;
    Grid.SetNumZeroed(GridWidth * GridHeight * GridDepth);

    // Lambda：获取网格坐标
    auto GetCellCoords = [CellSize](const FVector& Point) -> FIntVector
    {
        return FIntVector(
            FMath::FloorToInt(Point.X / CellSize),
            FMath::FloorToInt(Point.Y / CellSize),
            FMath::FloorToInt(Point.Z / CellSize)
        );
    };

    // 生成初始点
    const FVector InitialPoint(
        FMath::FRandRange(0.0f, Width),
        FMath::FRandRange(0.0f, Height),
        FMath::FRandRange(0.0f, Depth)
    );
    ActivePoints.Add(InitialPoint);
    Points.Add(InitialPoint);
    
    const FIntVector InitialCell = GetCellCoords(InitialPoint);
    const int32 InitialIndex = InitialCell.Z * (GridWidth * GridHeight) + 
                               InitialCell.Y * GridWidth + InitialCell.X;
    Grid[InitialIndex] = InitialPoint;

    // 主循环
    while (ActivePoints.Num() > 0)
    {
        const int32 Index = FMath::RandRange(0, ActivePoints.Num() - 1);
        const FVector Point = ActivePoints[Index];
        bool bFound = false;

        // 尝试在当前点周围生成新点
        for (int32 i = 0; i < MaxAttempts; ++i)
        {
            const FVector NewPoint = GenerateRandomPointAround3D(Point, Radius, 2.0f * Radius);
            
            if (IsValidPoint3D(NewPoint, Radius, Width, Height, Depth, 
                              Grid, GridWidth, GridHeight, GridDepth, CellSize))
            {
                ActivePoints.Add(NewPoint);
                Points.Add(NewPoint);
                
                const FIntVector NewCell = GetCellCoords(NewPoint);
                const int32 NewIndex = NewCell.Z * (GridWidth * GridHeight) + 
                                      NewCell.Y * GridWidth + NewCell.X;
                Grid[NewIndex] = NewPoint;
                
                bFound = true;
                break;
            }
        }

        // 如果未找到有效点，将当前点标记为不活跃
        if (!bFound)
        {
            ActivePoints.RemoveAt(Index);
        }
    }

    UE_LOG(LogXTools, Log, TEXT("GeneratePoissonPoints3D: 生成了 %d 个点 (区域: %.1fx%.1fx%.1f, 半径: %.1f)"),
        Points.Num(), Width, Height, Depth, Radius);

    return Points;
}

TArray<FVector> UXToolsLibrary::GeneratePoissonPointsInBox(
    UBoxComponent* BoundingBox,
    float Radius,
    int32 MaxAttempts,
    bool bWorldSpace,
    int32 TargetPointCount,
    float JitterStrength,
    bool bUseCache)
{
    // 输入验证
    if (!BoundingBox)
    {
        UE_LOG(LogXTools, Warning, TEXT("GeneratePoissonPointsInBox: 盒体组件无效"));
        return TArray<FVector>();
    }

    if (MaxAttempts <= 0)
    {
        UE_LOG(LogXTools, Warning, TEXT("GeneratePoissonPointsInBox: MaxAttempts必须大于0"));
        return TArray<FVector>();
    }

    // 获取盒体的缩放后尺寸（GetScaledBoxExtent已包含组件缩放）
    const FVector BoxExtent = BoundingBox->GetScaledBoxExtent();
    const FTransform BoxTransform = BoundingBox->GetComponentTransform();
    
    // 计算完整尺寸（Extent是半尺寸，直接乘2即可）
    const float Width = BoxExtent.X * 2.0f;
    const float Height = BoxExtent.Y * 2.0f;
    const float Depth = BoxExtent.Z * 2.0f;

    // 检测是否为平面（Z接近0）
    const bool bIs2DPlane = FMath::IsNearlyZero(Depth, 1.0f);

    // 处理目标点数模式：自动计算Radius
    float ActualRadius = Radius;
    if (TargetPointCount > 0)
    {
        ActualRadius = PoissonSamplingHelpers::CalculateRadiusFromTargetCount(
            TargetPointCount, Width, Height, Depth, bIs2DPlane);
        
        UE_LOG(LogXTools, Log, TEXT("GeneratePoissonPointsInBox: 根据目标点数 %d 计算得出 Radius = %.2f"), 
            TargetPointCount, ActualRadius);
    }

    // 验证最终的Radius
    if (ActualRadius <= 0.0f)
    {
        UE_LOG(LogXTools, Warning, TEXT("GeneratePoissonPointsInBox: 计算出的Radius无效 (%.2f)"), ActualRadius);
        return TArray<FVector>();
    }

    // 缓存系统检查（包含旋转信息）
    if (bUseCache)
    {
        FPoissonCacheKey CacheKey;
        CacheKey.BoxExtent = BoxExtent;
        CacheKey.Rotation = BoundingBox->GetComponentQuat();  // 包含旋转
        CacheKey.Radius = ActualRadius;
        CacheKey.TargetPointCount = TargetPointCount;
        CacheKey.JitterStrength = JitterStrength;
        CacheKey.bIs2D = bIs2DPlane;
        CacheKey.bWorldSpace = bWorldSpace;
        
        if (TOptional<TArray<FVector>> CachedPoints = FPoissonResultCache::Get().GetCached(CacheKey))
        {
            UE_LOG(LogXTools, Verbose, TEXT("GeneratePoissonPointsInBox: 使用缓存结果 (%d 个点)"), 
                CachedPoints.GetValue().Num());
            return CachedPoints.GetValue();
        }
    }

    TArray<FVector> Points;

    // 根据是否为平面选择不同的采样方式
    if (bIs2DPlane)
    {
        // 2D平面采样（XY平面）
        TArray<FVector2D> Points2D = GeneratePoissonPoints2D(Width, Height, ActualRadius, MaxAttempts);
        
        // 转换2D点为3D点（Z=0）
        Points.Reserve(Points2D.Num());
        for (const FVector2D& Point2D : Points2D)
        {
            Points.Add(FVector(Point2D.X, Point2D.Y, 0.0f));
        }
        
        UE_LOG(LogXTools, Log, TEXT("GeneratePoissonPointsInBox: 使用2D平面采样 (%.1fx%.1f)"), Width, Height);
    }
    else
    {
        // 3D体积采样
        Points = GeneratePoissonPoints3D(Width, Height, Depth, ActualRadius, MaxAttempts);
        
        UE_LOG(LogXTools, Log, TEXT("GeneratePoissonPointsInBox: 使用3D体积采样 (%.1fx%.1fx%.1f)"), 
            Width, Height, Depth);
    }

    // 转换坐标：从 [0, Size] 转换到 [-HalfSize, +HalfSize]
    for (FVector& Point : Points)
    {
        // 转换到局部空间中心对齐
        Point.X -= BoxExtent.X;
        Point.Y -= BoxExtent.Y;
        if (!bIs2DPlane)
        {
            Point.Z -= BoxExtent.Z;
        }
        // 2D模式下 Point.Z 保持为 0
    }

    // 如果指定了目标点数，严格裁剪到目标数量
    const int32 OriginalCount = Points.Num();
    PoissonSamplingHelpers::TrimToTargetCount(Points, TargetPointCount);
    
    if (TargetPointCount > 0 && OriginalCount > Points.Num())
    {
        UE_LOG(LogXTools, Log, TEXT("GeneratePoissonPointsInBox: 从 %d 个点随机裁剪到目标点数 %d"), 
            OriginalCount, TargetPointCount);
    }

    // 应用扰动噪波
    PoissonSamplingHelpers::ApplyJitter(Points, ActualRadius, JitterStrength);

    // 应用变换（始终应用旋转，世界坐标时额外应用位置）
    PoissonSamplingHelpers::ApplyTransform(Points, BoxTransform, bWorldSpace);
    
    if (bWorldSpace)
    {
        UE_LOG(LogXTools, Log, TEXT("GeneratePoissonPointsInBox: 生成了 %d 个点 (世界坐标, Radius=%.2f)"), 
            Points.Num(), ActualRadius);
    }
    else
    {
        UE_LOG(LogXTools, Log, TEXT("GeneratePoissonPointsInBox: 生成了 %d 个点 (局部坐标+旋转, Radius=%.2f)"), 
            Points.Num(), ActualRadius);
    }

    // 存入缓存（包含旋转信息）
    if (bUseCache)
    {
        FPoissonCacheKey CacheKey;
        CacheKey.BoxExtent = BoxExtent;
        CacheKey.Rotation = BoundingBox->GetComponentQuat();  // 包含旋转
        CacheKey.Radius = ActualRadius;
        CacheKey.TargetPointCount = TargetPointCount;
        CacheKey.JitterStrength = JitterStrength;
        CacheKey.bIs2D = bIs2DPlane;
        CacheKey.bWorldSpace = bWorldSpace;
        
        FPoissonResultCache::Get().Store(CacheKey, Points);
    }

    return Points;
}

TArray<FVector> UXToolsLibrary::GeneratePoissonPointsInBoxByVector(
    FVector BoxExtent,
    FTransform Transform,
    float Radius,
    int32 MaxAttempts,
    bool bWorldSpace,
    int32 TargetPointCount,
    float JitterStrength,
    bool bUseCache)
{
    // 输入验证
    if (BoxExtent.X <= 0.0f || BoxExtent.Y <= 0.0f || BoxExtent.Z < 0.0f)
    {
        UE_LOG(LogXTools, Warning, TEXT("GeneratePoissonPointsInBoxByVector: BoxExtent无效 (%s)"), *BoxExtent.ToString());
        return TArray<FVector>();
    }

    if (MaxAttempts <= 0)
    {
        UE_LOG(LogXTools, Warning, TEXT("GeneratePoissonPointsInBoxByVector: MaxAttempts必须大于0"));
        return TArray<FVector>();
    }

    // BoxExtent是半尺寸，计算完整尺寸
    const float Width = BoxExtent.X * 2.0f;
    const float Height = BoxExtent.Y * 2.0f;
    const float Depth = BoxExtent.Z * 2.0f;

    // 检测是否为平面（Z接近0）
    const bool bIs2DPlane = FMath::IsNearlyZero(Depth, 1.0f);

    // 处理目标点数模式：自动计算Radius
    float ActualRadius = Radius;
    if (TargetPointCount > 0)
    {
        ActualRadius = PoissonSamplingHelpers::CalculateRadiusFromTargetCount(
            TargetPointCount, Width, Height, Depth, bIs2DPlane);
        
        UE_LOG(LogXTools, Log, TEXT("GeneratePoissonPointsInBoxByVector: 根据目标点数 %d 计算得出 Radius = %.2f"), 
            TargetPointCount, ActualRadius);
    }

    // 验证最终的Radius
    if (ActualRadius <= 0.0f)
    {
        UE_LOG(LogXTools, Warning, TEXT("GeneratePoissonPointsInBoxByVector: 计算出的Radius无效 (%.2f)"), ActualRadius);
        return TArray<FVector>();
    }

    // 缓存系统检查（包含旋转信息，使用传入的BoxExtent）
    if (bUseCache)
    {
        FPoissonCacheKey CacheKey;
        CacheKey.BoxExtent = BoxExtent;
        CacheKey.Rotation = Transform.GetRotation();  // 包含旋转
        CacheKey.Radius = ActualRadius;
        CacheKey.TargetPointCount = TargetPointCount;
        CacheKey.JitterStrength = JitterStrength;
        CacheKey.bIs2D = bIs2DPlane;
        CacheKey.bWorldSpace = bWorldSpace;
        
        if (TOptional<TArray<FVector>> CachedPoints = FPoissonResultCache::Get().GetCached(CacheKey))
        {
            UE_LOG(LogXTools, Verbose, TEXT("GeneratePoissonPointsInBoxByVector: 使用缓存结果 (%d 个点)"), 
                CachedPoints.GetValue().Num());
            return CachedPoints.GetValue();
        }
    }

    TArray<FVector> Points;

    // 根据是否为平面选择不同的采样方式
    if (bIs2DPlane)
    {
        // 2D平面采样（XY平面）
        TArray<FVector2D> Points2D = GeneratePoissonPoints2D(Width, Height, ActualRadius, MaxAttempts);
        
        // 转换2D点为3D点（Z=0）
        Points.Reserve(Points2D.Num());
        for (const FVector2D& Point2D : Points2D)
        {
            Points.Add(FVector(Point2D.X, Point2D.Y, 0.0f));
        }
        
        UE_LOG(LogXTools, Log, TEXT("GeneratePoissonPointsInBoxByVector: 使用2D平面采样 (%.1fx%.1f)"), Width, Height);
    }
    else
    {
        // 3D体积采样
        Points = GeneratePoissonPoints3D(Width, Height, Depth, ActualRadius, MaxAttempts);
        
        UE_LOG(LogXTools, Log, TEXT("GeneratePoissonPointsInBoxByVector: 使用3D体积采样 (%.1fx%.1fx%.1f)"), 
            Width, Height, Depth);
    }

    // 转换坐标：从 [0, Size] 转换到 [-HalfSize, +HalfSize] （局部空间，中心对齐）
    for (FVector& Point : Points)
    {
        // 转换到局部空间：从[0,Width]转到[-BoxExtent,+BoxExtent]
        Point.X -= BoxExtent.X;
        Point.Y -= BoxExtent.Y;
        if (!bIs2DPlane)
        {
            Point.Z -= BoxExtent.Z;
        }
    }

    // 如果指定了目标点数，严格裁剪到目标数量
    const int32 OriginalCount = Points.Num();
    PoissonSamplingHelpers::TrimToTargetCount(Points, TargetPointCount);
    
    if (TargetPointCount > 0 && OriginalCount > Points.Num())
    {
        UE_LOG(LogXTools, Log, TEXT("GeneratePoissonPointsInBoxByVector: 从 %d 个点随机裁剪到目标点数 %d"), 
            OriginalCount, TargetPointCount);
    }

    // 应用扰动噪波
    PoissonSamplingHelpers::ApplyJitter(Points, ActualRadius, JitterStrength);

    // 应用变换（始终应用旋转，世界坐标时额外应用位置）
    PoissonSamplingHelpers::ApplyTransform(Points, Transform, bWorldSpace);
    
    if (bWorldSpace)
    {
        UE_LOG(LogXTools, Log, TEXT("GeneratePoissonPointsInBoxByVector: 生成了 %d 个点 (世界坐标, Radius=%.2f)"), 
            Points.Num(), ActualRadius);
    }
    else
    {
        UE_LOG(LogXTools, Log, TEXT("GeneratePoissonPointsInBoxByVector: 生成了 %d 个点 (局部坐标+旋转, Radius=%.2f)"), 
            Points.Num(), ActualRadius);
    }

    // 存入缓存（包含旋转信息）
    if (bUseCache)
    {
        FPoissonCacheKey CacheKey;
        CacheKey.BoxExtent = BoxExtent;
        CacheKey.Rotation = Transform.GetRotation();  // 包含旋转
        CacheKey.Radius = ActualRadius;
        CacheKey.TargetPointCount = TargetPointCount;
        CacheKey.JitterStrength = JitterStrength;
        CacheKey.bIs2D = bIs2DPlane;
        CacheKey.bWorldSpace = bWorldSpace;
        
        FPoissonResultCache::Get().Store(CacheKey, Points);
    }

    return Points;
}

// ========== 从流送版本（确定性随机）==========

TArray<FVector> UXToolsLibrary::GeneratePoissonPointsInBoxFromStream(
    const FRandomStream& RandomStream,
    UBoxComponent* BoundingBox,
    float Radius,
    int32 MaxAttempts,
    bool bWorldSpace,
    int32 TargetPointCount,
    float JitterStrength)
{
    if (!BoundingBox)
    {
        UE_LOG(LogXTools, Warning, TEXT("GeneratePoissonPointsInBoxFromStream: BoundingBox is nullptr"));
        return TArray<FVector>();
    }

    // 调用向量版本
    return GeneratePoissonPointsInBoxByVectorFromStream(
        RandomStream,
        BoundingBox->GetScaledBoxExtent(),
        BoundingBox->GetComponentTransform(),
        Radius,
        MaxAttempts,
        bWorldSpace,
        TargetPointCount,
        JitterStrength
    );
}

TArray<FVector> UXToolsLibrary::GeneratePoissonPointsInBoxByVectorFromStream(
    const FRandomStream& RandomStream,
    FVector BoxExtent,
    FTransform Transform,
    float Radius,
    int32 MaxAttempts,
    bool bWorldSpace,
    int32 TargetPointCount,
    float JitterStrength)
{
    using namespace PoissonSamplingHelpers;
    
    // 注意：需要修改Stream的内部状态，使用const_cast
    FRandomStream* Stream = const_cast<FRandomStream*>(&RandomStream);

    const float Width = BoxExtent.X * 2.0f;
    const float Height = BoxExtent.Y * 2.0f;
    const float Depth = BoxExtent.Z * 2.0f;
    const bool bIs2DPlane = FMath::IsNearlyZero(Depth, 1.0f);

    // 计算实际Radius
    float ActualRadius = Radius;
    if (TargetPointCount > 0 && Radius <= 0.0f)
    {
        ActualRadius = CalculateRadiusFromTargetCount(Width, Height, Depth, TargetPointCount, bIs2DPlane);
    }

    // 生成点（局部坐标，使用Stream替代FMath随机函数）
    TArray<FVector> Points;
    
    if (bIs2DPlane)
    {
        // 2D采样
        TArray<FVector2D> ActivePoints;
        TArray<FVector2D> Points2D;
        
        const float CellSize = ActualRadius / FMath::Sqrt(2.0f);
        const int32 GridWidth = FMath::CeilToInt(Width / CellSize);
        const int32 GridHeight = FMath::CeilToInt(Height / CellSize);
        
        TArray<FVector2D> Grid;
        Grid.SetNumZeroed(GridWidth * GridHeight);
        
        // 初始点（使用Stream）
        const FVector2D InitialPoint(Stream->FRandRange(0.0f, Width), Stream->FRandRange(0.0f, Height));
        ActivePoints.Add(InitialPoint);
        Points2D.Add(InitialPoint);
        
        const int32 InitCellX = FMath::FloorToInt(InitialPoint.X / CellSize);
        const int32 InitCellY = FMath::FloorToInt(InitialPoint.Y / CellSize);
        Grid[InitCellY * GridWidth + InitCellX] = InitialPoint;
        
        // 主循环
        while (ActivePoints.Num() > 0)
        {
            const int32 Index = Stream->RandRange(0, ActivePoints.Num() - 1);
            const FVector2D Point = ActivePoints[Index];
            bool bFound = false;
            
            for (int32 i = 0; i < MaxAttempts; ++i)
            {
                const FVector2D NewPoint = GenerateRandomPointAround2D(Point, ActualRadius, 2.0f * ActualRadius, Stream);
                
                if (IsValidPoint2D(NewPoint, ActualRadius, Width, Height, Grid, GridWidth, GridHeight, CellSize))
                {
                    ActivePoints.Add(NewPoint);
                    Points2D.Add(NewPoint);
                    
                    const int32 CellX = FMath::FloorToInt(NewPoint.X / CellSize);
                    const int32 CellY = FMath::FloorToInt(NewPoint.Y / CellSize);
                    Grid[CellY * GridWidth + CellX] = NewPoint;
                    
                    bFound = true;
                    break;
                }
            }
            
            if (!bFound)
            {
                ActivePoints.RemoveAt(Index);
            }
        }
        
        // 转换为3D
        Points.Reserve(Points2D.Num());
        for (const FVector2D& Point2D : Points2D)
        {
            Points.Add(FVector(Point2D.X - BoxExtent.X, Point2D.Y - BoxExtent.Y, 0.0f));
        }
    }
    else
    {
        // 3D采样
        TArray<FVector> ActivePoints;
        
        const float CellSize = ActualRadius / FMath::Sqrt(3.0f);
        const int32 GridWidth = FMath::CeilToInt(Width / CellSize);
        const int32 GridHeight = FMath::CeilToInt(Height / CellSize);
        const int32 GridDepth = FMath::CeilToInt(Depth / CellSize);
        
        TArray<FVector> Grid;
        Grid.SetNumZeroed(GridWidth * GridHeight * GridDepth);
        
        // 初始点
        const FVector InitialPoint(
            Stream->FRandRange(0.0f, Width),
            Stream->FRandRange(0.0f, Height),
            Stream->FRandRange(0.0f, Depth)
        );
        ActivePoints.Add(InitialPoint);
        Points.Add(InitialPoint);
        
        const int32 InitCellX = FMath::FloorToInt(InitialPoint.X / CellSize);
        const int32 InitCellY = FMath::FloorToInt(InitialPoint.Y / CellSize);
        const int32 InitCellZ = FMath::FloorToInt(InitialPoint.Z / CellSize);
        Grid[InitCellZ * GridWidth * GridHeight + InitCellY * GridWidth + InitCellX] = InitialPoint;
        
        // 主循环
        while (ActivePoints.Num() > 0)
        {
            const int32 Index = Stream->RandRange(0, ActivePoints.Num() - 1);
            const FVector Point = ActivePoints[Index];
            bool bFound = false;
            
            for (int32 i = 0; i < MaxAttempts; ++i)
            {
                const FVector NewPoint = GenerateRandomPointAround3D(Point, ActualRadius, 2.0f * ActualRadius, Stream);
                
                if (IsValidPoint3D(NewPoint, ActualRadius, Width, Height, Depth, Grid, GridWidth, GridHeight, GridDepth, CellSize))
                {
                    ActivePoints.Add(NewPoint);
                    Points.Add(NewPoint);
                    
                    const int32 CellX = FMath::FloorToInt(NewPoint.X / CellSize);
                    const int32 CellY = FMath::FloorToInt(NewPoint.Y / CellSize);
                    const int32 CellZ = FMath::FloorToInt(NewPoint.Z / CellSize);
                    Grid[CellZ * GridWidth * GridHeight + CellY * GridWidth + CellX] = NewPoint;
                    
                    bFound = true;
                    break;
                }
            }
            
            if (!bFound)
            {
                ActivePoints.RemoveAt(Index);
            }
        }
        
        // 转换坐标
        for (FVector& Point : Points)
        {
            Point -= BoxExtent;
        }
    }

    // 裁剪到目标数量（使用Stream）
    TrimToTargetCount(Points, TargetPointCount, Stream);

    // 应用扰动噪波（使用Stream）
    ApplyJitter(Points, ActualRadius, JitterStrength, Stream);

    // 应用变换
    ApplyTransform(Points, Transform, bWorldSpace);
    
    UE_LOG(LogXTools, Log, TEXT("GeneratePoissonPointsInBoxByVectorFromStream: 生成了 %d 个点 (确定性随机, Radius=%.2f)"), 
        Points.Num(), ActualRadius);

    return Points;
}

