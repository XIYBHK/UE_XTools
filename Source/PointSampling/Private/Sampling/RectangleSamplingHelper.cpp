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
	if (Spacing <= 0.0f)
	{
		return Points;
	}

	// 计算行列数
	int32 Rows = RowCount;
	int32 Cols = ColumnCount;

	// 如果行列数都未指定，则根据 PointCount 自动计算
	if (Rows <= 0 || Cols <= 0)
	{
		// 如果 PointCount 也未指定（-1），使用默认值
		int32 EffectivePointCount = (PointCount > 0) ? PointCount : 25; // 默认5x5
		CalculateOptimalRowsCols(EffectivePointCount, Rows, Cols);
	}

	// 计算实际生成的点数
	int32 ActualPointCount = Rows * Cols;
	// 如果指定了 PointCount，则限制生成数量
	if (PointCount > 0)
	{
		ActualPointCount = FMath::Min(PointCount, Rows * Cols);
	}
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
	if (Spacing <= 0.0f)
	{
		return Points;
	}

	// 计算行列数
	int32 Rows = RowCount;
	int32 Cols = ColumnCount;
	if (Rows <= 0 || Cols <= 0)
	{
		// 对于空心矩形，估算周长需要的点数
		int32 EffectivePointCount = (PointCount > 0) ? PointCount : 20; // 默认周长约20个点
		int32 EstimatedPerimeter = FMath::CeilToInt(FMath::Sqrt(EffectivePointCount * 4.0f));
		Rows = EstimatedPerimeter / 2;
		Cols = EstimatedPerimeter / 2;
		// 确保至少是2x2
		Rows = FMath::Max(2, Rows);
		Cols = FMath::Max(2, Cols);
	}

	// 计算最大可能生成的点数（空心矩形周长）
	int32 MaxPoints = 2 * (Rows + Cols) - 4; // 四条边的总点数（去掉重复的角点）
	int32 ActualPointCount = (PointCount > 0) ? FMath::Min(PointCount, MaxPoints) : MaxPoints;
	Points.Reserve(ActualPointCount);

	// 计算起始偏移
	float StartX = -(Cols - 1) * Spacing * 0.5f;
	float StartY = -(Rows - 1) * Spacing * 0.5f;

	// 生成边框点（上边、右边、下边、左边）
	int32 GeneratedCount = 0;

	// 上边
	for (int32 Col = 0; Col < Cols && GeneratedCount < ActualPointCount; ++Col)
	{
		Points.Add(FVector(StartX + Col * Spacing, StartY, 0.0f));
		++GeneratedCount;
	}

	// 右边（跳过角点）
	for (int32 Row = 1; Row < Rows && GeneratedCount < ActualPointCount; ++Row)
	{
		Points.Add(FVector(StartX + (Cols - 1) * Spacing, StartY + Row * Spacing, 0.0f));
		++GeneratedCount;
	}

	// 下边（从右到左，跳过角点）
	if (Rows > 1)
	{
		for (int32 Col = Cols - 2; Col >= 0 && GeneratedCount < ActualPointCount; --Col)
		{
			Points.Add(FVector(StartX + Col * Spacing, StartY + (Rows - 1) * Spacing, 0.0f));
			++GeneratedCount;
		}
	}

	// 左边（从下到上，跳过角点）
	if (Cols > 1)
	{
		for (int32 Row = Rows - 2; Row > 0 && GeneratedCount < ActualPointCount; --Row)
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
	if (Spacing <= 0.0f)
	{
		return Points;
	}

	// 如果 PointCount 未指定（-1），根据圈数估算合理的点数
	int32 ActualPointCount = PointCount;
	if (ActualPointCount <= 0)
	{
		// 每圈大约需要 100 个点
		ActualPointCount = FMath::CeilToInt(100.0f * SpiralTurns);
		ActualPointCount = FMath::Max(25, ActualPointCount); // 至少25个点
	}

	Points.Reserve(ActualPointCount);

	// 基于极坐标的连续螺旋曲线生成
	// 使用阿基米德螺旋：r = a + b*theta
	// a = 0.0f（从中心开始）
	// b = Spacing / (2*PI)（控制螺旋的紧密程度）
	const float b = Spacing / (2.0f * PI);
	
	// 总旋转角度（弧度）
	const float TotalTheta = SpiralTurns * 2.0f * PI;
	
	// 生成螺旋点
	for (int32 i = 0; i < ActualPointCount; ++i)
	{
		// 计算当前点的比例（0到1）
		float t = static_cast<float>(i) / (ActualPointCount - 1);
		
		// 计算当前角度和半径
		float theta = t * TotalTheta;
		float r = b * theta;
		
		// 转换为笛卡尔坐标
		FVector Point(
			r * FMath::Cos(theta),  // X
			r * FMath::Sin(theta),  // Y
			0.0f                     // Z
		);
		
		Points.Add(Point);
	}

	// 应用扰动
	if (JitterStrength > 0.0f)
	{
		ApplyJitter(Points, JitterStrength, Spacing, RandomStream);
	}

	UE_LOG(LogPointSampling, Log, TEXT("GenerateSpiralRectangle: 生成了 %d 个点 (间距: %.1f, 圈数: %.1f, 算法: 基于极坐标的连续螺旋曲线)"),
		Points.Num(), Spacing, SpiralTurns);

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

// ============================================================================
// 新增阵型实现（基于GitHub优秀实践）
// ============================================================================

TArray<FVector> FRectangleSamplingHelper::GenerateHexagonalGrid(
	int32 PointCount,
	float Spacing,
	int32 Rings,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0 || Rings < 0)
	{
		return Points;
	}

	// 六边形网格的六个方向向量（基于立方坐标系）
	const TArray<FVector> HexDirections = {
		FVector(Spacing, 0.0f, 0.0f),                                    // 右
		FVector(Spacing * 0.5f, Spacing * FMath::Sqrt(3.0f) * 0.5f, 0.0f), // 右上
		FVector(-Spacing * 0.5f, Spacing * FMath::Sqrt(3.0f) * 0.5f, 0.0f), // 左上
		FVector(-Spacing, 0.0f, 0.0f),                                   // 左
		FVector(-Spacing * 0.5f, -Spacing * FMath::Sqrt(3.0f) * 0.5f, 0.0f), // 左下
		FVector(Spacing * 0.5f, -Spacing * FMath::Sqrt(3.0f) * 0.5f, 0.0f)   // 右下
	};

	Points.Reserve(PointCount);

	// 中心点
	if (Points.Num() < PointCount)
	{
		Points.Add(FVector::ZeroVector);
	}

	// 生成环形结构（立方坐标系）
	for (int32 Ring = 1; Ring <= Rings && Points.Num() < PointCount; ++Ring)
	{
		// 每个环的起始位置：移动到右上方向的Ring步
		FVector RingStart = HexDirections[1] * Ring;

		// 遍历六条边
		for (int32 Direction = 0; Direction < 6 && Points.Num() < PointCount; ++Direction)
		{
			for (int32 Step = 0; Step < Ring && Points.Num() < PointCount; ++Step)
			{
				FVector Position = RingStart + HexDirections[Direction] * Step;
				Points.Add(Position);

				// 限制点数
				if (Points.Num() >= PointCount)
				{
					break;
				}
			}
		}
	}

	return Points;
}

