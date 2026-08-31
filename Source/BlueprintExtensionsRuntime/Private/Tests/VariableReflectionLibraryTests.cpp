/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Libraries/VariableReflectionLibrary.h"

#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVariableReflectionLibrary_RejectsPartiallyParsedValues,
	"XTools.BlueprintExtensionsRuntime.Variable.RejectsPartiallyParsedValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVariableReflectionLibrary_RejectsPartiallyParsedValues::RunTest(const FString& Parameters)
{
	AActor* Actor = NewObject<AActor>(GetTransientPackage());
	Actor->CustomTimeDilation = 1.0f;

	UVariableReflectionLibrary::SetValueByString(Actor, TEXT("CustomTimeDilation"), TEXT("2.5"));
	TestEqual(TEXT("完整数值文本应成功写入"), Actor->CustomTimeDilation, 2.5f);

	AddExpectedError(TEXT("按字符串设置变量失败"), EAutomationExpectedErrorFlags::Contains, 2);
	UVariableReflectionLibrary::SetValueByString(Actor, TEXT("CustomTimeDilation"), TEXT("3.5invalid"));
	TestEqual(TEXT("部分解析失败不应修改原值"), Actor->CustomTimeDilation, 2.5f);

	UVariableReflectionLibrary::SetValueByString(Actor, TEXT("CustomTimeDilation"), TEXT("4.5  \t"));
	TestEqual(TEXT("尾随空白不应阻止合法写入"), Actor->CustomTimeDilation, 4.5f);
	return true;
}

#endif
