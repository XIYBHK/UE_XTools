/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "GeometricFormationHelper.h"

TArray<FVector> FGeometricFormationHelper::GenerateHexagonalGrid(
	int32 PointCount,
	float Spacing,
	int32 Rings)

{
	TArray<FVector> Points;
	if (PointCount <= 0 || Rings < 0)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 蜂巢网格的六个方向向量
	const TArray<FVector> HexDirections = {
		FVector(1.0f, 0.0f, 0.0f),           // 右
		FVector(0.5f, FMath::Sqrt(3.0f)/2.0f, 0.0f),  // 右上
		FVector(-0.5f, FMath::Sqrt(3.0f)/2.0f, 0.0f), // 左上
		FVector(-1.0f, 0.0f, 0.0f),          // 左
		FVector(-0.5f, -FMath::Sqrt(3.0f)/2.0f, 0.0f), // 左下
		FVector(0.5f, -FMath::Sqrt(3.0f)/2.0f, 0.0f)   // 右下
	};

	// 中心点
	if (Points.Num() < PointCount)
	{
		Points.Add(FVector::ZeroVector);
	}

	// 生成环形结构
	for (int32 Ring = 1; Ring <= Rings && Points.Num() < PointCount; ++Ring)
	{
		// 每个环的起始位置
		FVector RingStart = HexDirections[4] * Ring * Spacing; // 从左下开始

		for (int32 Dir = 0; Dir < 6 && Points.Num() < PointCount; ++Dir)
		{
			for (int32 Step = 0; Step < Ring && Points.Num() < PointCount; ++Step)
			{
				FVector Position = RingStart + HexDirections[Dir] * Step * Spacing;
				Points.Add(Position);
			}
		}
	}

	return Points;
}

TArray<FVector> FGeometricFormationHelper::GenerateStarFormation(
	int32 PointCount,
	float OuterRadius,
	float InnerRadius,
	int32 PointsCount)

{
	TArray<FVector> Points;
	if (PointCount <= 0 || PointsCount < 3)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 星形的顶点：外圈和内圈交替
	const float AngleStep = 2.0f * PI / PointsCount;

	for (int32 i = 0; i < PointCount && i < PointsCount * 2; ++i)
	{
		int32 PointIndex = i / 2;
		bool bOuterPoint = (i % 2 == 0);

		float Angle = PointIndex * AngleStep;
		float Radius = bOuterPoint ? OuterRadius : InnerRadius;

		FVector Position(
			Radius * FMath::Cos(Angle),
			Radius * FMath::Sin(Angle),
			0.0f
		);

		Points.Add(Position);
	}

	return Points;
}

TArray<FVector> FGeometricFormationHelper::GenerateArchimedeanSpiral(
	int32 PointCount,
	float Spacing,
	float Turns)

{
	TArray<FVector> Points;
	if (PointCount <= 0)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 阿基米德螺旋：r = a + bθ
	const float AngleStep = (Turns * 2.0f * PI) / (PointCount - 1);
	const float GrowthRate = Spacing / (2.0f * PI); // b参数

	for (int32 i = 0; i < PointCount; ++i)
	{
		float Angle = i * AngleStep;
		float Radius = GrowthRate * Angle;

		FVector Position(
			Radius * FMath::Cos(Angle),
			Radius * FMath::Sin(Angle),
			0.0f
		);

		Points.Add(Position);
	}

	return Points;
}

TArray<FVector> FGeometricFormationHelper::GenerateLogarithmicSpiral(
	int32 PointCount,
	float GrowthFactor,
	float AngleStep)

{
	TArray<FVector> Points;
	if (PointCount <= 0)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 对数螺旋：r = a * e^(bθ)
	const float AngleIncrement = FMath::DegreesToRadians(AngleStep);
	const float B = FMath::Loge(GrowthFactor) / AngleIncrement; // 螺旋参数

	for (int32 i = 0; i < PointCount; ++i)
	{
		float Angle = i * AngleIncrement;
		float Radius = FMath::Exp(B * Angle);

		FVector Position(
			Radius * FMath::Cos(Angle),
			Radius * FMath::Sin(Angle),
			0.0f
		);

		Points.Add(Position);
	}

	return Points;
}

TArray<FVector> FGeometricFormationHelper::GenerateHeartFormation(
	int32 PointCount,
	float Size)

{
	TArray<FVector> Points;
	if (PointCount <= 0)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 心形曲线：参数化生成
	const float AngleStep = 2.0f * PI / PointCount;

	for (int32 i = 0; i < PointCount; ++i)
	{
		float t = i * AngleStep;
		FVector Position = HeartCurvePoint(t, Size);
		Points.Add(Position);
	}

	return Points;
}

TArray<FVector> FGeometricFormationHelper::GenerateFlowerFormation(
	int32 PointCount,
	float OuterRadius,
	float InnerRadius,
	int32 PetalCount)

{
	TArray<FVector> Points;
	if (PointCount <= 0 || PetalCount <= 0)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 花瓣曲线：每个花瓣的参数化生成
	const int32 PointsPerPetal = PointCount / PetalCount;
	const float AngleStep = 2.0f * PI / PointsPerPetal;

	for (int32 PetalIndex = 0; PetalIndex < PetalCount; ++PetalIndex)
	{
		for (int32 i = 0; i < PointsPerPetal && Points.Num() < PointCount; ++i)
		{
			float t = i * AngleStep;
			FVector Position = FlowerPetalPoint(t, PetalIndex, PetalCount, OuterRadius, InnerRadius);
			Points.Add(Position);
		}
	}

	return Points;
}

FVector FGeometricFormationHelper::HeartCurvePoint(float t, float Size)
{
	// 心形参数方程
	float x = Size * (16.0f * FMath::Pow(FMath::Sin(t), 3.0f));
	float y = Size * (13.0f * FMath::Cos(t) - 5.0f * FMath::Cos(2.0f * t) - 2.0f * FMath::Cos(3.0f * t) - FMath::Cos(4.0f * t));

	return FVector(x, y, 0.0f);
}

FVector FGeometricFormationHelper::FlowerPetalPoint(float t, int32 PetalIndex, int32 PetalCount, float OuterRadius, float InnerRadius)
{
	// 花瓣角度偏移
	float PetalAngle = (2.0f * PI * PetalIndex) / PetalCount;

	// 花瓣形状：椭圆形花瓣
	float LocalAngle = t;
	float Radius = InnerRadius + (OuterRadius - InnerRadius) * FMath::Abs(FMath::Cos(LocalAngle));

	float x = Radius * FMath::Cos(LocalAngle + PetalAngle);
	float y = Radius * FMath::Sin(LocalAngle + PetalAngle);

	return FVector(x, y, 0.0f);
}