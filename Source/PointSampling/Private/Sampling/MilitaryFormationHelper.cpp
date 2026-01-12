/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "MilitaryFormationHelper.h"

TArray<FVector> FMilitaryFormationHelper::GenerateWedgeFormation(
	int32 PointCount,
	float Spacing,
	float WedgeAngle,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 楔形阵型的布局逻辑：
	// - 第1个点在尖端 (0,0,0)
	// - 后续点依次分布在两侧，形成V形
	// - 左侧和右侧交替放置

	const float HalfWedgeAngle = FMath::DegreesToRadians(WedgeAngle * 0.5f);
	int32 LeftIndex = 1;  // 左侧分支的下一个点索引
	int32 RightIndex = 1; // 右侧分支的下一个点索引

	// 第1个点：楔形尖端
	Points.Add(FVector::ZeroVector);

	// 生成剩余的点
	for (int32 i = 1; i < PointCount; ++i)
	{
		bool bPlaceOnLeft = (i % 2 == 1); // 奇数点放左侧，偶数点放右侧

		int32 BranchIndex = bPlaceOnLeft ? LeftIndex : RightIndex;
		float Angle = bPlaceOnLeft ? -HalfWedgeAngle : HalfWedgeAngle;

		// 计算点的距离（基于分支层级）
		float Distance = BranchIndex * Spacing;

		// 计算位置
		FVector Position(
			Distance * FMath::Cos(Angle),
			Distance * FMath::Sin(Angle),
			0.0f
		);

		Points.Add(Position);

		// 更新分支索引
		if (bPlaceOnLeft)
		{
			LeftIndex++;
		}
		else
		{
			RightIndex++;
		}
	}

	return Points;
}

TArray<FVector> FMilitaryFormationHelper::GenerateColumnFormation(
	int32 PointCount,
	float Spacing,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 纵队阵型：单列纵队，所有点在一条直线上
	for (int32 i = 0; i < PointCount; ++i)
	{
		float Y = i * Spacing;
		Points.Add(FVector(0.0f, Y, 0.0f));
	}

	return Points;
}

TArray<FVector> FMilitaryFormationHelper::GenerateLineFormation(
	int32 PointCount,
	float Spacing,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 横队阵型：单排横队，最大火力覆盖
	float TotalWidth = (PointCount - 1) * Spacing;
	float StartX = -TotalWidth * 0.5f;

	for (int32 i = 0; i < PointCount; ++i)
	{
		float X = StartX + i * Spacing;
		Points.Add(FVector(X, 0.0f, 0.0f));
	}

	return Points;
}

TArray<FVector> FMilitaryFormationHelper::GenerateVeeFormation(
	int32 PointCount,
	float Spacing,
	float VeeAngle,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// V形阵型：尖端向后，形成倒V形
	const float HalfVeeAngle = FMath::DegreesToRadians(VeeAngle * 0.5f);
	int32 LeftIndex = 1;
	int32 RightIndex = 1;

	// 第1个点：V形底部
	Points.Add(FVector::ZeroVector);

	for (int32 i = 1; i < PointCount; ++i)
	{
		bool bPlaceOnLeft = (i % 2 == 1);
		int32 BranchIndex = bPlaceOnLeft ? LeftIndex : RightIndex;
		float Angle = bPlaceOnLeft ? -HalfVeeAngle : HalfVeeAngle;

		float Distance = BranchIndex * Spacing;

		// V形阵型尖端向后，所以Y坐标为负
		FVector Position(
			Distance * FMath::Cos(Angle),
			-Distance * FMath::Sin(Angle), // 向后
			0.0f
		);

		Points.Add(Position);

		if (bPlaceOnLeft)
		{
			LeftIndex++;
		}
		else
		{
			RightIndex++;
		}
	}

	return Points;
}

TArray<FVector> FMilitaryFormationHelper::GenerateEchelonFormation(
	int32 PointCount,
	float Spacing,
	int32 Direction,
	float EchelonAngle,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 梯形阵型：阶梯形布局
	const float AngleRad = FMath::DegreesToRadians(EchelonAngle);
	const float DirSign = (Direction >= 0) ? 1.0f : -1.0f;

	for (int32 i = 0; i < PointCount; ++i)
	{
		// 计算阶梯层级和位置
		int32 Row = i / 3; // 每3个点一层
		int32 Col = i % 3; // 每层的列索引

		float X = Col * Spacing;
		float Y = Row * Spacing;
		float OffsetX = Row * Spacing * DirSign * FMath::Tan(AngleRad);

		FVector Position(X + OffsetX, Y, 0.0f);
		Points.Add(Position);
	}

	return Points;
}