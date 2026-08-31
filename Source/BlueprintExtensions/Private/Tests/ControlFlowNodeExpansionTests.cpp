/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "K2Nodes/K2Node_ConditionalSequence.h"
#include "K2Nodes/K2Node_MultiBranch.h"
#include "K2Nodes/K2Node_MultiConditionalSelect.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_EditablePinBase.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_TemporaryVariable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/Package.h"

namespace
{
	class FControlFlowCompilerContext : public FKismetCompilerContext
	{
	public:
		FControlFlowCompilerContext(
			UBlueprint* Blueprint,
			FCompilerResultsLog& Results,
			const FKismetCompilerOptions& Options)
			: FKismetCompilerContext(Blueprint, Results, Options)
		{
			Schema = GetMutableDefault<UEdGraphSchema_K2>();
		}
	};

	UBlueprint* CreateControlFlowBlueprint(const TCHAR* BaseName)
	{
		const FName BlueprintName = MakeUniqueObjectName(
			GetTransientPackage(), UBlueprint::StaticClass(), FName(BaseName));
		UPackage* BlueprintPackage = CreatePackage(
			*FString::Printf(TEXT("/Game/%s"), *BlueprintName.ToString()));
		BlueprintPackage->SetFlags(RF_Transient);
		return FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(), BlueprintPackage, BlueprintName, BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_None);
	}

	UEdGraph* AddFunctionGraph(UBlueprint* Blueprint, const FName GraphName)
	{
		UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
			Blueprint, GraphName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		if (Graph)
		{
			FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, Graph, true, nullptr);
		}
		return Graph;
	}

	template <typename NodeType>
	NodeType* AddControlFlowNode(UEdGraph* Graph)
	{
		NodeType* Node = NewObject<NodeType>(Graph);
		Graph->AddNode(Node);
		Node->CreateNewGuid();
		static_cast<UK2Node*>(Node)->AllocateDefaultPins();
		return Node;
	}

	template <typename NodeType>
	bool AddExecFunction(
		FAutomationTestBase& Test,
		UBlueprint* Blueprint,
		const FName GraphName,
		UEdGraphPin* (NodeType::*GetDefaultExecPin)() const)
	{
		UEdGraph* Graph = AddFunctionGraph(Blueprint, GraphName);
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s函数图"), *GraphName.ToString()), Graph))
		{
			return false;
		}
		UK2Node_EditablePinBase* Entry = FBlueprintEditorUtils::GetEntryNode(Graph);
		UK2Node_FunctionResult* Result = Entry ? FBlueprintEditorUtils::FindOrCreateFunctionResultNode(Entry) : nullptr;
		NodeType* Node = AddControlFlowNode<NodeType>(Graph);
		const UEdGraphSchema* Schema = Graph->GetSchema();
		return Test.TestNotNull(*FString::Printf(TEXT("%s入口"), *GraphName.ToString()), Entry)
			&& Test.TestNotNull(*FString::Printf(TEXT("%s返回"), *GraphName.ToString()), Result)
			&& Test.TestTrue(*FString::Printf(TEXT("连接%s入口"), *GraphName.ToString()),
				Schema && Schema->TryCreateConnection(
					Entry->FindPin(UEdGraphSchema_K2::PN_Then), Node->FindPin(UEdGraphSchema_K2::PN_Execute)))
			&& Test.TestTrue(*FString::Printf(TEXT("连接%s默认出口"), *GraphName.ToString()),
				Schema->TryCreateConnection(
					(Node->*GetDefaultExecPin)(), Result->FindPin(UEdGraphSchema_K2::PN_Execute)));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FControlFlowNodesCompileExpansionTest,
	"XTools.BlueprintExtensions.ControlFlow.CompileExpansion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FControlFlowNodesCompileExpansionTest::RunTest(const FString& Parameters)
{
	AddExpectedError(
		TEXT("ScanPathsSynchronous: Package /Game/XToolsControlFlowCompileTest_"),
		EAutomationExpectedErrorFlags::Contains, 1);

	UBlueprint* Blueprint = CreateControlFlowBlueprint(TEXT("XToolsControlFlowCompileTest"));
	if (!TestNotNull(TEXT("创建控制流测试蓝图"), Blueprint))
	{
		return false;
	}

	const bool bConditionalSequenceAdded = AddExecFunction<UK2Node_ConditionalSequence>(
		*this, Blueprint, TEXT("RunConditionalSequence"), &UK2Node_ConditionalSequence::GetDefaultExecPin);
	const bool bMultiBranchAdded = AddExecFunction<UK2Node_MultiBranch>(
		*this, Blueprint, TEXT("RunMultiBranch"), &UK2Node_MultiBranch::GetDefaultExecPin);

	UEdGraph* SelectGraph = AddFunctionGraph(Blueprint, TEXT("RunMultiConditionalSelect"));
	if (!TestNotNull(TEXT("多条件选择函数图"), SelectGraph))
	{
		return false;
	}
	UK2Node_EditablePinBase* SelectEntry = FBlueprintEditorUtils::GetEntryNode(SelectGraph);
	UK2Node_FunctionResult* SelectResult = SelectEntry
		? FBlueprintEditorUtils::FindOrCreateFunctionResultNode(SelectEntry)
		: nullptr;
	if (!TestNotNull(TEXT("多条件选择函数入口"), SelectEntry)
		|| !TestNotNull(TEXT("多条件选择函数返回"), SelectResult))
	{
		return false;
	}
	FEdGraphPinType IntType;
	IntType.PinCategory = UEdGraphSchema_K2::PC_Int;
	UEdGraphPin* ResultValuePin = SelectResult->CreateUserDefinedPin(TEXT("ReturnValue"), IntType, EGPD_Input);
	UK2Node_MultiConditionalSelect* SelectNode = AddControlFlowNode<UK2Node_MultiConditionalSelect>(SelectGraph);
	UK2Node_TemporaryVariable* IntSource = AddControlFlowNode<UK2Node_TemporaryVariable>(SelectGraph);
	IntSource->VariableType = IntType;
	IntSource->ReconstructNode();

	const UEdGraphSchema* SelectSchema = SelectGraph ? SelectGraph->GetSchema() : nullptr;
	const bool bSelectAdded = TestNotNull(TEXT("多条件选择返回引脚"), ResultValuePin)
		&& TestTrue(TEXT("连接多条件选择类型源"),
			SelectSchema && SelectSchema->TryCreateConnection(IntSource->GetVariablePin(), SelectNode->FindPin(TEXT("Default"))))
		&& TestTrue(TEXT("连接多条件选择结果"),
			SelectSchema->TryCreateConnection(SelectNode->FindPin(TEXT("Return Value")), ResultValuePin))
		&& TestTrue(TEXT("连接多条件选择函数执行流"),
			SelectSchema->TryCreateConnection(
				SelectEntry->FindPin(UEdGraphSchema_K2::PN_Then), SelectResult->FindPin(UEdGraphSchema_K2::PN_Execute)));

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	return bConditionalSequenceAdded && bMultiBranchAdded && bSelectAdded
		&& TestEqual(TEXT("控制流节点展开后蓝图无警告编译成功"), Blueprint->Status, BS_UpToDate)
		&& !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FControlFlowNodesRejectMalformedPinsTest,
	"XTools.BlueprintExtensions.ControlFlow.RejectMalformedPins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FControlFlowNodesRejectMalformedPinsTest::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("Conditional Sequence node has invalid execution pins"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Multi Conditional Select node has invalid pins"), EAutomationExpectedErrorFlags::Contains, 1);

	UBlueprint* Blueprint = CreateControlFlowBlueprint(TEXT("XToolsControlFlowMalformedTest"));
	UEdGraph* EventGraph = Blueprint ? FBlueprintEditorUtils::FindEventGraph(Blueprint) : nullptr;
	if (!TestNotNull(TEXT("创建损坏节点测试蓝图"), Blueprint)
		|| !TestNotNull(TEXT("获取损坏节点测试事件图"), EventGraph))
	{
		return false;
	}

	FCompilerResultsLog Results;
	FKismetCompilerOptions Options;
	FControlFlowCompilerContext CompilerContext(Blueprint, Results, Options);

	UK2Node_ConditionalSequence* Sequence = AddControlFlowNode<UK2Node_ConditionalSequence>(EventGraph);
	Sequence->Pins.Remove(Sequence->FindPin(UEdGraphSchema_K2::PN_Execute));
	static_cast<UK2Node*>(Sequence)->ExpandNode(CompilerContext, EventGraph);

	UK2Node_MultiConditionalSelect* Select = AddControlFlowNode<UK2Node_MultiConditionalSelect>(EventGraph);
	Select->Pins.Remove(Select->FindPin(TEXT("Default")));
	static_cast<UK2Node*>(Select)->ExpandNode(CompilerContext, EventGraph);

	return TestEqual(TEXT("损坏控制流节点各产生一次编译错误"), Results.NumErrors, 2)
		&& TestEqual(TEXT("损坏控制流节点不降级为警告"), Results.NumWarnings, 0)
		&& !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMultiBranchRejectsMissingFunctionPinTest,
	"XTools.BlueprintExtensions.ControlFlow.MultiBranchRejectsMissingFunctionPin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMultiBranchRejectsMissingFunctionPinTest::RunTest(const FString& Parameters)
{
	AddExpectedError(
		TEXT("ScanPathsSynchronous: Package /Game/XToolsMultiBranchMalformedTest_"),
		EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("has invalid internal pins"), EAutomationExpectedErrorFlags::Contains, 1);

	UBlueprint* Blueprint = CreateControlFlowBlueprint(TEXT("XToolsMultiBranchMalformedTest"));
	UEdGraph* Graph = Blueprint ? AddFunctionGraph(Blueprint, TEXT("RunMalformedMultiBranch")) : nullptr;
	UK2Node_EditablePinBase* Entry = Graph ? FBlueprintEditorUtils::GetEntryNode(Graph) : nullptr;
	UK2Node_FunctionResult* Result = Entry ? FBlueprintEditorUtils::FindOrCreateFunctionResultNode(Entry) : nullptr;
	if (!TestNotNull(TEXT("创建MultiBranch损坏图"), Graph)
		|| !TestNotNull(TEXT("获取MultiBranch损坏图入口"), Entry)
		|| !TestNotNull(TEXT("获取MultiBranch损坏图返回"), Result))
	{
		return false;
	}

	UK2Node_MultiBranch* MultiBranch = AddControlFlowNode<UK2Node_MultiBranch>(Graph);
	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (!TestTrue(TEXT("连接损坏MultiBranch入口"), Schema && Schema->TryCreateConnection(
		Entry->FindPin(UEdGraphSchema_K2::PN_Then), MultiBranch->FindPin(UEdGraphSchema_K2::PN_Execute)))
		|| !TestTrue(TEXT("连接损坏MultiBranch默认出口"), Schema->TryCreateConnection(
			MultiBranch->GetDefaultExecPin(), Result->FindPin(UEdGraphSchema_K2::PN_Execute))))
	{
		return false;
	}

	MultiBranch->Pins.Remove(MultiBranch->GetFunctionPin());
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	return TestEqual(TEXT("缺失函数引脚的MultiBranch编译失败而不崩溃"), Blueprint->Status, BS_Error)
		&& !HasAnyErrors();
}

#endif
