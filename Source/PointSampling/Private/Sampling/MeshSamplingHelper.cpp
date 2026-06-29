/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "MeshSamplingHelper.h"
#include "PointSamplingTypes.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"

namespace
{
	constexpr int64 MaxEstimatedVoxelWorkingBytes = 1024LL * 1024LL * 1024LL;
	constexpr int64 WarningEstimatedVoxelWorkingBytes = 256LL * 1024LL * 1024LL;
	constexpr int64 MaxInitialSurfaceReserve = 262144;
	constexpr int32 MaxAllowedVoxelOutputCount = 5000000;
	constexpr int64 MaxSurfaceVoxelWorkUnits = 20000000LL;
	constexpr int64 MaxSolidVoxelWorkUnits = 50000000LL;
	constexpr double MinVoxelTriangleAxisSizeSquared = UE_DOUBLE_SMALL_NUMBER * UE_DOUBLE_SMALL_NUMBER;

	struct FMeshVoxelCell
	{
		bool bOccupied = false;
		bool bSurface = false;
		bool bColorAssigned = false;
		FLinearColor Color = FLinearColor::Transparent;
		int32 ColorSampleCount = 0;
		int32 MaterialIndex = INDEX_NONE;
		int32 MaterialVoteCount = 0;
	};

	struct FMeshVoxelSparseCell
	{
		int64 Key = 0;
		FMeshVoxelCell Cell;
	};

	struct FMeshSectionTriangleRange
	{
		int32 StartTriangle = 0;
		int32 EndTriangle = 0;
		int32 MaterialIndex = INDEX_NONE;
	};

	struct FTriangleBoxAxis
	{
		FVector Axis = FVector::ZeroVector;
		FVector AbsAxis = FVector::ZeroVector;
	};

	struct FTriangleBoxTestData
	{
		FVector P0 = FVector::ZeroVector;
		FVector P1 = FVector::ZeroVector;
		FVector P2 = FVector::ZeroVector;
		FTriangleBoxAxis Axes[10];
		int32 NumAxes = 0;
	};

	FORCEINLINE int32 ToLinearIndex(const int32 X, const int32 Y, const int32 Z, const FIntVector& Dims)
	{
		return (Z * Dims.Y + Y) * Dims.X + X;
	}

	FORCEINLINE int64 ToLinearIndex64(const int32 X, const int32 Y, const int32 Z, const FIntVector& Dims)
	{
		return (static_cast<int64>(Z) * Dims.Y + Y) * Dims.X + X;
	}

	FORCEINLINE FIntVector FromLinearIndex(const int32 LinearIndex, const FIntVector& Dims)
	{
		return FIntVector(
			LinearIndex % Dims.X,
			(LinearIndex / Dims.X) % Dims.Y,
			LinearIndex / (Dims.X * Dims.Y));
	}

	FORCEINLINE int64 SaturatingAdd(const int64 A, const int64 B)
	{
		if (A <= 0)
		{
			return FMath::Max<int64>(0, B);
		}

		if (B <= 0)
		{
			return A;
		}

		return A > MAX_int64 - B ? MAX_int64 : A + B;
	}

	FORCEINLINE int64 SaturatingMultiply(const int64 A, const int64 B)
	{
		if (A <= 0 || B <= 0)
		{
			return 0;
		}

		return A > MAX_int64 / B ? MAX_int64 : A * B;
	}

	FORCEINLINE bool IsValidVoxelIndex(const int32 X, const int32 Y, const int32 Z, const FIntVector& Dims)
	{
		return X >= 0 && Y >= 0 && Z >= 0 && X < Dims.X && Y < Dims.Y && Z < Dims.Z;
	}

	FORCEINLINE bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	bool IsValidVoxelFillMode(const EMeshVoxelFillMode FillMode)
	{
		return FillMode == EMeshVoxelFillMode::SurfaceOnly ||
			FillMode == EMeshVoxelFillMode::Solid;
	}

	bool IsFiniteTransform(const FTransform& Transform)
	{
		const FQuat Rotation = Transform.GetRotation();
		const double RotationSizeSquared = Rotation.SizeSquared();
		return !Transform.ContainsNaN() &&
			IsFiniteVector(Transform.GetLocation()) &&
			IsFiniteVector(Transform.GetScale3D()) &&
			FMath::IsFinite(Rotation.X) &&
			FMath::IsFinite(Rotation.Y) &&
			FMath::IsFinite(Rotation.Z) &&
			FMath::IsFinite(Rotation.W) &&
			RotationSizeSquared > SMALL_NUMBER;
	}

	bool TryCalculateVoxelDimensions(const FVector& MeshSize, const float VoxelSize, FIntVector& OutInnerDims, FIntVector& OutDims, int64& OutTotalVoxelCount)
	{
		auto CalculateAxis = [VoxelSize](const double Size, int64& OutAxis)
		{
			if (!FMath::IsFinite(Size) || Size < 0.0)
			{
				return false;
			}

			const double AxisValue = FMath::CeilToDouble(Size / static_cast<double>(VoxelSize));
			if (!FMath::IsFinite(AxisValue) || AxisValue > static_cast<double>(MAX_int32 - 2))
			{
				return false;
			}

			OutAxis = FMath::Max<int64>(1, static_cast<int64>(AxisValue));
			return true;
		};

		int64 InnerX = 0;
		int64 InnerY = 0;
		int64 InnerZ = 0;
		if (!CalculateAxis(MeshSize.X, InnerX) || !CalculateAxis(MeshSize.Y, InnerY) || !CalculateAxis(MeshSize.Z, InnerZ))
		{
			return false;
		}

		const int64 DimX = InnerX + 2;
		const int64 DimY = InnerY + 2;
		const int64 DimZ = InnerZ + 2;
		constexpr int64 MaxInt32AsInt64 = static_cast<int64>(MAX_int32);
		if (DimX > MaxInt32AsInt64 || DimY > MaxInt32AsInt64 || DimZ > MaxInt32AsInt64)
		{
			return false;
		}

		constexpr int64 MaxInt64 = MAX_int64;
		if (DimX > MaxInt64 / DimY)
		{
			OutTotalVoxelCount = MAX_int64;
			return false;
		}

		const int64 DimXY = DimX * DimY;
		if (DimXY > MaxInt64 / DimZ)
		{
			OutTotalVoxelCount = MAX_int64;
			return false;
		}

		OutTotalVoxelCount = DimXY * DimZ;
		OutInnerDims = FIntVector(static_cast<int32>(InnerX), static_cast<int32>(InnerY), static_cast<int32>(InnerZ));
		OutDims = FIntVector(static_cast<int32>(DimX), static_cast<int32>(DimY), static_cast<int32>(DimZ));
		return true;
	}

	int32 VoxelIndexFromAxis(const double Position, const double GridOrigin, const double VoxelSize, const int32 Dim)
	{
		const double RawIndex = (Position - GridOrigin) / VoxelSize;
		if (!FMath::IsFinite(RawIndex))
		{
			return 0;
		}

		const double FlooredIndex = FMath::FloorToDouble(RawIndex);
		const double ClampedIndex = FMath::Clamp(FlooredIndex, 0.0, static_cast<double>(Dim - 1));
		return static_cast<int32>(ClampedIndex);
	}

	FIntVector ScaledLocalPositionToVoxelIndex(const FVector& Position, const FVector& GridOrigin, const float VoxelSize, const FIntVector& Dims)
	{
		return FIntVector(
			VoxelIndexFromAxis(Position.X, GridOrigin.X, static_cast<double>(VoxelSize), Dims.X),
			VoxelIndexFromAxis(Position.Y, GridOrigin.Y, static_cast<double>(VoxelSize), Dims.Y),
			VoxelIndexFromAxis(Position.Z, GridOrigin.Z, static_cast<double>(VoxelSize), Dims.Z));
	}

	FVector VoxelCenter(const int32 X, const int32 Y, const int32 Z, const FVector& GridOrigin, const float VoxelSize)
	{
		const double VoxelSizeDouble = static_cast<double>(VoxelSize);
		return GridOrigin + FVector(
			(static_cast<double>(X) + 0.5) * VoxelSizeDouble,
			(static_cast<double>(Y) + 0.5) * VoxelSizeDouble,
			(static_cast<double>(Z) + 0.5) * VoxelSizeDouble);
	}

