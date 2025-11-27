/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "RectangleSamplingHelper.h"
#include "Math/UnrealMathUtility.h"

TArray<FVector> FRectangleSamplingHelper::GenerateSolidRectangle(
	int32 PointCount,
	float Spacing,
	int32 RowCount,
	int32 ColumnCount,
	float JitterStrength,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0 || Spacing <= 0.0f)
	{
		return Points;
	}

	// 计算行列数
	int32 Rows = RowCount;
	int32 Cols = ColumnCount;
	if (Rows <= 0 || Cols <= 0)
	{
		CalculateOptimalRowsCols(PointCount, Rows, Cols);
	}

	// 限制实际生成的点数
	int32 ActualPointCount = FMath::Min(PointCount, Rows * Cols);
	Points.Reserve(ActualPointCount);

	// 计算起始偏移（使阵型居中）
	float StartX = -(Cols - 1) * Spacing * 0.5f;
	float StartY = -(Rows - 1) * Spacing * 0.5f;

	// 生成网格点
	int32 GeneratedCount = 0;
	for (int32 Row = 0; Row < Rows && GeneratedCount < ActualPointCount; ++Row)
	{
		for (int32 Col = 0; Col < Cols && GeneratedCount < ActualPointCount; ++Col)
		{
			FVector Point(
				StartX + Col * Spacing,
				StartY + Row * Spacing,
				0.0f
			);
			Points.Add(Point);
			++GeneratedCount;
		}
	}

	// 应用扰动
	if (JitterStrength > 0.0f)
	{
		ApplyJitter(Points, JitterStrength, Spacing, RandomStream);
	}

	return Points;
}

TArray<FVector> FRectangleSamplingHelper::GenerateHollowRectangle(
	int32 PointCount,
	float Spacing,
	int32 RowCount,
	int32 ColumnCount,
	float JitterStrength,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0 || Spacing <= 0.0f)
	{
		return Points;
	}

	// 计算行列数
	int32 Rows = RowCount;
	int32 Cols = ColumnCount;
	if (Rows <= 0 || Cols <= 0)
	{
		// 对于空心矩形，估算周长需要的点数
		int32 EstimatedPerimeter = FMath::CeilToInt(FMath::Sqrt(PointCount * 4.0f));
		Rows = EstimatedPerimeter / 2;
		Cols = EstimatedPerimeter / 2;
	}

	Points.Reserve(PointCount);

	// 计算起始偏移
	float StartX = -(Cols - 1) * Spacing * 0.5f;
	float StartY = -(Rows - 1) * Spacing * 0.5f;

	// 生成边框点（上边、右边、下边、左边）
	int32 GeneratedCount = 0;

	// 上边
	for (int32 Col = 0; Col < Cols && GeneratedCount < PointCount; ++Col)
	{
		Points.Add(FVector(StartX + Col * Spacing, StartY, 0.0f));
		++GeneratedCount;
	}

	// 右边（跳过角点）
	for (int32 Row = 1; Row < Rows && GeneratedCount < PointCount; ++Row)
	{
		Points.Add(FVector(StartX + (Cols - 1) * Spacing, StartY + Row * Spacing, 0.0f));
		++GeneratedCount;
	}

	// 下边（从右到左，跳过角点）
	if (Rows > 1)
	{
		for (int32 Col = Cols - 2; Col >= 0 && GeneratedCount < PointCount; --Col)
		{
			Points.Add(FVector(StartX + Col * Spacing, StartY + (Rows - 1) * Spacing, 0.0f));
			++GeneratedCount;
		}
	}

	// 左边（从下到上，跳过角点）
	if (Cols > 1)
	{
		for (int32 Row = Rows - 2; Row > 0 && GeneratedCount < PointCount; --Row)
		{
			Points.Add(FVector(StartX, StartY + Row * Spacing, 0.0f));
			++GeneratedCount;
		}
	}

	// 应用扰动
	if (JitterStrength > 0.0f)
	{
		ApplyJitter(Points, JitterStrength, Spacing, RandomStream);
	}

	return Points;
}

TArray<FVector> FRectangleSamplingHelper::GenerateSpiralRectangle(
	int32 PointCount,
	float Spacing,
	float SpiralTurns,
	float JitterStrength,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0 || Spacing <= 0.0f)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 从中心开始螺旋向外
	// 方向：右 -> 下 -> 左 -> 上 -> 右（重复，每次步长增加）
	FVector CurrentPos = FVector::ZeroVector;
	Points.Add(CurrentPos);

	int32 GeneratedCount = 1;
	int32 StepSize = 1; // 当前方向的步数
	int32 DirectionIndex = 0; // 0=右, 1=下, 2=左, 3=上

	// 方向向量
	const FVector Directions[] = {
		FVector(Spacing, 0.0f, 0.0f),  // 右
		FVector(0.0f, Spacing, 0.0f),  // 下
		FVector(-Spacing, 0.0f, 0.0f), // 左
		FVector(0.0f, -Spacing, 0.0f)  // 上
	};

	while (GeneratedCount < PointCount)
	{
		// 每个方向走 StepSize 步
		for (int32 Step = 0; Step < StepSize && GeneratedCount < PointCount; ++Step)
		{
			CurrentPos += Directions[DirectionIndex];
			Points.Add(CurrentPos);
			++GeneratedCount;
		}

		// 切换方向
		DirectionIndex = (DirectionIndex + 1) % 4;

		// 每完成两个方向（右下 或 左上），步长增加
		if (DirectionIndex == 0 || DirectionIndex == 2)
		{
			++StepSize;
		}
	}

	// 应用扰动
	if (JitterStrength > 0.0f)
	{
		ApplyJitter(Points, JitterStrength, Spacing, RandomStream);
	}

	return Points;
}

void FRectangleSamplingHelper::CalculateOptimalRowsCols(int32 PointCount, int32& OutRows, int32& OutCols)
{
	// 尽量接近正方形的比例
	int32 SqrtCount = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(PointCount)));
	OutCols = SqrtCount;
	OutRows = FMath::CeilToInt(static_cast<float>(PointCount) / SqrtCount);
}

void FRectangleSamplingHelper::ApplyJitter(
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
