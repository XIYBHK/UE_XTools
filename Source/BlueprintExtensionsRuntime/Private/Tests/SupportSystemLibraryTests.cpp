/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Features/SupportSystemLibrary.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSupportSystemLibrary_TransformsFulcrumsToWorldSpace,
	"XTools.BlueprintExtensionsRuntime.Support.TransformsFulcrumsToWorldSpace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSupportSystemLibrary_TransformsFulcrumsToWorldSpace::RunTest(const FString& Parameters)
{
	const FTransform ObjectTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(100.0f, 200.0f, 300.0f));
	const TArray<FTransform> LocalFulcrums = {
		FTransform(FVector(50.0f, 0.0f, -20.0f)),
		FTransform(FVector(-50.0f, 0.0f, -20.0f))
	};

	const TArray<FTransform> WorldFulcrums = USupportSystemLibrary::GetWorldFulcrumTransform(ObjectTransform, LocalFulcrums);
	TestEqual(TEXT("世界支点数量应与局部支点一致"), WorldFulcrums.Num(), 2);
	if (WorldFulcrums.Num() == 2)
	{
		TestTrue(TEXT("支点应应用对象的旋转和平移"),
			WorldFulcrums[0].GetLocation().Equals(FVector(100.0f, 250.0f, 280.0f), UE_KINDA_SMALL_NUMBER));
		TestTrue(TEXT("支点相对位置应保持镜像"),
			WorldFulcrums[1].GetLocation().Equals(FVector(100.0f, 150.0f, 280.0f), UE_KINDA_SMALL_NUMBER));
	}

	FTransform InvalidTransform = FTransform::Identity;
	InvalidTransform.SetTranslation(FVector(NAN, 0.0f, 0.0f));
	TestEqual(TEXT("非法对象变换应返回空结果"),
		USupportSystemLibrary::GetWorldFulcrumTransform(InvalidTransform, LocalFulcrums).Num(), 0);

	return true;
}

#endif