	FIntVector ClampToInteriorVoxelRange(const FIntVector& Index, const FIntVector& InnerDims)
	{
		return FIntVector(
			FMath::Clamp(Index.X, 1, InnerDims.X),
			FMath::Clamp(Index.Y, 1, InnerDims.Y),
			FMath::Clamp(Index.Z, 1, InnerDims.Z));
	}

	int64 EstimateBoundsSurfaceVoxelCount(const FIntVector& InnerDims)
	{
		const int64 XY = SaturatingMultiply(InnerDims.X, InnerDims.Y);
		const int64 XZ = SaturatingMultiply(InnerDims.X, InnerDims.Z);
		const int64 YZ = SaturatingMultiply(InnerDims.Y, InnerDims.Z);
		return SaturatingMultiply(SaturatingAdd(SaturatingAdd(XY, XZ), YZ), 2);
	}

	int64 EstimateVoxelVolume(const FIntVector& Dims)
	{
		return SaturatingMultiply(SaturatingMultiply(Dims.X, Dims.Y), Dims.Z);
	}

	FLinearColor MakeMaterialSlotColor(const int32 MaterialIndex)
	{
		static const FLinearColor Palette[] = {
			FLinearColor(0.95f, 0.22f, 0.20f, 1.0f),
			FLinearColor(0.15f, 0.48f, 0.95f, 1.0f),
			FLinearColor(0.18f, 0.72f, 0.32f, 1.0f),
			FLinearColor(0.95f, 0.78f, 0.16f, 1.0f),
			FLinearColor(0.72f, 0.32f, 0.95f, 1.0f),
			FLinearColor(0.10f, 0.78f, 0.78f, 1.0f),
			FLinearColor(0.95f, 0.45f, 0.12f, 1.0f),
			FLinearColor(0.85f, 0.85f, 0.88f, 1.0f)
		};

		if (MaterialIndex == INDEX_NONE)
		{
			return FLinearColor::White;
		}

		const int64 PaletteIndex = FMath::Abs(static_cast<int64>(MaterialIndex)) % UE_ARRAY_COUNT(Palette);
		return Palette[static_cast<int32>(PaletteIndex)];
	}

	FTransform MakeScaledLocalToWorldTransform(const FTransform& Transform)
	{
		FQuat Rotation = Transform.GetRotation();
		Rotation.Normalize();
		return FTransform(Rotation, Transform.GetLocation(), FVector::OneVector);
	}

	bool HasNearlyZeroScaleAxis(const FVector& Scale)
	{
		return FMath::Abs(Scale.X) <= KINDA_SMALL_NUMBER ||
			FMath::Abs(Scale.Y) <= KINDA_SMALL_NUMBER ||
			FMath::Abs(Scale.Z) <= KINDA_SMALL_NUMBER;
	}

	bool OverlapsOnAxis(const FTriangleBoxAxis& AxisData, const FVector& V0, const FVector& V1, const FVector& V2, const FVector& Extent)
	{
		const double P0 = FVector::DotProduct(V0, AxisData.Axis);
		const double P1 = FVector::DotProduct(V1, AxisData.Axis);
		const double P2 = FVector::DotProduct(V2, AxisData.Axis);
		const double MinP = FMath::Min3(P0, P1, P2);
		const double MaxP = FMath::Max3(P0, P1, P2);
		const double Radius = FVector::DotProduct(Extent, AxisData.AbsAxis);

		return !(MinP > Radius || MaxP < -Radius);
	}

	void AddTriangleAxis(const FVector& Axis, FTriangleBoxTestData& OutTestData)
	{
		if (Axis.SizeSquared() > MinVoxelTriangleAxisSizeSquared && OutTestData.NumAxes < UE_ARRAY_COUNT(OutTestData.Axes))
		{
			FTriangleBoxAxis& AxisData = OutTestData.Axes[OutTestData.NumAxes++];
			AxisData.Axis = Axis;
			AxisData.AbsAxis = FVector(FMath::Abs(Axis.X), FMath::Abs(Axis.Y), FMath::Abs(Axis.Z));
		}
	}

	FTriangleBoxTestData BuildTriangleBoxTestData(const FVector& P0, const FVector& P1, const FVector& P2)
	{
		FTriangleBoxTestData TestData;
		TestData.P0 = P0;
		TestData.P1 = P1;
		TestData.P2 = P2;

		const FVector E0 = P1 - P0;
		const FVector E1 = P2 - P1;
		const FVector E2 = P0 - P2;
		AddTriangleAxis(FVector::CrossProduct(E0, P2 - P0), TestData);

		static const FVector BoxAxes[] = {
			FVector(1.0f, 0.0f, 0.0f),
			FVector(0.0f, 1.0f, 0.0f),
			FVector(0.0f, 0.0f, 1.0f)
		};

		const FVector TriangleEdges[] = { E0, E1, E2 };
		for (const FVector& Edge : TriangleEdges)
		{
			for (const FVector& BoxAxis : BoxAxes)
			{
				AddTriangleAxis(FVector::CrossProduct(Edge, BoxAxis), TestData);
			}
		}

		return TestData;
	}

	bool TriangleIntersectsBox(const FVector& BoxCenter, const FVector& BoxExtent, const FTriangleBoxTestData& TestData)
	{
		const FVector V0 = TestData.P0 - BoxCenter;
		const FVector V1 = TestData.P1 - BoxCenter;
		const FVector V2 = TestData.P2 - BoxCenter;

		if (FMath::Min3(V0.X, V1.X, V2.X) > BoxExtent.X || FMath::Max3(V0.X, V1.X, V2.X) < -BoxExtent.X ||
			FMath::Min3(V0.Y, V1.Y, V2.Y) > BoxExtent.Y || FMath::Max3(V0.Y, V1.Y, V2.Y) < -BoxExtent.Y ||
			FMath::Min3(V0.Z, V1.Z, V2.Z) > BoxExtent.Z || FMath::Max3(V0.Z, V1.Z, V2.Z) < -BoxExtent.Z)
		{
			return false;
		}

		for (int32 AxisIndex = 0; AxisIndex < TestData.NumAxes; ++AxisIndex)
		{
			const FTriangleBoxAxis& AxisData = TestData.Axes[AxisIndex];
			if (!OverlapsOnAxis(AxisData, V0, V1, V2, BoxExtent))
			{
				return false;
			}
		}

		return true;
	}

	void BuildSectionTriangleRanges(const FStaticMeshLODResources& LOD, const int32 NumTriangles, TArray<FMeshSectionTriangleRange>& OutRanges)
	{
		OutRanges.Reset();
		OutRanges.Reserve(LOD.Sections.Num());

		for (const FStaticMeshSection& Section : LOD.Sections)
		{
			const int64 StartTriangle64 = static_cast<int64>(Section.FirstIndex / 3);
			const int64 EndTriangle64 = FMath::Min<int64>(
				NumTriangles,
				SaturatingAdd(StartTriangle64, static_cast<int64>(Section.NumTriangles)));
			if (StartTriangle64 < EndTriangle64 && StartTriangle64 <= MAX_int32 && EndTriangle64 <= MAX_int32)
			{
				FMeshSectionTriangleRange& Range = OutRanges.AddDefaulted_GetRef();
				Range.StartTriangle = static_cast<int32>(StartTriangle64);
				Range.EndTriangle = static_cast<int32>(EndTriangle64);
				Range.MaterialIndex = Section.MaterialIndex;
			}
		}

		OutRanges.Sort([](const FMeshSectionTriangleRange& A, const FMeshSectionTriangleRange& B)
		{
			return A.StartTriangle < B.StartTriangle;
		});
	}

