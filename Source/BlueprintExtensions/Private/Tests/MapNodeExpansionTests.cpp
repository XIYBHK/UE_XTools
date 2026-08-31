/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "K2Nodes/K2Node_MapAppend.h"
#include "K2Nodes/K2Node_MapIdentical.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_TemporaryVariable.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/Package.h"

namespace
{
	class FMapNodeCompilerContext : public FKismetCompilerContext
	{
	public:
		FMapNodeCompilerContext(
			UBlueprint* Blueprint,
			FCompilerResultsLog& Results,
			const FKismetCompilerOptions& Options)
			: FKismetCompilerContext(Blueprint, Results, Options)
		{
			Schema = GetMutableDefault<UEdGraphSchema_K2>();
		}
	};

	UBlueprint* CreateMapNodeTestBlueprint(const TCHAR* BaseName, UEdGraph*& OutEventGraph)
	{
		const FName BlueprintName = MakeUniqueObjectName(
			GetTransientPackage(), UBlueprint::StaticClass(), FName(BaseName));
		UPackage* BlueprintPackage = CreatePackage(
			*FString::Printf(TEXT("/Game/%s"), *BlueprintName.ToString()));
		BlueprintPackage->SetFlags(RF_Transient);
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(), BlueprintPackage, BlueprintName, BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_None);
		OutEventGraph = Blueprint ? FBlueprintEditorUtils::FindEventGraph(Blueprint) : nullptr;
		return Blueprint;
	}

	FEdGraphPinType MakeIntStringMapType()
	{
		FEdGraphPinType MapType;
		MapType.PinCategory = UEdGraphSchema_K2::PC_Int;
		MapType.ContainerType = EPinContainerType::Map;
		MapType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_String;
		return MapType;
	}

	UK2Node_TemporaryVariable* AddMapVariable(UEdGraph* Graph)
	{
		UK2Node_TemporaryVariable* Variable = NewObject<UK2Node_TemporaryVariable>(Graph);
		Graph->AddNode(Variable);
		Variable->CreateNewGuid();
		Variable->VariableType = MakeIntStringMapType();
		Variable->AllocateDefaultPins();
		return Variable;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintExtensions_MapAppendCompilesExpansion,
	"XTools.BlueprintExtensions.MapNodes.MapAppendCompilesExpansion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintExtensions_MapAppendCompilesExpansion::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("ScanPathsSynchronous: Package /Game/XToolsMapAppendTest_"),
		EAutomationExpectedErrorFlags::Contains, 1);

	UEdGraph* EventGraph = nullptr;
	UBlueprint* Blueprint = CreateMapNodeTestBlueprint(TEXT("XToolsMapAppendTest"), EventGraph);
	if (!TestNotNull(TEXT("应创建 MapAppend 测试蓝图"), Blueprint) ||
		!TestNotNull(TEXT("应找到 MapAppend 测试事件图"), EventGraph))
	{
		return false;
	}

	UK2Node_MapAppend* AppendNode = NewObject<UK2Node_MapAppend>(EventGraph);
	EventGraph->AddNode(AppendNode);
	AppendNode->CreateNewGuid();
	AppendNode->AllocateDefaultPins();

	UK2Node_TemporaryVariable* TargetMap = AddMapVariable(EventGraph);
	UK2Node_TemporaryVariable* SourceMap = AddMapVariable(EventGraph);
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!TestTrue(TEXT("应连接目标 Map"), Schema->TryCreateConnection(
		TargetMap->GetVariablePin(), AppendNode->FindPin(TEXT("TargetMap"), EGPD_Input))) ||
		!TestTrue(TEXT("应连接源 Map"), Schema->TryCreateConnection(
			SourceMap->GetVariablePin(), AppendNode->FindPin(TEXT("SourceMap"), EGPD_Input))))
	{
		return false;
	}

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	TestEqual(TEXT("MapAppend 展开后蓝图应无警告编译成功"), Blueprint->Status, BS_UpToDate);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintExtensions_MapIdenticalCompilesExpansion,
	"XTools.BlueprintExtensions.MapNodes.MapIdenticalCompilesExpansion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintExtensions_MapIdenticalCompilesExpansion::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("ScanPathsSynchronous: Package /Game/XToolsMapIdenticalTest_"),
		EAutomationExpectedErrorFlags::Contains, 1);

	UEdGraph* EventGraph = nullptr;
	UBlueprint* Blueprint = CreateMapNodeTestBlueprint(TEXT("XToolsMapIdenticalTest"), EventGraph);
	if (!TestNotNull(TEXT("应创建 MapIdentical 测试蓝图"), Blueprint) ||
		!TestNotNull(TEXT("应找到 MapIdentical 测试事件图"), EventGraph))
	{
		return false;
	}

	UK2Node_MapIdentical* IdenticalNode = NewObject<UK2Node_MapIdentical>(EventGraph);
	EventGraph->AddNode(IdenticalNode);
	IdenticalNode->CreateNewGuid();
	IdenticalNode->AllocateDefaultPins();

	UK2Node_TemporaryVariable* MapA = AddMapVariable(EventGraph);
	UK2Node_TemporaryVariable* MapB = AddMapVariable(EventGraph);
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!TestTrue(TEXT("应连接 MapA"), Schema->TryCreateConnection(
		MapA->GetVariablePin(), IdenticalNode->GetMapAPin())) ||
		!TestTrue(TEXT("应连接 MapB"), Schema->TryCreateConnection(
			MapB->GetVariablePin(), IdenticalNode->GetMapBPin())))
	{
		return false;
	}

	UK2Node_CallFunction* BoolConsumer = NewObject<UK2Node_CallFunction>(EventGraph);
	EventGraph->AddNode(BoolConsumer);
	BoolConsumer->CreateNewGuid();
	BoolConsumer->FunctionReference.SetExternalMember(
		GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool),
		UKismetMathLibrary::StaticClass());
	BoolConsumer->AllocateDefaultPins();
	TestTrue(TEXT("应连接 MapIdentical 返回值消费者"), Schema->TryCreateConnection(
		IdenticalNode->GetReturnValuePin(), BoolConsumer->FindPin(TEXT("A"), EGPD_Input)));

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	TestEqual(TEXT("MapIdentical 展开后蓝图应无警告编译成功"), Blueprint->Status, BS_UpToDate);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintExtensions_MapNodesRejectMissingPins,
	"XTools.BlueprintExtensions.MapNodes.RejectMissingPins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintExtensions_MapNodesRejectMissingPins::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("节点引脚不完整"), EAutomationExpectedErrorFlags::Contains, 2);

	UEdGraph* EventGraph = nullptr;
	UBlueprint* Blueprint = CreateMapNodeTestBlueprint(TEXT("XToolsMapNodesMissingPinsTest"), EventGraph);
	if (!TestNotNull(TEXT("创建Map残缺节点蓝图"), Blueprint)
		|| !TestNotNull(TEXT("获取Map残缺节点事件图"), EventGraph))
	{
		return false;
	}

	FKismetCompilerOptions Options;
	UK2Node_MapAppend* AppendNode = NewObject<UK2Node_MapAppend>(EventGraph);
	EventGraph->AddNode(AppendNode);
	AppendNode->AllocateDefaultPins();
	AppendNode->Pins.Remove(AppendNode->FindPin(TEXT("TargetMap"), EGPD_Input));
	FCompilerResultsLog AppendResults;
	FMapNodeCompilerContext AppendCompilerContext(Blueprint, AppendResults, Options);
	AppendNode->ExpandNode(AppendCompilerContext, EventGraph);

	UK2Node_MapIdentical* IdenticalNode = NewObject<UK2Node_MapIdentical>(EventGraph);
	EventGraph->AddNode(IdenticalNode);
	IdenticalNode->AllocateDefaultPins();
	IdenticalNode->Pins.Remove(IdenticalNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output));
	FCompilerResultsLog IdenticalResults;
	FMapNodeCompilerContext IdenticalCompilerContext(Blueprint, IdenticalResults, Options);
	IdenticalNode->ExpandNode(IdenticalCompilerContext, EventGraph);

	return TestEqual(TEXT("MapAppend缺失引脚产生编译错误"), AppendResults.NumErrors, 1)
		&& TestEqual(TEXT("MapIdentical缺失引脚产生编译错误"), IdenticalResults.NumErrors, 1)
		&& TestEqual(TEXT("MapAppend缺失引脚不降级为警告"), AppendResults.NumWarnings, 0)
		&& TestEqual(TEXT("MapIdentical缺失引脚不降级为警告"), IdenticalResults.NumWarnings, 0)
		&& !HasAnyErrors();
}

#endif
