/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "CircleSamplingHelper.h"
#include "Math/UnrealMathUtility.h"

// 黄金角常量（137.5077640500378546463487度）
static constexpr float GOLDEN_ANGLE = 137.5077640500378546463487f;

TArray<FVector> FCircleSamplingHelper::GenerateCircle(
	int32 PointCount,
	float Radius,
	bool bIs3D,
	ECircleDistributionMode DistributionMode,
	float MinDistance,
	float StartAngle,
	bool bClockwise,
	float JitterStrength,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0 || Radius <= 0.0f)
	{
		return Points;
	}

	// 根据分布模式生成点位
	switch (DistributionMode)
	{
	case ECircleDistributionMode::Uniform:
		Points = GenerateUniform(PointCount, Radius, bIs3D, StartAngle, bClockwise);
		break;

	case ECircleDistributionMode::Fibonacci:
		Points = GenerateFibonacci(PointCount, Radius, bIs3D);
		break;

	case ECircleDistributionMode::Poisson:
		Points = GeneratePoisson(PointCount, Radius, bIs3D, MinDistance, RandomStream);
		break;

	default:
		Points = GenerateUniform(PointCount, Radius, bIs3D, StartAngle, bClockwise);
		break;
	}

	// 应用扰动（泊松分布不需要扰动，因为本身就是随机的）
	if (JitterStrength > 0.0f && DistributionMode != ECircleDistributionMode::Poisson)
	{
		ApplyJitter(Points, JitterStrength, Radius, RandomStream);
	}

	return Points;
}

TArray<FVector> FCircleSamplingHelper::GenerateSnowflake(
	int32 PointCount,
	float Radius,
	int32 SnowflakeLayers,
	float Spacing,
	float JitterStrength,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0 || Radius <= 0.0f || SnowflakeLayers <= 0)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 计算每层的半径
	TArray<float> LayerRadii;
	LayerRadii.Reserve(SnowflakeLayers);

	if (SnowflakeLayers == 1)
	{
		// 只有一层，就是最外层
		LayerRadii.Add(Radius);
	}
	else
	{
		// 多层：从最外层向内，间距为 Spacing
		for (int32 i = 0; i < SnowflakeLayers; ++i)
		{
			float LayerRadius = Radius - i * Spacing;
			if (LayerRadius > 0.0f)
			{
				LayerRadii.Add(LayerRadius);
			}
			else
			{
				break; // 半径为负，停止
			}
		}
	}

	int32 ActualLayers = LayerRadii.Num();
	if (ActualLayers == 0)
	{
		return Points;
	}

	// 计算每层的点数（按层数比例分配）
	int32 PointsPerLayer = FMath::Max(1, PointCount / ActualLayers);

	// 生成各层的点
	int32 GeneratedCount = 0;
	for (int32 LayerIndex = 0; LayerIndex < ActualLayers && GeneratedCount < PointCount; ++LayerIndex)
	{
		float LayerRadius = LayerRadii[LayerIndex];

		// 最后一层用剩余所有点数
		int32 LayerPointCount = (LayerIndex == ActualLayers - 1)
			? (PointCount - GeneratedCount)
			: PointsPerLayer;

		if (LayerPointCount <= 0)
			continue;

		// 每层的起始角度偏移，避免所有层的点对齐
		float AngleOffset = LayerIndex * 30.0f; // 每层偏移30度

		// 生成这一层的圆周点
		TArray<FVector> LayerPoints;
		GenerateArcPoints(LayerPoints, LayerPointCount, LayerRadius, AngleOffset, AngleOffset + 360.0f, true);

		Points.Append(LayerPoints);
		GeneratedCount += LayerPoints.Num();
	}

	// 应用扰动
	if (JitterStrength > 0.0f)
	{
		ApplyJitter(Points, JitterStrength, Radius * 0.1f, RandomStream);
	}

	return Points;
}