	int32 GetMaterialIndexForTriangle(const int32 TriangleIndex, const TArray<FMeshSectionTriangleRange>& Ranges, int32& InOutRangeIndex)
	{
		while (Ranges.IsValidIndex(InOutRangeIndex) && TriangleIndex >= Ranges[InOutRangeIndex].EndTriangle)
		{
			++InOutRangeIndex;
		}

		if (Ranges.IsValidIndex(InOutRangeIndex) &&
			TriangleIndex >= Ranges[InOutRangeIndex].StartTriangle &&
			TriangleIndex < Ranges[InOutRangeIndex].EndTriangle)
		{
			return Ranges[InOutRangeIndex].MaterialIndex;
		}

		return INDEX_NONE;
	}

	void AddBoundaryEmptyCellsToQueue(const FIntVector& Dims, const TArray<FMeshVoxelCell>& Cells, TArray<uint8>& Outside, TArray<int32>& Queue)
	{
		auto TryAdd = [&](const int32 X, const int32 Y, const int32 Z)
		{
			const int32 LinearIndex = ToLinearIndex(X, Y, Z, Dims);
			if (!Cells[LinearIndex].bOccupied && Outside[LinearIndex] == 0)
			{
				Outside[LinearIndex] = 1;
				Queue.Add(LinearIndex);
			}
		};

		for (int32 Z = 0; Z < Dims.Z; ++Z)
		{
			for (int32 Y = 0; Y < Dims.Y; ++Y)
			{
				TryAdd(0, Y, Z);
				TryAdd(Dims.X - 1, Y, Z);
			}
		}

		for (int32 Z = 0; Z < Dims.Z; ++Z)
		{
			for (int32 X = 0; X < Dims.X; ++X)
			{
				TryAdd(X, 0, Z);
				TryAdd(X, Dims.Y - 1, Z);
			}
		}

		for (int32 Y = 0; Y < Dims.Y; ++Y)
		{
			for (int32 X = 0; X < Dims.X; ++X)
			{
				TryAdd(X, Y, 0);
				TryAdd(X, Y, Dims.Z - 1);
			}
		}
	}

	void FloodFillOutside(const FIntVector& Dims, const TArray<FMeshVoxelCell>& Cells, TArray<uint8>& Outside)
	{
		TArray<int32> Queue;
		const int64 BoundaryReserve = SaturatingAdd(
			SaturatingAdd(
				SaturatingMultiply(SaturatingMultiply(Dims.X, Dims.Y), 2),
				SaturatingMultiply(SaturatingMultiply(Dims.X, Dims.Z), 2)),
			SaturatingMultiply(SaturatingMultiply(Dims.Y, Dims.Z), 2));
		Queue.Reserve(static_cast<int32>(FMath::Min<int64>(BoundaryReserve, 65536)));
		AddBoundaryEmptyCellsToQueue(Dims, Cells, Outside, Queue);

		static const FIntVector Neighbors[] = {
			FIntVector(1, 0, 0),
			FIntVector(-1, 0, 0),
			FIntVector(0, 1, 0),
			FIntVector(0, -1, 0),
			FIntVector(0, 0, 1),
			FIntVector(0, 0, -1)
		};

		for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
		{
			const int32 LinearIndex = Queue[QueueIndex];
			const FIntVector VoxelIndex = FromLinearIndex(LinearIndex, Dims);

			for (const FIntVector& Offset : Neighbors)
			{
				const int32 NX = VoxelIndex.X + Offset.X;
				const int32 NY = VoxelIndex.Y + Offset.Y;
				const int32 NZ = VoxelIndex.Z + Offset.Z;
				if (!IsValidVoxelIndex(NX, NY, NZ, Dims))
				{
					continue;
				}

				const int32 NeighborIndex = ToLinearIndex(NX, NY, NZ, Dims);
				if (!Cells[NeighborIndex].bOccupied && Outside[NeighborIndex] == 0)
				{
					Outside[NeighborIndex] = 1;
					Queue.Add(NeighborIndex);
				}
			}
		}
	}

	void PropagateSurfaceColorToInterior(const FIntVector& Dims, TArray<FMeshVoxelCell>& Cells, const TArray<int32>& SurfaceSeedIndices)
	{
		TArray<int32> Queue;
		Queue.Reserve(FMath::Min(SurfaceSeedIndices.Num(), 65536));

		for (const int32 Index : SurfaceSeedIndices)
		{
			if (Cells.IsValidIndex(Index) && Cells[Index].bSurface && Cells[Index].bColorAssigned)
			{
				Queue.Add(Index);
			}
		}

		static const FIntVector Neighbors[] = {
			FIntVector(1, 0, 0),
			FIntVector(-1, 0, 0),
			FIntVector(0, 1, 0),
			FIntVector(0, -1, 0),
			FIntVector(0, 0, 1),
			FIntVector(0, 0, -1)
		};

		for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
		{
			const int32 LinearIndex = Queue[QueueIndex];
			const FIntVector VoxelIndex = FromLinearIndex(LinearIndex, Dims);

			for (const FIntVector& Offset : Neighbors)
			{
				const int32 NX = VoxelIndex.X + Offset.X;
				const int32 NY = VoxelIndex.Y + Offset.Y;
				const int32 NZ = VoxelIndex.Z + Offset.Z;
				if (!IsValidVoxelIndex(NX, NY, NZ, Dims))
				{
					continue;
				}

				const int32 NeighborIndex = ToLinearIndex(NX, NY, NZ, Dims);
				FMeshVoxelCell& Neighbor = Cells[NeighborIndex];
				if (Neighbor.bOccupied && !Neighbor.bColorAssigned)
				{
					Neighbor.Color = Cells[LinearIndex].Color;
					Neighbor.MaterialIndex = Cells[LinearIndex].MaterialIndex;
					Neighbor.bColorAssigned = true;
					Queue.Add(NeighborIndex);
				}
			}
		}
	}

	void AccumulateMaterialVote(FMeshVoxelCell& Cell, const int32 MaterialIndex)
	{
		if (MaterialIndex == INDEX_NONE)
		{
			return;
		}

		if (Cell.MaterialIndex == INDEX_NONE || Cell.MaterialIndex == MaterialIndex)
		{
			Cell.MaterialIndex = MaterialIndex;
			++Cell.MaterialVoteCount;
			return;
		}

		--Cell.MaterialVoteCount;
		if (Cell.MaterialVoteCount <= 0)
		{
			Cell.MaterialIndex = MaterialIndex;
			Cell.MaterialVoteCount = 1;
		}
	}

	bool AccumulateSurfaceVoxel(FMeshVoxelCell& Cell, const FLinearColor& TriangleColor, const int32 MaterialIndex, int32& SurfaceVoxelWrites)
	{
		const bool bWasNewSurfaceVoxel = !Cell.bOccupied;
		if (bWasNewSurfaceVoxel)
		{
			++SurfaceVoxelWrites;
		}

		Cell.bOccupied = true;
		Cell.bSurface = true;
		Cell.bColorAssigned = true;
		if (Cell.ColorSampleCount <= 0)
		{
			Cell.Color = TriangleColor;
			Cell.ColorSampleCount = 1;
		}
		else
		{
			++Cell.ColorSampleCount;
			Cell.Color += (TriangleColor - Cell.Color) * (1.0f / static_cast<float>(Cell.ColorSampleCount));
		}

		AccumulateMaterialVote(Cell, MaterialIndex);

		return bWasNewSurfaceVoxel;
	}

	void LogInvalidTriangleCount(const int32 InvalidTriangleCount)
	{
		if (InvalidTriangleCount > 0)
		{
			UE_LOG(LogPointSampling, Warning, TEXT("[体素点位] 跳过 %d 个无效三角形"), InvalidTriangleCount);
		}
	}

	void LogDegenerateTriangleCount(const int32 DegenerateTriangleCount)
	{
		if (DegenerateTriangleCount > 0)
		{
			UE_LOG(LogPointSampling, Warning,
				TEXT("[体素点位] 跳过 %d 个退化或面积过小的三角形。若表面缺失，请检查网格清理、LOD或VoxelSize"),
				DegenerateTriangleCount);
		}
	}

