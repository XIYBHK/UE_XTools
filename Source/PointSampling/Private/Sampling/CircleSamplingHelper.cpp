/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "CircleSamplingHelper.h"
#include "FormationSamplingInternal.h"
#include "Math/UnrealMathUtility.h"
#include "Algorithms/PoissonDiskSampling.h"

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
		OutPoints.Add(FormationSamplingInternal::PolarToCartesian(Radius, CurrentAngle));
	}
}

void FCircleSamplingHelper::ApplyJitter(
	TArray<FVector>& Points,
	float JitterStrength,
	float BaseRadius,
	FRandomStream& RandomStream)
{
	// 保持原有功能：Scale = BaseRadius * 0.1f，JitterStrength 需要 Clamp
	const float ClampedStrength = FMath::Clamp(JitterStrength, 0.0f, 1.0f);
	const float Scale = BaseRadius * 0.1f * ClampedStrength;
	FormationSamplingInternal::ApplyJitter2D(Points, ClampedStrength, Scale, RandomStream);
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
		// 3D球体 - 使用 Fibonacci 球面采样（GitHub 最佳实践，无极点聚集问题）
		// 参考：gptoolbox/fibonacci_sphere_sampling.m, bestjunh/Fibonacci-sphere
		return GenerateFibonacci(PointCount, Radius, true);
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
			const float R = Radius * FMath::Sqrt(i / static_cast<float>(PointCount));
			Points.Add(FormationSamplingInternal::PolarToCartesian(R, Angle));
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

	// 使用Robert Bridson's Fast Poisson Disk Sampling算法，适配圆形/球体区域
	if (bIs3D)
	{
		// 3D球体泊松采样 - 使用FPoissonDiskSampling::GeneratePoisson3D，然后筛选球体内的点
		// 计算包围球体的立方体大小
		float CubeSize = Radius * 2.0f;
		
		// 生成立方体泊松采样
		TArray<FVector2D> Poisson2D = FPoissonDiskSampling::GeneratePoisson2D(CubeSize, CubeSize, MinDistance, 30);
		
		// 将2D点转换为3D点，并筛选球体内的点
		for (const FVector2D& Point2D : Poisson2D)
		{
			// 在球体内部随机生成Z坐标
			float X = Point2D.X - Radius;
			float Y = Point2D.Y - Radius;
			
			// 计算该XY平面上球体能容纳的Z范围
			float R2 = Radius * Radius;
			float XY2 = X * X + Y * Y;
			
			if (XY2 < R2)
			{
				float MaxZ = FMath::Sqrt(R2 - XY2);
				float Z = RandomStream.FRandRange(-MaxZ, MaxZ);
				
				FVector Point3D(X, Y, Z);
				Points.Add(Point3D);
				
				if (Points.Num() >= PointCount)
				{
					break;
				}
			}
		}
	}
	else
	{
		// 2D圆形泊松采样 - 使用FPoissonDiskSampling::GeneratePoisson2D，然后筛选圆形内的点
		// 计算包围圆形的正方形大小
		float SquareSize = Radius * 2.0f;
		
		// 生成正方形泊松采样
		TArray<FVector2D> Poisson2D = FPoissonDiskSampling::GeneratePoisson2D(SquareSize, SquareSize, MinDistance, 30);
		
		// 筛选圆形内的点
		for (const FVector2D& Point2D : Poisson2D)
		{
			// 将点转换到以原点为中心的坐标
			float X = Point2D.X - Radius;
			float Y = Point2D.Y - Radius;
			
			// 检查是否在圆形内
			if (X * X + Y * Y <= Radius * Radius)
			{
				Points.Add(FVector(X, Y, 0.0f));
				
				if (Points.Num() >= PointCount)
				{
					break;
				}
			}
		}
	}

	// 如果生成的点数不够，补充随机点（使用拒绝采样）
	if (Points.Num() < PointCount)
	{
		int32 MaxAttempts = (PointCount - Points.Num()) * 30;
		int32 Attempts = 0;
		
		while (Points.Num() < PointCount && Attempts < MaxAttempts)
		{
			FVector Candidate;
			
			if (bIs3D)
			{
				// 在球体内随机生成点
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
				const float R = FMath::Sqrt(RandomStream.FRand()) * Radius;
				const float Theta = RandomStream.FRand() * 2.0f * PI;
				Candidate = FormationSamplingInternal::PolarToCartesian(R, Theta);
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
	}

	UE_LOG(LogPointSampling, Log, TEXT("GeneratePoisson (Circle): 生成了 %d 个点 (半径: %.1f, 最小距离: %.1f, 算法: Robert Bridson's Fast Poisson Disk Sampling)"),
		Points.Num(), Radius, MinDistance);

	return Points;
}

// ============================================================================
// 新增阵型实现（基于GitHub优秀实践）
// ============================================================================

TArray<FVector> FCircleSamplingHelper::GenerateGoldenSpiral(
	int32 PointCount,
	float MaxRadius,
	float JitterStrength,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0 || MaxRadius <= 0.0f)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 黄金角（弧度）
	const float GoldenAngleRad = GOLDEN_ANGLE * PI / 180.0f;

	for (int32 i = 0; i < PointCount; ++i)
	{
		// 计算角度（黄金角累积）
		float Angle = i * GoldenAngleRad;

		// 计算半径（线性增长，但可以通过参数调整）
		float Radius = (static_cast<float>(i) / (PointCount - 1)) * MaxRadius;

		// 极坐标转换为笛卡尔坐标
		FVector Point(
			FMath::Cos(Angle) * Radius,
			FMath::Sin(Angle) * Radius,
			0.0f
		);

		Points.Add(Point);
	}

	// 应用扰动
	if (JitterStrength > 0.0f)
	{
		ApplyJitter(Points, JitterStrength, MaxRadius, RandomStream);
	}

	return Points;
}

TArray<FVector> FCircleSamplingHelper::GenerateCircularGrid(
	int32 PointCount,
	float MaxRadius,
	int32 RadialDivisions,
	int32 AngularDivisions,
	float JitterStrength,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0 || MaxRadius <= 0.0f || RadialDivisions <= 0 || AngularDivisions <= 0)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 计算每层的半径步长
	const float RadialStep = MaxRadius / RadialDivisions;
	const float AngularStep = 360.0f / AngularDivisions;

	int32 GeneratedCount = 0;

	// 生成极坐标网格
	for (int32 Radial = 0; Radial < RadialDivisions && GeneratedCount < PointCount; ++Radial)
	{
		float Radius = (Radial + 1) * RadialStep; // 从内向外

		for (int32 Angular = 0; Angular < AngularDivisions && GeneratedCount < PointCount; ++Angular)
		{
			const float Angle = Angular * AngularStep;
			// 极坐标转换为笛卡尔坐标
			Points.Add(FormationSamplingInternal::PolarToCartesian(Radius, FMath::DegreesToRadians(Angle)));
			GeneratedCount++;
		}
	}

	// 应用扰动
	if (JitterStrength > 0.0f)
	{
		ApplyJitter(Points, JitterStrength, MaxRadius, RandomStream);
	}

	return Points;
}

