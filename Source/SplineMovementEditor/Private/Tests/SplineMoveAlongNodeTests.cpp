/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "K2Node_SplineMoveAlong.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"

namespace
{
	UK2Node_CustomEvent* AddCustomEvent(UEdGraph* Graph, const FName EventName)
	{
		UK2Node_CustomEvent* EventNode = NewObject<UK2Node_CustomEvent>(Graph);
		Graph->AddNode(EventNode);
		EventNode->CreateNewGuid();
		EventNode->CustomFunctionName = EventName;
		EventNode->AllocateDefaultPins();
		return EventNode;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSplineMovementEditor_SplineMoveNodeCompilesInterruptExpansion,
	"XTools.SplineMovementEditor.SplineMoveNode.CompilesInterruptExpansion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSplineMovementEditor_SplineMoveNodeCompilesInterruptExpansion::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("ScanPathsSynchronous: Package /Game/SplineMoveNodeTest_"),
		EAutomationExpectedErrorFlags::Contains, 1);

	const FName BlueprintName = MakeUniqueObjectName(
		GetTransientPackage(), UBlueprint::StaticClass(), TEXT("SplineMoveNodeTest"));
	UPackage* BlueprintPackage = CreatePackage(
		*FString::Printf(TEXT("/Game/%s"), *BlueprintName.ToString()));
	BlueprintPackage->SetFlags(RF_Transient);
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		BlueprintPackage,
		BlueprintName,
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		NAME_None);
	UEdGraph* EventGraph = Blueprint ? FBlueprintEditorUtils::FindEventGraph(Blueprint) : nullptr;
	if (!TestNotNull(TEXT("应创建样条移动节点测试蓝图"), Blueprint) ||
		!TestNotNull(TEXT("应找到样条移动节点测试事件图"), EventGraph))
	{
		return false;
	}

	UK2Node_SplineMoveAlong* MoveNode = NewObject<UK2Node_SplineMoveAlong>(EventGraph);
	EventGraph->AddNode(MoveNode);
	MoveNode->CreateNewGuid();
	MoveNode->AllocateDefaultPins();

	UEdGraphPin* ExecutePin = MoveNode->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
	UEdGraphPin* InterruptPin = MoveNode->FindPin(TEXT("Interrupt"), EGPD_Input);
	if (!TestNotNull(TEXT("样条移动节点应包含执行引脚"), ExecutePin) ||
		!TestNotNull(TEXT("样条移动节点应包含中断引脚"), InterruptPin))
	{
		return false;
	}

	UK2Node_CustomEvent* StartEvent = AddCustomEvent(EventGraph, TEXT("StartSplineMove"));
	UK2Node_CustomEvent* InterruptEvent = AddCustomEvent(EventGraph, TEXT("InterruptSplineMove"));
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!TestTrue(TEXT("开始事件应连接移动执行引脚"),
		Schema->TryCreateConnection(StartEvent->FindPinChecked(UEdGraphSchema_K2::PN_Then), ExecutePin)) ||
		!TestTrue(TEXT("中断事件应连接移动中断引脚"),
			Schema->TryCreateConnection(InterruptEvent->FindPinChecked(UEdGraphSchema_K2::PN_Then), InterruptPin)))
	{
		return false;
	}

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	TestEqual(TEXT("样条移动中断展开后蓝图应无警告编译成功"), Blueprint->Status, BS_UpToDate);
	return true;
}

#endif
