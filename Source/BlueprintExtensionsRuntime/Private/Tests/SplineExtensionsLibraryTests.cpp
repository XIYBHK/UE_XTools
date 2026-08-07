/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Components/SplineComponent.h"
#include "Libraries/SplineExtensionsLibrary.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSplineExtensionsLibrary_QueriesAndSimplifiesSpline,
	"XTools.BlueprintExtensionsRuntime.Spline.QueriesAndSimplifiesSpline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSplineExtensionsLibrary_QueriesAndSimplifiesSpline::RunTest(const FString& Parameters)
{
	USplineComponent* Spline = NewObject<USplineComponent>(GetTransientPackage());
	TestNotNull(TEXT("应创建瞬态样条组件"), Spline);
	if (!Spline)
	{
		return false;
	}

	TestFalse(TEXT("空指针样条不应视为有效路径"), USplineExtensionsLibrary::SplinePathValid(nullptr));
	TestTrue(TEXT("空指针样条起点应为零"), USplineExtensionsLibrary::GetSplineStart(nullptr).IsNearlyZero());

	const TArray<FVector> CollinearPath = {
		FVector(0.0f, 0.0f, 0.0f),
		FVector(50.0f, 0.0f, 0.0f),
		FVector(100.0f, 0.0f, 0.0f)
	};
	USplineExtensionsLibrary::SimplifySpline(Spline, CollinearPath);

	TestTrue(TEXT("多点样条应视为有效路径"), USplineExtensionsLibrary::SplinePathValid(Spline));
	TestTrue(TEXT("样条起点应与输入一致"), USplineExtensionsLibrary::GetSplineStart(Spline).Equals(CollinearPath[0], UE_KINDA_SMALL_NUMBER));
	TestTrue(TEXT("样条终点应与输入一致"), USplineExtensionsLibrary::GetSplineEnd(Spline).Equals(CollinearPath.Last(), UE_KINDA_SMALL_NUMBER));

	const TArray<FVector> Path = USplineExtensionsLibrary::GetSplinePath(Spline);
	TestEqual(TEXT("路径查询应返回简化后的样条点"), Path.Num(), 2);
	TestTrue(TEXT("共线中间点应被简化"), Spline->GetNumberOfSplinePoints() == 2);

	return true;
}

#endif