	int64 EstimateSolidWorkingBytes(const int64 DenseVoxelCount, const int64 MaxPossibleOutputCount, const int32 MaxVoxelCount)
	{
		const int64 OutputCount = FMath::Min<int64>(MaxPossibleOutputCount, MaxVoxelCount);
		const int64 DenseCellBytes = SaturatingAdd(
			SaturatingAdd(static_cast<int64>(sizeof(FMeshVoxelCell)), static_cast<int64>(sizeof(uint8))),
			static_cast<int64>(sizeof(int32)));
		return SaturatingAdd(
			SaturatingAdd(
				SaturatingMultiply(DenseVoxelCount, DenseCellBytes),
				SaturatingMultiply(OutputCount, static_cast<int64>(sizeof(FMeshVoxelPoint)))),
			SaturatingMultiply(OutputCount, static_cast<int64>(sizeof(int32))));
	}

	int64 EstimateSourceWorkingBytes(const int32 NumVertices, const int32 NumSectionRanges)
	{
		return SaturatingAdd(
			SaturatingMultiply(NumVertices, static_cast<int64>(sizeof(FVector))),
			SaturatingMultiply(NumSectionRanges, static_cast<int64>(sizeof(FMeshSectionTriangleRange))));
	}

	int64 EstimateSurfaceWorkingBytes(const int64 SurfaceCount)
	{
		const int64 ApproxMapCellBytes = static_cast<int64>(sizeof(int64)) + static_cast<int64>(sizeof(int32)) + 64;
		const int64 BytesPerSurfaceVoxel = SaturatingAdd(
			SaturatingAdd(ApproxMapCellBytes, static_cast<int64>(sizeof(FMeshVoxelSparseCell))),
			static_cast<int64>(sizeof(FMeshVoxelPoint)));
		return SaturatingMultiply(SurfaceCount, BytesPerSurfaceVoxel);
	}

	int64 CalculateVoxelWorkBudget(const EMeshVoxelFillMode FillMode, const int32 NumTriangles, const int32 MaxVoxelCount, const int64 TotalVoxelCount)
	{
		const int64 TriangleBudget = SaturatingMultiply(NumTriangles, 256);
		if (FillMode == EMeshVoxelFillMode::Solid)
		{
			const int64 SolidBudget = SaturatingAdd(SaturatingMultiply(TotalVoxelCount, 64), TriangleBudget);
			return FMath::Clamp<int64>(SolidBudget, 1048576LL, MaxSolidVoxelWorkUnits);
		}

		const int64 SurfaceBudget = SaturatingAdd(SaturatingMultiply(MaxVoxelCount, 64), TriangleBudget);
		return FMath::Clamp<int64>(SurfaceBudget, 262144LL, MaxSurfaceVoxelWorkUnits);
	}
}

TArray<FVector> FMeshSamplingHelper::GenerateFromStaticMesh(
	UStaticMesh* StaticMesh,
	const FTransform& Transform,
	int32 LODLevel,
	bool bBoundaryVerticesOnly,
	int32 MaxPoints)
{
	TArray<FVector> Points;

	if (!StaticMesh || !StaticMesh->HasValidRenderData())
	{
		return Points;
	}

	FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
	if (!RenderData)
	{
		return Points;
	}

	if (!RenderData->LODResources.IsValidIndex(LODLevel))
	{
		LODLevel = 0;
		if (!RenderData->LODResources.IsValidIndex(LODLevel))
		{
			return Points;
		}
	}

	FStaticMeshLODResources& LOD = RenderData->LODResources[LODLevel];

	return bBoundaryVerticesOnly
		? GenerateBoundaryVertices(LOD, Transform, MaxPoints)
		: GenerateFromMeshTriangles(LOD, Transform, MaxPoints);
}

