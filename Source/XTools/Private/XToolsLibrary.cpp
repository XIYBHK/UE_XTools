// Fill out your copyright notice in the Description page of Project Settings.

#include "XToolsLibrary.h"
#include "DrawDebugHelpers.h"

FVector UXToolsLibrary::CalculateBezierPoint(const TArray<FVector>& Points, float Progress, bool bShowDebug, float Duration, FBezierDebugColors DebugColors, FBezierSpeedOptions SpeedOptions)
{
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
    if (bShowDebug && GWorld)
    {
        // 绘制控制点
        for (const FVector& Point : Points)
        {
            DrawDebugSphere(GWorld, Point, 8.0f, 8, DebugColors.ControlPointColor.ToFColor(true), false, Duration);
        }

        // 绘制控制点之间的连线
        for (int32 i = 0; i < Points.Num() - 1; ++i)
        {
            DrawDebugLine(GWorld, Points[i], Points[i + 1], DebugColors.ControlLineColor.ToFColor(true), false, Duration);
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
                DrawDebugPoint(GWorld, WorkPoints[CurrentIndex], 4.0f, 
                    DebugColors.IntermediatePointColor.ToFColor(true), false, Duration);
                // 绘制中间连线
                DrawDebugLine(GWorld, P1, P2, 
                    DebugColors.IntermediateLineColor.ToFColor(true), false, Duration);
                
                CurrentIndex++;
            }
        }

        // 绘制结果点（显示时间更长）
        const float ResultPointDuration = Duration * 5.0f;
        DrawDebugPoint(GWorld, ResultPoint, 20.0f, DebugColors.ResultPointColor.ToFColor(true), false, ResultPointDuration);
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