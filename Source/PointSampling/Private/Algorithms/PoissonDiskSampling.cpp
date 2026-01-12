/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#include "Algorithms/PoissonDiskSampling.h"
#include "Algorithms/PoissonSamplingHelpers.h"
#include "Core/SamplingCache.h"
#include "Components/BoxComponent.h"
#include "PointSamplingTypes.h"

using namespace PoissonSamplingHelpers;

// ============================================================================
// 基础2D/3D采样（委托给内部实现）
// ============================================================================

TArray<FVector2D> FPoissonDiskSampling::GeneratePoisson2D(
	float Width,
	float Height,
	float Radius,
	int32 MaxAttempts)
{
	TArray<FVector2D> Points = GenerateOptimizedPoisson2D(Width, Height, Radius, MaxAttempts, nullptr);

	UE_LOG(LogPointSampling, Verbose, TEXT("GeneratePoisson2D: 生成了 %d 个点 (区域: %.1fx%.1f, 半径: %.1f)"),
		Points.Num(), Width, Height, Radius);

	return Points;
}

TArray<FVector2D> FPoissonDiskSampling::GeneratePoisson2DFromStream(
	const FRandomStream& RandomStream,
	float Width,
	float Height,
	float Radius,
	int32 MaxAttempts)
{
	TArray<FVector2D> Points = GenerateOptimizedPoisson2D(Width, Height, Radius, MaxAttempts, &RandomStream);

	UE_LOG(LogPointSampling, Verbose, TEXT("GeneratePoisson2DFromStream: 生成了 %d 个点 (区域: %.1fx%.1f, 半径: %.1f)"),
		Points.Num(), Width, Height, Radius);

	return Points;
}

TArray<FVector> FPoissonDiskSampling::GeneratePoisson3D(
	float Width,
	float Height,
	float Depth,
	float Radius,
	int32 MaxAttempts)
{
	TArray<FVector> Points = GenerateOptimizedPoisson3D(Width, Height, Depth, Radius, MaxAttempts, nullptr);

	UE_LOG(LogPointSampling, Verbose, TEXT("GeneratePoisson3D: 生成了 %d 个点 (区域: %.1fx%.1fx%.1f, 半径: %.1f)"),
		Points.Num(), Width, Height, Depth, Radius);

	return Points;
}

TArray<FVector> FPoissonDiskSampling::GeneratePoisson3DFromStream(
	const FRandomStream& RandomStream,
	float Width,
	float Height,
	float Depth,
	float Radius,
	int32 MaxAttempts)
{
	TArray<FVector> Points = GenerateOptimizedPoisson3D(Width, Height, Depth, Radius, MaxAttempts, &RandomStream);

	UE_LOG(LogPointSampling, Verbose, TEXT("GeneratePoisson3DFromStream: 生成了 %d 个点 (区域: %.1fx%.1fx%.1f, 半径: %.1f)"),
		Points.Num(), Width, Height, Depth, Radius);

	return Points;
}

// ============================================================================
// Box组件采样（原XToolsLibrary实现的直接复制）
// ============================================================================

TArray<FVector> FPoissonDiskSampling::GeneratePoissonInBox(
	const UBoxComponent* BoxComponent,
	float Radius,
	int32 MaxAttempts,
	EPoissonCoordinateSpace CoordinateSpace,
	int32 TargetPointCount,
	float JitterStrength,
	bool bUseCache)
{
	// 输入验证
	if (!BoxComponent)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("GeneratePoissonInBox: 盒体组件无效"));
		return TArray<FVector>();
	}

	if (MaxAttempts <= 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("GeneratePoissonInBox: MaxAttempts必须大于0"));
		return TArray<FVector>();
	}

	// 采样区域：统一使用ScaledExtent，确保采样范围随Box视觉大小变化
	const FTransform BoxTransform = BoxComponent->GetComponentTransform();
	const FVector BoxExtent = BoxComponent->GetScaledBoxExtent();

	return GeneratePoissonInBoxByVector(
		BoxExtent,
		BoxTransform,
		Radius,
		MaxAttempts,
		CoordinateSpace,
		TargetPointCount,
		JitterStrength,
		bUseCache
	);
}