TArray<FMeshVoxelPoint> FMeshSamplingHelper::GenerateVoxelPointsFromStaticMesh(
	UStaticMesh* StaticMesh,
	const FTransform& Transform,
	float VoxelSize,
	EMeshVoxelFillMode FillMode,
	int32 LODLevel,
	int32 MaxVoxelCount)
{
	TArray<FMeshVoxelPoint> VoxelPoints;

	if (!StaticMesh || !StaticMesh->HasValidRenderData())
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[体素点位] StaticMesh为空或没有有效渲染数据"));
		return VoxelPoints;
	}

	if (VoxelSize <= KINDA_SMALL_NUMBER || !FMath::IsFinite(VoxelSize))
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[体素点位] VoxelSize无效: %.4f"), VoxelSize);
		return VoxelPoints;
	}

	if (!IsFiniteTransform(Transform))
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[体素点位] Transform包含NaN/Inf或退化旋转，无法生成稳定体素点位"));
		return VoxelPoints;
	}

	const int32 RequestedMaxVoxelCount = MaxVoxelCount;
	MaxVoxelCount = FMath::Clamp(MaxVoxelCount, 1, MaxAllowedVoxelOutputCount);
	if (RequestedMaxVoxelCount != MaxVoxelCount)
	{
		UE_LOG(LogPointSampling, Warning,
			TEXT("[体素点位] MaxVoxelCount=%d已夹取到%d。同步蓝图节点最多返回%d个体素；更大规模建议离线分块预生成"),
			RequestedMaxVoxelCount, MaxVoxelCount, MaxAllowedVoxelOutputCount);
	}

	if (!IsValidVoxelFillMode(FillMode))
	{
		UE_LOG(LogPointSampling, Warning,
			TEXT("[体素点位] FillMode=%d无效，已回退为仅表面模式"),
			static_cast<int32>(FillMode));
		FillMode = EMeshVoxelFillMode::SurfaceOnly;
	}

	FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
	if (!RenderData)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[体素点位] 无法获取StaticMesh渲染数据"));
		return VoxelPoints;
	}

	if (RenderData->LODResources.Num() <= 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[体素点位] StaticMesh没有LOD资源"));
		return VoxelPoints;
	}

	const int32 RequestedLODLevel = FMath::Max(0, LODLevel);
	const int32 ClampedLODLevel = FMath::Min(RequestedLODLevel, RenderData->LODResources.Num() - 1);
	const int32 EffectiveLODLevel = RenderData->GetCurrentFirstLODIdx(ClampedLODLevel);
	if (!RenderData->LODResources.IsValidIndex(EffectiveLODLevel))
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[体素点位] StaticMesh没有可用LOD，或请求LOD已被流送卸载"));
		return VoxelPoints;
	}

	const FStaticMeshLODResources& LOD = RenderData->LODResources[EffectiveLODLevel];
	const FPositionVertexBuffer& VertexBuffer = LOD.VertexBuffers.PositionVertexBuffer;
	const FColorVertexBuffer& ColorVertexBuffer = LOD.VertexBuffers.ColorVertexBuffer;
	const FRawStaticIndexBuffer& IndexBuffer = LOD.IndexBuffer;

	if (!VertexBuffer.GetAllowCPUAccess() || !IndexBuffer.GetAllowCPUAccess())
	{
		UE_LOG(LogPointSampling, Warning,
			TEXT("[体素点位] StaticMesh '%s' 的LOD%d没有CPU可读顶点/索引数据。运行时使用请在资产中启用Allow CPU Access，或改用编辑器预生成结果。"),
			*StaticMesh->GetName(), EffectiveLODLevel);
		return VoxelPoints;
	}

	const int32 NumVertices = VertexBuffer.GetNumVertices();
	const int32 NumIndices = IndexBuffer.GetNumIndices();
	const int32 NumTriangles = NumIndices / 3;
	if (NumVertices == 0 || NumTriangles == 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[体素点位] LOD没有顶点或三角形数据"));
		return VoxelPoints;
	}

	if (NumIndices % 3 != 0)
	{
		UE_LOG(LogPointSampling, Warning,
			TEXT("[体素点位] LOD%d索引数量%d不是3的倍数，末尾%d个索引将被忽略"),
			EffectiveLODLevel, NumIndices, NumIndices % 3);
	}

	TArray<FMeshSectionTriangleRange> TriangleSectionRanges;
	BuildSectionTriangleRanges(LOD, NumTriangles, TriangleSectionRanges);
	if (TriangleSectionRanges.Num() == 0)
	{
		UE_LOG(LogPointSampling, Warning,
			TEXT("[体素点位] LOD%d没有有效Section材质范围，输出MaterialIndex将为INDEX_NONE，缺少顶点色时颜色会回退为白色"),
			EffectiveLODLevel);
	}

	const int64 EstimatedSourceBytes = EstimateSourceWorkingBytes(NumVertices, TriangleSectionRanges.Num());
	if (EstimatedSourceBytes > MaxEstimatedVoxelWorkingBytes)
	{
		UE_LOG(LogPointSampling, Warning,
			TEXT("[体素点位] 源网格预处理预计工作内存约 %.1f MiB，超过保护上限1024 MiB。请降低LOD或简化网格"),
			static_cast<double>(EstimatedSourceBytes) / (1024.0 * 1024.0));
		return VoxelPoints;
	}

	const FVector MeshScale = Transform.GetScale3D();
	if (HasNearlyZeroScaleAxis(MeshScale))
	{
		UE_LOG(LogPointSampling, Warning,
			TEXT("[体素点位] Transform缩放存在接近0的轴 (%.6f, %.6f, %.6f)，无法生成稳定体素点位"),
			MeshScale.X, MeshScale.Y, MeshScale.Z);
		return VoxelPoints;
	}

	// Voxelization runs in scaled-local space: Transform scale is baked into vertices,
	// while rotation and translation are applied only when emitting world-space points.
	TArray<FVector> ScaledLocalPositions;
	ScaledLocalPositions.SetNumUninitialized(NumVertices);

	FBox MeshBounds(EForceInit::ForceInit);
	const FTransform ScaledLocalToWorld = MakeScaledLocalToWorldTransform(Transform);
	const FQuat OutputRotation = ScaledLocalToWorld.GetRotation();
	const FVector OutputScale = FVector::OneVector;
	for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
	{
		const FVector ScaledLocalPosition = FVector(VertexBuffer.VertexPosition(VertexIndex)) * MeshScale;
		if (!IsFiniteVector(ScaledLocalPosition))
		{
			UE_LOG(LogPointSampling, Warning,
				TEXT("[体素点位] StaticMesh '%s' 的LOD%d包含NaN/Inf顶点数据，顶点索引=%d"),
				*StaticMesh->GetName(), EffectiveLODLevel, VertexIndex);
			return VoxelPoints;
		}

		ScaledLocalPositions[VertexIndex] = ScaledLocalPosition;
		MeshBounds += ScaledLocalPosition;
	}

	if (!MeshBounds.IsValid)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[体素点位] 网格包围盒无效"));
		return VoxelPoints;
	}

	const FVector MeshSize = MeshBounds.GetSize();
	FIntVector InnerDims = FIntVector::ZeroValue;
	FIntVector Dims = FIntVector::ZeroValue;
	int64 TotalVoxelCount64 = 0;
	if (!TryCalculateVoxelDimensions(MeshSize, VoxelSize, InnerDims, Dims, TotalVoxelCount64))
	{
		UE_LOG(LogPointSampling, Warning,
			TEXT("[体素点位] 体素网格过大或尺寸无效。MeshSize=(%.2f, %.2f, %.2f), VoxelSize=%.4f，请增大VoxelSize或检查Transform缩放"),
			MeshSize.X, MeshSize.Y, MeshSize.Z, VoxelSize);
		return VoxelPoints;
	}

	const int64 MaxPossibleSolidOutputCount = EstimateVoxelVolume(InnerDims);
	if (FillMode == EMeshVoxelFillMode::Solid)
	{
		if (TotalVoxelCount64 > static_cast<int64>(MAX_int32))
		{
			UE_LOG(LogPointSampling, Warning,
				TEXT("[体素点位] 内部填充工作网格需要%lld个格子，超过运行时数组索引上限。请增大VoxelSize"),
				TotalVoxelCount64);
			return VoxelPoints;
		}

		const int64 EstimatedSolidBytes = EstimateSolidWorkingBytes(TotalVoxelCount64, MaxPossibleSolidOutputCount, MaxVoxelCount);
		const int64 EstimatedTotalBytes = SaturatingAdd(EstimatedSourceBytes, EstimatedSolidBytes);
		if (EstimatedTotalBytes > MaxEstimatedVoxelWorkingBytes)
		{
			UE_LOG(LogPointSampling, Warning,
				TEXT("[体素点位] 内部填充预计工作内存约 %.1f MiB，超过保护上限1024 MiB。请增大VoxelSize或降低MaxVoxelCount"),
				static_cast<double>(EstimatedTotalBytes) / (1024.0 * 1024.0));
			return VoxelPoints;
		}

		if (EstimatedTotalBytes > WarningEstimatedVoxelWorkingBytes)
		{
			UE_LOG(LogPointSampling, Warning,
				TEXT("[体素点位] 内部填充预计工作内存约 %.1f MiB，可能造成明显卡顿"),
				static_cast<double>(EstimatedTotalBytes) / (1024.0 * 1024.0));
		}
	}

	if (FillMode == EMeshVoxelFillMode::SurfaceOnly && TotalVoxelCount64 > MaxVoxelCount)
	{
		UE_LOG(LogPointSampling, Verbose,
			TEXT("[体素点位] 表面模式使用稀疏存储，包围盒格子数%lld超过MaxVoxelCount=%d；达到输出上限后会提前停止扫描"),
			TotalVoxelCount64, MaxVoxelCount);
	}

	const int32 TotalVoxelCount = (FillMode == EMeshVoxelFillMode::Solid) ? static_cast<int32>(TotalVoxelCount64) : 0;
	const FVector GridOrigin = MeshBounds.Min - FVector(VoxelSize);
	const FVector BoxExtent(VoxelSize * 0.5f);

	TArray<FMeshVoxelCell> DenseCells;
	TArray<int32> DenseSurfaceIndices;
	TArray<FMeshVoxelSparseCell> SurfaceCells;
	TMap<int64, int32> SurfaceIndexByKey;
	const int64 SurfaceWorkingBytesPerVoxel = EstimateSurfaceWorkingBytes(1);
	bool bSurfaceMemoryWarningEmitted = false;
	if (FillMode == EMeshVoxelFillMode::Solid)
	{
		DenseCells.SetNum(TotalVoxelCount);
		DenseSurfaceIndices.Reserve(static_cast<int32>(FMath::Min<int64>(MaxPossibleSolidOutputCount, MaxVoxelCount)));
	}
	else
	{
		const int64 ExpectedSurfaceCount = FMath::Min<int64>(
			MaxVoxelCount,
			FMath::Max<int64>(
				64,
				FMath::Max(SaturatingMultiply(NumTriangles, 2), EstimateBoundsSurfaceVoxelCount(InnerDims))));
		const int64 SurfaceReserve = FMath::Min<int64>(ExpectedSurfaceCount, MaxInitialSurfaceReserve);
		const int64 EstimatedSurfaceBytes = SaturatingAdd(EstimatedSourceBytes, EstimateSurfaceWorkingBytes(ExpectedSurfaceCount));
		if (EstimatedSurfaceBytes > MaxEstimatedVoxelWorkingBytes)
		{
			UE_LOG(LogPointSampling, Warning,
				TEXT("[体素点位] 表面体素化预估工作内存可能达到 %.1f MiB；将按实际输出动态保护，接近1024 MiB时提前停止"),
				static_cast<double>(EstimatedSurfaceBytes) / (1024.0 * 1024.0));
			bSurfaceMemoryWarningEmitted = true;
		}
		else if (EstimatedSurfaceBytes > WarningEstimatedVoxelWorkingBytes)
		{
			UE_LOG(LogPointSampling, Warning,
				TEXT("[体素点位] 表面体素化预计工作内存约 %.1f MiB，可能造成明显卡顿"),
				static_cast<double>(EstimatedSurfaceBytes) / (1024.0 * 1024.0));
			bSurfaceMemoryWarningEmitted = true;
		}

		SurfaceCells.Reserve(static_cast<int32>(SurfaceReserve));
		SurfaceIndexByKey.Reserve(static_cast<int32>(SurfaceReserve));
	}

	const bool bHasMatchingVertexColors = ColorVertexBuffer.GetNumVertices() == NumVertices;
	const bool bUseVertexColors = bHasMatchingVertexColors && ColorVertexBuffer.GetAllowCPUAccess();
	if (ColorVertexBuffer.GetNumVertices() > 0 && !bUseVertexColors)
	{
		UE_LOG(LogPointSampling, Warning,
			TEXT("[体素点位] LOD%d存在顶点色但无法用于运行时颜色采样（顶点色CPU访问=%s，顶点色数量=%d，位置顶点数量=%d），将回退为材质槽稳定色"),
			EffectiveLODLevel,
			ColorVertexBuffer.GetAllowCPUAccess() ? TEXT("true") : TEXT("false"),
			ColorVertexBuffer.GetNumVertices(),
			NumVertices);
	}

	if (RequestedLODLevel != EffectiveLODLevel)
	{
		UE_LOG(LogPointSampling, Warning,
			TEXT("[体素点位] 请求LOD%d被夹取或受流送状态调整，使用LOD%d"),
			RequestedLODLevel, EffectiveLODLevel);
	}

	int32 InvalidTriangleCount = 0;
	int32 DegenerateTriangleCount = 0;
	int32 SurfaceVoxelWrites = 0;
	bool bSurfaceOutputTruncated = false;
	bool bSurfaceMemoryBudgetExceeded = false;
	bool bStopVoxelScan = false;
	bool bWorkBudgetExceeded = false;
	int32 UnmappedMaterialTriangleCount = 0;
	const int64 WorkBudget = CalculateVoxelWorkBudget(FillMode, NumTriangles, MaxVoxelCount, TotalVoxelCount64);
	int64 WorkUnits = 0;
	int64 CandidateTests = 0;
	const int32 MaxVertexIndex = NumVertices - 1;
	int32 CurrentSectionRangeIndex = 0;
	const double VoxelSizeDouble = static_cast<double>(VoxelSize);
	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles && !bStopVoxelScan; ++TriangleIndex)
	{
		++WorkUnits;
		if (WorkUnits > WorkBudget)
		{
			bWorkBudgetExceeded = true;
			bStopVoxelScan = true;
			break;
		}

		const int32 I0 = IndexBuffer.GetIndex(TriangleIndex * 3);
		const int32 I1 = IndexBuffer.GetIndex(TriangleIndex * 3 + 1);
		const int32 I2 = IndexBuffer.GetIndex(TriangleIndex * 3 + 2);

		if (I0 < 0 || I1 < 0 || I2 < 0 || I0 > MaxVertexIndex || I1 > MaxVertexIndex || I2 > MaxVertexIndex)
		{
			++InvalidTriangleCount;
			continue;
		}

		const FVector P0 = ScaledLocalPositions[I0];
		const FVector P1 = ScaledLocalPositions[I1];
		const FVector P2 = ScaledLocalPositions[I2];
		if (FVector::CrossProduct(P1 - P0, P2 - P0).SizeSquared() <= MinVoxelTriangleAxisSizeSquared)
		{
			++DegenerateTriangleCount;
			continue;
		}
		const FTriangleBoxTestData TriangleBoxTestData = BuildTriangleBoxTestData(P0, P1, P2);

		const FVector TriMin(
			FMath::Min3(P0.X, P1.X, P2.X),
			FMath::Min3(P0.Y, P1.Y, P2.Y),
			FMath::Min3(P0.Z, P1.Z, P2.Z));
		const FVector TriMax(
			FMath::Max3(P0.X, P1.X, P2.X),
			FMath::Max3(P0.Y, P1.Y, P2.Y),
			FMath::Max3(P0.Z, P1.Z, P2.Z));

		const FIntVector MinIndex = ClampToInteriorVoxelRange(ScaledLocalPositionToVoxelIndex(TriMin - BoxExtent, GridOrigin, VoxelSize, Dims), InnerDims);
		const FIntVector MaxIndex = ClampToInteriorVoxelRange(ScaledLocalPositionToVoxelIndex(TriMax + BoxExtent, GridOrigin, VoxelSize, Dims), InnerDims);
		const int32 MaterialIndex = GetMaterialIndexForTriangle(TriangleIndex, TriangleSectionRanges, CurrentSectionRangeIndex);
		if (MaterialIndex == INDEX_NONE)
		{
			++UnmappedMaterialTriangleCount;
		}

		const FLinearColor TriangleColor = bUseVertexColors
			? (FLinearColor(ColorVertexBuffer.VertexColor(I0)) +
			   FLinearColor(ColorVertexBuffer.VertexColor(I1)) +
			   FLinearColor(ColorVertexBuffer.VertexColor(I2))) * (1.0f / 3.0f)
			: MakeMaterialSlotColor(MaterialIndex);

		for (int32 Z = MinIndex.Z; Z <= MaxIndex.Z && !bStopVoxelScan; ++Z)
		{
			const double CenterZ = GridOrigin.Z + (static_cast<double>(Z) + 0.5) * VoxelSizeDouble;
			for (int32 Y = MinIndex.Y; Y <= MaxIndex.Y && !bStopVoxelScan; ++Y)
			{
				const double CenterY = GridOrigin.Y + (static_cast<double>(Y) + 0.5) * VoxelSizeDouble;
				double CenterX = GridOrigin.X + (static_cast<double>(MinIndex.X) + 0.5) * VoxelSizeDouble;
				for (int32 X = MinIndex.X; X <= MaxIndex.X; ++X, CenterX += VoxelSizeDouble)
				{
					++CandidateTests;
					++WorkUnits;
					if (WorkUnits > WorkBudget)
					{
						bWorkBudgetExceeded = true;
						bStopVoxelScan = true;
						break;
					}

					const FVector Center(CenterX, CenterY, CenterZ);
					if (!TriangleIntersectsBox(Center, BoxExtent, TriangleBoxTestData))
					{
						continue;
					}

					if (FillMode == EMeshVoxelFillMode::SurfaceOnly)
					{
						const int64 LinearIndex = ToLinearIndex64(X, Y, Z, Dims);
						FMeshVoxelCell* Cell = nullptr;
						if (const int32* ExistingSurfaceIndex = SurfaceIndexByKey.Find(LinearIndex))
						{
							Cell = &SurfaceCells[*ExistingSurfaceIndex].Cell;
						}

						if (!Cell)
						{
							if (SurfaceVoxelWrites >= MaxVoxelCount)
							{
								bSurfaceOutputTruncated = true;
								bStopVoxelScan = true;
								break;
							}

							const int64 NextSurfaceCount = static_cast<int64>(SurfaceVoxelWrites) + 1;
							const int64 RuntimeSurfaceBytes = SaturatingAdd(EstimatedSourceBytes, SaturatingMultiply(NextSurfaceCount, SurfaceWorkingBytesPerVoxel));
							if (RuntimeSurfaceBytes > MaxEstimatedVoxelWorkingBytes)
							{
								bSurfaceMemoryBudgetExceeded = true;
								bStopVoxelScan = true;
								break;
							}

							if (!bSurfaceMemoryWarningEmitted && RuntimeSurfaceBytes > WarningEstimatedVoxelWorkingBytes)
							{
								UE_LOG(LogPointSampling, Warning,
									TEXT("[体素点位] 表面体素化工作内存已接近 %.1f MiB，继续生成可能造成明显卡顿"),
									static_cast<double>(RuntimeSurfaceBytes) / (1024.0 * 1024.0));
								bSurfaceMemoryWarningEmitted = true;
							}

							const int32 NewSurfaceIndex = SurfaceCells.AddDefaulted();
							FMeshVoxelSparseCell& NewSurfaceCell = SurfaceCells[NewSurfaceIndex];
							NewSurfaceCell.Key = LinearIndex;
							SurfaceIndexByKey.Add(LinearIndex, NewSurfaceIndex);
							Cell = &NewSurfaceCell.Cell;
						}

						AccumulateSurfaceVoxel(*Cell, TriangleColor, MaterialIndex, SurfaceVoxelWrites);
						continue;
					}

					const int32 LinearIndex = ToLinearIndex(X, Y, Z, Dims);
					FMeshVoxelCell& Cell = DenseCells[LinearIndex];
					if (AccumulateSurfaceVoxel(Cell, TriangleColor, MaterialIndex, SurfaceVoxelWrites))
					{
						DenseSurfaceIndices.Add(LinearIndex);
					}
				}
			}
		}
	}

	if (bWorkBudgetExceeded && FillMode == EMeshVoxelFillMode::Solid)
	{
		LogInvalidTriangleCount(InvalidTriangleCount);
		LogDegenerateTriangleCount(DegenerateTriangleCount);
		UE_LOG(LogPointSampling, Warning,
			TEXT("[体素点位] 内部填充体素化工作量达到%lld，超过保护预算%lld，已中止并返回空结果。请增大VoxelSize或降低LOD"),
			WorkUnits, WorkBudget);
		return VoxelPoints;
	}

	LogInvalidTriangleCount(InvalidTriangleCount);
	LogDegenerateTriangleCount(DegenerateTriangleCount);
	if (UnmappedMaterialTriangleCount > 0)
	{
		UE_LOG(LogPointSampling, Warning,
			TEXT("[体素点位] %d个有效三角形没有匹配到LOD Section材质范围，相关体素MaterialIndex可能为INDEX_NONE"),
			UnmappedMaterialTriangleCount);
	}

	if (SurfaceVoxelWrites == 0)
	{
		UE_LOG(LogPointSampling, Warning,
			TEXT("[体素点位] 未检测到表面体素。请检查LOD、VoxelSize、Allow CPU Access和网格三角形数据"));
	}

	if (bSurfaceOutputTruncated)
	{
		UE_LOG(LogPointSampling, Warning,
			TEXT("[体素点位] 表面体素数量达到MaxVoxelCount=%d，已提前停止扫描并截断结果。请增大MaxVoxelCount或增大VoxelSize"),
			MaxVoxelCount);
	}

	if (bWorkBudgetExceeded)
	{
		UE_LOG(LogPointSampling, Warning,
			TEXT("[体素点位] 表面体素化工作量达到%lld，超过保护预算%lld，已提前停止扫描并返回部分结果。请增大VoxelSize或提高MaxVoxelCount"),
			WorkUnits, WorkBudget);
	}

	if (bSurfaceMemoryBudgetExceeded)
	{
		UE_LOG(LogPointSampling, Warning,
			TEXT("[体素点位] 表面体素化达到1024 MiB工作内存保护上限，已提前停止扫描并返回部分结果。请增大VoxelSize、降低LOD或降低MaxVoxelCount"));
	}

	int32 InteriorVoxelCount = 0;
	if (FillMode == EMeshVoxelFillMode::Solid)
	{
		UE_LOG(LogPointSampling, Verbose,
			TEXT("[体素点位] 内部填充假定输入网格基本封闭；开口、自交或极薄模型可能只产生表面点或局部误填。"));

		TArray<uint8> Outside;
		Outside.SetNumZeroed(TotalVoxelCount);
		FloodFillOutside(Dims, DenseCells, Outside);

		for (int32 Index = 0; Index < TotalVoxelCount; ++Index)
		{
			FMeshVoxelCell& Cell = DenseCells[Index];
			if (!Cell.bOccupied && Outside[Index] == 0)
			{
				Cell.bOccupied = true;
				Cell.bSurface = false;
				Cell.bColorAssigned = false;
				++InteriorVoxelCount;
			}
		}

		if (InteriorVoxelCount > 0)
		{
			PropagateSurfaceColorToInterior(Dims, DenseCells, DenseSurfaceIndices);
			DenseSurfaceIndices.Empty();
		}
		else if (SurfaceVoxelWrites > 0)
		{
			UE_LOG(LogPointSampling, Warning,
				TEXT("[体素点位] 内部填充未检测到内部体素。模型可能不封闭、存在开口/自交，或VoxelSize相对模型过大"));
		}
	}
	else
	{
		SurfaceIndexByKey.Empty();
	}

	const int64 ExpectedOutputCount = FillMode == EMeshVoxelFillMode::Solid
		? SaturatingAdd(static_cast<int64>(SurfaceVoxelWrites), static_cast<int64>(InteriorVoxelCount))
		: static_cast<int64>(SurfaceVoxelWrites);
	VoxelPoints.Reserve(static_cast<int32>(FMath::Min<int64>(ExpectedOutputCount, MaxVoxelCount)));
	bool bFinalOutputTruncated = false;
	auto AppendVoxelPoint = [&](const int32 X, const int32 Y, const int32 Z, const FMeshVoxelCell& Cell) -> bool
	{
		if (!Cell.bOccupied)
		{
			return true;
		}

		if (VoxelPoints.Num() >= MaxVoxelCount)
		{
			bFinalOutputTruncated = true;
			return false;
		}

		FMeshVoxelPoint Point;
		// Centers are converted back to world space after the scaled-local voxel test.
		Point.Position = ScaledLocalToWorld.TransformPosition(VoxelCenter(X, Y, Z, GridOrigin, VoxelSize));
		Point.Transform = FTransform(OutputRotation, Point.Position, OutputScale);
		Point.VoxelSize = VoxelSize;
		Point.GridIndex = FIntVector(X - 1, Y - 1, Z - 1);
		Point.Color = Cell.bColorAssigned ? Cell.Color : FLinearColor::White;
		Point.MaterialIndex = Cell.MaterialIndex;
		Point.bIsSurface = Cell.bSurface;
		VoxelPoints.Add(Point);
		return true;
	};

	if (FillMode == EMeshVoxelFillMode::Solid)
	{
		for (int32 Z = 1; Z <= InnerDims.Z && !bFinalOutputTruncated; ++Z)
		{
			for (int32 Y = 1; Y <= InnerDims.Y && !bFinalOutputTruncated; ++Y)
			{
				for (int32 X = 1; X <= InnerDims.X; ++X)
				{
					const int32 LinearIndex = ToLinearIndex(X, Y, Z, Dims);
					if (!AppendVoxelPoint(X, Y, Z, DenseCells[LinearIndex]))
					{
						break;
					}
				}
			}
		}
	}
	else
	{
		SurfaceCells.Sort([](const FMeshVoxelSparseCell& A, const FMeshVoxelSparseCell& B)
		{
			return A.Key < B.Key;
		});

		const int64 DimX = Dims.X;
		const int64 DimXY = static_cast<int64>(Dims.X) * Dims.Y;
		for (const FMeshVoxelSparseCell& SurfaceCell : SurfaceCells)
		{
			const int64 SurfaceKey = SurfaceCell.Key;
			const int32 X = static_cast<int32>(SurfaceKey % DimX);
			const int32 Y = static_cast<int32>((SurfaceKey / DimX) % Dims.Y);
			const int32 Z = static_cast<int32>(SurfaceKey / DimXY);
			if (!AppendVoxelPoint(X, Y, Z, SurfaceCell.Cell))
			{
				break;
			}
		}
	}

	if (bFinalOutputTruncated && FillMode == EMeshVoxelFillMode::Solid)
	{
		UE_LOG(LogPointSampling, Warning,
			TEXT("[体素点位] 内部填充输出达到MaxVoxelCount=%d，已截断结果。请增大VoxelSize或提高MaxVoxelCount"),
			MaxVoxelCount);
	}

	UE_LOG(LogPointSampling, Log,
		TEXT("[体素点位] 完成: StaticMesh=%s, LOD=%d, 模式=%s, VoxelSize=%.2f, 体素范围=%dx%dx%d, 工作网格=%dx%dx%d, 表面=%d, 内部=%d, 输出=%d, 候选测试=%lld, 工作量=%lld/%lld%s%s"),
		*StaticMesh->GetName(),
		EffectiveLODLevel,
		FillMode == EMeshVoxelFillMode::Solid ? TEXT("内部填充") : TEXT("仅表面"),
		VoxelSize,
		InnerDims.X,
		InnerDims.Y,
		InnerDims.Z,
		Dims.X,
		Dims.Y,
		Dims.Z,
		SurfaceVoxelWrites,
		InteriorVoxelCount,
		VoxelPoints.Num(),
		CandidateTests,
		WorkUnits,
		WorkBudget,
		(bSurfaceOutputTruncated || bFinalOutputTruncated) ? TEXT(", 已截断") : TEXT(""),
		bWorkBudgetExceeded || bSurfaceMemoryBudgetExceeded ? TEXT(", 保护预算提前停止") : TEXT(""));

	return VoxelPoints;
}

