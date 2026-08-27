/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraphSchema_K2.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_MakeArray.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "RandomShuffleArrayLibrary.h"
#include "UObject/Package.h"

namespace
{
	UBlueprint* CreateTestBlueprint(const TCHAR* BaseName)
	{
		const FName BlueprintName = MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), FName(BaseName));
		return FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			GetTransientPackage(),
			BlueprintName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			NAME_None);
	}

	FEdGraphPinType MakeIntArrayPinType()
	{
		FEdGraphPinType PinType;
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		PinType.ContainerType = EPinContainerType::Array;
		return PinType;
	}

	bool ContainsOnly(const TArray<int32>& Values, const TArray<int32>& AllowedValues)
	{
		for (const int32 Value : Values)
		{
			if (!AllowedValues.Contains(Value))
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRandomShuffleBlueprint_ExecutesCustomThunkArrayNode,
	"XTools.BlueprintExtensions.RandomShuffle.ExecutesCustomThunkArrayNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRandomShuffleBlueprint_ExecutesCustomThunkArrayNode::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = CreateTestBlueprint(TEXT("XToolsRandomShuffleBlueprintTest"));
	TestNotNull(TEXT("应创建瞬态测试蓝图"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		TEXT("RunRandomSample"),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass());
	TestNotNull(TEXT("应创建测试函数图"), FunctionGraph);
	if (!FunctionGraph)
	{
		return false;
	}
	FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, FunctionGraph, true, nullptr);

	UK2Node_EditablePinBase* Entry = FBlueprintEditorUtils::GetEntryNode(FunctionGraph);
	UK2Node_FunctionResult* Result = FBlueprintEditorUtils::FindOrCreateFunctionResultNode(Entry);
	TestNotNull(TEXT("应创建函数入口和返回节点"), Entry);
	TestNotNull(TEXT("应创建函数返回节点"), Result);
	if (!Entry || !Result)
	{
		return false;
	}

	const FEdGraphPinType IntArrayType = MakeIntArrayPinType();
	UEdGraphPin* ReturnPin = Result->CreateUserDefinedPin(TEXT("ReturnValue"), IntArrayType, EGPD_Input);
	TestNotNull(TEXT("测试函数应有整数数组返回值"), ReturnPin);
	if (!ReturnPin)
	{
		return false;
	}

	UK2Node_MakeArray* MakeArray = NewObject<UK2Node_MakeArray>(FunctionGraph);
	FunctionGraph->AddNode(MakeArray, true, false);
	MakeArray->AllocateDefaultPins();
	MakeArray->AddInputPin();
	MakeArray->AddInputPin();
	UEdGraphPin* MakeArrayOutput = MakeArray->GetOutputPin();
	MakeArrayOutput->PinType = IntArrayType;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UEdGraphPin* InputPin = MakeArray->FindPin(MakeArray->GetPinName(Index));
		InputPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		InputPin->DefaultValue = FString::FromInt((Index + 1) * 10);
	}

	UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(FunctionGraph);
	FunctionGraph->AddNode(CallNode, true, false);
	UFunction* SampleFunction = URandomShuffleArrayLibrary::StaticClass()->FindFunctionByName(
		FName(TEXT("Array_UnweightedRandomSample")));
	TestNotNull(TEXT("应找到随机采样UFUNCTION"), SampleFunction);
	if (!SampleFunction)
	{
		return false;
	}
	CallNode->SetFromFunction(SampleFunction);
	CallNode->AllocateDefaultPins();

	UEdGraphPin* InputArrayPin = CallNode->FindPin(TEXT("InputArray"));
	UEdGraphPin* CountPin = CallNode->FindPin(TEXT("Count"));
	UEdGraphPin* ResultPin = CallNode->FindPin(TEXT("Result"));
	TestNotNull(TEXT("随机采样节点应生成数组输入引脚"), InputArrayPin);
	TestNotNull(TEXT("随机采样节点应生成数量输入引脚"), CountPin);
	TestNotNull(TEXT("随机采样节点应生成数组输出引脚"), ResultPin);
	if (!InputArrayPin || !CountPin || !ResultPin)
	{
		return false;
	}
	InputArrayPin->PinType = IntArrayType;
	ResultPin->PinType = IntArrayType;
	CountPin->DefaultValue = TEXT("5");

	const UEdGraphSchema* Schema = FunctionGraph->GetSchema();
	TestTrue(TEXT("应连接数组字面量到CustomThunk输入"), Schema->TryCreateConnection(MakeArrayOutput, InputArrayPin));
	TestTrue(TEXT("应连接CustomThunk输出到蓝图函数返回值"), Schema->TryCreateConnection(ResultPin, ReturnPin));
	TestTrue(TEXT("应连接函数入口到CustomThunk"), Schema->TryCreateConnection(Entry->FindPin(UEdGraphSchema_K2::PN_Then), CallNode->FindPin(UEdGraphSchema_K2::PN_Execute)));
	TestTrue(TEXT("应连接CustomThunk到函数返回"), Schema->TryCreateConnection(CallNode->GetThenPin(), Result->FindPin(UEdGraphSchema_K2::PN_Execute)));

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	TestTrue(TEXT("编译后应生成蓝图类"), Blueprint->GeneratedClass != nullptr);
	if (!Blueprint->GeneratedClass)
	{
		return false;
	}

	UFunction* TestFunction = Blueprint->GeneratedClass->FindFunctionByName(TEXT("RunRandomSample"));
	TestNotNull(TEXT("编译后应生成测试函数"), TestFunction);
	if (!TestFunction)
	{
		return false;
	}

	struct FRunRandomSampleParameters
	{
		TArray<int32> ReturnValue;
	};
	TestEqual(TEXT("测试函数参数布局应与返回数组一致"), TestFunction->ParmsSize, static_cast<int32>(sizeof(FRunRandomSampleParameters)));
	if (TestFunction->ParmsSize != sizeof(FRunRandomSampleParameters))
	{
		return false;
	}

	FRunRandomSampleParameters CallParameters;
	Blueprint->GeneratedClass->GetDefaultObject()->ProcessEvent(TestFunction, &CallParameters);
	TestEqual(TEXT("蓝图执行应返回请求数量"), CallParameters.ReturnValue.Num(), 5);
	TestTrue(TEXT("蓝图执行结果应来自输入数组"), ContainsOnly(CallParameters.ReturnValue, { 10, 20, 30 }));

	return !HasAnyErrors();
}

#endif
