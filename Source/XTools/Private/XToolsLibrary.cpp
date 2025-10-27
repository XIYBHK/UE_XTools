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
        FVector Position;            // 世界空间位置（仅World空间使用）
        FQuat Rotation;
        float Radius;
        int32 TargetPointCount;
        float JitterStrength;
        bool bIs2D;
        EPoissonCoordinateSpace CoordinateSpace;  // 替换 bWorldSpace
        
        bool operator==(const FPoissonCacheKey& Other) const
        {
            // 基础参数比较
            bool bBasicMatch = BoxExtent.Equals(Other.BoxExtent, 0.1f) &&
                   Rotation.Equals(Other.Rotation, 0.001f) &&
                   FMath::IsNearlyEqual(Radius, Other.Radius, 0.1f) &&
                   TargetPointCount == Other.TargetPointCount &&
                   FMath::IsNearlyEqual(JitterStrength, Other.JitterStrength, 0.01f) &&
                   bIs2D == Other.bIs2D &&
                   CoordinateSpace == Other.CoordinateSpace;
            
            // 仅World空间比较Position（Local/Raw空间输出相对坐标，与Position无关）
            if (bBasicMatch && CoordinateSpace == EPoissonCoordinateSpace::World)
            {
                return Position.Equals(Other.Position, 0.1f);
            }
            
            return bBasicMatch;
        }
        
        friend uint32 GetTypeHash(const FPoissonCacheKey& Key)
        {
            // 基础Hash（所有空间共用）
            uint32 Hash = HashCombine(
                HashCombine(
                    HashCombine(
                        HashCombine(
                            GetTypeHash(Key.BoxExtent),
                            GetTypeHash(Key.Rotation)
                        ),
                        GetTypeHash(Key.Radius)
                    ),
                    GetTypeHash(Key.TargetPointCount)
                ),
                GetTypeHash(Key.JitterStrength)
            );
            
            // 仅World空间包含Position的Hash
            if (Key.CoordinateSpace == EPoissonCoordinateSpace::World)
            {
                Hash = HashCombine(Hash, GetTypeHash(Key.Position));
            }
            
            // 添加bIs2D和CoordinateSpace
            Hash = HashCombine(Hash, 
                HashCombine(GetTypeHash(Key.bIs2D), static_cast<uint32>(Key.CoordinateSpace))
            );
            
            return Hash;
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
            // ✅ LRU策略：更新访问时间
            AccessTimes.Add(Key, FPlatformTime::Seconds());
            
            CacheHits++;
            return *Found;
        }
        CacheMisses++;
        return {};
    }
    
    void Store(const FPoissonCacheKey& Key, const TArray<FVector>& Points)
    {
        FScopeLock Lock(&CacheLock);
        
        // ✅ LRU淘汰策略：缓存满时移除最久未使用的条目
        if (Cache.Num() >= MaxCacheSize)
        {
            RemoveLRUEntry();
        }
        
        Cache.Add(Key, Points);
        AccessTimes.Add(Key, FPlatformTime::Seconds());
    }
    
    void ClearCache()
    {
        FScopeLock Lock(&CacheLock);
        Cache.Empty();
        AccessTimes.Empty();
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
    // ✅ 移除最久未使用的条目（LRU淘汰）
    void RemoveLRUEntry()
    {
        if (Cache.Num() == 0) return;
        
        // 找到最早访问的条目
        FPoissonCacheKey OldestKey;
        double OldestTime = TNumericLimits<double>::Max();
        
        for (const auto& Pair : AccessTimes)
        {
            if (Pair.Value < OldestTime)
            {
                OldestTime = Pair.Value;
                OldestKey = Pair.Key;
            }
        }
        
        // 删除最旧的条目
        Cache.Remove(OldestKey);
        AccessTimes.Remove(OldestKey);
        
        UE_LOG(LogXTools, Verbose, TEXT("泊松缓存: LRU淘汰一个条目，剩余 %d 个"), Cache.Num());
    }
    
    static constexpr int32 MaxCacheSize = 50;
    
    FCriticalSection CacheLock;
    TMap<FPoissonCacheKey, TArray<FVector>> Cache;
    TMap<FPoissonCacheKey, double> AccessTimes;  // ✅ 记录每个条目的最后访问时间
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
            // 泊松采样实际密度约为理论值的2-3倍
            // 使用1.8倍系数，使生成点数接近目标（略多10-20%，便于裁剪选优）
            return FMath::Sqrt(1.8f * Area / (TargetPointCount * PI));
        }
        else
        {
            // 3D情况：根据体积估算Radius
            const float Volume = Width * Height * Depth;
            // 泊松采样实际密度约为理论值的2-3倍
            // 使用1.0倍系数，使生成点数接近目标
            return FMath::Pow(1.0f * Volume / (TargetPointCount * PI), 1.0f / 3.0f);
        }
    }
    
    /**
     * 找到点的最近邻距离（优化版：使用平方距离）
     */
    static float FindNearestDistanceSquared(const FVector& Point, const TArray<FVector>& Points, int32 ExcludeIndex = -1)
    {
        float MinDistSq = FLT_MAX;
        
        for (int32 i = 0; i < Points.Num(); ++i)
        {
            if (i == ExcludeIndex) continue;
            
            const float DistSq = FVector::DistSquared(Point, Points[i]);
            if (DistSq < MinDistSq)
            {
                MinDistSq = DistSq;
            }
        }
        
        return MinDistSq;
    }
    
    /**
     * 智能裁剪：移除最拥挤的点，保持最优分布
     * @param Points 要裁剪的点数组
     * @param TargetCount 目标数量
     */
    static void TrimToOptimalDistribution(TArray<FVector>& Points, int32 TargetCount)
    {
        while (Points.Num() > TargetCount)
        {
            // 找出最拥挤的点（与最近邻距离最小）
            int32 MostCrowdedIndex = 0;
            float MinNearestDistSq = FLT_MAX;
            
            for (int32 i = 0; i < Points.Num(); ++i)
            {
                const float NearestDistSq = FindNearestDistanceSquared(Points[i], Points, i);
                if (NearestDistSq < MinNearestDistSq)
                {
                    MinNearestDistSq = NearestDistSq;
                    MostCrowdedIndex = i;
                }
            }
            
            // 移除最拥挤的点
            Points.RemoveAtSwap(MostCrowdedIndex);
        }
    }
    
    /**
     * 分层网格填充：用均匀分层采样补充不足的点
     * @param Points 已有的点数组（会原地追加）
     * @param TargetCount 目标总数量
     * @param BoxSize 采样空间大小（完整尺寸，非半尺寸）
     * @param MinDist 最小距离约束（放宽的半径）
     * @param bIs2D 是否为2D平面
     * @param Stream 可选的随机流（const指针）
     * ✅ const指针：FRandomStream的方法是const但使用mutable成员
     */
    static void FillWithStratifiedSampling(
        TArray<FVector>& Points,
        int32 TargetCount,
        FVector BoxSize,
        float MinDist,
        bool bIs2D,
        const FRandomStream* Stream = nullptr)
    {
        const int32 Needed = TargetCount - Points.Num();
        if (Needed <= 0) return;
        
        // 计算网格大小
        int32 GridSize;
        if (bIs2D)
        {
            GridSize = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Needed)));
        }
        else
        {
            GridSize = FMath::CeilToInt(FMath::Pow(static_cast<float>(Needed), 1.0f / 3.0f));
        }
        
        // 计算单元格大小
        const FVector CellSize = BoxSize / FMath::Max(GridSize, 1);
        const float MinDistSq = MinDist * MinDist;
        
        TArray<FVector> CandidatePoints;
        CandidatePoints.Reserve(Needed * 2);  // 预留额外空间
        
        // 在网格中生成候选点
        for (int32 i = 0; i < Needed * 2 && CandidatePoints.Num() < Needed * 2; ++i)
        {
            // 计算网格索引
            const int32 x = i % GridSize;
            const int32 y = (i / GridSize) % GridSize;
            const int32 z = bIs2D ? 0 : (i / (GridSize * GridSize)) % GridSize;
            
            // 在网格单元内随机位置
            FVector CellMin = FVector(x, y, z) * CellSize;
            
            float RandX, RandY, RandZ;
            if (Stream)
            {
                RandX = Stream->FRand();
                RandY = Stream->FRand();
                RandZ = bIs2D ? 0.0f : Stream->FRand();
            }
            else
            {
                RandX = FMath::FRand();
                RandY = FMath::FRand();
                RandZ = bIs2D ? 0.0f : FMath::FRand();
            }
            
            FVector NewPoint = CellMin + FVector(
                RandX * CellSize.X,
                RandY * CellSize.Y,
                RandZ * CellSize.Z
            );
            
            // 转换到局部空间（中心对齐）
            NewPoint -= BoxSize * 0.5f;
            if (bIs2D) NewPoint.Z = 0.0f;
            
            // 检查与已有泊松点的距离
            bool bValid = true;
            for (const FVector& ExistingPoint : Points)
            {
                if (FVector::DistSquared(NewPoint, ExistingPoint) < MinDistSq)
                {
                    bValid = false;
                    break;
                }
            }
            
            // 检查与已生成候选点的距离（避免候选点之间重叠）
            if (bValid)
            {
                for (const FVector& CandidatePoint : CandidatePoints)
                {
                    if (FVector::DistSquared(NewPoint, CandidatePoint) < MinDistSq)
                    {
                        bValid = false;
                        break;
                    }
                }
            }
            
            if (bValid)
            {
                CandidatePoints.Add(NewPoint);
            }
        }
        
        // 如果候选点不够，降低距离约束继续生成
        if (CandidatePoints.Num() < Needed)
        {
            const float RelaxedMinDistSq = MinDistSq * 0.5f;  // 进一步放宽到50%
            
            for (int32 i = 0; i < Needed * 2 && CandidatePoints.Num() < Needed; ++i)
            {
                FVector RandomPoint;
                if (Stream)
                {
                    RandomPoint = FVector(
                        Stream->FRandRange(-BoxSize.X * 0.5f, BoxSize.X * 0.5f),
                        Stream->FRandRange(-BoxSize.Y * 0.5f, BoxSize.Y * 0.5f),
                        bIs2D ? 0.0f : Stream->FRandRange(-BoxSize.Z * 0.5f, BoxSize.Z * 0.5f)
                    );
                }
                else
                {
                    RandomPoint = FVector(
                        FMath::FRandRange(-BoxSize.X * 0.5f, BoxSize.X * 0.5f),
                        FMath::FRandRange(-BoxSize.Y * 0.5f, BoxSize.Y * 0.5f),
                        bIs2D ? 0.0f : FMath::FRandRange(-BoxSize.Z * 0.5f, BoxSize.Z * 0.5f)
                    );
                }
                
                bool bValid = true;
                for (const FVector& ExistingPoint : Points)
                {
                    if (FVector::DistSquared(RandomPoint, ExistingPoint) < RelaxedMinDistSq)
                    {
                        bValid = false;
                        break;
                    }
                }
                
                // 检查与已生成候选点的距离
                if (bValid)
                {
                    for (const FVector& CandidatePoint : CandidatePoints)
                    {
                        if (FVector::DistSquared(RandomPoint, CandidatePoint) < RelaxedMinDistSq)
                        {
                            bValid = false;
                            break;
                        }
                    }
                }
                
                if (bValid)
                {
                    CandidatePoints.Add(RandomPoint);
                }
            }
        }
        
        // 如果还不够，继续填充（保留极小距离约束，避免完全重叠）
        if (CandidatePoints.Num() < Needed)
        {
            const float MinimalDistSq = MinDistSq * 0.25f;  // 25%的约束
            const int32 MaxAttempts = Needed * 10;  // 限制尝试次数，避免死循环
            int32 Attempts = 0;
            
            while (CandidatePoints.Num() < Needed && Attempts < MaxAttempts)
            {
                Attempts++;
                
                FVector RandomPoint;
                if (Stream)
                {
                    RandomPoint = FVector(
                        Stream->FRandRange(-BoxSize.X * 0.5f, BoxSize.X * 0.5f),
                        Stream->FRandRange(-BoxSize.Y * 0.5f, BoxSize.Y * 0.5f),
                        bIs2D ? 0.0f : Stream->FRandRange(-BoxSize.Z * 0.5f, BoxSize.Z * 0.5f)
                    );
                }
                else
                {
                    RandomPoint = FVector(
                        FMath::FRandRange(-BoxSize.X * 0.5f, BoxSize.X * 0.5f),
                        FMath::FRandRange(-BoxSize.Y * 0.5f, BoxSize.Y * 0.5f),
                        bIs2D ? 0.0f : FMath::FRandRange(-BoxSize.Z * 0.5f, BoxSize.Z * 0.5f)
                    );
                }
                
                // 检查与泊松点的极小距离（避免完全重叠）
                bool bValid = true;
                for (const FVector& ExistingPoint : Points)
                {
                    if (FVector::DistSquared(RandomPoint, ExistingPoint) < MinimalDistSq)
                    {
                        bValid = false;
                        break;
                    }
                }
                
                // 检查与候选点的极小距离
                if (bValid)
                {
                    for (const FVector& CandidatePoint : CandidatePoints)
                    {
                        if (FVector::DistSquared(RandomPoint, CandidatePoint) < MinimalDistSq)
                        {
                            bValid = false;
                            break;
                        }
                    }
                }
                
                if (bValid)
                {
                    CandidatePoints.Add(RandomPoint);
                }
            }
            
            // 如果尝试次数耗尽仍不够，记录警告
            if (CandidatePoints.Num() < Needed)
            {
                UE_LOG(LogXTools, Warning, 
                    TEXT("泊松采样: 空间过小，无法在保持最小距离的前提下生成 %d 个点，实际补充 %d 个（已有泊松点 %d 个）"),
                    Needed, CandidatePoints.Num(), Points.Num());
            }
        }
        
        // 随机选择需要的点数追加到原数组
        if (Stream)
        {
            // 使用流进行Fisher-Yates洗牌
            for (int32 i = CandidatePoints.Num() - 1; i > 0; --i)
            {
                const int32 j = Stream->RandRange(0, i);
                CandidatePoints.Swap(i, j);
            }
        }
        else
        {
            // 使用FMath进行Fisher-Yates洗牌
            for (int32 i = CandidatePoints.Num() - 1; i > 0; --i)
            {
                const int32 j = FMath::RandRange(0, i);
                CandidatePoints.Swap(i, j);
            }
        }
        
        // 追加到原数组
        for (int32 i = 0; i < Needed && i < CandidatePoints.Num(); ++i)
        {
            Points.Add(CandidatePoints[i]);
        }
    }
    
    /**
     * 智能调整点数到目标数量（混合策略）
     * @param Points 点数组
     * @param TargetCount 目标数量
     * @param BoxSize 采样空间大小（完整尺寸）
     * @param Radius 参考半径
     * @param bIs2D 是否为2D平面
     * @param Stream 可选的随机流（const指针）
     * ✅ const指针：FRandomStream的方法是const但使用mutable成员
     */
    static void AdjustToTargetCount(
        TArray<FVector>& Points,
        int32 TargetCount,
        FVector BoxSize,
        float Radius,
        bool bIs2D,
        const FRandomStream* Stream = nullptr)
    {
        if (TargetCount <= 0) return;
        
        const int32 CurrentCount = Points.Num();
        
        if (CurrentCount == TargetCount)
        {
            // 完美匹配，无需调整
            return;
        }
        else if (CurrentCount > TargetCount)
        {
            // 点太多：智能裁剪（保留最分散的点）
            UE_LOG(LogXTools, Log, TEXT("泊松采样: 从 %d 个点智能裁剪到 %d（移除拥挤点）"), 
                CurrentCount, TargetCount);
            
            TrimToOptimalDistribution(Points, TargetCount);
        }
        else
        {
            // 点不足：分层网格补充
            const float RelaxedRadius = Radius * 0.6f;  // 放宽到60%
            
            UE_LOG(LogXTools, Log, TEXT("泊松采样: 从 %d 个点补充到 %d（分层网格填充，距离约束=%.1f）"), 
                CurrentCount, TargetCount, RelaxedRadius);
            
            FillWithStratifiedSampling(Points, TargetCount, BoxSize, RelaxedRadius, bIs2D, Stream);
        }
    }
    
    /**
     * 应用扰动噪波到点集
     * @param Points 要扰动的点数组
     * @param Radius 参考半径（扰动范围基于此值）
     * @param JitterStrength 扰动强度 0-1（0=无扰动，1=最大扰动）
     * @param Stream 可选的随机流（const指针）
     * ✅ const指针：FRandomStream的方法是const但使用mutable成员
     */
    static void ApplyJitter(TArray<FVector>& Points, float Radius, float JitterStrength, const FRandomStream* Stream = nullptr)
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
     * 应用Transform变换
     * @param CoordinateSpace 坐标空间类型：
     *        - World：世界坐标（应用位置+旋转到世界空间）
     *        - Local：局部坐标（仅应用旋转，位置相对于Box中心）
     *        - Raw：原始坐标（完全不变换，供用户自行处理）
     * @param ScaleCompensation 缩放补偿（保留接口兼容性，未使用）
     */
    static void ApplyTransform(TArray<FVector>& Points, const FTransform& Transform, EPoissonCoordinateSpace CoordinateSpace, const FVector& ScaleCompensation = FVector::OneVector)
    {
        switch (CoordinateSpace)
        {
            case EPoissonCoordinateSpace::World:
            {
                // 世界空间：应用位置+旋转（不应用缩放，因为Extent已包含）
                FTransform TransformNoScale = Transform;
                TransformNoScale.SetScale3D(FVector::OneVector);
                
                for (FVector& Point : Points)
                {
                    Point = TransformNoScale.TransformPosition(Point);
                }
                break;
            }
            
            case EPoissonCoordinateSpace::Local:
            {
                // 局部空间：应用缩放补偿，输出相对于Box中心的点
                // 适用场景：AddInstance(World Space = false)
                // - 使用ScaledBoxExtent采样（保证点数随缩放增加）
                // - 输出时除以父缩放（避免AddInstance的双重缩放）
                // - 旋转由HISMC的父变换自动处理
                const FVector ParentScale = Transform.GetScale3D();
                
                for (FVector& Point : Points)
                {
                    // 除以父缩放，补偿AddInstance会再次应用的缩放
                    Point.X /= ParentScale.X;
                    Point.Y /= ParentScale.Y;
                    Point.Z /= ParentScale.Z;
                }
                break;
            }
            
            case EPoissonCoordinateSpace::Raw:
            {
                // 原始空间：应用缩放补偿（与Local相同的逻辑）
                // 点相对于Box中心，未旋转，已补偿缩放
                // 供用户自行处理坐标变换
                const FVector ParentScale = Transform.GetScale3D();
                
                for (FVector& Point : Points)
                {
                    Point.X /= ParentScale.X;
                    Point.Y /= ParentScale.Y;
                    Point.Z /= ParentScale.Z;
                }
                break;
            }
            
            default:
            {
                checkNoEntry();  // 不应到达此处
                break;
            }
        }
    }

    /**
     * 在给定点周围生成随机点（2D）
     * @param Stream 可选的随机流，nullptr时使用全局随机
     * ✅ const指针：FRandomStream的方法是const但使用mutable成员
     */
    static FVector2D GenerateRandomPointAround2D(const FVector2D& Point, float MinDist, float MaxDist, const FRandomStream* Stream = nullptr)
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
     * ✅ const指针：FRandomStream的方法是const但使用mutable成员
     */
    static FVector GenerateRandomPointAround3D(const FVector& Point, float MinDist, float MaxDist, const FRandomStream* Stream = nullptr)
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
    EPoissonCoordinateSpace CoordinateSpace,
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

    // 采样区域：统一使用ScaledExtent，确保采样范围随Box视觉大小变化
    // - 当父Actor缩放时，采样范围变大，点数增加，保持视觉密度不变
    // - Local/Raw空间会在ApplyTransform中对坐标除以父缩放，补偿AddInstance的缩放
    const FTransform BoxTransform = BoundingBox->GetComponentTransform();
    const FVector BoxExtent = BoundingBox->GetScaledBoxExtent();
    
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

    // 缓存系统检查（根据坐标空间模式使用对应的变换）
    if (bUseCache)
    {
        FPoissonCacheKey CacheKey;
        CacheKey.BoxExtent = BoxExtent;
        CacheKey.Position = BoxTransform.GetLocation();   // Position仅在World空间参与缓存比较
        CacheKey.Rotation = BoxTransform.GetRotation();
        CacheKey.Radius = ActualRadius;
        CacheKey.TargetPointCount = TargetPointCount;
        CacheKey.JitterStrength = JitterStrength;
        CacheKey.bIs2D = bIs2DPlane;
        CacheKey.CoordinateSpace = CoordinateSpace;
        
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

    // 应用扰动噪波（在调整点数之前）
    PoissonSamplingHelpers::ApplyJitter(Points, ActualRadius, JitterStrength);

    // 如果指定了目标点数，智能调整到精确数量（泊松主体 + 分层网格补充）
    if (TargetPointCount > 0)
    {
        const FVector BoxSize(Width, Height, Depth);
        PoissonSamplingHelpers::AdjustToTargetCount(Points, TargetPointCount, BoxSize, ActualRadius, bIs2DPlane);
    }

    // 应用变换（根据坐标空间类型）
    PoissonSamplingHelpers::ApplyTransform(Points, BoxTransform, CoordinateSpace);

    // 存入缓存（根据坐标空间模式使用对应的变换）
    if (bUseCache)
    {
        FPoissonCacheKey CacheKey;
        CacheKey.BoxExtent = BoxExtent;
        CacheKey.Position = BoxTransform.GetLocation();   // Position仅在World空间参与缓存比较
        CacheKey.Rotation = BoxTransform.GetRotation();
        CacheKey.Radius = ActualRadius;
        CacheKey.TargetPointCount = TargetPointCount;
        CacheKey.JitterStrength = JitterStrength;
        CacheKey.bIs2D = bIs2DPlane;
        CacheKey.CoordinateSpace = CoordinateSpace;
        
        FPoissonResultCache::Get().Store(CacheKey, Points);
    }

    return Points;
}

