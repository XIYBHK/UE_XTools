// Fill out your copyright notice in the Description page of Project Settings.

#include "XToolsLibrary.h"
#include "XToolsPrivatePCH.h"
#include "RandomShuffleArrayLibrary.h"

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
AActor* UXToolsLibrary::FindParentComponentByClass(UActorComponent* Component, TSubclassOf<AActor> ActorClass, const FString& ActorTag)
{
    if (!Component)
    {
        UE_LOG(LogTemp, Warning, TEXT("提供的组件无效"));
        return nullptr;
    }

    // 确保ActorClass是有效的Actor类
    if (ActorClass && !ActorClass->IsChildOf(AActor::StaticClass()))
    {
        UE_LOG(LogTemp, Warning, TEXT("提供的ActorClass无效"));
        return nullptr;
    }

    USceneComponent* SceneComp = Cast<USceneComponent>(Component);
    if (!SceneComp)
    {
        UE_LOG(LogTemp, Warning, TEXT("组件不是SceneComponent类型"));
        return nullptr;
    }

    // 设置最大深度，默认100层
    const int32 MaxIterations = XTOOLS_MAX_PARENT_DEPTH;
    int32 IterationCount = 0;
    FName TagName = FName(*ActorTag);
    AActor* HighestParent = nullptr;
    USceneComponent* ParentComp = SceneComp->GetAttachParent();
    
    // 调试信息
        UE_LOG(LogTemp, Log, TEXT("开始从组件查找父级: %s"), *Component->GetName());
    
    // 遍历所有父组件
    while (ParentComp && IterationCount < MaxIterations)
    {
        IterationCount++;
        
        AActor* ParentActor = ParentComp->GetOwner();
        
        // 如果两者都为空，直接记录当前父级并继续向上查找
        if (!ActorClass && ActorTag.IsEmpty())
        {
            HighestParent = ParentActor;
            ParentComp = ParentComp->GetAttachParent();
            continue;
        }
        
        // 如果指定了ActorClass，检查是否匹配
        if (ActorClass && (!ParentActor || !ParentActor->IsA(ActorClass)))
        {
            ParentComp = ParentComp->GetAttachParent();
            continue;
        }

        UE_LOG(LogTemp, Log, TEXT("正在检查父级Actor: %s"), *ParentActor->GetName());

        // 如果指定了ActorClass，检查是否匹配
        if (ActorClass && ParentActor->IsA(ActorClass))
        {
            // 记录当前匹配的父级
            HighestParent = ParentActor;
            UE_LOG(LogTemp, Log, TEXT("记录匹配类的父级: %s"), *ParentActor->GetName());

            // 如果同时指定了ActorTag且匹配，立即返回
            if (!ActorTag.IsEmpty() && ParentActor->Tags.Contains(TagName))
            {
                UE_LOG(LogTemp, Log, TEXT("找到匹配类和标签的父级: %s"), *ActorTag);
                return ParentActor;
            }
            
            // 即使Tag不匹配，也继续查找更高层级的父级
            // 最终会返回最高级匹配的父级
        }
        // 如果只指定了ActorTag，检查是否匹配
        else if (!ActorTag.IsEmpty() && ParentActor->Tags.Contains(TagName))
        {
            UE_LOG(LogTemp, Log, TEXT("找到匹配标签的父级: %s"), *ActorTag);
            return ParentActor; // 找到匹配Tag的父级，立即返回
        }
        
        ParentComp = ParentComp->GetAttachParent();
    }

    if (IterationCount >= MaxIterations)
    {
        UE_LOG(LogTemp, Error, TEXT("查找父级Actor时达到最大迭代次数"));
    }
    
    // 返回最高级匹配的父级
    return HighestParent;
}

#include "DrawDebugHelpers.h"

FVector UXToolsLibrary::CalculateBezierPoint(const UObject* Context,const TArray<FVector>& Points, float Progress, bool bShowDebug, float Duration, FBezierDebugColors DebugColors, FBezierSpeedOptions SpeedOptions)
{
    UWorld* World = Context->GetWorld();
    check(World);
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
        // 匀速模式
        // 在匀速模式下应用速率曲线
        if (SpeedOptions.SpeedCurve)
        {
            Progress = SpeedOptions.SpeedCurve->GetFloatValue(Progress);
        }
        
        const float TotalLength = CalculateCurveLength(Points);
        const float TargetDistance = TotalLength * Progress;
        const float Parameter = GetParameterByDistance(Points, TargetDistance, TotalLength);
        ResultPoint = CalculatePointAtParameter(Points, Parameter, WorkPoints);
    }
    else
    {
        // 默认模式（直接使用参数t，不应用速率曲线）
        ResultPoint = CalculatePointAtParameter(Points, Progress, WorkPoints);
    }

    // 如果开启调试，绘制控制点和连线
    if (bShowDebug && World)
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

