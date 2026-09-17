/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Algo/AllOf.h"
#include "FormationLibrary.h"
#include "FormationMathUtils.h"
#include "Misc/AutomationTest.h"
#include <limits>

namespace
{
	void TestFormation(FAutomationTestBase& Test, const TCHAR* Name, const FFormationData& Formation, int32 ExpectedCount)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s 应生成目标单位数"), Name), Formation.Positions.Num(), ExpectedCount);
		for (int32 Index = 0; Index < Formation.Positions.Num(); ++Index)
		{
			const FVector& Position = Formation.Positions[Index];
			Test.TestTrue(*FString::Printf(TEXT("%s 的点%d应为有限平面坐标"), Name, Index),
				FMath::IsFinite(Position.X) && FMath::IsFinite(Position.Y) &&
				FMath::IsFinite(Position.Z) && FMath::IsNearlyZero(Position.Z));
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationLibrary_GeneratesBuiltInFormations,
	"XTools.Formation.Library.GeneratesBuiltInFormations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationLibrary_GeneratesBuiltInFormations::RunTest(const FString& Parameters)
{
	const FVector Center(10.0f, 20.0f, 30.0f);
	const FRotator Rotation(0.0f, 45.0f, 0.0f);

	const FFormationData Square = UFormationLibrary::CreateSquareFormation(Center, Rotation, 6, 100.0f);
	TestFormation(*this, TEXT("方形阵"), Square, 6);

	const FFormationData Circle = UFormationLibrary::CreateCircleFormation(Center, Rotation, 6, 100.0f, 0.0f, true);
	TestFormation(*this, TEXT("圆形阵"), Circle, 6);
	if (Circle.Positions.Num() == 6)
	{
		TestTrue(TEXT("圆形阵所有点应位于指定半径"),
			Algo::AllOf(Circle.Positions, [](const FVector& Position)
			{
				return FMath::IsNearlyEqual(Position.Size(), 100.0f, UE_KINDA_SMALL_NUMBER);
			}));
	}

	const FFormationData HorizontalLine = UFormationLibrary::CreateLineFormation(Center, Rotation, 6, 100.0f, false);
	const FFormationData VerticalLine = UFormationLibrary::CreateLineFormation(Center, Rotation, 6, 100.0f, true);
	TestFormation(*this, TEXT("横向线形阵"), HorizontalLine, 6);
	TestFormation(*this, TEXT("纵向线形阵"), VerticalLine, 6);
	if (HorizontalLine.Positions.Num() == 6 && VerticalLine.Positions.Num() == 6)
	{
		TestTrue(TEXT("横向线形阵应沿X轴"), FMath::IsNearlyZero(HorizontalLine.Positions[0].Y));
		TestTrue(TEXT("纵向线形阵应沿Y轴"), FMath::IsNearlyZero(VerticalLine.Positions[0].X));
	}

	const FFormationData Triangle = UFormationLibrary::CreateTriangleFormation(Center, Rotation, 6, 100.0f);
	const FFormationData Arrow = UFormationLibrary::CreateArrowFormation(Center, Rotation, 6, 100.0f);
	const FFormationData Spiral = UFormationLibrary::CreateSpiralFormation(Center, Rotation, 6, 100.0f, 2.0f);
	const FFormationData SolidCircle = UFormationLibrary::CreateSolidCircleFormation(Center, Rotation, 12, 100.0f);
	const FFormationData Zigzag = UFormationLibrary::CreateZigzagFormation(Center, Rotation, 6, 100.0f, 50.0f);
	TestFormation(*this, TEXT("三角形阵"), Triangle, 6);
	TestFormation(*this, TEXT("箭头阵"), Arrow, 6);
	TestFormation(*this, TEXT("螺旋阵"), Spiral, 6);
	TestFormation(*this, TEXT("实心圆阵"), SolidCircle, 12);
	TestFormation(*this, TEXT("折线阵"), Zigzag, 6);
	if (Spiral.Positions.Num() == 6)
	{
		TestTrue(TEXT("螺旋阵应由中心向外扩展"),
			Spiral.Positions[0].IsNearlyZero() && Spiral.Positions.Last().Size() > Spiral.Positions[1].Size());
	}

	const TArray<FVector> CustomPositions = { FVector(-50.0f, 0.0f, 0.0f), FVector(50.0f, 0.0f, 0.0f) };
	const FFormationData Custom = UFormationLibrary::CreateCustomFormation(Center, Rotation, CustomPositions);
	TestFormation(*this, TEXT("自定义阵"), Custom, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationLibrary_TransformsAndValidatesData,
	"XTools.Formation.Library.TransformsAndValidatesData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationLibrary_TransformsAndValidatesData::RunTest(const FString& Parameters)
{
	const FFormationData Formation = UFormationLibrary::CreateLineFormation(
		FVector::ZeroVector, FRotator::ZeroRotator, 3, 100.0f, false);
	FString ErrorMessage;
	TestTrue(TEXT("有效阵型数据应通过验证"), UFormationLibrary::ValidateFormationData(Formation, ErrorMessage));

	const FBox Bounds = UFormationLibrary::GetFormationBounds(Formation);
	TestTrue(TEXT("阵型包围盒应覆盖首尾位置"),
		Bounds.Min.Equals(FVector(-100.0f, 0.0f, 0.0f), UE_KINDA_SMALL_NUMBER) &&
		Bounds.Max.Equals(FVector(100.0f, 0.0f, 0.0f), UE_KINDA_SMALL_NUMBER));

	const FFormationData Scaled = UFormationLibrary::ScaleFormation(Formation, 2.0f);
	TestTrue(TEXT("缩放阵型应缩放相对位置和间距"),
		Scaled.Positions[0].Equals(FVector(-200.0f, 0.0f, 0.0f), UE_KINDA_SMALL_NUMBER) &&
		FMath::IsNearlyEqual(Scaled.Spacing, 200.0f));

	const FFormationData Rotated = UFormationLibrary::RotateFormation(Formation, FRotator(0.0f, 90.0f, 0.0f));
	TestTrue(TEXT("旋转阵型应合成阵型旋转"), Rotated.Rotation.Equals(FRotator(0.0f, 90.0f, 0.0f)));

	const FFormationData Moved = UFormationLibrary::MoveFormation(Formation, FVector(1000.0f, 0.0f, 0.0f));
	TestTrue(TEXT("移动阵型应只变更中心"), Moved.CenterLocation.Equals(FVector(1000.0f, 0.0f, 0.0f)));

	const FFormationData Resized = UFormationLibrary::ResizeFormation(Formation, 5);
	TestFormation(*this, TEXT("重设单位数的线形阵"), Resized, 5);

	TestTrue(TEXT("相同阵型的直接映射成本应为零"),
		FMath::IsNearlyZero(UFormationLibrary::CalculateTransitionCost(
			Formation, Formation, EFormationTransitionMode::DirectMapping)));
	TestTrue(TEXT("不同单位数阵型的转换成本应拒绝计算"),
		UFormationLibrary::CalculateTransitionCost(Formation, Resized) < 0.0f);

	FFormationData InvalidFormation;
	TestFalse(TEXT("空阵型数据应验证失败"), UFormationLibrary::ValidateFormationData(InvalidFormation, ErrorMessage));
	TestFalse(TEXT("空阵型验证应提供原因"), ErrorMessage.IsEmpty());
	TestTrue(TEXT("两个空阵型计算变换成本应拒绝计算"),
		UFormationLibrary::CalculateTransitionCost(InvalidFormation, InvalidFormation) < 0.0f);

	FFormationData NonFiniteFormation = Formation;
	NonFiniteFormation.Positions[0].X = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("含非有限位置的阵型应验证失败"),
		UFormationLibrary::ValidateFormationData(NonFiniteFormation, ErrorMessage));
	TestTrue(TEXT("含非有限位置的阵型成本应拒绝计算"),
		UFormationLibrary::CalculateTransitionCost(NonFiniteFormation, Formation) < 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationMathUtils_ProducesBoundedForces,
	"XTools.Formation.MathUtils.ProducesBoundedForces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationMathUtils_ProducesBoundedForces::RunTest(const FString& Parameters)
{
	FBoidsMovementParams Params;
	Params.SeparationRadius = 100.0f;
	Params.AlignmentRadius = 100.0f;
	Params.CohesionRadius = 100.0f;
	Params.MaxSpeed = 10.0f;
	Params.MaxSteerForce = 2.0f;

	const TArray<FVector> Positions = { FVector::ZeroVector, FVector(10.0f, 0.0f, 0.0f) };
	const TArray<FVector> Velocities = { FVector::ZeroVector, FVector(5.0f, 0.0f, 0.0f) };
	TestTrue(TEXT("交叉路径应被识别"),
		FFormationMathUtils::DoPathsIntersect(
			FVector(-10.0f, 0.0f, 0.0f), FVector(10.0f, 0.0f, 0.0f),
			FVector(0.0f, -10.0f, 0.0f), FVector(0.0f, 10.0f, 0.0f)));
	TestFalse(TEXT("平行分离路径不应相交"),
		FFormationMathUtils::DoPathsIntersect(
			FVector(-10.0f, 10.0f, 0.0f), FVector(10.0f, 10.0f, 0.0f),
			FVector(-10.0f, -10.0f, 0.0f), FVector(10.0f, -10.0f, 0.0f), 0.0f));

	TestTrue(TEXT("分离力应远离邻居"), FFormationMathUtils::CalculateSeparationForce(0, Positions, Params).X < 0.0f);
	TestTrue(TEXT("对齐力应朝邻居速度方向"), FFormationMathUtils::CalculateAlignmentForce(0, Positions, Velocities, Params).X > 0.0f);
	TestTrue(TEXT("凝聚力应朝邻居方向"), FFormationMathUtils::CalculateCohesionForce(0, Positions, Params).X > 0.0f);
	TestTrue(TEXT("趋近力应受最大转向力限制"),
		FFormationMathUtils::CalculateSeekForce(FVector::ZeroVector, FVector(100.0f, 0.0f, 0.0f), FVector::ZeroVector, Params).Size() <= Params.MaxSteerForce * Params.SeekWeight);
	TestTrue(TEXT("向量限幅应限制长度"),
		FMath::IsNearlyEqual(FFormationMathUtils::LimitVector(FVector(10.0f, 0.0f, 0.0f), 2.0f).Size(), 2.0f));
	TestTrue(TEXT("缓动函数应使用幂函数"),
		FMath::IsNearlyEqual(FFormationMathUtils::ApplyEasing(0.5f, 2.0f), 0.25f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationLibrary_RejectsNonFiniteGeneratorInputs,
	"XTools.Formation.Library.RejectsNonFiniteGeneratorInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationLibrary_RejectsNonFiniteGeneratorInputs::RunTest(const FString& Parameters)
{
	const FVector Center = FVector::ZeroVector;
	const FRotator Rotation = FRotator::ZeroRotator;
	const auto CheckEmpty = [this](const TCHAR* Case, float Invalid, int32 Count, const FFormationData& Formation)
	{
		const FString Context = FString::Printf(TEXT("%s Invalid=%g Count=%d"), Case, Invalid, Count);
		TestEqual(*(Context + TEXT(" 非法输入应返回空阵型")), Formation.Positions.Num(), 0);
		TestTrue(*(Context + TEXT(" 空阵型不可携带非有限元数据")),
			FMath::IsFinite(Formation.CenterLocation.X) && FMath::IsFinite(Formation.CenterLocation.Y)
			&& FMath::IsFinite(Formation.CenterLocation.Z) && FMath::IsFinite(Formation.Rotation.Pitch)
			&& FMath::IsFinite(Formation.Rotation.Yaw) && FMath::IsFinite(Formation.Rotation.Roll)
			&& FMath::IsFinite(Formation.Size.X) && FMath::IsFinite(Formation.Size.Y)
			&& FMath::IsFinite(Formation.Spacing));
	};
	const float InvalidValues[] = {std::numeric_limits<float>::quiet_NaN(),
		std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
	for (float Invalid : InvalidValues)
	{
		// 覆盖空、单点早退和真实三角函数生成路径。
		for (int32 Count : {0, 1, 6})
		{
			CheckEmpty(TEXT("Circle.Radius"), Invalid, Count, UFormationLibrary::CreateCircleFormation(Center, Rotation, Count, Invalid));
			CheckEmpty(TEXT("Circle.StartAngle"), Invalid, Count, UFormationLibrary::CreateCircleFormation(Center, Rotation, Count, 100.0f, Invalid));
			CheckEmpty(TEXT("Spiral.Radius"), Invalid, Count, UFormationLibrary::CreateSpiralFormation(Center, Rotation, Count, Invalid));
			CheckEmpty(TEXT("Spiral.Turns"), Invalid, Count, UFormationLibrary::CreateSpiralFormation(Center, Rotation, Count, 100.0f, Invalid));
		}
		CheckEmpty(TEXT("Square.Spacing"), Invalid, 6, UFormationLibrary::CreateSquareFormation(Center, Rotation, 6, Invalid));
		CheckEmpty(TEXT("Line.Spacing"), Invalid, 6, UFormationLibrary::CreateLineFormation(Center, Rotation, 6, Invalid));
		CheckEmpty(TEXT("Triangle.Spacing"), Invalid, 6, UFormationLibrary::CreateTriangleFormation(Center, Rotation, 6, Invalid));
		CheckEmpty(TEXT("Arrow.Spacing"), Invalid, 6, UFormationLibrary::CreateArrowFormation(Center, Rotation, 6, Invalid));
		CheckEmpty(TEXT("SolidCircle.Radius"), Invalid, 6, UFormationLibrary::CreateSolidCircleFormation(Center, Rotation, 6, Invalid));
		CheckEmpty(TEXT("Zigzag.Spacing"), Invalid, 6, UFormationLibrary::CreateZigzagFormation(Center, Rotation, 6, Invalid));
		CheckEmpty(TEXT("Zigzag.Amplitude"), Invalid, 6, UFormationLibrary::CreateZigzagFormation(Center, Rotation, 6, 100.0f, Invalid));
		FVector InvalidCenter = Center;
		InvalidCenter.Z = Invalid;
		FRotator InvalidRotation = Rotation;
		InvalidRotation.Yaw = Invalid;
		CheckEmpty(TEXT("Circle.Center.Z"), Invalid, 6, UFormationLibrary::CreateCircleFormation(InvalidCenter, Rotation, 6, 100.0f));
		CheckEmpty(TEXT("Spiral.Rotation.Yaw"), Invalid, 6, UFormationLibrary::CreateSpiralFormation(Center, InvalidRotation, 6, 100.0f));
		CheckEmpty(TEXT("Custom.Positions[1].Z"), Invalid, 2, UFormationLibrary::CreateCustomFormation(Center, Rotation, {Center, InvalidCenter}));
		CheckEmpty(TEXT("Custom.Center.Z"), Invalid, 1, UFormationLibrary::CreateCustomFormation(InvalidCenter, Rotation, {Center}));
		CheckEmpty(TEXT("Custom.Rotation.Yaw"), Invalid, 1, UFormationLibrary::CreateCustomFormation(Center, InvalidRotation, {Center}));
	}

	const FFormationData NegativeCircle = UFormationLibrary::CreateCircleFormation(Center, Rotation, 6, -100.0f, -30.0f);
	const FFormationData NegativeSpiral = UFormationLibrary::CreateSpiralFormation(Center, Rotation, 6, -100.0f, -2.0f);
	TestFormation(*this, TEXT("有限负半径圆阵"), NegativeCircle, 6);
	TestFormation(*this, TEXT("有限负半径和圈数螺旋"), NegativeSpiral, 6);
	TestTrue(TEXT("负半径元数据应保留既有语义"), NegativeCircle.Size.X == -200.0f && NegativeSpiral.Size.X == -200.0f);
	const FFormationData NegativeLine = UFormationLibrary::CreateLineFormation(Center, Rotation, 3, -100.0f);
	TestFormation(*this, TEXT("有限负间距线阵"), NegativeLine, 3);
	TestTrue(TEXT("有限负间距应保留镜像位置和元数据"),
		NegativeLine.Spacing == -100.0f && NegativeLine.Positions[0].X == 100.0);
	return true;
}

#endif
