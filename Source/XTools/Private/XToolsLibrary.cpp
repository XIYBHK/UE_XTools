// Fill out your copyright notice in the Description page of Project Settings.

#include "XToolsLibrary.h"
#include "XToolsPrivatePCH.h"

/**
 * 按类和标签查找父Actor
 * 
 * 该函数会沿着组件的父子层级向上查找，直到找到匹配指定类和标签的父Actor。
 * 如果同时指定了ActorClass和ActorTag，会优先返回同时匹配的父级；
 * 如果只匹配ActorClass，会返回最高级匹配的父级；
 * 如果只匹配ActorTag，会返回第一个匹配的父级。
 * 
 * @param Component 要开始查找的起始组件，通常是一个场景组件（SceneComponent）
 * @param ActorClass 要查找的Actor类，必须是AActor的子类
 * @param ActorTag 要匹配的父Actor标签，为空时只按类查找
 * @return 找到的父Actor，如果未找到则返回nullptr
 * 
 * @note 查找顺序：
 * 1. 如果同时指定了ActorClass和ActorTag，优先返回同时匹配的父级
 * 2. 如果只指定了ActorClass，返回最高级匹配的父级
 * 3. 如果只指定了ActorTag，返回第一个匹配的父级
 * 
 * @example 
 * // 查找标签为"MainCharacter"的父级Character
 * ACharacter* ParentCharacter = Cast<ACharacter>(
 *     UXToolsLibrary::FindParentComponentByClass(
 *         MyComponent, 
 *         ACharacter::StaticClass(), 
 *         "MainCharacter"));
 */
AActor* UXToolsLibrary::FindParentComponentByClass(UActorComponent* Component, TSubclassOf<AActor> ActorClass, const FString& ActorTag)
{
    if (!Component)
    {
        return nullptr;
    }

    // 确保ActorClass是有效的Actor类
    if (ActorClass && !ActorClass->IsChildOf(AActor::StaticClass()))
    {
        return nullptr;
    }

    USceneComponent* SceneComp = Cast<USceneComponent>(Component);
    if (!SceneComp)
    {
        return nullptr;
    }

    USceneComponent* ParentComp = SceneComp->GetAttachParent();
    AActor* HighestParent = nullptr;
    int32 MaxIterations = 100; // 防止无限循环
    int32 IterationCount = 0;
    FName TagName = FName(*ActorTag);
    
    // 遍历所有父组件
    while (ParentComp && IterationCount < MaxIterations)
    {
        IterationCount++;
        
        AActor* ParentActor = ParentComp->GetOwner();
        
        // 如果指定了ActorClass，检查是否匹配
        if (ActorClass && (!ParentActor || !ParentActor->IsA(ActorClass)))
        {
            ParentComp = ParentComp->GetAttachParent();
            continue;
        }

        UE_LOG(LogTemp, Log, TEXT("Checking parent actor: %s"), *ParentActor->GetName());

        // 如果指定了ActorClass，检查是否匹配
        if (ActorClass && ParentActor->IsA(ActorClass))
        {
            // 记录当前匹配的父级
            HighestParent = ParentActor;
            UE_LOG(LogTemp, Log, TEXT("Recording parent with matching class: %s"), *ParentActor->GetName());

            // 如果同时指定了ActorTag且匹配，立即返回
            if (!ActorTag.IsEmpty() && ParentActor->Tags.Contains(TagName))
            {
                UE_LOG(LogTemp, Log, TEXT("Found parent with matching class and tag: %s"), *ActorTag);
                return ParentActor;
            }
            
            // 即使Tag不匹配，也继续查找更高层级的父级
            // 最终会返回最高级匹配的父级
        }
        // 如果只指定了ActorTag，检查是否匹配
        else if (!ActorTag.IsEmpty() && ParentActor->Tags.Contains(TagName))
        {
            UE_LOG(LogTemp, Log, TEXT("Found parent with matching tag: %s"), *ActorTag);
            return ParentActor; // 找到匹配Tag的父级，立即返回
        }
        
        ParentComp = ParentComp->GetAttachParent();
    }

    if (IterationCount >= MaxIterations)
    {
        UE_LOG(LogTemp, Error, TEXT("Reached maximum iteration count while searching for parent actor"));
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
