/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "TriangleSamplingHelper.h"
#include "Math/UnrealMathUtility.h"

TArray<FVector> FTriangleSamplingHelper::GenerateSolidTriangle(
	int32 PointCount,
	float Spacing,
	bool bInverted,
	float JitterStrength,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0 || Spacing <= 0.0f)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 计算需要的层数（从顶点开始，每层点数递增）
	int32 Layers = CalculateTriangleLayers(PointCount);

	// 等边三角形的高度步长
	float RowSpacing = Spacing * FMath::Sqrt(3.0f) * 0.5f;

	int32 GeneratedCount = 0;

	// 从顶点向底部生成
	for (int32 Row = 0; Row < Layers && GeneratedCount < PointCount; ++Row)
	{
		int32 PointsInRow = Row + 1; // 第0行1个点，第1行2个点...

		// 计算这一行的Y坐标（垂直位置）
		float YPos = Row * RowSpacing;

		// 计算这一行的起始X坐标（使其居中）
		float RowWidth = (PointsInRow - 1) * Spacing;
		float StartX = -RowWidth * 0.5f;

		// 生成这一行的点
		for (int32 Col = 0; Col < PointsInRow && GeneratedCount < PointCount; ++Col)
		{
			FVector Point(
				StartX + Col * Spacing,
				bInverted ? -YPos : YPos,  // 倒三角时Y坐标取反
				0.0f
			);

			Points.Add(Point);
			++GeneratedCount;
		}
	}

	// 居中：将整个三角形的中心移到原点
	if (Points.Num() > 0)
	{
		// 计算边界框
		FVector Min = Points[0];
		FVector Max = Points[0];
		for (const FVector& Point : Points)
		{
			Min = Min.ComponentMin(Point);
			Max = Max.ComponentMax(Point);
		}

		FVector Center = (Min + Max) * 0.5f;
		for (FVector& Point : Points)
		{
			Point -= Center;
		}
	}

	// 应用扰动
	if (JitterStrength > 0.0f)
	{
		ApplyJitter(Points, JitterStrength, Spacing, RandomStream);
	}

	return Points;
}

TArray<FVector> FTriangleSamplingHelper::GenerateHollowTriangle(
	int32 PointCount,
	float Spacing,
	bool bInverted,
	float JitterStrength,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0 || Spacing <= 0.0f)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 计算需要的边长（每条边的点数）
	int32 PointsPerEdge = FMath::Max(2, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(PointCount))));

	// 等边三角形的高度
	float Height = (PointsPerEdge - 1) * Spacing * FMath::Sqrt(3.0f) * 0.5f;
	float HalfWidth = (PointsPerEdge - 1) * Spacing * 0.5f;

	// 三角形的三个顶点
	FVector TopVertex(0.0f, bInverted ? Height : -Height, 0.0f);
	FVector LeftVertex(-HalfWidth, bInverted ? -Height : Height, 0.0f);
	FVector RightVertex(HalfWidth, bInverted ? -Height : Height, 0.0f);

	int32 GeneratedCount = 0;

	// 生成三条边
	// 边1: 顶点 -> 左顶点
	for (int32 i = 0; i < PointsPerEdge && GeneratedCount < PointCount; ++i)
	{
		float T = (PointsPerEdge > 1) ? (static_cast<float>(i) / (PointsPerEdge - 1)) : 0.5f;
		FVector Point = FMath::Lerp(TopVertex, LeftVertex, T);
		Points.Add(Point);
		++GeneratedCount;
	}

	// 边2: 左顶点 -> 右顶点（跳过起点避免重复）
	for (int32 i = 1; i < PointsPerEdge && GeneratedCount < PointCount; ++i)
	{
		float T = (PointsPerEdge > 1) ? (static_cast<float>(i) / (PointsPerEdge - 1)) : 0.5f;
		FVector Point = FMath::Lerp(LeftVertex, RightVertex, T);
		Points.Add(Point);
		++GeneratedCount;
	}

	// 边3: 右顶点 -> 顶点（跳过起点和终点避免重复）
	for (int32 i = 1; i < PointsPerEdge - 1 && GeneratedCount < PointCount; ++i)
	{
		float T = (PointsPerEdge > 1) ? (static_cast<float>(i) / (PointsPerEdge - 1)) : 0.5f;
		FVector Point = FMath::Lerp(RightVertex, TopVertex, T);
		Points.Add(Point);
		++GeneratedCount;
	}

	// 应用扰动
	if (JitterStrength > 0.0f)
	{
		ApplyJitter(Points, JitterStrength, Spacing, RandomStream);
	}

	return Points;
}

int32 FTriangleSamplingHelper::CalculateTriangleLayers(int32 PointCount)
{
	// 三角形点数公式：n层有 n*(n+1)/2 个点
	// 反解：n = (-1 + sqrt(1 + 8*PointCount)) / 2
	float Layers = (-1.0f + FMath::Sqrt(1.0f + 8.0f * PointCount)) * 0.5f;
	return FMath::CeilToInt(Layers);
}

void FTriangleSamplingHelper::ApplyJitter(
	TArray<FVector>& Points,
	float JitterStrength,
	float Spacing,
	FRandomStream& RandomStream)
{
	if (JitterStrength <= 0.0f || Points.Num() == 0)
	{
		return;
	}

	// 扰动范围是间距的一半
	float MaxJitter = Spacing * 0.5f * FMath::Clamp(JitterStrength, 0.0f, 1.0f);

	for (FVector& Point : Points)
	{
		Point.X += RandomStream.FRandRange(-MaxJitter, MaxJitter);
		Point.Y += RandomStream.FRandRange(-MaxJitter, MaxJitter);
	}
}