/**
 * 从网格三角形生成基于面积加权的采样点
 */
TArray<FVector> FMeshSamplingHelper::GenerateFromMeshTriangles(
	const FStaticMeshLODResources& LOD,
	const FTransform& Transform,
	int32 MaxPoints)
{
	TArray<FVector> Points;

	const FPositionVertexBuffer& VertexBuffer = LOD.VertexBuffers.PositionVertexBuffer;
	const FRawStaticIndexBuffer& IndexBuffer = LOD.IndexBuffer;

	if (VertexBuffer.GetNumVertices() == 0 || IndexBuffer.GetNumIndices() == 0)
	{
		return Points;
	}

	const int32 NumTriangles = IndexBuffer.GetNumIndices() / 3;

	TArray<float> TriangleAreas;
	TArray<int32> ValidTriangleIndices;
	float TotalArea = 0.0f;
	TriangleAreas.Reserve(NumTriangles);
	ValidTriangleIndices.Reserve(NumTriangles);

	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
	{
		const int32 Index0 = IndexBuffer.GetIndex(TriangleIndex * 3);
		const int32 Index1 = IndexBuffer.GetIndex(TriangleIndex * 3 + 1);
		const int32 Index2 = IndexBuffer.GetIndex(TriangleIndex * 3 + 2);

		// 修复：添加索引边界检查
		const int32 MaxVertexIndex = VertexBuffer.GetNumVertices() - 1;
		if (Index0 > MaxVertexIndex || Index1 > MaxVertexIndex || Index2 > MaxVertexIndex)
		{
			UE_LOG(LogPointSampling, Warning, TEXT("[网格采样] 三角形 %d 包含无效顶点索引: %d, %d, %d (最大: %d)"),
				TriangleIndex, Index0, Index1, Index2, MaxVertexIndex);
			continue;
		}

		const FVector V0(VertexBuffer.VertexPosition(Index0));
		const FVector V1(VertexBuffer.VertexPosition(Index1));
		const FVector V2(VertexBuffer.VertexPosition(Index2));

		const float Area = FVector::CrossProduct(V1 - V0, V2 - V0).Size() * 0.5f;

		TriangleAreas.Add(Area);
		ValidTriangleIndices.Add(TriangleIndex);
		TotalArea += Area;
	}

	if (TotalArea <= 0.0f)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[网格采样] 网格总面积为0或没有三角形"));
		return Points;
	}

	const int32 ValidTriangleCount = ValidTriangleIndices.Num();
	const int32 TargetPoints = (MaxPoints > 0) ? MaxPoints : FMath::Min(10000, ValidTriangleCount * 2);
	Points.Reserve(TargetPoints);

	UE_LOG(LogPointSampling, Log, TEXT("[网格采样] 开始基于面积的采样: %d/%d 个有效三角形, 总面积 %.2f, 目标点数 %d"),
		ValidTriangleCount, NumTriangles, TotalArea, TargetPoints);

	FRandomStream RandomStream(FMath::Rand());

	for (int32 ValidIndex = 0; ValidIndex < ValidTriangleCount && Points.Num() < TargetPoints; ++ValidIndex)
	{
		const float TriangleArea = TriangleAreas[ValidIndex];
		if (TriangleArea <= 0.0f)
		{
			continue;
		}

		const int32 PointsForThisTriangle = FMath::Max(1, FMath::RoundToInt((TriangleArea / TotalArea) * TargetPoints));
		const int32 TriangleIndex = ValidTriangleIndices[ValidIndex];

		for (int32 PointIndex = 0; PointIndex < PointsForThisTriangle && Points.Num() < TargetPoints; ++PointIndex)
		{
			float U = RandomStream.FRand();
			float V = RandomStream.FRand();

			if (U + V > 1.0f)
			{
				U = 1.0f - U;
				V = 1.0f - V;
			}

			const float W = 1.0f - U - V;

			const int32 Index0 = IndexBuffer.GetIndex(TriangleIndex * 3);
			const int32 Index1 = IndexBuffer.GetIndex(TriangleIndex * 3 + 1);
			const int32 Index2 = IndexBuffer.GetIndex(TriangleIndex * 3 + 2);

			const FVector V0(VertexBuffer.VertexPosition(Index0));
			const FVector V1(VertexBuffer.VertexPosition(Index1));
			const FVector V2(VertexBuffer.VertexPosition(Index2));

			const FVector LocalPoint = V0 * W + V1 * U + V2 * V;
			Points.Add(Transform.TransformPosition(LocalPoint));
		}
	}

	UE_LOG(LogPointSampling, Log, TEXT("[网格采样] 完成，生成 %d 个点"), Points.Num());

	return Points;
}

