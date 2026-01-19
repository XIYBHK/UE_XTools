/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "FormationSamplingLibrary.h"
#include "Sampling/RectangleSamplingHelper.h"
#include "Sampling/CircleSamplingHelper.h"
#include "Sampling/TriangleSamplingHelper.h"
#include "Sampling/SplineSamplingHelper.h"
#include "Sampling/MeshSamplingHelper.h"
#include "Sampling/TextureSamplingHelper.h"
#include "Sampling/MilitaryFormationHelper.h"
#include "Sampling/GeometricFormationHelper.h"
#include "Sampling/PointDeduplicationHelper.h"
#include "Sampling/FormationSamplingInternal.h"
#include "Components/SplineComponent.h"
#include "Algo/AnyOf.h"

// ============================================================================
// 辅助函数：坐标变换
// ============================================================================

namespace FormationSamplingInternal
{
	TArray<FVector> TransformPoints(
		const TArray<FVector>& LocalPoints,
		const FVector& CenterLocation,
		const FRotator& Rotation,
		EPoissonCoordinateSpace CoordinateSpace)
	{
		TArray<FVector> TransformedPoints;
		TransformedPoints.Reserve(LocalPoints.Num());

		const FTransform Transform(Rotation, CenterLocation);

		switch (CoordinateSpace)
		{
		case EPoissonCoordinateSpace::World:
			for (const FVector& LocalPoint : LocalPoints)
			{
				TransformedPoints.Add(Transform.TransformPosition(LocalPoint));
			}
			break;

		case EPoissonCoordinateSpace::Local:
			for (const FVector& LocalPoint : LocalPoints)
			{
				TransformedPoints.Add(LocalPoint + CenterLocation);
			}
			break;

		case EPoissonCoordinateSpace::Raw:
		default:
			TransformedPoints = LocalPoints;
			break;
		}

		return TransformedPoints;
	}

	void ApplyJitter(
		TArray<FVector>& Points,
		float JitterStrength,
		float Scale,
		FRandomStream& RandomStream)
	{
		if (JitterStrength <= 0.0f || Points.Num() == 0)
		{
			return;
		}

		const float JitterRange = JitterStrength * Scale;
		for (FVector& Point : Points)
		{
			Point.X += RandomStream.FRandRange(-JitterRange, JitterRange);
			Point.Y += RandomStream.FRandRange(-JitterRange, JitterRange);
		}
	}

	void ApplyHeightDistribution(
		TArray<FVector>& Points,
		float Height,
		FRandomStream& RandomStream)
	{
		if (Height <= 1.0f || Points.Num() == 0)
		{
			return;
		}

		const float HalfHeight = Height * 0.5f;
		for (FVector& Point : Points)
		{
			Point.Z = RandomStream.FRandRange(-HalfHeight, HalfHeight);
		}
	}

	bool ExtractSplineControlPoints(
		USplineComponent* SplineComponent,
		TArray<FVector>& OutControlPoints,
		int32 MinRequiredPoints,
		const FString& ContextName)
	{
		if (!SplineComponent)
		{
			UE_LOG(LogPointSampling, Warning, TEXT("[%s] 样条组件指针为空"), *ContextName);
			return false;
		}

		const int32 NumPoints = SplineComponent->GetNumberOfSplinePoints();
		if (NumPoints < MinRequiredPoints)
		{
			UE_LOG(LogPointSampling, Warning, TEXT("[%s] 样条组件至少需要%d个控制点"), *ContextName, MinRequiredPoints);
			return false;
		}

		OutControlPoints.Reserve(NumPoints);
		for (int32 i = 0; i < NumPoints; ++i)
		{
			OutControlPoints.Add(SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World));
		}

