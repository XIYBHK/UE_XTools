/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "FormationSamplingLibrary.h"
#include "PointSamplingLibrary.h"
#include "Misc/AutomationTest.h"

namespace
{
	void TestFinitePoints(FAutomationTestBase& Test, const FString& Name, const TArray<FVector>& Points)
	{
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			Test.TestTrue(
				*FString::Printf(TEXT("%s 的点%d坐标应为有限数"), *Name, Index),
				FMath::IsFinite(Points[Index].X) &&
				FMath::IsFinite(Points[Index].Y) &&
				FMath::IsFinite(Points[Index].Z));
		}
	}

	void TestPointCount(FAutomationTestBase& Test, const FString& Name, const TArray<FVector>& Points, int32 ExpectedCount)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s 应返回目标点数"), *Name), Points.Num(), ExpectedCount);
		TestFinitePoints(Test, Name, Points);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationSampling_PrimitiveAlgorithms,
	"XTools.PointSampling.Formation.PrimitiveAlgorithms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationSampling_PrimitiveAlgorithms::RunTest(const FString& Parameters)
{
	constexpr EPoissonCoordinateSpace RawSpace = EPoissonCoordinateSpace::Raw;

	const TArray<FVector> SolidRectangle = UFormationSamplingLibrary::GenerateSolidRectangle(
		6, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 2, 3, 1.0f, RawSpace);
	TestPointCount(*this, TEXT("实心矩形"), SolidRectangle, 6);

	const TArray<FVector> HollowRectangle = UFormationSamplingLibrary::GenerateHollowRectangle(
		8, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 0, 0, 1.0f, RawSpace);
	TestPointCount(*this, TEXT("空心矩形"), HollowRectangle, 8);

	const TArray<FVector> SpiralRectangle = UFormationSamplingLibrary::GenerateSpiralRectangle(
		8, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 2.0f, 1.0f, RawSpace);
	TestPointCount(*this, TEXT("螺旋矩形"), SpiralRectangle, 8);
	if (SpiralRectangle.Num() == 8)
	{
		TestTrue(TEXT("螺旋矩形应从中心向外扩展"),
			SpiralRectangle[0].IsNearlyZero() &&
			SpiralRectangle.Last().SizeSquared() > SpiralRectangle[1].SizeSquared());
	}

	const TArray<FVector> SolidTriangle = UFormationSamplingLibrary::GenerateSolidTriangle(
		9, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, false, RawSpace);
	TestPointCount(*this, TEXT("实心三角形"), SolidTriangle, 9);

	const TArray<FVector> HollowTriangle = UFormationSamplingLibrary::GenerateHollowTriangle(
		9, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, false, RawSpace);
	TestPointCount(*this, TEXT("空心三角形"), HollowTriangle, 9);
	if (HollowTriangle.Num() == 9)
	{
		TestTrue(TEXT("空心三角形闭合采样不应重复首尾顶点"),
			!HollowTriangle[0].Equals(HollowTriangle.Last(), UE_KINDA_SMALL_NUMBER));
	}

	const TArray<FTransform> Circle = UFormationSamplingLibrary::GenerateCircle(
		12, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, false, false,
		ECircleDistributionMode::Uniform, 50.0f, 0.0f, true, RawSpace);
	TestEqual(TEXT("空心圆应返回目标点数"), Circle.Num(), 12);
	for (int32 Index = 0; Index < Circle.Num(); ++Index)
	{
		const FVector Direction = Circle[Index].GetLocation().GetSafeNormal();
		TestTrue(*FString::Printf(TEXT("空心圆点%d应位于圆周"), Index),
			FMath::IsNearlyEqual(Circle[Index].GetLocation().Size(), 100.0f, UE_KINDA_SMALL_NUMBER));
		TestTrue(*FString::Printf(TEXT("空心圆点%d的变换应朝外"), Index),
			FVector::DotProduct(Circle[Index].GetRotation().RotateVector(FVector::ForwardVector), Direction) > 0.999f);
	}

	const TArray<FTransform> SolidCircle = UFormationSamplingLibrary::GenerateCircle(
		12, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, false, true,
		ECircleDistributionMode::Uniform, 50.0f, 0.0f, true, RawSpace);
	TestEqual(TEXT("实心圆应严格返回目标点数"), SolidCircle.Num(), 12);

	const TArray<FTransform> SolidSphere = UFormationSamplingLibrary::GenerateCircle(
		24, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, true, true,
		ECircleDistributionMode::Uniform, 50.0f, 0.0f, true, RawSpace,
		0.0f, 0, 0.0f, 0.0f, 1, EPointArrayOrderMode::SphereBottomToTopClockwise);
	TestEqual(TEXT("实心球应严格返回目标点数"), SolidSphere.Num(), 24);
	if (SolidSphere.Num() == 24)
	{
		TestTrue(TEXT("球体按层排序应从下方开始"),
			SolidSphere[0].GetLocation().Z <= SolidSphere.Last().GetLocation().Z);
	}

	const TArray<FTransform> SolidSphereWithDefaultMinimum = UFormationSamplingLibrary::GenerateCircle(
		12, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, true, true,
		ECircleDistributionMode::Uniform, 50.0f, 0.0f, true, RawSpace);
	TestEqual(TEXT("实心球默认最小点数不应突破总点数"), SolidSphereWithDefaultMinimum.Num(), 12);

	const TArray<FTransform> LayerSpacedSolidSphere = UFormationSamplingLibrary::GenerateCircle(
		100, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, true, true,
		ECircleDistributionMode::Uniform, 50.0f, 0.0f, true, RawSpace,
		0.0f, 0, 25.0f, 50.0f, 1, EPointArrayOrderMode::SphereBottomToTopClockwise);
	TestEqual(TEXT("指定实心球层间距应保持总点数"), LayerSpacedSolidSphere.Num(), 100);
	TArray<float> LayerHeights;
	for (const FTransform& Transform : LayerSpacedSolidSphere)
	{
		const FVector Location = Transform.GetLocation();
		TestTrue(TEXT("实心球点位应位于球体边界内"), Location.Size() <= 100.01f);

		bool bKnownLayer = false;
		for (const float Height : LayerHeights)
		{
			bKnownLayer |= FMath::IsNearlyEqual(Location.Z, Height, UE_KINDA_SMALL_NUMBER);
		}
		if (!bKnownLayer)
		{
			LayerHeights.Add(Location.Z);
		}

		if (FMath::IsNearlyZero(Location.Z, UE_KINDA_SMALL_NUMBER))
		{
			const float RingRadius = FVector(Location.X, Location.Y, 0.0f).Size();
			TestTrue(TEXT("实心球中心截面的环间距应可控"),
				FMath::IsNearlyEqual(RingRadius / 25.0f, FMath::RoundToFloat(RingRadius / 25.0f), UE_KINDA_SMALL_NUMBER));
		}
	}
	TestEqual(TEXT("指定50厘米实心球层间距应生成5个纬度层"), LayerHeights.Num(), 5);

	const TArray<FVector> Snowflake = UFormationSamplingLibrary::GenerateSnowflake(
		12, FVector::ZeroVector, FRotator::ZeroRotator, 150.0f, 3, 50.0f, RawSpace);
	TestPointCount(*this, TEXT("雪花形"), Snowflake, 12);

	const TArray<FVector> SnowflakeArc = UFormationSamplingLibrary::GenerateSnowflakeArc(
		12, FVector::ZeroVector, FRotator::ZeroRotator, 150.0f, 3, 50.0f, 180.0f, 0.0f, RawSpace);
	TestPointCount(*this, TEXT("雪花弧形"), SnowflakeArc, 12);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationSampling_MilitaryAlgorithms,
	"XTools.PointSampling.Formation.MilitaryAlgorithms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationSampling_MilitaryAlgorithms::RunTest(const FString& Parameters)
{
	constexpr EPoissonCoordinateSpace RawSpace = EPoissonCoordinateSpace::Raw;
	const TArray<FVector> Wedge = UFormationSamplingLibrary::GenerateWedgeFormation(
		5, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 60.0f, RawSpace);
	TestPointCount(*this, TEXT("楔形阵"), Wedge, 5);
	if (Wedge.Num() >= 3)
	{
		TestTrue(TEXT("楔形阵应从尖端向两侧展开"), Wedge[0].IsNearlyZero() && Wedge[1].Y < 0.0f && Wedge[2].Y > 0.0f);
	}

	const TArray<FVector> Column = UFormationSamplingLibrary::GenerateColumnFormation(
		5, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, RawSpace);
	TestPointCount(*this, TEXT("纵队阵"), Column, 5);
	for (const FVector& Point : Column)
	{
		TestTrue(TEXT("纵队阵应保持在Y轴"), FMath::IsNearlyZero(Point.X) && FMath::IsNearlyZero(Point.Z));
	}

	const TArray<FVector> Line = UFormationSamplingLibrary::GenerateLineFormation(
		5, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, RawSpace);
	TestPointCount(*this, TEXT("横队阵"), Line, 5);
	for (const FVector& Point : Line)
	{
		TestTrue(TEXT("横队阵应保持在X轴"), FMath::IsNearlyZero(Point.Y) && FMath::IsNearlyZero(Point.Z));
	}

	const TArray<FVector> Vee = UFormationSamplingLibrary::GenerateVeeFormation(
		5, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 60.0f, RawSpace);
	TestPointCount(*this, TEXT("V形阵"), Vee, 5);
	if (Vee.Num() >= 3)
	{
		TestTrue(TEXT("V形阵应与楔形阵具有相反的侧向展开"),
			Vee[1].Y > 0.0f && Vee[2].Y < 0.0f);
	}

	const TArray<FVector> RightEchelon = UFormationSamplingLibrary::GenerateEchelonFormation(
		6, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 1, 30.0f, RawSpace);
	const TArray<FVector> LeftEchelon = UFormationSamplingLibrary::GenerateEchelonFormation(
		6, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, -1, 30.0f, RawSpace);
	TestPointCount(*this, TEXT("右梯形阵"), RightEchelon, 6);
	TestPointCount(*this, TEXT("左梯形阵"), LeftEchelon, 6);
	if (RightEchelon.Num() >= 4 && LeftEchelon.Num() >= 4)
	{
		TestTrue(TEXT("左右梯形阵第二层偏移方向应相反"),
			RightEchelon[3].X > LeftEchelon[3].X);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationSampling_GeometricAlgorithms,
	"XTools.PointSampling.Formation.GeometricAlgorithms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationSampling_GeometricAlgorithms::RunTest(const FString& Parameters)
{
	constexpr EPoissonCoordinateSpace RawSpace = EPoissonCoordinateSpace::Raw;
	const TArray<FVector> HexagonalGrid = UFormationSamplingLibrary::GenerateHexagonalGrid(
		7, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 1, RawSpace);
	TestPointCount(*this, TEXT("蜂巢阵"), HexagonalGrid, 7);

	const TArray<FVector> Star = UFormationSamplingLibrary::GenerateStarFormation(
		10, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 50.0f, 5, RawSpace);
	TestPointCount(*this, TEXT("星形阵"), Star, 10);
	if (Star.Num() >= 2)
	{
		TestTrue(TEXT("星形阵应交替使用内外半径"),
			FMath::IsNearlyEqual(Star[0].Size(), 100.0f, UE_KINDA_SMALL_NUMBER) &&
			FMath::IsNearlyEqual(Star[1].Size(), 50.0f, UE_KINDA_SMALL_NUMBER));
	}

	const TArray<FVector> Archimedean = UFormationSamplingLibrary::GenerateArchimedeanSpiral(
		8, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 2.0f, RawSpace);
	TestPointCount(*this, TEXT("阿基米德螺旋"), Archimedean, 8);
	if (Archimedean.Num() >= 2)
	{
		TestTrue(TEXT("阿基米德螺旋应由中心向外扩展"),
			Archimedean[0].IsNearlyZero() && Archimedean.Last().Size() > Archimedean[1].Size());
	}

	const TArray<FVector> Logarithmic = UFormationSamplingLibrary::GenerateLogarithmicSpiral(
		8, FVector::ZeroVector, FRotator::ZeroRotator, 1.1f, 20.0f, RawSpace);
	TestPointCount(*this, TEXT("对数螺旋"), Logarithmic, 8);

	const TArray<FVector> Heart = UFormationSamplingLibrary::GenerateHeartFormation(
		8, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, RawSpace);
	TestPointCount(*this, TEXT("心形阵"), Heart, 8);

	const TArray<FVector> Flower = UFormationSamplingLibrary::GenerateFlowerFormation(
		10, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 50.0f, 5, RawSpace);
	TestPointCount(*this, TEXT("花瓣阵"), Flower, 10);

	const TArray<FVector> GoldenSpiral = UFormationSamplingLibrary::GenerateGoldenSpiralFormation(
		8, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, RawSpace);
	TestPointCount(*this, TEXT("黄金螺旋"), GoldenSpiral, 8);
	if (GoldenSpiral.Num() >= 2)
	{
		TestTrue(TEXT("黄金螺旋应从中心扩展到最大半径"),
			GoldenSpiral[0].IsNearlyZero() &&
			FMath::IsNearlyEqual(GoldenSpiral.Last().Size(), 100.0f, UE_KINDA_SMALL_NUMBER));
	}

	const TArray<FVector> CircularGrid = UFormationSamplingLibrary::GenerateCircularGridFormation(
		12, FVector::ZeroVector, FRotator::ZeroRotator, 120.0f, 3, 4, RawSpace);
	TestPointCount(*this, TEXT("圆形网格"), CircularGrid, 12);

	const TArray<FVector> RoseCurve = UFormationSamplingLibrary::GenerateRoseCurveFormation(
		9, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 3, RawSpace);
	TestPointCount(*this, TEXT("玫瑰曲线"), RoseCurve, 9);

	const TArray<int32> AutomaticRings;
	const TArray<FVector> ConcentricRings = UFormationSamplingLibrary::GenerateConcentricRingsFormation(
		12, FVector::ZeroVector, FRotator::ZeroRotator, AutomaticRings, 120.0f, 3, RawSpace);
	TestPointCount(*this, TEXT("同心圆环"), ConcentricRings, 12);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationSampling_SortingAlgorithms,
	"XTools.PointSampling.Formation.SortingAlgorithms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationSampling_SortingAlgorithms::RunTest(const FString& Parameters)
{
	const TArray<FVector> GridPoints = {
		FVector(0.0f, 10.0f, 0.0f),
		FVector(10.0f, -10.0f, 0.0f),
		FVector(-10.0f, -10.0f, 0.0f),
		FVector(0.0f, -10.0f, 0.0f)
	};
	const TArray<FVector> GridSorted = UFormationSamplingLibrary::SortPointArray(
		GridPoints, EPointArrayOrderMode::GridBottomToTopLeftToRight);
	TestEqual(TEXT("网格排序应保留全部点位"), GridSorted.Num(), GridPoints.Num());
	if (GridSorted.Num() == GridPoints.Num())
	{
		TestTrue(TEXT("网格排序应从下至上且每行从左至右"),
			GridSorted[0].Equals(FVector(-10.0f, -10.0f, 0.0f)) &&
			GridSorted[1].Equals(FVector(0.0f, -10.0f, 0.0f)) &&
			GridSorted[2].Equals(FVector(10.0f, -10.0f, 0.0f)));
	}

	const TArray<FVector> RingPoints = {
		FVector(0.0f, 10.0f, 0.0f),
		FVector(-10.0f, 0.0f, 0.0f),
		FVector(0.0f, -10.0f, 0.0f),
		FVector(10.0f, 0.0f, 0.0f)
	};
	const TArray<FVector> Clockwise = UFormationSamplingLibrary::SortPointArray(
		RingPoints, EPointArrayOrderMode::CircleClockwise);
	TestEqual(TEXT("圆形排序应保留全部点位"), Clockwise.Num(), RingPoints.Num());
	if (Clockwise.Num() == RingPoints.Num())
	{
		TestTrue(TEXT("圆形顺时针排序应从起始角开始"),
			Clockwise[0].Equals(FVector(10.0f, 0.0f, 0.0f)) &&
			Clockwise[1].Equals(FVector(0.0f, -10.0f, 0.0f)) &&
			Clockwise[2].Equals(FVector(-10.0f, 0.0f, 0.0f)));
	}

	TArray<FTransform> Transforms;
	for (const FVector& Point : RingPoints)
	{
		Transforms.Emplace(FRotator::ZeroRotator, Point);
	}
	const TArray<FTransform> ReversedTransforms = UFormationSamplingLibrary::SortTransformArray(
		Transforms, EPointArrayOrderMode::CircleClockwise, FVector::ZeroVector,
		FRotator::ZeroRotator, 0.0f, 1.0f, true);
	TestEqual(TEXT("变换排序应保留全部变换"), ReversedTransforms.Num(), Transforms.Num());
	if (ReversedTransforms.Num() == Transforms.Num())
	{
		TestTrue(TEXT("反转索引应反转完整排序结果"),
			ReversedTransforms[0].GetLocation().Equals(FVector(0.0f, 10.0f, 0.0f)));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationSampling_GenericDispatcher,
	"XTools.PointSampling.Formation.GenericDispatcher",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationSampling_GenericDispatcher::RunTest(const FString& Parameters)
{
	const TArray<EPointSamplingMode> Modes = {
		EPointSamplingMode::SolidRectangle,
		EPointSamplingMode::HollowRectangle,
		EPointSamplingMode::SpiralRectangle,
		EPointSamplingMode::SolidTriangle,
		EPointSamplingMode::HollowTriangle,
		EPointSamplingMode::Circle,
		EPointSamplingMode::Snowflake,
		EPointSamplingMode::SnowflakeArc,
		EPointSamplingMode::Wedge,
		EPointSamplingMode::Column,
		EPointSamplingMode::Line,
		EPointSamplingMode::Vee,
		EPointSamplingMode::Echelon,
		EPointSamplingMode::EchelonLeft,
		EPointSamplingMode::EchelonRight,
		EPointSamplingMode::HexagonalGrid,
		EPointSamplingMode::Star,
		EPointSamplingMode::ArchimedeanSpiral,
		EPointSamplingMode::LogarithmicSpiral,
		EPointSamplingMode::Heart,
		EPointSamplingMode::Flower,
		EPointSamplingMode::GoldenSpiral,
		EPointSamplingMode::CircularGrid,
		EPointSamplingMode::RoseCurve,
		EPointSamplingMode::ConcentricRings
	};

	for (const EPointSamplingMode Mode : Modes)
	{
		const TArray<FVector> Points = UPointSamplingLibrary::GenerateFormation(
			Mode, 12, FVector::ZeroVector, FRotator::ZeroRotator,
			EPoissonCoordinateSpace::Raw, 100.0f, 0.0f, 0, 3.0f, 50.0f, 5);
		TestTrue(*FString::Printf(TEXT("通用阵型模式%d应产生点位"), static_cast<int32>(Mode)), !Points.IsEmpty());
		TestFinitePoints(*this, FString::Printf(TEXT("通用阵型模式%d"), static_cast<int32>(Mode)), Points);
	}

	return true;
}

#endif
