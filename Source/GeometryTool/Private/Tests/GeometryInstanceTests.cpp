/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "GeometryInstance.h"
#include "Components/BoxComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGeometryInstance_GeneratesConcentricCirclePoints,
	"XTools.GeometryTool.GeometryInstance.GeneratesConcentricCirclePoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGeometryInstance_GeneratesConcentricCirclePoints::RunTest(const FString& Parameters)
{
	UGeometryInstance* GeometryInstance = NewObject<UGeometryInstance>();
	TestNotNull(TEXT("几何实例组件应可创建"), GeometryInstance);
	if (!GeometryInstance)
	{
		return false;
	}

	const TArray<FTransform> Points = GeometryInstance->GetPointsByCircle(3, 0.0f, 3, 100.0f);
	TestEqual(TEXT("三层圆形点阵应返回中心、3点内环和6点外环"), Points.Num(), 10);
	if (Points.Num() == 10)
	{
		TestTrue(TEXT("圆形点阵应以中心点开始"), Points[0].GetLocation().IsNearlyZero());
		for (int32 Index = 1; Index < Points.Num(); ++Index)
		{
			const FVector Location = Points[Index].GetLocation();
			TestTrue(*FString::Printf(TEXT("圆形点阵点%d应为有限平面坐标"), Index),
				FMath::IsFinite(Location.X) && FMath::IsFinite(Location.Y) && FMath::IsNearlyZero(Location.Z));
		}
		TestTrue(TEXT("第一层圆环应使用半径增量"), FMath::IsNearlyEqual(Points[1].GetLocation().Size(), 100.0f));
		TestTrue(TEXT("第二层圆环应使用两倍半径增量"), FMath::IsNearlyEqual(Points.Last().GetLocation().Size(), 200.0f));
	}

	const TArray<FTransform> DegeneratePoints = GeometryInstance->GetPointsByCircle(2, 0.0f, 3, 100.0f);
	TestEqual(TEXT("初始环点不足时只保留中心点"), DegeneratePoints.Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGeometryInstance_BoxSamplingKeepsOneLayerPerAxisWhenDistanceExceedsLength,
	"XTools.GeometryTool.GeometryInstance.BoxSamplingKeepsOneLayerPerAxisWhenDistanceExceedsLength",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGeometryInstance_BoxSamplingKeepsOneLayerPerAxisWhenDistanceExceedsLength::RunTest(const FString& Parameters)
{
	UGeometryInstance* GeometryInstance = NewObject<UGeometryInstance>();
	TestNotNull(TEXT("几何实例组件应可创建"), GeometryInstance);
	if (!GeometryInstance)
	{
		return false;
	}

	// 几何条件：盒体全尺寸 (100,100,100)，采样间距 1000 —— 三个轴向长度均小于间距。
	// 每个轴应各保留一个采样层（floor(100/1000)+1=1），共 1 个点，不返回零层也不崩溃。
	UBoxComponent* SmallBox = NewObject<UBoxComponent>();
	SmallBox->SetBoxExtent(FVector(50.0f, 50.0f, 50.0f));
	const TArray<FTransform> SingleLayerPoints = GeometryInstance->GetPointsByShape(
		SmallBox, false, 1000.0f, 0.0f, false,
		FRotator::ZeroRotator, FRotator::ZeroRotator, false,
		FVector::OneVector, FVector::OneVector, false,
		FRotator::ZeroRotator, 0);

	TestEqual(TEXT("三轴长度均小于间距时应生成恰好一个点"), SingleLayerPoints.Num(), 1);
	if (SingleLayerPoints.Num() == 1)
	{
		const FVector Location = SingleLayerPoints[0].GetLocation();
		TestTrue(TEXT("唯一采样点应位于盒体中心且坐标有限"),
			FMath::IsFinite(Location.X) && FMath::IsFinite(Location.Y) && FMath::IsFinite(Location.Z) &&
			Location.IsNearlyZero());
	}

	// 对照：仅 Y 轴长度 1500 超过间距 1000（X/Z 仍为 100）。
	// X、Z 轴各保留 1 层，Y 轴为 floor(1500/1000)+1=2 层，共 2 个点，间距沿 Y 对称分布。
	UBoxComponent* LongBox = NewObject<UBoxComponent>();
	LongBox->SetBoxExtent(FVector(50.0f, 750.0f, 50.0f));
	const TArray<FTransform> TwoLayerPoints = GeometryInstance->GetPointsByShape(
		LongBox, false, 1000.0f, 0.0f, false,
		FRotator::ZeroRotator, FRotator::ZeroRotator, false,
		FVector::OneVector, FVector::OneVector, false,
		FRotator::ZeroRotator, 0);

	TestEqual(TEXT("仅一轴超过间距时该轴应生成两层，其余轴各一层"), TwoLayerPoints.Num(), 2);
	if (TwoLayerPoints.Num() == 2)
	{
		const FVector First = TwoLayerPoints[0].GetLocation();
		const FVector Second = TwoLayerPoints[1].GetLocation();
		TestTrue(TEXT("两层采样点应沿Y轴对称分布在±500"),
			FMath::IsNearlyEqual(First.Y, -500.0f) && FMath::IsNearlyZero(First.X) && FMath::IsNearlyZero(First.Z) &&
			FMath::IsNearlyEqual(Second.Y, 500.0f) && FMath::IsNearlyZero(Second.X) && FMath::IsNearlyZero(Second.Z));
	}

	return true;
}

#endif