/**
 * 生成网格边界顶点
 */
TArray<FVector> FMeshSamplingHelper::GenerateBoundaryVertices(
	const FStaticMeshLODResources& LOD,
	const FTransform& Transform,
	int32 MaxPoints)
{
	TArray<FVector> Points;

	const FPositionVertexBuffer& VertexBuffer = LOD.VertexBuffers.PositionVertexBuffer;
	const FRawStaticIndexBuffer& IndexBuffer = LOD.IndexBuffer;

	if (VertexBuffer.GetNumVertices() == 0 || IndexBuffer.GetNumIndices() == 0)
	{
		return Points;
	}

	const int32 NumTriangles = IndexBuffer.GetNumIndices() / 3;

	TMap<TPair<int32, int32>, int32> EdgeUsageCount;

	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
	{
		const int32 I0 = IndexBuffer.GetIndex(TriangleIndex * 3);
		const int32 I1 = IndexBuffer.GetIndex(TriangleIndex * 3 + 1);
		const int32 I2 = IndexBuffer.GetIndex(TriangleIndex * 3 + 2);

		EdgeUsageCount.FindOrAdd(TPair<int32, int32>(FMath::Min(I0, I1), FMath::Max(I0, I1)))++;
		EdgeUsageCount.FindOrAdd(TPair<int32, int32>(FMath::Min(I1, I2), FMath::Max(I1, I2)))++;
		EdgeUsageCount.FindOrAdd(TPair<int32, int32>(FMath::Min(I2, I0), FMath::Max(I2, I0)))++;
	}

	TSet<int32> BoundaryVertexIndices;

	for (const auto& EdgeCount : EdgeUsageCount)
	{
		if (EdgeCount.Value == 1)
		{
			BoundaryVertexIndices.Add(EdgeCount.Key.Key);
			BoundaryVertexIndices.Add(EdgeCount.Key.Value);
		}
	}

	const int32 MaxBoundaryPoints = (MaxPoints > 0) ? MaxPoints : BoundaryVertexIndices.Num();
	Points.Reserve(MaxBoundaryPoints);

	UE_LOG(LogPointSampling, Log, TEXT("[网格采样] 找到 %d 个边界顶点，采样 %d 个"),
		BoundaryVertexIndices.Num(), MaxBoundaryPoints);

	TArray<int32> BoundaryVertices(BoundaryVertexIndices.Array());
	const int32 Step = FMath::Max(1, BoundaryVertices.Num() / MaxBoundaryPoints);

	for (int32 i = 0; i < BoundaryVertices.Num() && Points.Num() < MaxBoundaryPoints; i += Step)
	{
		const int32 VertexIndex = BoundaryVertices[i];
		const FVector LocalPoint(VertexBuffer.VertexPosition(VertexIndex));
		Points.Add(Transform.TransformPosition(LocalPoint));
	}

	UE_LOG(LogPointSampling, Log, TEXT("[网格采样] 生成 %d 个边界顶点"), Points.Num());

	return Points;
}