TArray<FVector> FPoissonDiskSampling::GeneratePoissonInBoxByVector(
	FVector BoxExtent,
	FTransform Transform,
	float Radius,
	int32 MaxAttempts,
	EPoissonCoordinateSpace CoordinateSpace,
	int32 TargetPointCount,
	float JitterStrength,
	bool bUseCache)
{
	// 输入验证
	if (BoxExtent.X <= 0.0f || BoxExtent.Y <= 0.0f || BoxExtent.Z < 0.0f)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("GeneratePoissonInBoxByVector: BoxExtent无效 (%s)"), *BoxExtent.ToString());
		return TArray<FVector>();
	}

	if (MaxAttempts <= 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("GeneratePoissonInBoxByVector: MaxAttempts必须大于0"));
		return TArray<FVector>();
	}

	// BoxExtent是半尺寸，计算完整尺寸
	const float Width = BoxExtent.X * 2.0f;
	const float Height = BoxExtent.Y * 2.0f;
	const float Depth = BoxExtent.Z * 2.0f;

	// 初步检测是否为平面（Z接近0）
	bool bIs2D = FMath::IsNearlyZero(Depth, 1.0f);

	// 处理目标点数模式：自动计算Radius
	float ActualRadius = Radius;
	if (TargetPointCount > 0)
	{
		ActualRadius = CalculateRadiusFromTargetCount(
			TargetPointCount, Width, Height, Depth, bIs2D);
		
		UE_LOG(LogPointSampling, Log, TEXT("GeneratePoissonInBoxByVector: 根据目标点数 %d 计算得出 Radius = %.2f"), 
			TargetPointCount, ActualRadius);
	}

	// 验证最终的Radius
	if (ActualRadius <= 0.0f)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("GeneratePoissonInBoxByVector: 计算出的Radius无效 (%.2f)"), ActualRadius);
		return TArray<FVector>();
	}

	// 重新评估是否使用2D采样：当Depth相对于Radius过小时，3D采样效率极低
	// 因为球壳采样范围[Radius, 2*Radius]会导致大量候选点超出Z边界被拒绝
	if (!bIs2D && Depth < ActualRadius)
	{
		UE_LOG(LogPointSampling, Log, TEXT("GeneratePoissonInBoxByVector: Depth(%.1f) < Radius(%.1f)，自动降级为2D采样"), 
			Depth, ActualRadius);
		bIs2D = true;
	}

	// 缓存系统检查（包含位置和旋转信息，使用传入的BoxExtent）
	if (bUseCache)
	{
		FPoissonCacheKey CacheKey;
		CacheKey.BoxExtent = BoxExtent;
		CacheKey.Position = Transform.GetLocation();  // Position仅在World空间参与缓存比较
		CacheKey.Rotation = Transform.GetRotation();
		CacheKey.Scale = Transform.GetScale3D();
		CacheKey.Radius = ActualRadius;
		CacheKey.TargetPointCount = TargetPointCount;
		CacheKey.MaxAttempts = MaxAttempts;
		CacheKey.JitterStrength = JitterStrength;
		CacheKey.bIs2D = bIs2D;
		CacheKey.CoordinateSpace = CoordinateSpace;
		
		if (TOptional<TArray<FVector>> CachedPoints = FSamplingCache::Get().GetCached(CacheKey))
		{
			UE_LOG(LogPointSampling, Verbose, TEXT("GeneratePoissonInBoxByVector: 使用缓存结果 (%d 个点)"), 
				CachedPoints.GetValue().Num());
			return CachedPoints.GetValue();
		}
	}

	TArray<FVector> Points;

	// 根据是否为平面选择不同的采样方式
	if (bIs2D)
	{
		// 2D平面采样（XY平面）
		TArray<FVector2D> Points2D = GeneratePoisson2D(Width, Height, ActualRadius, MaxAttempts);
		
		// 转换2D点为3D点（Z=0）
		Points.Reserve(Points2D.Num());
		for (const FVector2D& Point2D : Points2D)
		{
			Points.Add(FVector(Point2D.X, Point2D.Y, 0.0f));
		}
		
		UE_LOG(LogPointSampling, Verbose, TEXT("泊松采样: 2D平面 | BoxExtent=(%.1f,%.1f) | 采样空间=[0,%.1f]x[0,%.1f] | 结果范围=[±%.1f,±%.1f]"), 
			BoxExtent.X, BoxExtent.Y, Width, Height, BoxExtent.X, BoxExtent.Y);
	}
	else
	{
		// 3D体积采样
		Points = GeneratePoisson3D(Width, Height, Depth, ActualRadius, MaxAttempts);
		
		UE_LOG(LogPointSampling, Verbose, TEXT("泊松采样: 3D体积 | BoxExtent=(%.1f,%.1f,%.1f) | 采样空间=[0,%.1f]x[0,%.1f]x[0,%.1f] | 结果范围=[±%.1f,±%.1f,±%.1f]"), 
			BoxExtent.X, BoxExtent.Y, BoxExtent.Z, Width, Height, Depth, BoxExtent.X, BoxExtent.Y, BoxExtent.Z);
	}

	// 转换坐标：从 [0, Size] 转换到 [-HalfSize, +HalfSize] （局部空间，中心对齐）
	for (FVector& Point : Points)
	{
		// 转换到局部空间：从[0,Width]转到[-BoxExtent,+BoxExtent]
		Point.X -= BoxExtent.X;
		Point.Y -= BoxExtent.Y;
		if (!bIs2D)
		{
			Point.Z -= BoxExtent.Z;
		}
	}

	// 应用扰动噪波（在调整点数之前）
	ApplyJitter(Points, ActualRadius, JitterStrength, bIs2D);

	// 如果指定了目标点数，智能调整到精确数量（泊松主体 + 分层网格补充）
	if (TargetPointCount > 0)
	{
		const FVector BoxSize(Width, Height, Depth);
		AdjustToTargetCount(Points, TargetPointCount, BoxSize, ActualRadius, bIs2D);
	}

	// 应用变换（根据坐标空间类型）
	ApplyTransform(Points, Transform, CoordinateSpace);
	
	// 日志输出
	const TCHAR* SpaceTypeName = 
		(CoordinateSpace == EPoissonCoordinateSpace::World) ? TEXT("世界空间") :
		(CoordinateSpace == EPoissonCoordinateSpace::Local) ? TEXT("局部空间") : TEXT("原始空间");
	
	UE_LOG(LogPointSampling, Verbose, TEXT("泊松采样完成: %d个点 | Radius=%.2f | 坐标=%s"), 
		Points.Num(), ActualRadius, SpaceTypeName);

	// 存入缓存（包含位置和旋转信息）
	if (bUseCache)
	{
		FPoissonCacheKey CacheKey;
		CacheKey.BoxExtent = BoxExtent;
		CacheKey.Position = Transform.GetLocation();  // Position仅在World空间参与缓存比较
		CacheKey.Rotation = Transform.GetRotation();
		CacheKey.Scale = Transform.GetScale3D();
		CacheKey.Radius = ActualRadius;
		CacheKey.TargetPointCount = TargetPointCount;
		CacheKey.MaxAttempts = MaxAttempts;
		CacheKey.JitterStrength = JitterStrength;
		CacheKey.bIs2D = bIs2D;
		CacheKey.CoordinateSpace = CoordinateSpace;
		
		FSamplingCache::Get().Store(CacheKey, Points);
	}

	return Points;
}