float UXToolsLibrary::CalculateCurveLength(const TArray<FVector>& Points, int32 Segments)
{
    float Length = 0.0f;
    TArray<FVector> WorkPoints;
    FVector PrevPoint = CalculatePointAtParameter(Points, 0.0f, WorkPoints);

    for (int32 i = 1; i <= Segments; ++i)
    {
        const float t = static_cast<float>(i) / Segments;
        const FVector CurrentPoint = CalculatePointAtParameter(Points, t, WorkPoints);
        Length += FVector::Distance(PrevPoint, CurrentPoint);
        PrevPoint = CurrentPoint;
    }

    return Length;
}

float UXToolsLibrary::GetParameterByDistance(const TArray<FVector>& Points, float TargetDistance, float TotalLength, int32 Segments)
{
    float AccumulatedLength = 0.0f;
    TArray<FVector> WorkPoints;
    FVector PrevPoint = CalculatePointAtParameter(Points, 0.0f, WorkPoints);

    for (int32 i = 1; i <= Segments; ++i)
    {
        const float t = static_cast<float>(i) / Segments;
        const FVector CurrentPoint = CalculatePointAtParameter(Points, t, WorkPoints);
        const float SegmentLength = FVector::Distance(PrevPoint, CurrentPoint);
        
        if (AccumulatedLength + SegmentLength >= TargetDistance)
        {
            const float ExcessLength = AccumulatedLength + SegmentLength - TargetDistance;
            const float SegmentProgress = 1.0f - (ExcessLength / SegmentLength);
            const float PrevT = static_cast<float>(i - 1) / Segments;
            return FMath::Lerp(PrevT, t, SegmentProgress);
        }

        AccumulatedLength += SegmentLength;
        PrevPoint = CurrentPoint;
    }

    return 1.0f;
}

TArray<int32> UXToolsLibrary::TestPRDDistribution(float BaseChance)
{
    // 初始化结果数组，大小为13（0-12）
    TArray<int32> Distribution;
    Distribution.Init(0, 13);

    // 记录当前失败次数
    int32 CurrentFailureCount = 1;
    float ActualChance = 0.f;
    int32 TotalSuccesses = 0;
    int32 TotalTests = 0;
    TArray<int32> FailureTests;
    FailureTests.Init(0, 13);

    // 持续测试直到获得10000次成功
    while (TotalSuccesses < 10000)
    {
        int32 NextFailureCount = 0;
        TotalTests++;
        
        // 调用PRD函数
        const bool bSuccess = URandomShuffleArrayLibrary::PseudoRandomBool(
            BaseChance, 
            NextFailureCount, 
            ActualChance, 
            CurrentFailureCount);

        // 记录当前失败次数下的结果
        if (CurrentFailureCount <= 12)
        {
            FailureTests[CurrentFailureCount]++;
            if (bSuccess)
            {
                Distribution[CurrentFailureCount]++;
                TotalSuccesses++;
            }
        }

        // 更新失败次数
        CurrentFailureCount = NextFailureCount;
    }

    // 输出统计结果到日志
    UE_LOG(LogTemp, Log, TEXT("PRD Distribution Test Results (BaseChance = %.2f):"), BaseChance);
    UE_LOG(LogTemp, Log, TEXT("Total Tests: %d"), TotalTests);
    UE_LOG(LogTemp, Log, TEXT("失败次数 | 成功次数 | 实际成功率 | 理论成功率 | 测试次数"));
    UE_LOG(LogTemp, Log, TEXT("----------------------------------------"));
    
    for (int32 i = 0; i <= 12; ++i)
    {
        float TestChance = 0.f;
        float TestActualChance = 0.f;
        int32 TestFailureCount = 0;
        
        // 获取当前失败次数的理论概率
        URandomShuffleArrayLibrary::PseudoRandomBool(BaseChance, TestFailureCount, TestActualChance, i);
        const float ExpectedChance = TestActualChance;
        
        // 计算实际成功率
        const float SuccessRate = FailureTests[i] > 0 ? static_cast<float>(Distribution[i]) / FailureTests[i] : 0.0f;
        
        UE_LOG(LogTemp, Log, TEXT("%d次 | %d | %.2f%% | %.2f%% | %d"), 
            i, 
            Distribution[i],
            SuccessRate * 100.0f,
            ExpectedChance * 100.0f,
            FailureTests[i]);
    }
    UE_LOG(LogTemp, Log, TEXT("Total Successes: %d"), TotalSuccesses);

    return Distribution;
}
