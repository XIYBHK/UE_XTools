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
#include "Components/SplineComponent.h"

// ============================================================================
// 辅助函数：坐标变换
// ============================================================================

namespace FormationSamplingInternal
{
	/**
	 * 将局部坐标转换到目标坐标空间
	 */
	static TArray<FVector> TransformPoints(
		const TArray<FVector>& LocalPoints,
		const FVector& CenterLocation,
		const FRotator& Rotation,
		EPoissonCoordinateSpace CoordinateSpace)
	{
		TArray<FVector> TransformedPoints;
		TransformedPoints.Reserve(LocalPoints.Num());

		FTransform Transform(Rotation, CenterLocation);

		switch (CoordinateSpace)
		{
		case EPoissonCoordinateSpace::World:
			// 世界空间：应用位置和旋转
			for (const FVector& LocalPoint : LocalPoints)
			{
				TransformedPoints.Add(Transform.TransformPosition(LocalPoint));
			}
			break;

		case EPoissonCoordinateSpace::Local:
			// 局部空间：仅应用位置偏移，不旋转
			for (const FVector& LocalPoint : LocalPoints)
			{
				TransformedPoints.Add(LocalPoint + CenterLocation);
			}
			break;

		case EPoissonCoordinateSpace::Raw:
		default:
			// 原始空间：直接返回局部坐标
			TransformedPoints = LocalPoints;
			break;
		}

		return TransformedPoints;
	}

	/**
	 * 应用位置扰动
	 * @param Points 点位数组（会被修改）
	 * @param JitterStrength 扰动强度 (0-1)
	 * @param Scale 扰动缩放系数（通常是Spacing或Radius）
	 * @param RandomStream 随机流
	 */
	static void ApplyJitter(
		TArray<FVector>& Points,
		float JitterStrength,
		float Scale,
		FRandomStream& RandomStream)
	{
		if (JitterStrength > 0.0f && Points.Num() > 0)
		{
			const float JitterRange = JitterStrength * Scale;
			for (FVector& Point : Points)
			{
				Point.X += RandomStream.FRandRange(-JitterRange, JitterRange);
				Point.Y += RandomStream.FRandRange(-JitterRange, JitterRange);
			}
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
	// 创建随机流
	FRandomStream RandomStream(RandomSeed);

	// 生成局部坐标点
	TArray<FVector> LocalPoints = FRectangleSamplingHelper::GenerateSolidRectangle(
		PointCount, Spacing, RowCount, ColumnCount, JitterStrength, RandomStream
	);

	// 应用高度参数（在Z轴上分布）
	if (Height > 1.0f && LocalPoints.Num() > 0)
	{
		float HalfHeight = Height * 0.5f;
		for (FVector& Point : LocalPoints)
		{
			// 在[-HalfHeight, HalfHeight]范围内随机分布Z坐标
			Point.Z = RandomStream.FRandRange(-HalfHeight, HalfHeight);
		}
	}

	// 转换到目标坐标空间
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

	// 应用高度参数（在Z轴上分布）
	if (Height > 1.0f && LocalPoints.Num() > 0)
	{
		float HalfHeight = Height * 0.5f;
		for (FVector& Point : LocalPoints)
		{
			// 在[-HalfHeight, HalfHeight]范围内随机分布Z坐标
			Point.Z = RandomStream.FRandRange(-HalfHeight, HalfHeight);
		}
	}

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

	// 应用高度参数（在Z轴上分布）
	if (Height > 1.0f && LocalPoints.Num() > 0)
	{
		float HalfHeight = Height * 0.5f;
		for (FVector& Point : LocalPoints)
		{
			// 在[-HalfHeight, HalfHeight]范围内随机分布Z坐标
			Point.Z = RandomStream.FRandRange(-HalfHeight, HalfHeight);
		}
	}

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

	// 验证样条组件有效性
	if (!SplineComponent)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[样条线采样] 样条组件指针为空"));
		return Points;
	}

	// 从样条组件提取控制点（世界坐标）
	TArray<FVector> SplineControlPoints;
	int32 NumPoints = SplineComponent->GetNumberOfSplinePoints();
	SplineControlPoints.Reserve(NumPoints);

	for (int32 i = 0; i < NumPoints; ++i)
	{
		SplineControlPoints.Add(SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World));
	}

