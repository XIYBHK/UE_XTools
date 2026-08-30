/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_DEV_AUTOMATION_TESTS

#include "XToolsCore.h"

#include "Containers/Set.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FXToolsLogCategoriesCoverageTest,
	"XTools.Core.LogCategories.CoverageAndUniqueness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXToolsLogCategoriesCoverageTest::RunTest(const FString& Parameters)
{
	const TArray<FName>& Categories = FXToolsLogCategories::Get();
	TSet<FName> UniqueCategories;
	for (const FName Category : Categories)
	{
		UniqueCategories.Add(Category);
	}

	TestEqual(TEXT("统一日志分类清单不应包含重复项"), UniqueCategories.Num(), Categories.Num());
	TestTrue(TEXT("统一日志级别应覆盖 BlueprintAssist 结点轨道日志"),
		UniqueCategories.Contains(TEXT("LogKnotTrackCreator")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
