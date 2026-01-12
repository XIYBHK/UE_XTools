/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "SplineSamplingHelper.h"
#include "PointDeduplicationHelper.h"
#include "PointSamplingTypes.h"
#include "Algorithms/PoissonDiskSampling.h"
#include "Math/UnrealMathUtility.h"

TArray<FVector> FSplineSamplingHelper::GenerateAlongSpline(
	int32 PointCount,
	const TArray<FVector>& ControlPoints,
	bool bClosedSpline)
{
	TArray<FVector> Points;
	if (PointCount <= 0 || ControlPoints.Num() < 2)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 如果控制点数量不足，直接线性插值
	if (ControlPoints.Num() == 2)
	{
		for (int32 i = 0; i < PointCount; ++i)
		{
			float T = (PointCount > 1) ? (static_cast<float>(i) / (PointCount - 1)) : 0.5f;
			FVector Point = FMath::Lerp(ControlPoints[0], ControlPoints[1], T);
			Points.Add(Point);
		}
		return Points;
	}

	UE_LOG(LogPointSampling, Log, TEXT("[样条线采样] 开始等距采样: %d 个控制点, %d 个目标点, %s"),
		ControlPoints.Num(), PointCount, bClosedSpline ? TEXT("闭合") : TEXT("开放"));

	// 计算总弧长（用于等距采样）
	const float TotalArcLength = CalculateSplineArcLength(ControlPoints, bClosedSpline);

	if (TotalArcLength <= 0.0f)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[样条线采样] 样条线总长度为0"));
		return Points;
	}

	// 等距采样：按弧长均匀分布点
	const float ArcStep = TotalArcLength / (PointCount - 1);

	for (int32 i = 0; i < PointCount; ++i)
	{
		const float TargetArcLength = i * ArcStep;

		// 找到对应的参数T值
		float ParameterT = FindParameterByArcLength(ControlPoints, TargetArcLength, bClosedSpline, TotalArcLength);

		// 计算样条点
		FVector Point = EvaluateSplineAtParameter(ControlPoints, ParameterT, bClosedSpline);
		Points.Add(Point);
	}

	UE_LOG(LogPointSampling, Log, TEXT("[样条线采样] 完成等距采样，总弧长: %.2f"), TotalArcLength);

	// 去除重复点（容差设为很小）
	if (Points.Num() > 1)
	{
		int32 OriginalCount, RemovedCount;
		FPointDeduplicationHelper::RemoveDuplicatePointsWithStats(
			Points,
			1.0f,  // 容差：1cm（UE单位）
			OriginalCount,
			RemovedCount
		);

		if (RemovedCount > 0)
		{
			UE_LOG(LogPointSampling, Log,
				TEXT("[样条线采样] 去重：原始 %d 个点 -> 去除 %d 个重复点 -> 剩余 %d 个点"),
				OriginalCount, RemovedCount, Points.Num());
		}
	}

	return Points;
}

FVector FSplineSamplingHelper::CatmullRomInterpolate(
	const FVector& P0,
	const FVector& P1,
	const FVector& P2,
	const FVector& P3,
	float T)
{
	// Catmull-Rom 样条插值公式
	// q(t) = 0.5 * (2*P1 + (-P0 + P2)*t + (2*P0 - 5*P1 + 4*P2 - P3)*t^2 + (-P0 + 3*P1 - 3*P2 + P3)*t^3)

	float T2 = T * T;
	float T3 = T2 * T;

	FVector Result = 0.5f * (
		2.0f * P1 +
		(-P0 + P2) * T +
		(2.0f * P0 - 5.0f * P1 + 4.0f * P2 - P3) * T2 +
		(-P0 + 3.0f * P1 - 3.0f * P2 + P3) * T3
	);

	return Result;
}

