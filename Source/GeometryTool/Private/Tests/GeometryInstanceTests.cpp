/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "GeometryInstance.h"
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

#endif
