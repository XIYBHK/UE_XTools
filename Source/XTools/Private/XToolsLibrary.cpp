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

// ✅ 平台安全的内存统计工具
class FPlatformSafeMemoryStats
{
public:
    static SIZE_T GetSafeMemoryUsage()
    {
        // Win64 平台专用优化
        #if PLATFORM_WINDOWS && PLATFORM_64BITS
            try
            {
                const FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
                return Stats.UsedPhysical;
            }
            catch (...)
            {
                UE_LOG(LogXTools, Warning, TEXT("无法获取内存统计信息，使用默认值"));
                return 0;
            }
        #else
            // 其他平台返回默认值
            UE_LOG(LogXTools, Warning, TEXT("当前平台不支持内存统计"));
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

    ResultBuilder.Append(TEXT("✅ 点阵生成缓存清理完成\n"));
    ResultBuilder.Append(TEXT("- '在模型中生成点阵'功能缓存已清空\n"));
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


