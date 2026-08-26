/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "FormationSamplingLibrary.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRectangleGridSampling_GeneratesIndependentSpacing,
	"XTools.PointSampling.RectangleGrid.GeneratesIndependentSpacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRectangleGridSampling_GeneratesIndependentSpacing::RunTest(const FString& Parameters)
{
	const TArray<FVector> Points = UFormationSamplingLibrary::GenerateRectangleGrid(
		2,
		3,
		100.0f,
		200.0f,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EPoissonCoordinateSpace::Raw);

	const TArray<FVector> ExpectedPoints = {
		FVector(-100.0, -100.0, 0.0),
		FVector(0.0, -100.0, 0.0),
		FVector(100.0, -100.0, 0.0),
		FVector(-100.0, 100.0, 0.0),
		FVector(0.0, 100.0, 0.0),
		FVector(100.0, 100.0, 0.0)
	};

	TestEqual(TEXT("2行3列应生成6个点"), Points.Num(), ExpectedPoints.Num());
	for (int32 Index = 0; Index < FMath::Min(Points.Num(), ExpectedPoints.Num()); ++Index)
	{
		TestTrue(
			FString::Printf(TEXT("点%d应使用独立横纵间距并保持居中"), Index),
			Points[Index].Equals(ExpectedPoints[Index], UE_KINDA_SMALL_NUMBER));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRectangleGridSampling_AppliesTransformAndRejectsInvalidInput,
	"XTools.PointSampling.RectangleGrid.AppliesTransformAndRejectsInvalidInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRectangleGridSampling_AppliesTransformAndRejectsInvalidInput::RunTest(const FString& Parameters)
{
	const TArray<FVector> TransformedPoints = UFormationSamplingLibrary::GenerateRectangleGrid(
		1,
		2,
		100.0f,
		200.0f,
		FVector(10.0, 20.0, 30.0),
		FRotator(0.0, 90.0, 0.0),
		EPoissonCoordinateSpace::World);

	TestEqual(TEXT("1行2列应生成2个点"), TransformedPoints.Num(), 2);
	if (TransformedPoints.Num() == 2)
	{
		TestTrue(TEXT("第一个点应应用世界旋转和偏移"),
			TransformedPoints[0].Equals(FVector(10.0, -30.0, 30.0), UE_KINDA_SMALL_NUMBER));
		TestTrue(TEXT("第二个点应应用世界旋转和偏移"),
			TransformedPoints[1].Equals(FVector(10.0, 70.0, 30.0), UE_KINDA_SMALL_NUMBER));
	}

	TestTrue(TEXT("行数为0时应返回空数组"),
		UFormationSamplingLibrary::GenerateRectangleGrid(
			0, 2, 100.0f, 100.0f, FVector::ZeroVector, FRotator::ZeroRotator,
			EPoissonCoordinateSpace::Raw).IsEmpty());
	TestTrue(TEXT("横向间距为0时应返回空数组"),
		UFormationSamplingLibrary::GenerateRectangleGrid(
			2, 2, 0.0f, 100.0f, FVector::ZeroVector, FRotator::ZeroRotator,
			EPoissonCoordinateSpace::Raw).IsEmpty());
	TestTrue(TEXT("点数乘积超过int32容量时应在分配前返回空数组"),
		UFormationSamplingLibrary::GenerateRectangleGrid(
			50000, 50000, 100.0f, 100.0f, FVector::ZeroVector, FRotator::ZeroRotator,
			EPoissonCoordinateSpace::Raw).IsEmpty());

	return true;
}

#endif