	if (SplineControlPoints.Num() < 2)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[样条线采样] 样条组件至少需要2个控制点"));
		return Points;
	}

	// 样条线采样：控制点已经是世界坐标
	Points = FSplineSamplingHelper::GenerateAlongSpline(
		PointCount, SplineControlPoints, bClosedSpline
	);

	// 根据坐标空间类型处理
	// 由于样条线控制点通常已经是世界坐标，这里主要处理 Raw 和 Local 的情况
	if (CoordinateSpace == EPoissonCoordinateSpace::Raw && Points.Num() > 0)
	{
		// Raw 空间：将点位转换为相对于第一个控制点的局部坐标
		if (SplineControlPoints.Num() > 0)
		{
			FVector Origin = SplineControlPoints[0];
			for (FVector& Point : Points)
			{
				Point -= Origin;
			}
		}
	}
	else if (CoordinateSpace == EPoissonCoordinateSpace::Local && Points.Num() > 0)
	{
		// Local 空间：保持相对关系，转换为相对于质心的坐标
		FVector Centroid = FVector::ZeroVector;
		for (const FVector& Point : Points)
		{
			Centroid += Point;
		}
		Centroid /= Points.Num();

		for (FVector& Point : Points)
		{
			Point -= Centroid;
		}
	}
	// World 空间：直接返回原始点位

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

	// 验证样条组件有效性
	if (!SplineComponent)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[样条线边界采样] 样条组件指针为空"));
		return Points;
	}

	// 从样条组件提取控制点（世界坐标）
	TArray<FVector> SplineControlPoints;
	int32 NumPoints = SplineComponent->GetNumberOfSplinePoints();
	SplineControlPoints.Reserve(NumPoints);

	for (int32 i = 0; i < NumPoints; ++i)
	{
		SplineControlPoints.Add(SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World));
	}

	if (SplineControlPoints.Num() < 3)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[样条线边界采样] 样条组件至少需要3个控制点形成封闭区域"));
		return Points;
	}

	FRandomStream RandomStream(RandomSeed);

	// 调用helper生成边界内的泊松采样点
	Points = FSplineSamplingHelper::GenerateWithinBoundary(
		TargetPointCount, SplineControlPoints, MinDistance, RandomStream
	);

	// 根据坐标空间类型处理
	// 样条线控制点通常已经是世界坐标
	if (CoordinateSpace == EPoissonCoordinateSpace::Raw && Points.Num() > 0)
	{
		// Raw 空间：相对于第一个控制点
		if (SplineControlPoints.Num() > 0)
		{
			FVector Origin = SplineControlPoints[0];
			for (FVector& Point : Points)
			{
				Point -= Origin;
			}
		}
	}
	else if (CoordinateSpace == EPoissonCoordinateSpace::Local && Points.Num() > 0)
	{
		// Local 空间：相对于质心
		FVector Centroid = FVector::ZeroVector;
		for (const FVector& Point : Points)
		{
			Centroid += Point;
		}
		Centroid /= Points.Num();

		for (FVector& Point : Points)
		{
			Point -= Centroid;
		}
	}
	// World 空间：直接返回

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
	EPoissonCoordinateSpace CoordinateSpace)
{
	// 从静态网格体提取顶点
	TArray<FVector> Points = FMeshSamplingHelper::GenerateFromStaticMesh(
		StaticMesh, Transform, LODLevel, bBoundaryVerticesOnly, MaxPoints
	);

	// 根据坐标空间类型处理
	if (CoordinateSpace == EPoissonCoordinateSpace::Raw && Points.Num() > 0)
	{
		// Raw 空间：转换为相对于变换原点的坐标
		FVector Origin = Transform.GetLocation();
		for (FVector& Point : Points)
		{
			Point -= Origin;
		}
	}
	else if (CoordinateSpace == EPoissonCoordinateSpace::Local && Points.Num() > 0)
	{
		// Local 空间：转换为相对于质心的坐标
		FVector Centroid = FVector::ZeroVector;
		for (const FVector& Point : Points)
		{
			Centroid += Point;
		}
		Centroid /= Points.Num();

		for (FVector& Point : Points)
		{
			Point -= Centroid;
		}
	}
	// World 空间：保持原始坐标

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

	// 检查平台数据
	FTexturePlatformData* PlatformData = Texture->GetPlatformData();
	if (!PlatformData || PlatformData->Mips.Num() == 0)
	{
		UE_LOG(LogPointSampling, Error, TEXT("[纹理验证] 纹理平台数据无效"));
		return false;
	}

	// 获取纹理信息
	int32 Width = Texture->GetSizeX();
	int32 Height = Texture->GetSizeY();
	EPixelFormat PixelFormat = PlatformData->PixelFormat;

	UE_LOG(LogPointSampling, Display, TEXT("========================================"));
	UE_LOG(LogPointSampling, Display, TEXT("[纹理验证] 纹理: %s"), *Texture->GetName());
	UE_LOG(LogPointSampling, Display, TEXT("[纹理验证] 尺寸: %dx%d"), Width, Height);
	UE_LOG(LogPointSampling, Display, TEXT("[纹理验证] 像素格式: %s (%d)"), GetPixelFormatString(PixelFormat), (int32)PixelFormat);
	UE_LOG(LogPointSampling, Display, TEXT("[纹理验证] Mip 级别数: %d"), PlatformData->Mips.Num());

	// 检查纹理压缩设置
	TextureCompressionSettings CompressionSettings = Texture->CompressionSettings;
	UE_LOG(LogPointSampling, Display, TEXT("[纹理验证] 压缩设置: %d"), (int32)CompressionSettings);

	// 检查是否为支持的格式
	bool bIsSupportedFormat = (PixelFormat == PF_B8G8R8A8) ||
	                          (PixelFormat == PF_R8G8B8A8) ||
	                          (PixelFormat == PF_A8R8G8B8) ||
	                          (PixelFormat == PF_FloatRGBA);

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
	float MaxRadius,
	int32 RingCount,
	const TArray<int32>& PointsPerRing,
	EPoissonCoordinateSpace CoordinateSpace,
	float JitterStrength,
	int32 RandomSeed)
{
	FRandomStream RandomStream(RandomSeed);

	TArray<FVector> LocalPoints = FCircleSamplingHelper::GenerateConcentricRings(
		PointCount, MaxRadius, RingCount, PointsPerRing, JitterStrength, RandomStream
	);

	return FormationSamplingInternal::TransformPoints(LocalPoints, CenterLocation, Rotation, CoordinateSpace);
}