float FSplineSamplingHelper::CalculateSplineLength(
	const TArray<FVector>& ControlPoints,
	bool bClosedSpline,
	int32 Segments)
{
	if (ControlPoints.Num() < 2)
	{
		return 0.0f;
	}

	float TotalLength = 0.0f;
	int32 NumSegments = bClosedSpline ? ControlPoints.Num() : (ControlPoints.Num() - 1);

	for (int32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
	{
		int32 Idx0 = (SegmentIndex - 1 + ControlPoints.Num()) % ControlPoints.Num();
		int32 Idx1 = SegmentIndex % ControlPoints.Num();
		int32 Idx2 = (SegmentIndex + 1) % ControlPoints.Num();
		int32 Idx3 = (SegmentIndex + 2) % ControlPoints.Num();

		if (!bClosedSpline)
		{
			if (SegmentIndex == 0) Idx0 = 0;
			if (SegmentIndex == NumSegments - 1) Idx3 = ControlPoints.Num() - 1;
		}

		const FVector& P0 = ControlPoints[Idx0];
		const FVector& P1 = ControlPoints[Idx1];
		const FVector& P2 = ControlPoints[Idx2];
		const FVector& P3 = ControlPoints[Idx3];

		// 用小步长近似计算曲线长度
		FVector PrevPoint = P1;
		for (int32 i = 1; i <= Segments; ++i)
		{
			float T = static_cast<float>(i) / Segments;
			FVector CurrentPoint = CatmullRomInterpolate(P0, P1, P2, P3, T);
			TotalLength += FVector::Dist(PrevPoint, CurrentPoint);
			PrevPoint = CurrentPoint;
		}
	}

		return TotalLength;
	}

	return TotalLength;
}

/**
 * 计算样条线的总弧长
 */
