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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGeometryInstance_UsesContinuousRandomRanges,
	"XTools.GeometryTool.GeometryInstance.UsesContinuousRandomRanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGeometryInstance_UsesContinuousRandomRanges::RunTest(const FString& Parameters)
{
	UGeometryInstance* GeometryInstance = NewObject<UGeometryInstance>();
	UBoxComponent* Box = NewObject<UBoxComponent>();
	Box->SetBoxExtent(FVector(50.0f));
	if (!TestNotNull(TEXT("几何实例组件应可创建"), GeometryInstance) ||
		!TestNotNull(TEXT("盒体组件应可创建"), Box))
	{
		return false;
	}

	const TArray<FTransform> ForwardRange = GeometryInstance->GetPointsByShape(
		Box, false, 1000.0f, 0.25f, false,
		FRotator(0.25f, 0.25f, 0.25f), FRotator(0.75f, 0.75f, 0.75f), true,
		FVector(0.8f), FVector(1.2f), true, FRotator::ZeroRotator, 17);

	TestEqual(TEXT("单层盒体应生成一个采样点"), ForwardRange.Num(), 1);
	if (ForwardRange.Num() != 1)
	{
		return false;
	}

	const FTransform& Transform = ForwardRange[0];
	const FVector Location = Transform.GetLocation();
	const FVector Scale = Transform.GetScale3D();
	const FRotator Rotation = Transform.Rotator();
	TestTrue(TEXT("噪声应保持在小数范围内且不退化为整型量化"),
		Location.SizeSquared() > 0.0f && Location.SizeSquared() <= FMath::Square(0.25f) * 3.0f);
	TestTrue(TEXT("随机缩放应落在 0.8 到 1.2 的连续范围"),
		Scale.X >= 0.8f && Scale.X <= 1.2f && Scale.Y >= 0.8f && Scale.Y <= 1.2f &&
		Scale.Z >= 0.8f && Scale.Z <= 1.2f);
	TestTrue(TEXT("随机旋转应落在 0.25 到 0.75 度的连续范围"),
		Rotation.Pitch >= 0.25f && Rotation.Pitch <= 0.75f &&
		Rotation.Yaw >= 0.25f && Rotation.Yaw <= 0.75f &&
		Rotation.Roll >= 0.25f && Rotation.Roll <= 0.75f);

	const TArray<FTransform> ReversedRange = GeometryInstance->GetPointsByShape(
		Box, false, 1000.0f, 0.0f, false,
		FRotator(2.0f, 2.0f, 2.0f), FRotator(-2.0f, -2.0f, -2.0f), true,
		FVector(1.2f), FVector(0.8f), true, FRotator::ZeroRotator, 19);
	TestEqual(TEXT("反向随机范围仍应生成一个采样点"), ReversedRange.Num(), 1);
	if (ReversedRange.Num() == 1)
	{
		const FTransform& ReversedTransform = ReversedRange[0];
		const FVector ReversedScale = ReversedTransform.GetScale3D();
		const FRotator ReversedRotation = ReversedTransform.Rotator();
		TestTrue(TEXT("反向缩放范围应规范化为 0.8 到 1.2"),
			ReversedScale.X >= 0.8f && ReversedScale.X <= 1.2f &&
			ReversedScale.Y >= 0.8f && ReversedScale.Y <= 1.2f &&
			ReversedScale.Z >= 0.8f && ReversedScale.Z <= 1.2f);
		TestTrue(TEXT("反向旋转范围应规范化为 -2 到 2 度"),
			ReversedRotation.Pitch >= -2.0f && ReversedRotation.Pitch <= 2.0f &&
			ReversedRotation.Yaw >= -2.0f && ReversedRotation.Yaw <= 2.0f &&
			ReversedRotation.Roll >= -2.0f && ReversedRotation.Roll <= 2.0f);
	}

	return true;
}

#endif
