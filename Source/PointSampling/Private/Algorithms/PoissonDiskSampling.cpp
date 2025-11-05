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
// 基础2D/3D采样（原XToolsLibrary实现的直接复制）
// ============================================================================

TArray<FVector2D> FPoissonDiskSampling::GeneratePoisson2D(
	float Width,
	float Height,
	float Radius,
	int32 MaxAttempts)
{
	// 输入验证
	if (Width <= 0.0f || Height <= 0.0f || Radius <= 0.0f || MaxAttempts <= 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("GeneratePoisson2D: 无效的输入参数"));
		return TArray<FVector2D>();
	}

	TArray<FVector2D> ActivePoints;
	TArray<FVector2D> Points;

	// 计算网格参数
	const float CellSize = Radius / FMath::Sqrt(2.0f);
	const int32 GridWidth = FMath::CeilToInt(Width / CellSize);
	const int32 GridHeight = FMath::CeilToInt(Height / CellSize);

	// 初始化网格
	TArray<FVector2D> Grid;
	Grid.SetNumZeroed(GridWidth * GridHeight);

	// Lambda：获取网格坐标
	auto GetCellCoords = [CellSize](const FVector2D& Point) -> FIntPoint
	{
		return FIntPoint(
			FMath::FloorToInt(Point.X / CellSize),
			FMath::FloorToInt(Point.Y / CellSize)
		);
	};

	// 生成初始点
	const FVector2D InitialPoint(FMath::FRandRange(0.0f, Width), FMath::FRandRange(0.0f, Height));
	ActivePoints.Add(InitialPoint);
	Points.Add(InitialPoint);
	
	const FIntPoint InitialCell = GetCellCoords(InitialPoint);
	Grid[InitialCell.Y * GridWidth + InitialCell.X] = InitialPoint;

	// 主循环
	while (ActivePoints.Num() > 0)
	{
		const int32 Index = FMath::RandRange(0, ActivePoints.Num() - 1);
		const FVector2D Point = ActivePoints[Index];
		bool bFound = false;

		// 尝试在当前点周围生成新点
		for (int32 i = 0; i < MaxAttempts; ++i)
		{
			const FVector2D NewPoint = GenerateRandomPointAround2D(Point, Radius, 2.0f * Radius);
			
			if (IsValidPoint2D(NewPoint, Radius, Width, Height, Grid, GridWidth, GridHeight, CellSize))
			{
				ActivePoints.Add(NewPoint);
				Points.Add(NewPoint);
				
				const FIntPoint NewCell = GetCellCoords(NewPoint);
				Grid[NewCell.Y * GridWidth + NewCell.X] = NewPoint;
				
				bFound = true;
				break;
			}
		}

		// 如果未找到有效点，将当前点标记为不活跃
		if (!bFound)
		{
			ActivePoints.RemoveAt(Index);
		}
	}

	UE_LOG(LogPointSampling, Log, TEXT("GeneratePoisson2D: 生成了 %d 个点 (区域: %.1fx%.1f, 半径: %.1f)"),
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
	// 输入验证
	if (Width <= 0.0f || Height <= 0.0f || Depth <= 0.0f || Radius <= 0.0f || MaxAttempts <= 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("GeneratePoisson3D: 无效的输入参数"));
		return TArray<FVector>();
	}

	TArray<FVector> ActivePoints;
	TArray<FVector> Points;

	// 计算网格参数（3D中使用sqrt(3)而不是sqrt(2)）
	const float CellSize = Radius / FMath::Sqrt(3.0f);
	const int32 GridWidth = FMath::CeilToInt(Width / CellSize);
	const int32 GridHeight = FMath::CeilToInt(Height / CellSize);
	const int32 GridDepth = FMath::CeilToInt(Depth / CellSize);

	// 初始化网格
	TArray<FVector> Grid;
	Grid.SetNumZeroed(GridWidth * GridHeight * GridDepth);

	// Lambda：获取网格坐标
	auto GetCellCoords = [CellSize](const FVector& Point) -> FIntVector
	{
		return FIntVector(
			FMath::FloorToInt(Point.X / CellSize),
			FMath::FloorToInt(Point.Y / CellSize),
			FMath::FloorToInt(Point.Z / CellSize)
		);
	};

	// 生成初始点
	const FVector InitialPoint(
		FMath::FRandRange(0.0f, Width),
		FMath::FRandRange(0.0f, Height),
		FMath::FRandRange(0.0f, Depth)
	);
	ActivePoints.Add(InitialPoint);
	Points.Add(InitialPoint);
	
	const FIntVector InitialCell = GetCellCoords(InitialPoint);
	const int32 InitialIndex = InitialCell.Z * (GridWidth * GridHeight) + 
							   InitialCell.Y * GridWidth + InitialCell.X;
	Grid[InitialIndex] = InitialPoint;

	// 主循环
	while (ActivePoints.Num() > 0)
	{
		const int32 Index = FMath::RandRange(0, ActivePoints.Num() - 1);
		const FVector Point = ActivePoints[Index];
		bool bFound = false;

		// 尝试在当前点周围生成新点
		for (int32 i = 0; i < MaxAttempts; ++i)
		{
			const FVector NewPoint = GenerateRandomPointAround3D(Point, Radius, 2.0f * Radius);
			
			if (IsValidPoint3D(NewPoint, Radius, Width, Height, Depth, 
							  Grid, GridWidth, GridHeight, GridDepth, CellSize))
			{
				ActivePoints.Add(NewPoint);
				Points.Add(NewPoint);
				
				const FIntVector NewCell = GetCellCoords(NewPoint);
				const int32 NewIndex = NewCell.Z * (GridWidth * GridHeight) + 
									  NewCell.Y * GridWidth + NewCell.X;
				Grid[NewIndex] = NewPoint;
				
				bFound = true;
				break;
			}
		}

		// 如果未找到有效点，将当前点标记为不活跃
		if (!bFound)
		{
			ActivePoints.RemoveAt(Index);
		}
	}

	UE_LOG(LogPointSampling, Log, TEXT("GeneratePoisson3D: 生成了 %d 个点 (区域: %.1fx%.1fx%.1f, 半径: %.1f)"),
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

	// 检测是否为平面（Z接近0）
	const bool bIs2D = FMath::IsNearlyZero(Depth, 1.0f);

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

	// 缓存系统检查（包含位置和旋转信息，使用传入的BoxExtent）
	if (bUseCache)
	{
		FPoissonCacheKey CacheKey;
		CacheKey.BoxExtent = BoxExtent;
		CacheKey.Position = Transform.GetLocation();  // Position仅在World空间参与缓存比较
		CacheKey.Rotation = Transform.GetRotation();
		CacheKey.Radius = ActualRadius;
		CacheKey.TargetPointCount = TargetPointCount;
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
		
		UE_LOG(LogPointSampling, Log, TEXT("泊松采样: 2D平面 | BoxExtent=(%.1f,%.1f) | 采样空间=[0,%.1f]x[0,%.1f] | 结果范围=[±%.1f,±%.1f]"), 
			BoxExtent.X, BoxExtent.Y, Width, Height, BoxExtent.X, BoxExtent.Y);
	}
	else
	{
		// 3D体积采样
		Points = GeneratePoisson3D(Width, Height, Depth, ActualRadius, MaxAttempts);
		
		UE_LOG(LogPointSampling, Log, TEXT("泊松采样: 3D体积 | BoxExtent=(%.1f,%.1f,%.1f) | 采样空间=[0,%.1f]x[0,%.1f]x[0,%.1f] | 结果范围=[±%.1f,±%.1f,±%.1f]"), 
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
	ApplyJitter(Points, ActualRadius, JitterStrength);

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
	
	UE_LOG(LogPointSampling, Log, TEXT("泊松采样完成: %d个点 | Radius=%.2f | 坐标=%s"), 
		Points.Num(), ActualRadius, SpaceTypeName);

	// 存入缓存（包含位置和旋转信息）
	if (bUseCache)
	{
		FPoissonCacheKey CacheKey;
		CacheKey.BoxExtent = BoxExtent;
		CacheKey.Position = Transform.GetLocation();  // Position仅在World空间参与缓存比较
		CacheKey.Rotation = Transform.GetRotation();
		CacheKey.Radius = ActualRadius;
		CacheKey.TargetPointCount = TargetPointCount;
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
	const bool bIs2D = FMath::IsNearlyZero(Depth, 1.0f);

	// 计算实际Radius
	float ActualRadius = Radius;
	if (TargetPointCount > 0 && Radius <= 0.0f)
	{
		ActualRadius = CalculateRadiusFromTargetCount(TargetPointCount, Width, Height, Depth, bIs2D);
	}

	// 验证最终的Radius
	if (ActualRadius <= 0.0f)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("GeneratePoissonInBoxByVectorFromStream: 计算出的Radius无效 (%.2f)，请指定有效的Radius或TargetPointCount"), ActualRadius);
		return TArray<FVector>();
	}

	// 生成点（局部坐标，使用Stream替代FMath随机函数）
	TArray<FVector> Points;
	
	if (bIs2D)
	{
		// 2D采样
		TArray<FVector2D> ActivePoints;
		TArray<FVector2D> Points2D;
		
		const float CellSize = ActualRadius / FMath::Sqrt(2.0f);
		const int32 GridWidth = FMath::CeilToInt(Width / CellSize);
		const int32 GridHeight = FMath::CeilToInt(Height / CellSize);
		
		TArray<FVector2D> Grid;
		Grid.SetNumZeroed(GridWidth * GridHeight);
		
		// 初始点（使用RandomStream）
		const FVector2D InitialPoint(RandomStream.FRandRange(0.0f, Width), RandomStream.FRandRange(0.0f, Height));
		ActivePoints.Add(InitialPoint);
		Points2D.Add(InitialPoint);
		
		const int32 InitCellX = FMath::FloorToInt(InitialPoint.X / CellSize);
		const int32 InitCellY = FMath::FloorToInt(InitialPoint.Y / CellSize);
		Grid[InitCellY * GridWidth + InitCellX] = InitialPoint;
		
		// 主循环
		while (ActivePoints.Num() > 0)
		{
			const int32 Index = RandomStream.RandRange(0, ActivePoints.Num() - 1);
			const FVector2D Point = ActivePoints[Index];
			bool bFound = false;
			
			for (int32 i = 0; i < MaxAttempts; ++i)
			{
				//  传递const指针
				const FVector2D NewPoint = GenerateRandomPointAround2D(Point, ActualRadius, 2.0f * ActualRadius, &RandomStream);
				
				if (IsValidPoint2D(NewPoint, ActualRadius, Width, Height, Grid, GridWidth, GridHeight, CellSize))
				{
					ActivePoints.Add(NewPoint);
					Points2D.Add(NewPoint);
					
					const int32 CellX = FMath::FloorToInt(NewPoint.X / CellSize);
					const int32 CellY = FMath::FloorToInt(NewPoint.Y / CellSize);
					Grid[CellY * GridWidth + CellX] = NewPoint;
					
					bFound = true;
					break;
				}
			}
			
			if (!bFound)
			{
				ActivePoints.RemoveAt(Index);
			}
		}
		
		// 转换为3D
		Points.Reserve(Points2D.Num());
		for (const FVector2D& Point2D : Points2D)
		{
			Points.Add(FVector(Point2D.X - BoxExtent.X, Point2D.Y - BoxExtent.Y, 0.0f));
		}
	}
	else
	{
		// 3D采样
		TArray<FVector> ActivePoints;
		
		const float CellSize = ActualRadius / FMath::Sqrt(3.0f);
		const int32 GridWidth = FMath::CeilToInt(Width / CellSize);
		const int32 GridHeight = FMath::CeilToInt(Height / CellSize);
		const int32 GridDepth = FMath::CeilToInt(Depth / CellSize);
		
		TArray<FVector> Grid;
		Grid.SetNumZeroed(GridWidth * GridHeight * GridDepth);
		
		// 初始点（使用RandomStream）
		const FVector InitialPoint(
			RandomStream.FRandRange(0.0f, Width),
			RandomStream.FRandRange(0.0f, Height),
			RandomStream.FRandRange(0.0f, Depth)
		);
		ActivePoints.Add(InitialPoint);
		Points.Add(InitialPoint);
		
		const int32 InitCellX = FMath::FloorToInt(InitialPoint.X / CellSize);
		const int32 InitCellY = FMath::FloorToInt(InitialPoint.Y / CellSize);
		const int32 InitCellZ = FMath::FloorToInt(InitialPoint.Z / CellSize);
		Grid[InitCellZ * GridWidth * GridHeight + InitCellY * GridWidth + InitCellX] = InitialPoint;
		
		// 主循环
		while (ActivePoints.Num() > 0)
		{
			const int32 Index = RandomStream.RandRange(0, ActivePoints.Num() - 1);
			const FVector Point = ActivePoints[Index];
			bool bFound = false;
			
			for (int32 i = 0; i < MaxAttempts; ++i)
			{
				//  传递const指针
				const FVector NewPoint = GenerateRandomPointAround3D(Point, ActualRadius, 2.0f * ActualRadius, &RandomStream);
				
				if (IsValidPoint3D(NewPoint, ActualRadius, Width, Height, Depth, Grid, GridWidth, GridHeight, GridDepth, CellSize))
				{
					ActivePoints.Add(NewPoint);
					Points.Add(NewPoint);
					
					const int32 CellX = FMath::FloorToInt(NewPoint.X / CellSize);
					const int32 CellY = FMath::FloorToInt(NewPoint.Y / CellSize);
					const int32 CellZ = FMath::FloorToInt(NewPoint.Z / CellSize);
					Grid[CellZ * GridWidth * GridHeight + CellY * GridWidth + CellX] = NewPoint;
					
					bFound = true;
					break;
				}
			}
			
			if (!bFound)
			{
				ActivePoints.RemoveAt(Index);
			}
		}
		
		// 转换坐标
		for (FVector& Point : Points)
		{
			Point -= BoxExtent;
		}
	}

	// 应用扰动噪波（使用RandomStream，在调整点数之前）
	ApplyJitter(Points, ActualRadius, JitterStrength, &RandomStream);

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
