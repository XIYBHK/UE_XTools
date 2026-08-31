/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "K2Nodes/K2Node_ForEachArrayReverse.h"
#include "K2Nodes/K2Node_ForEachMap.h"
#include "K2Nodes/K2Node_ForEachSet.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_TemporaryVariable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/Package.h"

namespace
{
	class FContainerLoopCompilerContext : public FKismetCompilerContext
	{
	public:
		FContainerLoopCompilerContext(
			UBlueprint* Blueprint,
			FCompilerResultsLog& Results,
			const FKismetCompilerOptions& Options)
			: FKismetCompilerContext(Blueprint, Results, Options)
		{
			Schema = GetMutableDefault<UEdGraphSchema_K2>();
		}
	};

	UBlueprint* CreateContainerLoopBlueprint(const TCHAR* BaseName, UEdGraph*& OutEventGraph)
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

	template <typename NodeType>
	NodeType* AddContainerLoopNode(UEdGraph* Graph)
	{
		NodeType* Node = NewObject<NodeType>(Graph);
		Graph->AddNode(Node);
		Node->CreateNewGuid();
		Node->AllocateDefaultPins();
		return Node;
	}

	template <typename NodeType>
	bool CompileContainerLoop(
		FAutomationTestBase& Test,
		const TCHAR* BaseName,
		const TCHAR* Description,
		EPinContainerType ContainerType,
		UEdGraphPin* (NodeType::*GetContainerPin)() const,
		const FName ValueCategory = NAME_None)
	{
		Test.AddExpectedError(
			FString::Printf(TEXT("ScanPathsSynchronous: Package /Game/%s_"), BaseName),
			EAutomationExpectedErrorFlags::Contains, 1);

		UEdGraph* EventGraph = nullptr;
		UBlueprint* Blueprint = CreateContainerLoopBlueprint(BaseName, EventGraph);
		if (!Test.TestNotNull(*FString::Printf(TEXT("创建%s测试蓝图"), Description), Blueprint)
			|| !Test.TestNotNull(*FString::Printf(TEXT("获取%s测试事件图"), Description), EventGraph))
		{
			return false;
		}

		NodeType* LoopNode = AddContainerLoopNode<NodeType>(EventGraph);
		UK2Node_TemporaryVariable* ContainerSource = AddContainerLoopNode<UK2Node_TemporaryVariable>(EventGraph);
		ContainerSource->VariableType.PinCategory = UEdGraphSchema_K2::PC_Int;
		ContainerSource->VariableType.ContainerType = ContainerType;
		ContainerSource->VariableType.PinValueType.TerminalCategory = ValueCategory;
		ContainerSource->ReconstructNode();

		UEdGraphPin* ContainerPin = (LoopNode->*GetContainerPin)();
		const UEdGraphSchema* Schema = EventGraph->GetSchema();
		if (!Test.TestTrue(*FString::Printf(TEXT("连接%s容器输入"), Description),
			Schema && Schema->TryCreateConnection(ContainerSource->GetVariablePin(), ContainerPin)))
		{
			return false;
		}
		LoopNode->NotifyPinConnectionListChanged(ContainerPin);

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		return Test.TestEqual(*FString::Printf(TEXT("%s展开后蓝图无警告编译成功"), Description),
			Blueprint->Status, BS_UpToDate);
	}

	template <typename NodeType>
	bool RejectUnconnectedContainer(
		FAutomationTestBase& Test,
		UBlueprint* Blueprint,
		UEdGraph* EventGraph,
		const TCHAR* Description)
	{
		NodeType* LoopNode = AddContainerLoopNode<NodeType>(EventGraph);
		FCompilerResultsLog Results;
		FKismetCompilerOptions Options;
		FContainerLoopCompilerContext CompilerContext(Blueprint, Results, Options);
		LoopNode->ExpandNode(CompilerContext, EventGraph);
		return Test.TestEqual(*FString::Printf(TEXT("%s未连接输入产生编译错误"), Description),
			Results.NumErrors, 1)
			&& Test.TestEqual(*FString::Printf(TEXT("%s未连接输入不降级为警告"), Description),
				Results.NumWarnings, 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FContainerLoopsCompileExpansionTest,
	"XTools.BlueprintExtensions.ContainerLoops.CompileExpansion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FContainerLoopsCompileExpansionTest::RunTest(const FString& Parameters)
{
	const bool bMapPassed = CompileContainerLoop<UK2Node_ForEachMap>(
		*this, TEXT("XToolsForEachMapCompileTest"), TEXT("Map循环"), EPinContainerType::Map,
		&UK2Node_ForEachMap::GetMapPin, UEdGraphSchema_K2::PC_String);
	const bool bSetPassed = CompileContainerLoop<UK2Node_ForEachSet>(
		*this, TEXT("XToolsForEachSetCompileTest"), TEXT("Set循环"), EPinContainerType::Set,
		&UK2Node_ForEachSet::GetSetPin);
	const bool bArrayPassed = CompileContainerLoop<UK2Node_ForEachArrayReverse>(
		*this, TEXT("XToolsForEachArrayReverseCompileTest"), TEXT("倒序数组循环"), EPinContainerType::Array,
		&UK2Node_ForEachArrayReverse::GetArrayPin);
	return bMapPassed && bSetPassed && bArrayPassed && !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FContainerLoopsRejectUnconnectedInputTest,
	"XTools.BlueprintExtensions.ContainerLoops.RejectUnconnectedInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FContainerLoopsRejectUnconnectedInputTest::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("Map pin must be connected"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Set pin must be connected"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Array pin must be connected"), EAutomationExpectedErrorFlags::Contains, 1);

	UEdGraph* EventGraph = nullptr;
	UBlueprint* Blueprint = CreateContainerLoopBlueprint(
		TEXT("XToolsContainerLoopValidationTest"), EventGraph);
	if (!TestNotNull(TEXT("创建容器循环校验蓝图"), Blueprint)
		|| !TestNotNull(TEXT("获取容器循环校验事件图"), EventGraph))
	{
		return false;
	}

	const bool bMapPassed = RejectUnconnectedContainer<UK2Node_ForEachMap>(
		*this, Blueprint, EventGraph, TEXT("Map循环"));
	const bool bSetPassed = RejectUnconnectedContainer<UK2Node_ForEachSet>(
		*this, Blueprint, EventGraph, TEXT("Set循环"));
	const bool bArrayPassed = RejectUnconnectedContainer<UK2Node_ForEachArrayReverse>(
		*this, Blueprint, EventGraph, TEXT("倒序数组循环"));
	return bMapPassed && bSetPassed && bArrayPassed && !HasAnyErrors();
}

#endif
