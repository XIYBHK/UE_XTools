/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Sampling/MeshSamplingHelper.h"
#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshResources.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	TStrongObjectPtr<UStaticMesh> MakeVoxelCube()
	{
		TStrongObjectPtr<UStaticMesh> Mesh(NewObject<UStaticMesh>());
		TUniquePtr<FStaticMeshRenderData> Data = MakeUnique<FStaticMeshRenderData>();
		Data->AllocateLODResources(1);
		Data->CurrentFirstLODIdx = 0;
		FStaticMeshLODResources& LOD = Data->LODResources[0];
		const TArray<FVector3f> Positions = {
			{0, 0, 0}, {120, 0, 0}, {120, 120, 0}, {0, 120, 0},
			{0, 0, 120}, {120, 0, 120}, {120, 120, 120}, {0, 120, 120}
		};
		LOD.VertexBuffers.PositionVertexBuffer.Init(Positions, true);
		LOD.VertexBuffers.StaticMeshVertexBuffer.Init(8, 1, true);
		LOD.VertexBuffers.ColorVertexBuffer.Init(8, true);
		for (uint32 Index = 0; Index < 8; ++Index)
		{
			LOD.VertexBuffers.ColorVertexBuffer.VertexColor(Index) = FColor::Red;
		}
		const TArray<uint32> Indices = {
			0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
			0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2,
			0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5
		};
		LOD.IndexBuffer.SetIndices(Indices, EIndexBufferStride::Force32Bit);
		LOD.IndexBuffer.TrySetAllowCPUAccess(true);
		// The fixture has CPU buffers only; mark their size so LOD residency checks accept it.
		LOD.BuffersSize = Positions.Num() * sizeof(FVector3f) + Indices.Num() * sizeof(uint32) + 8 * sizeof(FColor);
		FStaticMeshSection& Section = LOD.Sections.AddDefaulted_GetRef();
		Section.FirstIndex = 0;
		Section.NumTriangles = 12;
		Section.MinVertexIndex = 0;
		Section.MaxVertexIndex = 7;
		Section.MaterialIndex = 0;
		Mesh->SetRenderData(MoveTemp(Data));
		return Mesh;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMeshVoxelCubeContractTest,
	"XTools.PointSampling.Voxel.CubeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMeshVoxelCubeContractTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UStaticMesh> Mesh = MakeVoxelCube();
	const FTransform Transforms[] = {
		FTransform::Identity,
		FTransform(FRotator(17, 63, -12), FVector(301, -47, 91), FVector(-1, 2, 0.5))
	};
	for (const FTransform& Transform : Transforms)
	{
		const FVector ScaledEnd = FVector(120) * Transform.GetScale3D();
		const FVector BoundsMin(FMath::Min(0.0, ScaledEnd.X), FMath::Min(0.0, ScaledEnd.Y), FMath::Min(0.0, ScaledEnd.Z));
		const FIntVector Dims(FMath::RoundToInt(FMath::Abs(ScaledEnd.X) / 20),
			FMath::RoundToInt(FMath::Abs(ScaledEnd.Y) / 20), FMath::RoundToInt(FMath::Abs(ScaledEnd.Z) / 20));
		for (const EMeshVoxelFillMode Mode : {EMeshVoxelFillMode::SurfaceOnly, EMeshVoxelFillMode::Solid})
		{
			const TArray<FMeshVoxelPoint> Points = FMeshSamplingHelper::GenerateVoxelPointsFromStaticMesh(Mesh.Get(), Transform, 20, Mode, 0, 1000);
			const TArray<FMeshVoxelPoint> Repeated = FMeshSamplingHelper::GenerateVoxelPointsFromStaticMesh(Mesh.Get(), Transform, 20, Mode, 0, 1000);
			const int32 Volume = Dims.X * Dims.Y * Dims.Z;
			const int32 Interior = (Dims.X - 2) * (Dims.Y - 2) * (Dims.Z - 2);
			TestEqual(TEXT("封闭立方体体素数量"), Points.Num(), Mode == EMeshVoxelFillMode::Solid ? Volume : Volume - Interior);
			TestEqual(TEXT("重复调用数量稳定"), Repeated.Num(), Points.Num());
			int32 PreviousKey = -1;
			int32 SurfaceCount = 0;
			for (int32 Index = 0; Index < Points.Num(); ++Index)
			{
				const FMeshVoxelPoint& Point = Points[Index];
				const FIntVector Grid = Point.GridIndex;
				const int32 Key = Grid.X + Dims.X * (Grid.Y + Dims.Y * Grid.Z);
				TestTrue(TEXT("输出按Z/Y/X稳定排序且不重复"), Key > PreviousKey);
				PreviousKey = Key;
				const FVector LocalCenter = BoundsMin + (FVector(Grid) + FVector(0.5)) * 20;
				const FVector ExpectedPosition = Transform.GetRotation().RotateVector(LocalCenter) + Transform.GetTranslation();
				TestTrue(TEXT("缩放仅烘焙一次，旋转平移保持一致"), Point.Position.Equals(ExpectedPosition, 0.0001));
				TestTrue(TEXT("输出变换位置和旋转正确"), Point.Transform.GetLocation().Equals(Point.Position) && Point.Transform.GetRotation().Equals(Transform.GetRotation()));
				TestTrue(TEXT("体素输出单位缩放"), Point.Transform.GetScale3D().Equals(FVector::OneVector));
				TestTrue(TEXT("顶点颜色及内部传播保持红色"), Point.Color.Equals(FLinearColor::Red, 0.0001f));
				TestEqual(TEXT("Section材质索引"), Point.MaterialIndex, 0);
				TestEqual(TEXT("体素尺寸"), Point.VoxelSize, 20.f);
				const bool bExpectedSurface = Grid.X == 0 || Grid.Y == 0 || Grid.Z == 0 || Grid.X == Dims.X - 1 || Grid.Y == Dims.Y - 1 || Grid.Z == Dims.Z - 1;
				TestEqual(TEXT("表面与内部分类"), Point.bIsSurface, bExpectedSurface);
				SurfaceCount += Point.bIsSurface ? 1 : 0;
				if (Repeated.IsValidIndex(Index))
				{
					TestTrue(TEXT("重复输出一致"), Repeated[Index].GridIndex == Grid && Repeated[Index].Position == Point.Position && Repeated[Index].Color == Point.Color);
				}
			}
			TestEqual(TEXT("表面数量"), SurfaceCount, Volume - Interior);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMeshVoxelGuardTest,
	"XTools.PointSampling.Voxel.Guards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMeshVoxelGuardTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UStaticMesh> Mesh = MakeVoxelCube();
	AddExpectedError(TEXT("已提前停止扫描并截断结果"), EAutomationExpectedErrorFlags::Contains, 1);
	const auto Surface = FMeshSamplingHelper::GenerateVoxelPointsFromStaticMesh(Mesh.Get(), FTransform::Identity, 20, EMeshVoxelFillMode::SurfaceOnly, 0, 7);
	TestEqual(TEXT("稀疏表面限制"), Surface.Num(), 7);
	AddExpectedError(TEXT("内部填充输出达到MaxVoxelCount"), EAutomationExpectedErrorFlags::Contains, 1);
	const auto Solid = FMeshSamplingHelper::GenerateVoxelPointsFromStaticMesh(Mesh.Get(), FTransform::Identity, 20, EMeshVoxelFillMode::Solid, 0, 7);
	TestEqual(TEXT("内部输出限制"), Solid.Num(), 7);
	AddExpectedError(TEXT("请求LOD99被夹取"), EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("LOD回退仍可采样"), FMeshSamplingHelper::GenerateVoxelPointsFromStaticMesh(Mesh.Get(), FTransform::Identity, 20, EMeshVoxelFillMode::Solid, 99, 1000).Num(), 216);
	AddExpectedError(TEXT("超过运行时数组索引上限"), EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("超大Solid网格分配前拒绝"), FMeshSamplingHelper::GenerateVoxelPointsFromStaticMesh(Mesh.Get(), FTransform::Identity, 0.01f, EMeshVoxelFillMode::Solid, 0, 1000).Num(), 0);
	AddExpectedError(TEXT("超过保护上限1024 MiB"), EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("内存预算分配前拒绝"), FMeshSamplingHelper::GenerateVoxelPointsFromStaticMesh(Mesh.Get(), FTransform::Identity, 0.35f, EMeshVoxelFillMode::Solid, 0, 1000).Num(), 0);
	AddExpectedError(TEXT("缩放存在接近0的轴"), EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("退化变换拒绝"), FMeshSamplingHelper::GenerateVoxelPointsFromStaticMesh(Mesh.Get(), FTransform(FQuat::Identity, FVector::ZeroVector, FVector(1, 0, 1)), 20, EMeshVoxelFillMode::Solid, 0, 1000).Num(), 0);
	FStaticMeshLODResources& LOD = Mesh->GetRenderData()->LODResources[0];
	TArray<uint32> Indices;
	LOD.IndexBuffer.GetCopy(Indices);
	Indices.Append({0, 0, 0, 0, 1, 999});
	LOD.IndexBuffer.SetIndices(Indices, EIndexBufferStride::Force32Bit);
	LOD.Sections[0].NumTriangles = 14;
	AddExpectedError(TEXT("退化或面积过小的三角形"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("个无效三角形"), EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("无效及退化三角形不改变有效封闭网格"), FMeshSamplingHelper::GenerateVoxelPointsFromStaticMesh(Mesh.Get(), FTransform::Identity, 20, EMeshVoxelFillMode::Solid, 0, 1000).Num(), 216);
	return true;
}

#endif