float FSplineSamplingHelper::CalculateSplineArcLength(
	const TArray<FVector>& ControlPoints,
	bool bClosedSpline)
{
	if (ControlPoints.Num() < 2)
	{
		return 0.0f;
	}

	float TotalLength = 0.0f;
	const int32 NumSegments = bClosedSpline ? ControlPoints.Num() : (ControlPoints.Num() - 1);
	const int32 SamplesPerSegment = 10; // 每段采样10个点用于近似弧长

	for (int32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
	{
		// 获取控制点
		int32 Idx0 = (SegmentIndex - 1 + ControlPoints.Num()) % ControlPoints.Num();
		int32 Idx1 = SegmentIndex % ControlPoints.Num();
		int32 Idx2 = (SegmentIndex + 1) % ControlPoints.Num();
		int32 Idx3 = (SegmentIndex + 2) % ControlPoints.Num();

		if (!bClosedSpline)
		{
			if (SegmentIndex == 0) Idx0 = 0;
			if (SegmentIndex == NumSegments - 1) Idx3 = ControlPoints.Num() - 1;
		}

		const FVector& P0 = ControlPoints[Idx0];
		const FVector& P1 = ControlPoints[Idx1];
		const FVector& P2 = ControlPoints[Idx2];
		const FVector& P3 = ControlPoints[Idx3];

		// 数值积分计算弧长
		FVector PrevPoint = P1;
		for (int32 i = 1; i <= SamplesPerSegment; ++i)
		{
			float T = static_cast<float>(i) / SamplesPerSegment;
			FVector CurrentPoint = CatmullRomInterpolate(P0, P1, P2, P3, T);
			TotalLength += FVector::Dist(PrevPoint, CurrentPoint);
			PrevPoint = CurrentPoint;
		}
	}

	return TotalLength;
}

/**
 * 根据弧长找到对应的参数T值（二分查找）
 */
float FSplineSamplingHelper::FindParameterByArcLength(
	const TArray<FVector>& ControlPoints,
	float TargetArcLength,
	bool bClosedSpline,
	float TotalArcLength)
{
	if (TargetArcLength <= 0.0f)
	{
		return 0.0f;
	}

	if (TargetArcLength >= TotalArcLength)
	{
		return bClosedSpline ? 1.0f : static_cast<float>(ControlPoints.Num() - 1);
	}

	const int32 NumSegments = bClosedSpline ? ControlPoints.Num() : (ControlPoints.Num() - 1);
	float AccumulatedLength = 0.0f;

	// 找到目标弧长所在的段
	for (int32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
	{
		int32 Idx0 = (SegmentIndex - 1 + ControlPoints.Num()) % ControlPoints.Num();
		int32 Idx1 = SegmentIndex % ControlPoints.Num();
		int32 Idx2 = (SegmentIndex + 1) % ControlPoints.Num();
		int32 Idx3 = (SegmentIndex + 2) % ControlPoints.Num();

		if (!bClosedSpline)
		{
			if (SegmentIndex == 0) Idx0 = 0;
			if (SegmentIndex == NumSegments - 1) Idx3 = ControlPoints.Num() - 1;
		}

		const FVector& P0 = ControlPoints[Idx0];
		const FVector& P1 = ControlPoints[Idx1];
		const FVector& P2 = ControlPoints[Idx2];
		const FVector& P3 = ControlPoints[Idx3];

		// 计算这一段的弧长
		float SegmentLength = 0.0f;
		FVector PrevPoint = P1;
		const int32 SamplesPerSegment = 20;

		for (int32 i = 1; i <= SamplesPerSegment; ++i)
		{
			float T = static_cast<float>(i) / SamplesPerSegment;
			FVector CurrentPoint = CatmullRomInterpolate(P0, P1, P2, P3, T);
			SegmentLength += FVector::Dist(PrevPoint, CurrentPoint);
			PrevPoint = CurrentPoint;
		}

		// 检查目标弧长是否在这一段内
		if (AccumulatedLength + SegmentLength >= TargetArcLength)
		{
			// 在这一段内，使用二分查找找到精确的T值
			float RemainingLength = TargetArcLength - AccumulatedLength;
			float SegmentT = FindTByArcLengthInSegment(P0, P1, P2, P3, RemainingLength, SegmentLength);
			return SegmentIndex + SegmentT;
		}

		AccumulatedLength += SegmentLength;
	}

	// 理论上不会到达这里
	return bClosedSpline ? 1.0f : static_cast<float>(ControlPoints.Num() - 1);
}

/**
 * 在单个段内根据弧长找到T值
 */
float FSplineSamplingHelper::FindTByArcLengthInSegment(
	const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3,
	float TargetLength, float SegmentLength)
{
	// 二分查找
	float Low = 0.0f;
	float High = 1.0f;
	const int32 MaxIterations = 20;

	for (int32 Iter = 0; Iter < MaxIterations; ++Iter)
	{
		float Mid = (Low + High) * 0.5f;
		FVector MidPoint = CatmullRomInterpolate(P0, P1, P2, P3, Mid);

		// 计算从起点到Mid的弧长
		float ArcLengthToMid = 0.0f;
		FVector PrevPoint = P1;
		const int32 Samples = 10;

		for (int32 i = 1; i <= Samples; ++i)
		{
			float T = Mid * i / Samples;
			FVector CurrentPoint = CatmullRomInterpolate(P0, P1, P2, P3, T);
			ArcLengthToMid += FVector::Dist(PrevPoint, CurrentPoint);
			PrevPoint = CurrentPoint;
		}

		if (ArcLengthToMid < TargetLength)
		{
			Low = Mid;
		}
		else
		{
			High = Mid;
		}
	}

	return (Low + High) * 0.5f;
}

/**
 * 在给定的参数T处计算样条点
 */
FVector FSplineSamplingHelper::EvaluateSplineAtParameter(
	const TArray<FVector>& ControlPoints,
	float ParameterT,
	bool bClosedSpline)
{
	const int32 NumSegments = bClosedSpline ? ControlPoints.Num() : (ControlPoints.Num() - 1);
	const int32 SegmentIndex = FMath::FloorToInt(ParameterT);
	const float LocalT = ParameterT - SegmentIndex;

	// 获取控制点
	int32 Idx0 = (SegmentIndex - 1 + ControlPoints.Num()) % ControlPoints.Num();
	int32 Idx1 = SegmentIndex % ControlPoints.Num();
	int32 Idx2 = (SegmentIndex + 1) % ControlPoints.Num();
	int32 Idx3 = (SegmentIndex + 2) % ControlPoints.Num();

	if (!bClosedSpline)
	{
		if (SegmentIndex == 0) Idx0 = 0;
		if (SegmentIndex >= NumSegments - 1) Idx3 = ControlPoints.Num() - 1;
	}

	const FVector& P0 = ControlPoints[Idx0];
	const FVector& P1 = ControlPoints[Idx1];
	const FVector& P2 = ControlPoints[Idx2];
	const FVector& P3 = ControlPoints[Idx3];

	return CatmullRomInterpolate(P0, P1, P2, P3, LocalT);
}

	// 1. 计算边界框（AABB - Bridson算法需要矩形输入区域）
	FVector BoundsMin, BoundsMax;
	CalculateBoundingBox(ControlPoints, BoundsMin, BoundsMax);

	FVector BoundsSize = BoundsMax - BoundsMin;
	float Width = BoundsSize.X;
	float Height = BoundsSize.Y;
	float Area = Width * Height;

	// 2. 自动计算最小距离（如果需要）
	float Radius = MinDistance;
	if (Radius <= 0.0f && TargetPointCount > 0)
	{
		// 基于目标点数和多边形实际面积估算
		// 注意：AABB面积 >= 多边形面积，所以这是保守估计
		float PointArea = Area / TargetPointCount;
		Radius = FMath::Sqrt(PointArea) * 0.8f; // 0.8系数补偿AABB过大的问题
	}
	else if (Radius <= 0.0f)
	{
		// 默认最小距离
		Radius = FMath::Max(Width, Height) * 0.05f;
	}

	// 3. 使用Bridson泊松圆盘采样算法在AABB内生成候选点
	// 这比简单的dart throwing高效得多：O(n) vs O(n²)
	TArray<FVector2D> CandidatePoints2D = FPoissonDiskSampling::GeneratePoisson2DFromStream(
		RandomStream,
		Width,
		Height,
		Radius,
		30  // MaxAttempts - Bridson算法的标准值
	);

	// 4. 将2D点转换为3D，并过滤掉多边形外的点
	Points.Reserve(CandidatePoints2D.Num());

	for (const FVector2D& Point2D : CandidatePoints2D)
	{
		// 转换为世界坐标（相对于AABB左下角）
		FVector Point3D(
			BoundsMin.X + Point2D.X,
			BoundsMin.Y + Point2D.Y,
			BoundsMin.Z  // 保持Z坐标
		);

		// 检查是否在多边形内部（射线法）
		if (IsPointInPolygon(Point3D, ControlPoints))
		{
			Points.Add(Point3D);

			// 如果指定了目标点数且已达到，提前退出
			if (TargetPointCount > 0 && Points.Num() >= TargetPointCount)
			{
				break;
			}
		}
	}

	// 去除重复/重叠点位（防御性编程，泊松采样理论上不应有重复点）
	if (Points.Num() > 0)
	{
		int32 OriginalCount, RemovedCount;
		FPointDeduplicationHelper::RemoveDuplicatePointsWithStats(
			Points,
			Radius * 0.5f,  // 容差：MinDistance 的一半
			OriginalCount,
			RemovedCount
		);

		if (RemovedCount > 0)
		{
			UE_LOG(LogPointSampling, Log,
				TEXT("[样条线边界采样] 去重：原始 %d 个点 -> 去除 %d 个重复点 -> 剩余 %d 个点"),
				OriginalCount, RemovedCount, Points.Num());
		}
	}

	return Points;
}

bool FSplineSamplingHelper::IsPointInPolygon(
	const FVector& Point,
	const TArray<FVector>& Polygon)
{
	// 射线法（Ray Casting Algorithm）
	// 从点向右发射一条射线，计算与多边形边的交点数
	// 奇数 = 内部，偶数 = 外部

	int32 NumVertices = Polygon.Num();
	if (NumVertices < 3)
	{
		return false;
	}

	int32 Crossings = 0;

	for (int32 i = 0; i < NumVertices; ++i)
	{
		int32 j = (i + 1) % NumVertices;

		const FVector& Vi = Polygon[i];
		const FVector& Vj = Polygon[j];

		// 检查边是否跨越测试点的Y坐标
		if (((Vi.Y > Point.Y) != (Vj.Y > Point.Y)))
		{
			// 计算射线与边的交点的X坐标
			float IntersectX = (Vj.X - Vi.X) * (Point.Y - Vi.Y) / (Vj.Y - Vi.Y) + Vi.X;

			// 如果交点在测试点右侧，计数+1
			if (Point.X < IntersectX)
			{
				++Crossings;
			}
		}
	}

	// 奇数交点 = 内部
	return (Crossings % 2) == 1;
}

void FSplineSamplingHelper::CalculateBoundingBox(
	const TArray<FVector>& Polygon,
	FVector& OutMin,
	FVector& OutMax)
{
	if (Polygon.Num() == 0)
	{
		OutMin = FVector::ZeroVector;
		OutMax = FVector::ZeroVector;
		return;
	}

	// 初始化为第一个点
	OutMin = Polygon[0];
	OutMax = Polygon[0];

	// 遍历所有点，找到最小/最大值
	for (int32 i = 1; i < Polygon.Num(); ++i)
	{
		const FVector& Point = Polygon[i];

		OutMin.X = FMath::Min(OutMin.X, Point.X);
		OutMin.Y = FMath::Min(OutMin.Y, Point.Y);
		OutMin.Z = FMath::Min(OutMin.Z, Point.Z);

		OutMax.X = FMath::Max(OutMax.X, Point.X);
		OutMax.Y = FMath::Max(OutMax.Y, Point.Y);
		OutMax.Z = FMath::Max(OutMax.Z, Point.Z);
	}
}
