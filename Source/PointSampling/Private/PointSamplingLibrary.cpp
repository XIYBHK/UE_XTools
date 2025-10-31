/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#include "PointSamplingLibrary.h"
#include "Algorithms/PoissonDiskSampling.h"

// ============================================================================
// 基础2D/3D采样
// ============================================================================

TArray<FVector2D> UPointSamplingLibrary::GeneratePoissonPoints2D(
	float Width,
	float Height,
	float Radius,
	int32 MaxAttempts)
{
	return FPoissonDiskSampling::GeneratePoisson2D(Width, Height, Radius, MaxAttempts);
}

TArray<FVector> UPointSamplingLibrary::GeneratePoissonPoints3D(
	float Width,
	float Height,
	float Depth,
	float Radius,
	int32 MaxAttempts)
{
	return FPoissonDiskSampling::GeneratePoisson3D(Width, Height, Depth, Radius, MaxAttempts);
}

// ============================================================================
// Box组件采样
// ============================================================================

TArray<FVector> UPointSamplingLibrary::GeneratePoissonPointsInBox(
	UBoxComponent* BoxComponent,
	float Radius,
	int32 MaxAttempts,
	EPoissonCoordinateSpace CoordinateSpace,
	int32 TargetPointCount,
	float JitterStrength,
	bool bUseCache)
{
	return FPoissonDiskSampling::GeneratePoissonInBox(
		BoxComponent,
		Radius,
		MaxAttempts,
		CoordinateSpace,
		TargetPointCount,
		JitterStrength,
		bUseCache
	);
}

TArray<FVector> UPointSamplingLibrary::GeneratePoissonPointsInBoxByVector(
	FVector BoxExtent,
	FTransform Transform,
	float Radius,
	int32 MaxAttempts,
	EPoissonCoordinateSpace CoordinateSpace,
	int32 TargetPointCount,
	float JitterStrength,
	bool bUseCache)
{
	return FPoissonDiskSampling::GeneratePoissonInBoxByVector(
		BoxExtent,
		Transform,
		Radius,
		MaxAttempts,
		CoordinateSpace,
		TargetPointCount,
		JitterStrength,
		bUseCache
	);
}

// ============================================================================
// FromStream版本（确定性随机）
// ============================================================================

TArray<FVector> UPointSamplingLibrary::GeneratePoissonPointsInBoxFromStream(
	const FRandomStream& RandomStream,
	UBoxComponent* BoxComponent,
	float Radius,
	int32 MaxAttempts,
	EPoissonCoordinateSpace CoordinateSpace,
	int32 TargetPointCount,
	float JitterStrength)
{
	return FPoissonDiskSampling::GeneratePoissonInBoxFromStream(
		RandomStream,
		BoxComponent,
		Radius,
		MaxAttempts,
		CoordinateSpace,
		TargetPointCount,
		JitterStrength
	);
}

TArray<FVector> UPointSamplingLibrary::GeneratePoissonPointsInBoxByVectorFromStream(
	const FRandomStream& RandomStream,
	FVector BoxExtent,
	FTransform Transform,
	float Radius,
	int32 MaxAttempts,
	EPoissonCoordinateSpace CoordinateSpace,
	int32 TargetPointCount,
	float JitterStrength)
{
	return FPoissonDiskSampling::GeneratePoissonInBoxByVectorFromStream(
		RandomStream,
		BoxExtent,
		Transform,
		Radius,
		MaxAttempts,
		CoordinateSpace,
		TargetPointCount,
		JitterStrength
	);
}

// ============================================================================
// 缓存管理
// ============================================================================

void UPointSamplingLibrary::ClearPoissonSamplingCache()
{
	FPoissonDiskSampling::ClearCache();
}

void UPointSamplingLibrary::GetPoissonSamplingCacheStats(int32& OutHits, int32& OutMisses)
{
	FPoissonDiskSampling::GetCacheStats(OutHits, OutMisses);
}