// ============================================================================
// FromStream版本（流送，原XToolsLibrary实现的直接复制）
// ============================================================================

TArray<FVector> FPoissonDiskSampling::GeneratePoissonInBoxFromStream(
	const FRandomStream& RandomStream,
	const UBoxComponent* BoxComponent,
	float Radius,
	int32 MaxAttempts,
	EPoissonCoordinateSpace CoordinateSpace,
	int32 TargetPointCount,
	float JitterStrength)
{
	if (!BoxComponent)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("GeneratePoissonInBoxFromStream: BoundingBox is nullptr"));
		return TArray<FVector>();
	}

	if (MaxAttempts <= 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("GeneratePoissonInBoxFromStream: MaxAttempts必须大于0"));
		return TArray<FVector>();
	}

	// 采样区域：统一使用ScaledExtent，确保采样范围随Box视觉大小变化
	const FTransform BoxTransform = BoxComponent->GetComponentTransform();
	const FVector BoxExtent = BoxComponent->GetScaledBoxExtent();
	
	// 调用向量版本
	return GeneratePoissonInBoxByVectorFromStream(
		RandomStream,
		BoxExtent,
		BoxTransform,
		Radius,
		MaxAttempts,
		CoordinateSpace,
		TargetPointCount,
		JitterStrength
	);
}

