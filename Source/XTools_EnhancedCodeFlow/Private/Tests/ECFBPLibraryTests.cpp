// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "BP/ECFBPLibrary.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FECFBPLibrary_ValidatesInstanceIdsAndConvertsSoftPaths,
	"XTools.EnhancedCodeFlow.BlueprintLibrary.ValidatesInstanceIdsAndConvertsSoftPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FECFBPLibrary_ValidatesInstanceIdsAndConvertsSoftPaths::RunTest(const FString& Parameters)
{
	FECFHandleBP InvalidHandle;
	bool bHandleValid = true;
	UECFBPLibrary::IsECFHandleValid(bHandleValid, InvalidHandle);
	TestFalse(TEXT("默认ECF句柄应无效"), bHandleValid);

	FECFInstanceIdBP InstanceId;
	FECFInstanceIdBP ValidatedInstanceId;
	UECFBPLibrary::ECFValidateInstanceId(InstanceId, ValidatedInstanceId);
	bool bInstanceIdValid = false;
	UECFBPLibrary::IsECFInstanceIdValid(bInstanceIdValid, InstanceId);
	TestTrue(TEXT("验证实例ID应为无效输入生成有效ID"), bInstanceIdValid);
	TestFalse(TEXT("有效实例ID的字符串表示不应为空"), UECFBPLibrary::Conv_ECFInstanceIdToString(ValidatedInstanceId).IsEmpty());

	const TArray<TSoftObjectPtr<UObject>> SoftObjects = {
		TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Test/AssetA.AssetA"))),
		TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Test/AssetB.AssetB")))
	};
	const TArray<FSoftObjectPath> Paths = UECFBPLibrary::ConvertSoftObjectPtrToSoftPath(SoftObjects);
	TestEqual(TEXT("软对象转换应保留数组数量"), Paths.Num(), SoftObjects.Num());
	if (Paths.Num() == SoftObjects.Num())
	{
		TestEqual(TEXT("软对象转换应保留第一个路径"), Paths[0], SoftObjects[0].ToSoftObjectPath());
		TestEqual(TEXT("软对象转换应保留第二个路径"), Paths[1], SoftObjects[1].ToSoftObjectPath());
	}

	return true;
}

#endif