		return true;
	}

	void ConvertPointsToCoordinateSpace(
		TArray<FVector>& Points,
		EPoissonCoordinateSpace CoordinateSpace,
		const FVector& OriginOffset = FVector::ZeroVector)
	{
		if (Points.Num() == 0)
		{
			return;
		}

		switch (CoordinateSpace)
		{
		case EPoissonCoordinateSpace::Raw:
			for (FVector& Point : Points)
			{
				Point -= OriginOffset;
			}
			break;

		case EPoissonCoordinateSpace::Local:
			{
				const FVector Centroid = FormationSamplingInternal::CalculateCentroid(Points);
				for (FVector& Point : Points)
				{
					Point -= Centroid;
				}
			}
			break;

		case EPoissonCoordinateSpace::World:
		default:
			break;
		}
	}

	FVector CalculateCentroid(const TArray<FVector>& Points)
	{
		if (Points.Num() == 0)
		{
			return FVector::ZeroVector;
		}

		FVector Centroid = FVector::ZeroVector;
		for (const FVector& Point : Points)
		{
			Centroid += Point;
		}
		return Centroid / Points.Num();
	}

	FVector PolarToCartesian(float Radius, float AngleRad, float Z)
	{
		return FVector(FMath::Cos(AngleRad) * Radius, FMath::Sin(AngleRad) * Radius, Z);
	}

	void ApplyJitter2D(
		TArray<FVector>& Points,
		float JitterStrength,
		float Scale,
		FRandomStream& RandomStream)
	{
		if (JitterStrength <= 0.0f || Points.Num() == 0)
		{
			return;
		}

		const float JitterRange = JitterStrength * Scale;
		for (FVector& Point : Points)
		{
			Point.X += RandomStream.FRandRange(-JitterRange, JitterRange);
			Point.Y += RandomStream.FRandRange(-JitterRange, JitterRange);
		}
	}
}

// ============================================================================
// 矩形类阵型实现
// ============================================================================