TArray<FVector> FCircleSamplingHelper::GenerateSnowflakeArc(
	int32 PointCount,
	float Radius,
	int32 SnowflakeLayers,
	float Spacing,
	float ArcAngle,
	float StartAngle,
	float JitterStrength,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0 || Radius <= 0.0f || SnowflakeLayers <= 0 || ArcAngle <= 0.0f)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 计算每层的半径
	TArray<float> LayerRadii;
	LayerRadii.Reserve(SnowflakeLayers);

	if (SnowflakeLayers == 1)
	{
		LayerRadii.Add(Radius);
	}
	else
	{
		for (int32 i = 0; i < SnowflakeLayers; ++i)
		{
			float LayerRadius = Radius - i * Spacing;
			if (LayerRadius > 0.0f)
			{
				LayerRadii.Add(LayerRadius);
			}
			else
			{
				break;
			}
		}
	}

	int32 ActualLayers = LayerRadii.Num();
	if (ActualLayers == 0)
	{
		return Points;
	}

	// 计算每层的点数
	int32 PointsPerLayer = FMath::Max(1, PointCount / ActualLayers);

	// 生成各层的弧形点
	int32 GeneratedCount = 0;
	for (int32 LayerIndex = 0; LayerIndex < ActualLayers && GeneratedCount < PointCount; ++LayerIndex)
	{
		float LayerRadius = LayerRadii[LayerIndex];

		int32 LayerPointCount = (LayerIndex == ActualLayers - 1)
			? (PointCount - GeneratedCount)
			: PointsPerLayer;

		if (LayerPointCount <= 0)
			continue;

		// 生成这一层的弧形点（所有层使用相同的起始角度和弧度范围）
		TArray<FVector> LayerPoints;
		float EndAngle = StartAngle + ArcAngle;
		GenerateArcPoints(LayerPoints, LayerPointCount, LayerRadius, StartAngle, EndAngle, true);

		Points.Append(LayerPoints);
		GeneratedCount += LayerPoints.Num();
	}

	// 应用扰动
	if (JitterStrength > 0.0f)
	{
		ApplyJitter(Points, JitterStrength, Radius * 0.1f, RandomStream);
	}

	return Points;
}

void FCircleSamplingHelper::GenerateArcPoints(
	TArray<FVector>& OutPoints,
	int32 PointCount,
	float Radius,
	float StartAngleDeg,
	float EndAngleDeg,
	bool bClockwise)
{
	if (PointCount <= 0 || Radius <= 0.0f)
	{
		return;
	}

	// 转换角度为弧度
	float StartAngleRad = FMath::DegreesToRadians(StartAngleDeg);
	float EndAngleRad = FMath::DegreesToRadians(EndAngleDeg);

	// 计算角度跨度
	float AngleSpan = EndAngleRad - StartAngleRad;
	if (!bClockwise)
	{
		AngleSpan = -AngleSpan;
	}

	// 生成点位
	for (int32 i = 0; i < PointCount; ++i)
	{
		float T = (PointCount > 1) ? (static_cast<float>(i) / (PointCount - 1)) : 0.5f;
		float CurrentAngle = StartAngleRad + T * AngleSpan;

		// 计算圆周上的点位（UE使用Y轴向前，X轴向右的左手坐标系）
		FVector Point(
			FMath::Cos(CurrentAngle) * Radius,  // X
			FMath::Sin(CurrentAngle) * Radius,  // Y
			0.0f                                 // Z
		);

		OutPoints.Add(Point);
	}
}

void FCircleSamplingHelper::ApplyJitter(
	TArray<FVector>& Points,
	float JitterStrength,
	float BaseRadius,
	FRandomStream& RandomStream)
{
	if (JitterStrength <= 0.0f || Points.Num() == 0)
	{
		return;
	}

	// 扰动范围基于半径的比例
	float MaxJitter = BaseRadius * 0.1f * FMath::Clamp(JitterStrength, 0.0f, 1.0f);

	for (FVector& Point : Points)
	{
		// 在XY平面上应用随机扰动
		Point.X += RandomStream.FRandRange(-MaxJitter, MaxJitter);
		Point.Y += RandomStream.FRandRange(-MaxJitter, MaxJitter);
	}
}

// ============================================================================
// 分布模式实现
// ============================================================================

TArray<FVector> FCircleSamplingHelper::GenerateUniform(
	int32 PointCount,
	float Radius,
	bool bIs3D,
	float StartAngle,
	bool bClockwise)
{
	TArray<FVector> Points;
	Points.Reserve(PointCount);

	if (bIs3D)
	{
		// 3D球体 - 使用经纬度网格（有极点聚集问题，但简单直观）
		int32 LatitudeCount = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(PointCount) / 2.0f));
		int32 LongitudeCount = FMath::CeilToInt(static_cast<float>(PointCount) / LatitudeCount);

		int32 GeneratedCount = 0;
		for (int32 i = 0; i < LatitudeCount && GeneratedCount < PointCount; ++i)
		{
			// 纬度：从-90°到90°
			float Lat = -90.0f + (180.0f * i / (LatitudeCount - 1));
			float LatRad = FMath::DegreesToRadians(Lat);
			float RadiusAtLat = FMath::Cos(LatRad);

			int32 PointsInRing = (i == 0 || i == LatitudeCount - 1) ? 1 : LongitudeCount;

			for (int32 j = 0; j < PointsInRing && GeneratedCount < PointCount; ++j)
			{
				// 经度：从0°到360°
				float Lon = 360.0f * j / PointsInRing;
				float LonRad = FMath::DegreesToRadians(Lon);

				FVector Point(
					FMath::Cos(LonRad) * RadiusAtLat * Radius,
					FMath::Sin(LatRad) * Radius,
					FMath::Sin(LonRad) * RadiusAtLat * Radius
				);

				Points.Add(Point);
				++GeneratedCount;
			}
		}
	}
	else
	{
		// 2D圆形 - 圆周均匀角度分布
		float EndAngle = StartAngle + 360.0f;
		GenerateArcPoints(Points, PointCount, Radius, StartAngle, EndAngle, bClockwise);
	}

	return Points;
}

