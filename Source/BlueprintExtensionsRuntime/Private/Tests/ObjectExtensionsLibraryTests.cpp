/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Libraries/ObjectExtensionsLibrary.h"

#include "Curves/CurveFloat.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UObject/Class.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObjectExtensionsLibrary_FindsObjectsByClass,
	"XTools.BlueprintExtensionsRuntime.Object.FindsObjectsByClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectExtensionsLibrary_FindsObjectsByClass::RunTest(const FString& Parameters)
{
	struct FGetObjectFromMapParameters
	{
		TMap<UClass*, UObject*> FindMap;
		UClass* FindClass = nullptr;
		UObject* ReturnValue = nullptr;
	};

	UObject* FoundObject = NewObject<UCurveFloat>(GetTransientPackage());
	UObject* OtherObject = NewObject<UCurveFloat>(GetTransientPackage());
	const TMap<UClass*, UObject*> ObjectMap = {
		{ UObject::StaticClass(), FoundObject },
		{ AActor::StaticClass(), OtherObject }
	};
	UFunction* Function = UObjectExtensionsLibrary::StaticClass()->FindFunctionByName(FName(TEXT("GetObjectFromMap")));
	TestNotNull(TEXT("应找到对象查询反射函数"), Function);
	if (!Function)
	{
		return false;
	}
	TestEqual(TEXT("反射参数结构应匹配对象查询函数"), Function->ParmsSize, static_cast<int32>(sizeof(FGetObjectFromMapParameters)));
	if (Function->ParmsSize != sizeof(FGetObjectFromMapParameters))
	{
		return false;
	}

	auto FindObject = [&ObjectMap, Function](UClass* FindClass)
	{
		FGetObjectFromMapParameters CallParameters;
		CallParameters.FindMap = ObjectMap;
		CallParameters.FindClass = FindClass;
		UObjectExtensionsLibrary::StaticClass()->GetDefaultObject()->ProcessEvent(Function, &CallParameters);
		return CallParameters.ReturnValue;
	};

	TestTrue(TEXT("匹配类应返回对应对象"),
		FindObject(UObject::StaticClass()) == FoundObject);
	TestTrue(TEXT("另一匹配类应返回对应对象"),
		FindObject(AActor::StaticClass()) == OtherObject);
	TestNull(TEXT("未匹配类应返回空对象"),
		FindObject(UActorComponent::StaticClass()));
	TestNull(TEXT("空类查询应返回空对象"),
		FindObject(nullptr));

	return true;
}

#endif