TArray<FVector> UFormationSamplingLibrary::GenerateSolidRectangle(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float Spacing,
	int32 RowCount,
	int32 ColumnCount,
	float Height,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FRectangleSamplingHelper::GenerateSolidRectangle(
		PointCount, Spacing, RowCount, ColumnCount, JitterStrength, RandomStream
	);

	FormationSamplingInternal::ApplyHeightDistribution(LocalPoints, Height, RandomStream);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

TArray<FVector> UFormationSamplingLibrary::GenerateHollowRectangle(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float Spacing,
	int32 RowCount,
	int32 ColumnCount,
	float Height,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FRectangleSamplingHelper::GenerateHollowRectangle(
		PointCount, Spacing, RowCount, ColumnCount, JitterStrength, RandomStream
	);

	FormationSamplingInternal::ApplyHeightDistribution(LocalPoints, Height, RandomStream);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

TArray<FVector> UFormationSamplingLibrary::GenerateSpiralRectangle(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float Spacing,
	float SpiralTurns,
	float Height,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FRectangleSamplingHelper::GenerateSpiralRectangle(
		PointCount, Spacing, SpiralTurns, JitterStrength, RandomStream
	);

	FormationSamplingInternal::ApplyHeightDistribution(LocalPoints, Height, RandomStream);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

// ============================================================================
// 三角形类阵型实现（占位符）
// ============================================================================

TArray<FVector> UFormationSamplingLibrary::GenerateSolidTriangle(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float Spacing,
	bool bInverted,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FTriangleSamplingHelper::GenerateSolidTriangle(
		PointCount, Spacing, bInverted, JitterStrength, RandomStream
	);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

TArray<FVector> UFormationSamplingLibrary::GenerateHollowTriangle(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float Spacing,
	bool bInverted,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FTriangleSamplingHelper::GenerateHollowTriangle(
		PointCount, Spacing, bInverted, JitterStrength, RandomStream
	);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

// ============================================================================
// 圆形和雪花类阵型实现（占位符）
// ============================================================================

TArray<FVector> UFormationSamplingLibrary::GenerateCircle(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float Radius,
	bool bIs3D,
	ECircleDistributionMode DistributionMode,
	float MinDistance,
	float StartAngle,
	bool bClockwise,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FCircleSamplingHelper::GenerateCircle(
		PointCount, Radius, bIs3D, DistributionMode, MinDistance,
		StartAngle, bClockwise, JitterStrength, RandomStream
	);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

TArray<FVector> UFormationSamplingLibrary::GenerateSnowflake(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float Radius,
	int32 SnowflakeLayers,
	float Spacing,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FCircleSamplingHelper::GenerateSnowflake(
		PointCount, Radius, SnowflakeLayers, Spacing, JitterStrength, RandomStream
	);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

TArray<FVector> UFormationSamplingLibrary::GenerateSnowflakeArc(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float Radius,
	int32 SnowflakeLayers,
	float Spacing,
	float ArcAngle,
	float StartAngle,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FCircleSamplingHelper::GenerateSnowflakeArc(
		PointCount, Radius, SnowflakeLayers, Spacing, ArcAngle, StartAngle, JitterStrength, RandomStream
	);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

// ============================================================================
// 样条线采样实现（占位符）
// ============================================================================

TArray<FVector> UFormationSamplingLibrary::GenerateAlongSpline(
	int32 PointCount,
	USplineComponent* SplineComponent,
	bool bClosedSpline,
	EPoissonCoordinateSpace CoordinateSpace)
{
	TArray<FVector> Points;
	TArray<FVector> SplineControlPoints;

	if (!FormationSamplingInternal::ExtractSplineControlPoints(SplineComponent, SplineControlPoints, 2, TEXT("样条线采样")))
	{
		return Points;
	}

	Points = FSplineSamplingHelper::GenerateAlongSpline(PointCount, SplineControlPoints, bClosedSpline);

	FormationSamplingInternal::ConvertPointsToCoordinateSpace(Points, CoordinateSpace, SplineControlPoints[0]);

	return Points;
}

TArray<FVector> UFormationSamplingLibrary::GenerateSplineBoundary(
	int32 TargetPointCount,
	USplineComponent* SplineComponent,
	float MinDistance,
	EPoissonCoordinateSpace CoordinateSpace,
	int32 RandomSeed)
{
	TArray<FVector> Points;
	TArray<FVector> SplineControlPoints;

	if (!FormationSamplingInternal::ExtractSplineControlPoints(SplineComponent, SplineControlPoints, 3, TEXT("样条线边界采样")))
	{
		return Points;
	}

	FRandomStream RandomStream(RandomSeed);
	Points = FSplineSamplingHelper::GenerateWithinBoundary(
		TargetPointCount, SplineControlPoints, MinDistance, RandomStream
	);

	FormationSamplingInternal::ConvertPointsToCoordinateSpace(Points, CoordinateSpace, SplineControlPoints[0]);

	return Points;
}

// ============================================================================
// 网格采样实现（占位符）
// ============================================================================

TArray<FVector> UFormationSamplingLibrary::GenerateFromStaticMesh(
	UStaticMesh* StaticMesh,
	FTransform Transform,
	int32 MaxPoints,
	int32 LODLevel,
	bool bBoundaryVerticesOnly,
	float DeduplicationRadius,
	bool bGridAlignedDedup,
	EPoissonCoordinateSpace CoordinateSpace)
{
	TArray<FVector> Points = FMeshSamplingHelper::GenerateFromStaticMesh(
		StaticMesh, Transform, LODLevel, bBoundaryVerticesOnly, MaxPoints
	);

	if (DeduplicationRadius > 0.0f && Points.Num() > 1)
	{
		int32 OriginalCount, RemovedCount;

		if (bGridAlignedDedup)
		{
			FPointDeduplicationHelper::RemoveDuplicatePointsGridAligned(
				Points, DeduplicationRadius, OriginalCount, RemovedCount);
		}
		else
		{
			FPointDeduplicationHelper::RemoveDuplicatePointsWithStats(
				Points, DeduplicationRadius, OriginalCount, RemovedCount);
		}

		if (RemovedCount > 0)
		{
			UE_LOG(LogPointSampling, Log,
				TEXT("[网格采样] 去重(%s): %d -> %d (移除 %d, 半径=%.1f)"),
				bGridAlignedDedup ? TEXT("网格对齐") : TEXT("距离过滤"),
				OriginalCount, Points.Num(), RemovedCount, DeduplicationRadius);
		}
	}

	FormationSamplingInternal::ConvertPointsToCoordinateSpace(Points, CoordinateSpace, Transform.GetLocation());

	return Points;
}

#if WITH_EDITOR
bool UFormationSamplingLibrary::ValidateTextureForSampling(UTexture2D* Texture)
{
	if (!Texture)
	{
		UE_LOG(LogPointSampling, Error, TEXT("[纹理验证] 纹理指针为空"));
		return false;
	}

	FTexturePlatformData* PlatformData = Texture->GetPlatformData();
	if (!PlatformData || PlatformData->Mips.Num() == 0)
	{
		UE_LOG(LogPointSampling, Error, TEXT("[纹理验证] 纹理平台数据无效"));
		return false;
	}

	const int32 Width = Texture->GetSizeX();
	const int32 Height = Texture->GetSizeY();
	const EPixelFormat PixelFormat = PlatformData->PixelFormat;

	UE_LOG(LogPointSampling, Display, TEXT("========================================"));
	UE_LOG(LogPointSampling, Display, TEXT("[纹理验证] 纹理: %s"), *Texture->GetName());
	UE_LOG(LogPointSampling, Display, TEXT("[纹理验证] 尺寸: %dx%d"), Width, Height);
	UE_LOG(LogPointSampling, Display, TEXT("[纹理验证] 像素格式: %s (%d)"), GetPixelFormatString(PixelFormat), static_cast<int32>(PixelFormat));
	UE_LOG(LogPointSampling, Display, TEXT("[纹理验证] Mip 级别数: %d"), PlatformData->Mips.Num());
	UE_LOG(LogPointSampling, Display, TEXT("[纹理验证] 压缩设置: %d"), static_cast<int32>(Texture->CompressionSettings));

	constexpr EPixelFormat SupportedFormats[] = { PF_B8G8R8A8, PF_R8G8B8A8, PF_A8R8G8B8, PF_FloatRGBA };
	const bool bIsSupportedFormat = Algo::AnyOf(SupportedFormats, [&](EPixelFormat Format) { return PixelFormat == Format; });

	if (!bIsSupportedFormat)
	{
		UE_LOG(LogPointSampling, Error, TEXT("[纹理验证] 纹理格式不支持！"));
		UE_LOG(LogPointSampling, Error, TEXT("========================================"));
		UE_LOG(LogPointSampling, Error, TEXT("请在纹理资产中进行以下设置："));
		UE_LOG(LogPointSampling, Error, TEXT("  1. Compression Settings -> VectorDisplacementmap (RGBA8)"));
		UE_LOG(LogPointSampling, Error, TEXT("  2. Mip Gen Settings -> NoMipmaps"));
		UE_LOG(LogPointSampling, Error, TEXT("  3. sRGB -> 取消勾选"));
		UE_LOG(LogPointSampling, Error, TEXT("  4. 点击 'Save' 保存纹理"));
		UE_LOG(LogPointSampling, Error, TEXT("========================================"));
		return false;
	}

	UE_LOG(LogPointSampling, Display, TEXT("[纹理验证] 纹理设置正确，可以用于点采样"));
	UE_LOG(LogPointSampling, Display, TEXT("========================================"));
	return true;
}
#endif // WITH_EDITOR

// ============================================================================
// 军事阵型实现
// ============================================================================

TArray<FVector> UFormationSamplingLibrary::GenerateWedgeFormation(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float Spacing,
	float WedgeAngle,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FMilitaryFormationHelper::GenerateWedgeFormation(
		PointCount, Spacing, WedgeAngle
	);

	FormationSamplingInternal::ApplyJitter(LocalPoints, JitterStrength, Spacing, RandomStream);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

TArray<FVector> UFormationSamplingLibrary::GenerateColumnFormation(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float Spacing,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FMilitaryFormationHelper::GenerateColumnFormation(
		PointCount, Spacing
	);

	FormationSamplingInternal::ApplyJitter(LocalPoints, JitterStrength, Spacing, RandomStream);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

TArray<FVector> UFormationSamplingLibrary::GenerateLineFormation(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float Spacing,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FMilitaryFormationHelper::GenerateLineFormation(
		PointCount, Spacing
	);

	FormationSamplingInternal::ApplyJitter(LocalPoints, JitterStrength, Spacing, RandomStream);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

TArray<FVector> UFormationSamplingLibrary::GenerateVeeFormation(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float Spacing,
	float VeeAngle,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FMilitaryFormationHelper::GenerateVeeFormation(
		PointCount, Spacing, VeeAngle
	);

	FormationSamplingInternal::ApplyJitter(LocalPoints, JitterStrength, Spacing, RandomStream);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

TArray<FVector> UFormationSamplingLibrary::GenerateEchelonFormation(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float Spacing,
	int32 Direction,
	float EchelonAngle,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FMilitaryFormationHelper::GenerateEchelonFormation(
		PointCount, Spacing, Direction, EchelonAngle
	);

	FormationSamplingInternal::ApplyJitter(LocalPoints, JitterStrength, Spacing, RandomStream);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

// ============================================================================
// 几何阵型实现
// ============================================================================

TArray<FVector> UFormationSamplingLibrary::GenerateHexagonalGrid(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float Spacing,
	int32 Rings,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FGeometricFormationHelper::GenerateHexagonalGrid(
		PointCount, Spacing, Rings
	);

	FormationSamplingInternal::ApplyJitter(LocalPoints, JitterStrength, Spacing, RandomStream);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

TArray<FVector> UFormationSamplingLibrary::GenerateStarFormation(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float OuterRadius,
	float InnerRadius,
	int32 PointsCount,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FGeometricFormationHelper::GenerateStarFormation(
		PointCount, OuterRadius, InnerRadius, PointsCount
	);

	FormationSamplingInternal::ApplyJitter(LocalPoints, JitterStrength, OuterRadius * 0.1f, RandomStream);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

TArray<FVector> UFormationSamplingLibrary::GenerateArchimedeanSpiral(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float Spacing,
	float Turns,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FGeometricFormationHelper::GenerateArchimedeanSpiral(
		PointCount, Spacing, Turns
	);

	FormationSamplingInternal::ApplyJitter(LocalPoints, JitterStrength, Spacing, RandomStream);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

TArray<FVector> UFormationSamplingLibrary::GenerateLogarithmicSpiral(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float GrowthFactor,
	float AngleStep,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FGeometricFormationHelper::GenerateLogarithmicSpiral(
		PointCount, GrowthFactor, AngleStep
	);

	FormationSamplingInternal::ApplyJitter(LocalPoints, JitterStrength, 10.0f, RandomStream);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

TArray<FVector> UFormationSamplingLibrary::GenerateHeartFormation(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float Size,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FGeometricFormationHelper::GenerateHeartFormation(
		PointCount, Size
	);

	FormationSamplingInternal::ApplyJitter(LocalPoints, JitterStrength, Size * 0.1f, RandomStream);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

TArray<FVector> UFormationSamplingLibrary::GenerateFlowerFormation(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float OuterRadius,
	float InnerRadius,
	int32 PetalCount,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FGeometricFormationHelper::GenerateFlowerFormation(
		PointCount, OuterRadius, InnerRadius, PetalCount
	);

	FormationSamplingInternal::ApplyJitter(LocalPoints, JitterStrength, OuterRadius * 0.1f, RandomStream);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

// ============================================================================
// 高级圆形阵型实现
// ============================================================================

TArray<FVector> UFormationSamplingLibrary::GenerateGoldenSpiralFormation(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float MaxRadius,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FCircleSamplingHelper::GenerateGoldenSpiral(
		PointCount, MaxRadius, JitterStrength, RandomStream
	);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

TArray<FVector> UFormationSamplingLibrary::GenerateCircularGridFormation(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float MaxRadius,
	int32 RadialDivisions,
	int32 AngularDivisions,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FCircleSamplingHelper::GenerateCircularGrid(
		PointCount, MaxRadius, RadialDivisions, AngularDivisions, JitterStrength, RandomStream
	);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

TArray<FVector> UFormationSamplingLibrary::GenerateRoseCurveFormation(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	float MaxRadius,
	int32 Petals,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FCircleSamplingHelper::GenerateRoseCurve(
		PointCount, MaxRadius, Petals, JitterStrength, RandomStream
	);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}

TArray<FVector> UFormationSamplingLibrary::GenerateConcentricRingsFormation(
	int32 PointCount,
	FVector CenterLocation,
	FRotator Rotation,
	const TArray<int32>& PointsPerRing,
	float MaxRadius,
	int32 RingCount,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	// 如果 PointsPerRing 为空，使用默认值 {6, 12, 18, 24}
	const TArray<int32>& ActualPointsPerRing = PointsPerRing.Num() > 0
		? PointsPerRing
		: TArray<int32>{6, 12, 18, 24};

	TArray<FVector> LocalPoints = FCircleSamplingHelper::GenerateConcentricRings(
		PointCount, MaxRadius, RingCount, ActualPointsPerRing, JitterStrength, RandomStream
	);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}