TArray<FVector> FPoissonDiskSampling::GeneratePoissonInBoxByVectorFromStream(
	const FRandomStream& RandomStream,
	FVector BoxExtent,
	FTransform Transform,
	float Radius,
	int32 MaxAttempts,
	EPoissonCoordinateSpace CoordinateSpace,
	int32 TargetPointCount,
	float JitterStrength)
{
	// 输入验证
	if (BoxExtent.X <= 0.0f || BoxExtent.Y <= 0.0f || BoxExtent.Z < 0.0f)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("GeneratePoissonInBoxByVectorFromStream: BoxExtent无效 (%s)"), *BoxExtent.ToString());
		return TArray<FVector>();
	}

	if (MaxAttempts <= 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("GeneratePoissonInBoxByVectorFromStream: MaxAttempts必须大于0"));
		return TArray<FVector>();
	}

	const float Width = BoxExtent.X * 2.0f;
	const float Height = BoxExtent.Y * 2.0f;
	const float Depth = BoxExtent.Z * 2.0f;
	bool bIs2D = FMath::IsNearlyZero(Depth, 1.0f);

	// 计算实际Radius（与非Stream版本行为一致：TargetPointCount优先）
	float ActualRadius = Radius;
	if (TargetPointCount > 0)
	{
		ActualRadius = CalculateRadiusFromTargetCount(TargetPointCount, Width, Height, Depth, bIs2D);
	}

	// 验证最终的Radius
	if (ActualRadius <= 0.0f)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("GeneratePoissonInBoxByVectorFromStream: 计算出的Radius无效 (%.2f)，请指定有效的Radius或TargetPointCount"), ActualRadius);
		return TArray<FVector>();
	}

	// 重新评估是否使用2D采样：当Depth相对于Radius过小时，3D采样效率极低
	if (!bIs2D && Depth < ActualRadius)
	{
		UE_LOG(LogPointSampling, Log, TEXT("GeneratePoissonInBoxByVectorFromStream: Depth(%.1f) < Radius(%.1f)，自动降级为2D采样"), 
			Depth, ActualRadius);
		bIs2D = true;
	}

	// 生成点（使用统一的内部实现，传入RandomStream）
	TArray<FVector> Points;
	
	if (bIs2D)
	{
		// 2D采样（使用内部函数）
		TArray<FVector2D> Points2D = GeneratePoisson2DInternal(Width, Height, ActualRadius, MaxAttempts, &RandomStream);
		
		// 转换为3D并偏移到局部空间
		Points.Reserve(Points2D.Num());
		for (const FVector2D& Point2D : Points2D)
		{
			Points.Add(FVector(Point2D.X - BoxExtent.X, Point2D.Y - BoxExtent.Y, 0.0f));
		}
	}
	else
	{
		// 3D采样（使用内部函数）
		Points = GeneratePoisson3DInternal(Width, Height, Depth, ActualRadius, MaxAttempts, &RandomStream);
		
		// 转换坐标到局部空间
		for (FVector& Point : Points)
		{
			Point -= BoxExtent;
		}
	}

	// 应用扰动噪波（使用RandomStream，在调整点数之前）
	ApplyJitter(Points, ActualRadius, JitterStrength, bIs2D, &RandomStream);

	// 如果指定了目标点数，智能调整到精确数量（泊松主体 + 分层网格补充）
	if (TargetPointCount > 0)
	{
		const FVector BoxSize(Width, Height, Depth);
		AdjustToTargetCount(Points, TargetPointCount, BoxSize, ActualRadius, bIs2D, &RandomStream);
	}

	// 应用变换（根据坐标空间类型）
	ApplyTransform(Points, Transform, CoordinateSpace);

	return Points;
}

// ============================================================================
// 缓存管理
// ============================================================================

void FPoissonDiskSampling::ClearCache()
{
	FSamplingCache::Get().ClearCache();
	UE_LOG(LogPointSampling, Log, TEXT("泊松采样缓存已清空"));
}

void FPoissonDiskSampling::GetCacheStats(int32& OutHits, int32& OutMisses)
{
	FSamplingCache::Get().GetStats(OutHits, OutMisses);
}