TArray<FVector> FCircleSamplingHelper::GenerateFibonacci(
	int32 PointCount,
	float Radius,
	bool bIs3D)
{
	TArray<FVector> Points;
	Points.Reserve(PointCount);

	if (bIs3D)
	{
		// 3D球体 - 斐波那契球面（最均匀的球面分布）
		for (int32 i = 0; i < PointCount; ++i)
		{
			// 黄金角度（弧度）
			float Theta = GOLDEN_ANGLE * PI / 180.0f * i;

			// y坐标从1到-1均匀分布
			float Y = 1.0f - (i / static_cast<float>(PointCount - 1)) * 2.0f;

			// 在该高度的圆的半径
			float RadiusAtY = FMath::Sqrt(1.0f - Y * Y);

			FVector Point(
				FMath::Cos(Theta) * RadiusAtY * Radius,
				Y * Radius,
				FMath::Sin(Theta) * RadiusAtY * Radius
			);

			Points.Add(Point);
		}
	}
	else
	{
		// 2D圆形 - 黄金角螺旋（Vogel's method）
		for (int32 i = 0; i < PointCount; ++i)
		{
			// 黄金角度（度转弧度）
			float Angle = i * GOLDEN_ANGLE * PI / 180.0f;

			// 半径按sqrt分布，使点密度均匀
			float R = Radius * FMath::Sqrt(i / static_cast<float>(PointCount));

			FVector Point(
				FMath::Cos(Angle) * R,
				FMath::Sin(Angle) * R,
				0.0f
			);

			Points.Add(Point);
		}
	}

	return Points;
}

TArray<FVector> FCircleSamplingHelper::GeneratePoisson(
	int32 PointCount,
	float Radius,
	bool bIs3D,
	float MinDistance,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;

	// 如果MinDistance太小或为0，自动计算一个合理值
	if (MinDistance <= 0.0f)
	{
		// 基于点数和区域大小估算
		if (bIs3D)
		{
			// 球体体积
			float Volume = (4.0f / 3.0f) * PI * Radius * Radius * Radius;
			float PointVolume = Volume / PointCount;
			MinDistance = FMath::Pow(PointVolume, 1.0f / 3.0f);
		}
		else
		{
			// 圆形面积
			float Area = PI * Radius * Radius;
			float PointArea = Area / PointCount;
			MinDistance = FMath::Sqrt(PointArea);
		}
	}

	// 简化的泊松盘采样（dart throwing algorithm）
	// 注意：这是一个简化版本，完整的泊松采样需要空间划分（grid）来加速
	Points.Reserve(PointCount);

	int32 MaxAttempts = PointCount * 30; // 最大尝试次数
	int32 Attempts = 0;

	while (Points.Num() < PointCount && Attempts < MaxAttempts)
	{
		FVector Candidate;

		if (bIs3D)
		{
			// 在球体内随机生成点
			// 使用拒绝采样（rejection sampling）
			do
			{
				Candidate = FVector(
					RandomStream.FRandRange(-Radius, Radius),
					RandomStream.FRandRange(-Radius, Radius),
					RandomStream.FRandRange(-Radius, Radius)
				);
			} while (Candidate.SizeSquared() > Radius * Radius);
		}
		else
		{
			// 在圆形内随机生成点
			float R = FMath::Sqrt(RandomStream.FRand()) * Radius;
			float Theta = RandomStream.FRand() * 2.0f * PI;
			Candidate = FVector(
				FMath::Cos(Theta) * R,
				FMath::Sin(Theta) * R,
				0.0f
			);
		}

		// 检查是否与现有点保持最小距离
		bool bTooClose = false;
		for (const FVector& ExistingPoint : Points)
		{
			if (FVector::DistSquared(Candidate, ExistingPoint) < MinDistance * MinDistance)
			{
				bTooClose = true;
				break;
			}
		}

		if (!bTooClose)
		{
			Points.Add(Candidate);
		}

		++Attempts;
	}

	return Points;
}
