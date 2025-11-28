/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "PointDeduplicationHelper.h"
#include "PointSamplingTypes.h"

int32 FPointDeduplicationHelper::RemoveDuplicatePoints(TArray<FVector>& Points, float Tolerance)
{
	if (Points.Num() == 0 || Tolerance <= 0.0f)
	{
		return Points.Num();
	}

	// 使用容差的平方避免 Sqrt 运算
	const float ToleranceSq = Tolerance * Tolerance;
	const float CellSize = Tolerance; // 单元尺寸 = 容差

	// 空间哈希：CellIndex -> 该单元内的点索引列表
	TMap<FIntVector, TArray<int32>> SpatialHash;
	SpatialHash.Reserve(Points.Num() / 4); // 预估每个单元约 4 个点

	// 去重后的点列表
	TArray<FVector> UniquePoints;
	UniquePoints.Reserve(Points.Num());

	// 遍历所有点
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		const FVector& Point = Points[i];
		FIntVector CellIndex = GetCellIndex(Point, CellSize);

		bool bIsDuplicate = false;

		// 检查当前单元和相邻 26 个单元（3x3x3 网格）
		for (int32 dx = -1; dx <= 1; ++dx)
		{
			for (int32 dy = -1; dy <= 1; ++dy)
			{
				for (int32 dz = -1; dz <= 1; ++dz)
				{
					FIntVector NeighborCell(
						CellIndex.X + dx,
						CellIndex.Y + dy,
						CellIndex.Z + dz
					);

					// 检查该单元是否有点
					const TArray<int32>* ExistingIndices = SpatialHash.Find(NeighborCell);
					if (ExistingIndices)
					{
						// 检查是否与该单元中的任意点重复
						for (int32 ExistingIndex : *ExistingIndices)
						{
							const FVector& ExistingPoint = UniquePoints[ExistingIndex];
							float DistSq = FVector::DistSquared(Point, ExistingPoint);

							if (DistSq < ToleranceSq)
							{
								bIsDuplicate = true;
								break;
							}
						}

						if (bIsDuplicate)
							break;
					}
				}

				if (bIsDuplicate)
					break;
			}

			if (bIsDuplicate)
				break;
		}

		// 如果不是重复点，添加到结果
		if (!bIsDuplicate)
		{
			int32 NewIndex = UniquePoints.Add(Point);

			// 将点添加到空间哈希
			TArray<int32>& CellPoints = SpatialHash.FindOrAdd(CellIndex);
			CellPoints.Add(NewIndex);
		}
	}

	// 替换原数组
	Points = MoveTemp(UniquePoints);

	return Points.Num();
}

void FPointDeduplicationHelper::RemoveDuplicatePointsWithStats(
	TArray<FVector>& Points,
	float Tolerance,
	int32& OutOriginalCount,
	int32& OutRemovedCount)
{
	OutOriginalCount = Points.Num();
	int32 FinalCount = RemoveDuplicatePoints(Points, Tolerance);
	OutRemovedCount = OutOriginalCount - FinalCount;
}

FIntVector FPointDeduplicationHelper::GetCellIndex(const FVector& Point, float CellSize)
{
	// 将坐标映射到整数网格
	// 使用 Floor 确保负坐标也能正确映射
	return FIntVector(
		FMath::FloorToInt(Point.X / CellSize),
		FMath::FloorToInt(Point.Y / CellSize),
		FMath::FloorToInt(Point.Z / CellSize)
	);
}

bool FPointDeduplicationHelper::IsPointDuplicate(
	const FVector& Point,
	const TArray<int32>& ExistingPoints,
	const TArray<FVector>& AllPoints,
	float ToleranceSq)
{
	for (int32 Index : ExistingPoints)
	{
		const FVector& ExistingPoint = AllPoints[Index];
		float DistSq = FVector::DistSquared(Point, ExistingPoint);

		if (DistSq < ToleranceSq)
		{
			return true;
		}
	}

	return false;
}