TArray<FVector> FRectangleSamplingHelper::GenerateDiagonalFormation(
	int32 PointCount,
	float Spacing,
	int32 Direction,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0 || Spacing <= 0.0f)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 计算最佳行列数
	int32 Rows, Cols;
	CalculateOptimalRowsCols(PointCount, Rows, Cols);

	// 对角线阵型：按对角线方向排列
	const float DirSign = (Direction >= 0) ? 1.0f : -1.0f;
	const float StartOffset = -(FMath::Max(Rows, Cols) - 1) * Spacing * 0.5f;

	int32 GeneratedCount = 0;
	for (int32 Diagonal = 0; Diagonal < (Rows + Cols - 1) && GeneratedCount < PointCount; ++Diagonal)
	{
		// 计算对角线上的起始点
		int32 StartRow = (DirSign > 0) ? FMath::Max(0, Diagonal - Cols + 1) : FMath::Min(Diagonal, Rows - 1);
		int32 StartCol = (DirSign > 0) ? FMath::Min(Diagonal, Cols - 1) : FMath::Max(0, Diagonal - Rows + 1);

		// 沿对角线遍历
		int32 Row = StartRow;
		int32 Col = StartCol;

		while (Row >= 0 && Row < Rows && Col >= 0 && Col < Cols && GeneratedCount < PointCount)
		{
			FVector Point(
				StartOffset + Col * Spacing,
				StartOffset + Row * Spacing,
				0.0f
			);
			Points.Add(Point);
			GeneratedCount++;

			// 移动到下一个对角线位置
			if (DirSign > 0)
			{
				Row++;
				Col--;
			}
			else
			{
				Row--;
				Col++;
			}
		}
	}

	return Points;
}

TArray<FVector> FRectangleSamplingHelper::GenerateCheckerboardFormation(
	int32 PointCount,
	float Spacing,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0 || Spacing <= 0.0f)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 计算网格尺寸
	int32 Rows, Cols;
	CalculateOptimalRowsCols(PointCount, Rows, Cols);

	const float StartOffset = -(FMath::Max(Rows, Cols) - 1) * Spacing * 0.5f;

	// 棋盘阵型：只在黑格或白格上放置点
	bool bStartWithBlack = true; // 可以随机选择起始颜色

	int32 GeneratedCount = 0;
	for (int32 Row = 0; Row < Rows && GeneratedCount < PointCount; ++Row)
	{
		for (int32 Col = 0; Col < Cols && GeneratedCount < PointCount; ++Col)
		{
			// 检查是否为目标颜色（黑或白）
			bool bIsBlackSquare = ((Row + Col) % 2 == 0);
			if (bIsBlackSquare == bStartWithBlack)
			{
				FVector Point(
					StartOffset + Col * Spacing,
					StartOffset + Row * Spacing,
					0.0f
				);
				Points.Add(Point);
				GeneratedCount++;
			}
		}
	}

	return Points;
}
