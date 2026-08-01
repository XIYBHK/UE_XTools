/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Libraries/TransformExtensionsLibrary.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTransformExtensionsLibrary_ExtractsTransformMembersAndAxes,
	"XTools.BlueprintExtensionsRuntime.Transform.ExtractsMembersAndAxes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTransformExtensionsLibrary_ExtractsTransformMembersAndAxes::RunTest(const FString& Parameters)
{
	const FTransform Transform(FRotator(0.0f, 90.0f, 0.0f), FVector(100.0f, -50.0f, 25.0f));

	TestTrue(TEXT("位置查询应返回变换平移"),
		UTransformExtensionsLibrary::TLocation(Transform).Equals(Transform.GetTranslation(), UE_KINDA_SMALL_NUMBER));
	TestTrue(TEXT("旋转查询应返回变换旋转"),
		UTransformExtensionsLibrary::TRotation(Transform).Equals(Transform.Rotator(), UE_KINDA_SMALL_NUMBER));
	TestTrue(TEXT("前向轴查询应返回X单位轴"),
		UTransformExtensionsLibrary::AxisForward(Transform).Equals(FVector::RightVector, UE_KINDA_SMALL_NUMBER));
	TestTrue(TEXT("右向轴查询应返回Y单位轴"),
		UTransformExtensionsLibrary::AxisRight(Transform).Equals(-FVector::ForwardVector, UE_KINDA_SMALL_NUMBER));
	TestTrue(TEXT("上向轴查询应返回Z单位轴"),
		UTransformExtensionsLibrary::AxisUp(Transform).Equals(FVector::UpVector, UE_KINDA_SMALL_NUMBER));

	return true;
}

#endif