TArray<FVector> UXToolsLibrary::GeneratePoissonPointsInBoxByVector(
    FVector BoxExtent,
    FTransform Transform,
    float Radius,
    int32 MaxAttempts,
    EPoissonCoordinateSpace CoordinateSpace,
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

    // 缓存系统检查（包含位置和旋转信息，使用传入的BoxExtent）
    if (bUseCache)
    {
        FPoissonCacheKey CacheKey;
        CacheKey.BoxExtent = BoxExtent;
        CacheKey.Position = Transform.GetLocation();  // Position仅在World空间参与缓存比较
        CacheKey.Rotation = Transform.GetRotation();
        CacheKey.Radius = ActualRadius;
        CacheKey.TargetPointCount = TargetPointCount;
        CacheKey.JitterStrength = JitterStrength;
        CacheKey.bIs2D = bIs2DPlane;
        CacheKey.CoordinateSpace = CoordinateSpace;
        
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
        
        UE_LOG(LogXTools, Log, TEXT("泊松采样: 2D平面 | BoxExtent=(%.1f,%.1f) | 采样空间=[0,%.1f]x[0,%.1f] | 结果范围=[±%.1f,±%.1f]"), 
            BoxExtent.X, BoxExtent.Y, Width, Height, BoxExtent.X, BoxExtent.Y);
    }
    else
    {
        // 3D体积采样
        Points = GeneratePoissonPoints3D(Width, Height, Depth, ActualRadius, MaxAttempts);
        
        UE_LOG(LogXTools, Log, TEXT("泊松采样: 3D体积 | BoxExtent=(%.1f,%.1f,%.1f) | 采样空间=[0,%.1f]x[0,%.1f]x[0,%.1f] | 结果范围=[±%.1f,±%.1f,±%.1f]"), 
            BoxExtent.X, BoxExtent.Y, BoxExtent.Z, Width, Height, Depth, BoxExtent.X, BoxExtent.Y, BoxExtent.Z);
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

    // 应用扰动噪波（在调整点数之前）
    PoissonSamplingHelpers::ApplyJitter(Points, ActualRadius, JitterStrength);

    // 如果指定了目标点数，智能调整到精确数量（泊松主体 + 分层网格补充）
    if (TargetPointCount > 0)
    {
        const FVector BoxSize(Width, Height, Depth);
        PoissonSamplingHelpers::AdjustToTargetCount(Points, TargetPointCount, BoxSize, ActualRadius, bIs2DPlane);
    }

    // 应用变换（根据坐标空间类型）
    PoissonSamplingHelpers::ApplyTransform(Points, Transform, CoordinateSpace);
    
    // 日志输出
    const TCHAR* SpaceTypeName = 
        (CoordinateSpace == EPoissonCoordinateSpace::World) ? TEXT("世界空间") :
        (CoordinateSpace == EPoissonCoordinateSpace::Local) ? TEXT("局部空间") : TEXT("原始空间");
    
    UE_LOG(LogXTools, Log, TEXT("泊松采样完成: %d个点 | Radius=%.2f | 坐标=%s"), 
        Points.Num(), ActualRadius, SpaceTypeName);

    // 存入缓存（包含位置和旋转信息）
    if (bUseCache)
    {
        FPoissonCacheKey CacheKey;
        CacheKey.BoxExtent = BoxExtent;
        CacheKey.Position = Transform.GetLocation();  // Position仅在World空间参与缓存比较
        CacheKey.Rotation = Transform.GetRotation();
        CacheKey.Radius = ActualRadius;
        CacheKey.TargetPointCount = TargetPointCount;
        CacheKey.JitterStrength = JitterStrength;
        CacheKey.bIs2D = bIs2DPlane;
        CacheKey.CoordinateSpace = CoordinateSpace;
        
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
    EPoissonCoordinateSpace CoordinateSpace,
    int32 TargetPointCount,
    float JitterStrength)
{
    if (!BoundingBox)
    {
        UE_LOG(LogXTools, Warning, TEXT("GeneratePoissonPointsInBoxFromStream: BoundingBox is nullptr"));
        return TArray<FVector>();
    }

    if (MaxAttempts <= 0)
    {
        UE_LOG(LogXTools, Warning, TEXT("GeneratePoissonPointsInBoxFromStream: MaxAttempts必须大于0"));
        return TArray<FVector>();
    }

    // 采样区域：统一使用ScaledExtent，确保采样范围随Box视觉大小变化
    // - 当父Actor缩放时，采样范围变大，点数增加，保持视觉密度不变
    // - Local/Raw空间会在ApplyTransform中对坐标除以父缩放，补偿AddInstance的缩放
    const FTransform BoxTransform = BoundingBox->GetComponentTransform();
    const FVector BoxExtent = BoundingBox->GetScaledBoxExtent();
    
    // 调用向量版本
    return GeneratePoissonPointsInBoxByVectorFromStream(
        RandomStream,
        BoxExtent,
        BoxTransform,
        Radius,
        MaxAttempts,
        CoordinateSpace,
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
    EPoissonCoordinateSpace CoordinateSpace,
    int32 TargetPointCount,
    float JitterStrength)
{
    using namespace PoissonSamplingHelpers;
    
    // 输入验证
    if (BoxExtent.X <= 0.0f || BoxExtent.Y <= 0.0f || BoxExtent.Z < 0.0f)
    {
        UE_LOG(LogXTools, Warning, TEXT("GeneratePoissonPointsInBoxByVectorFromStream: BoxExtent无效 (%s)"), *BoxExtent.ToString());
        return TArray<FVector>();
    }

    if (MaxAttempts <= 0)
    {
        UE_LOG(LogXTools, Warning, TEXT("GeneratePoissonPointsInBoxByVectorFromStream: MaxAttempts必须大于0"));
        return TArray<FVector>();
    }

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

    // 验证最终的Radius
    if (ActualRadius <= 0.0f)
    {
        UE_LOG(LogXTools, Warning, TEXT("GeneratePoissonPointsInBoxByVectorFromStream: 计算出的Radius无效 (%.2f)，请指定有效的Radius或TargetPointCount"), ActualRadius);
        return TArray<FVector>();
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
        
        // 初始点（使用RandomStream）
        const FVector2D InitialPoint(RandomStream.FRandRange(0.0f, Width), RandomStream.FRandRange(0.0f, Height));
        ActivePoints.Add(InitialPoint);
        Points2D.Add(InitialPoint);
        
        const int32 InitCellX = FMath::FloorToInt(InitialPoint.X / CellSize);
        const int32 InitCellY = FMath::FloorToInt(InitialPoint.Y / CellSize);
        Grid[InitCellY * GridWidth + InitCellX] = InitialPoint;
        
        // 主循环
        while (ActivePoints.Num() > 0)
        {
            const int32 Index = RandomStream.RandRange(0, ActivePoints.Num() - 1);
            const FVector2D Point = ActivePoints[Index];
            bool bFound = false;
            
            for (int32 i = 0; i < MaxAttempts; ++i)
            {
                // ✅ 传递const指针
                const FVector2D NewPoint = GenerateRandomPointAround2D(Point, ActualRadius, 2.0f * ActualRadius, &RandomStream);
                
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
        
        // 初始点（使用RandomStream）
        const FVector InitialPoint(
            RandomStream.FRandRange(0.0f, Width),
            RandomStream.FRandRange(0.0f, Height),
            RandomStream.FRandRange(0.0f, Depth)
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
            const int32 Index = RandomStream.RandRange(0, ActivePoints.Num() - 1);
            const FVector Point = ActivePoints[Index];
            bool bFound = false;
            
            for (int32 i = 0; i < MaxAttempts; ++i)
            {
                // ✅ 传递const指针
                const FVector NewPoint = GenerateRandomPointAround3D(Point, ActualRadius, 2.0f * ActualRadius, &RandomStream);
                
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

    // 应用扰动噪波（使用RandomStream，在调整点数之前）
    ApplyJitter(Points, ActualRadius, JitterStrength, &RandomStream);

    // 如果指定了目标点数，智能调整到精确数量（泊松主体 + 分层网格补充）
    if (TargetPointCount > 0)
    {
        const FVector BoxSize(Width, Height, Depth);
        AdjustToTargetCount(Points, TargetPointCount, BoxSize, ActualRadius, bIs2DPlane, &RandomStream);
    }

    // 应用变换（根据坐标空间类型）
    ApplyTransform(Points, Transform, CoordinateSpace);

    return Points;
}