TArray<FVector> FCircleSamplingHelper::GenerateRoseCurve(
	int32 PointCount,
	float MaxRadius,
	int32 Petals,
	float JitterStrength,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0 || MaxRadius <= 0.0f || Petals <= 0)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 玫瑰曲线：r = a * cos(k * θ)
	// k = 花瓣数，a = 最大半径
	const float K = static_cast<float>(Petals);
	const float A = MaxRadius;

	// 生成玫瑰曲线上的点
	for (int32 i = 0; i < PointCount; ++i)
	{
		// 参数t从0到2π
		float T = (static_cast<float>(i) / (PointCount - 1)) * 2.0f * PI;

		// 计算半径
		float Radius = A * FMath::Cos(K * T);

		// 极坐标转换为笛卡尔坐标
		FVector Point = FormationSamplingInternal::PolarToCartesian(Radius, T);

		Points.Add(Point);
	}

	// 应用扰动
	if (JitterStrength > 0.0f)
	{
		ApplyJitter(Points, JitterStrength, MaxRadius, RandomStream);
	}

	return Points;
}

TArray<FVector> FCircleSamplingHelper::GenerateConcentricRings(
	int32 PointCount,
	float MaxRadius,
	int32 RingCount,
	const TArray<int32>& PointsPerRing,
	float JitterStrength,
	FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (PointCount <= 0 || MaxRadius <= 0.0f || RingCount <= 0)
	{
		return Points;
	}

	Points.Reserve(PointCount);

	// 计算每层的半径
	const float RadialStep = MaxRadius / RingCount;

	int32 GeneratedCount = 0;

	for (int32 Ring = 0; Ring < RingCount && GeneratedCount < PointCount; ++Ring)
	{
		float Radius = (Ring + 1) * RadialStep;

		// 获取该层的点数
		int32 PointsInRing = (Ring < PointsPerRing.Num()) ? PointsPerRing[Ring] : 8; // 默认8个点

		// 确保至少有点
		PointsInRing = FMath::Max(1, PointsInRing);

		for (int32 Point = 0; Point < PointsInRing && GeneratedCount < PointCount; ++Point)
		{
			// 计算角度
			float Angle = (360.0f * Point) / PointsInRing;

			// 极坐标转换为笛卡尔坐标
			FVector Position = FormationSamplingInternal::PolarToCartesian(Radius, FMath::DegreesToRadians(Angle));

			Points.Add(Position);
			GeneratedCount++;
		}
	}

	// 应用扰动
	if (JitterStrength > 0.0f)
	{
		ApplyJitter(Points, JitterStrength, MaxRadius, RandomStream);
	}

	return Points;
}
