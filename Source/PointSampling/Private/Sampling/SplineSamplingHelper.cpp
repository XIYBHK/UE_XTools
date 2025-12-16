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

	// 使用 Catmull-Rom 样条插值
	int32 NumSegments = bClosedSpline ? ControlPoints.Num() : (ControlPoints.Num() - 1);

	// 计算每段应该分配的点数（按段长度比例分配）
	TArray<int32> PointsPerSegment;
	PointsPerSegment.SetNum(NumSegments);

	int32 PointsPerSegmentBase = FMath::Max(1, PointCount / NumSegments);
	int32 RemainingPoints = PointCount - (PointsPerSegmentBase * NumSegments);

	for (int32 i = 0; i < NumSegments; ++i)
	{
		PointsPerSegment[i] = PointsPerSegmentBase;
		if (i < RemainingPoints)
		{
			++PointsPerSegment[i];
		}
	}

	// 生成各段的点
	int32 GeneratedCount = 0;
	for (int32 SegmentIndex = 0; SegmentIndex < NumSegments && GeneratedCount < PointCount; ++SegmentIndex)
	{
		// 获取当前段的四个控制点（P0, P1, P2, P3）
		int32 Idx0 = (SegmentIndex - 1 + ControlPoints.Num()) % ControlPoints.Num();
		int32 Idx1 = SegmentIndex % ControlPoints.Num();
		int32 Idx2 = (SegmentIndex + 1) % ControlPoints.Num();
		int32 Idx3 = (SegmentIndex + 2) % ControlPoints.Num();

		// 对于开放样条线，边界处理
		if (!bClosedSpline)
		{
			if (SegmentIndex == 0)
			{
				Idx0 = 0; // 起始段：P0=P1
			}
			if (SegmentIndex == NumSegments - 1)
			{
				Idx3 = ControlPoints.Num() - 1; // 结束段：P3=P2
			}
		}

		const FVector& P0 = ControlPoints[Idx0];
		const FVector& P1 = ControlPoints[Idx1];
		const FVector& P2 = ControlPoints[Idx2];
		const FVector& P3 = ControlPoints[Idx3];

		int32 SegmentPoints = PointsPerSegment[SegmentIndex];

		// 生成这一段的点（最后一段包含终点，其他段不包含终点以避免重复）
		int32 EndExclusive = (SegmentIndex == NumSegments - 1) ? SegmentPoints : (SegmentPoints - 1);
		for (int32 i = 0; i < EndExclusive && GeneratedCount < PointCount; ++i)
		{
			float T = (SegmentPoints > 1) ? (static_cast<float>(i) / (SegmentPoints - 1)) : 0.5f;
			FVector Point = CatmullRomInterpolate(P0, P1, P2, P3, T);
			Points.Add(Point);
			++GeneratedCount;
		}
	}

	// 去除重复/重叠点位（样条线段连接处可能产生重复点）
	if (Points.Num() > 0)
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

// ============================================================================
// 样条线边界泊松采样（使用Bridson算法）
// ============================================================================

TArray<FVector> FSplineSamplingHelper::GenerateWithinBoundary(
	int32 TargetPointCount,
	const TArray<FVector>& ControlPoints,
	float MinDistance,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;

	// 验证输入
	if (ControlPoints.Num() < 3)
	{
		// 至少需要3个点才能形成闭合区域
		return Points;
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
