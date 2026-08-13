/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "TriangleSamplingHelper.h"
#include "FormationSamplingInternal.h"
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
			// 计算当前点的基本坐标
			float BaseX = StartX + Col * Spacing;
			float BaseY = bInverted ? -YPos : YPos; // 倒三角时Y坐标取反
			
			// 应用行偏移，使点分布更均匀（类似六边形网格）
			float XOffset = (Row % 2 == 1) ? Spacing * 0.5f : 0.0f;
			
			FVector Point(
				BaseX + XOffset,
				BaseY,
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

	UE_LOG(LogPointSampling, Log, TEXT("GenerateSolidTriangle: 生成了 %d 个点 (间距: %.1f, 层数: %d, 倒三角: %s, 算法: 改进的等边三角形采样)"),
		Points.Num(), Spacing, Layers, bInverted ? TEXT("是") : TEXT("否"));

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

	// 以目标弧长构成居中的等边三角形，并在闭合周长上使用半开区间取样。
	const float SideLength = static_cast<float>(PointCount) * Spacing / 3.0f;
	const float Height = SideLength * FMath::Sqrt(3.0f) * 0.5f;
	const float Direction = bInverted ? 1.0f : -1.0f;
	const FVector TopVertex(0.0f, Direction * Height * (2.0f / 3.0f), 0.0f);
	const FVector LeftVertex(-SideLength * 0.5f, -Direction * Height / 3.0f, 0.0f);
	const FVector RightVertex(SideLength * 0.5f, -Direction * Height / 3.0f, 0.0f);

	for (int32 Index = 0; Index < PointCount; ++Index)
	{
		const float PerimeterProgress = static_cast<float>(Index) * 3.0f / static_cast<float>(PointCount);
		const int32 EdgeIndex = FMath::Min(FMath::FloorToInt(PerimeterProgress), 2);
		const float EdgeProgress = PerimeterProgress - EdgeIndex;

		switch (EdgeIndex)
		{
		case 0:
			Points.Add(FMath::Lerp(TopVertex, LeftVertex, EdgeProgress));
			break;
		case 1:
			Points.Add(FMath::Lerp(LeftVertex, RightVertex, EdgeProgress));
			break;
		default:
			Points.Add(FMath::Lerp(RightVertex, TopVertex, EdgeProgress));
			break;
		}
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
	// 保持原有功能：Scale = Spacing * 0.5f，JitterStrength 需要 Clamp
	// 注意：Strength 由 ApplyJitter2D 内部统一乘算，此处不得混入 Scale，否则强度被平方
	const float ClampedStrength = FMath::Clamp(JitterStrength, 0.0f, 1.0f);
	const float Scale = Spacing * 0.5f;
	FormationSamplingInternal::ApplyJitter2D(Points, ClampedStrength, Scale, RandomStream);
}
