#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "K2Nodes/K2Node_ForEachArrayReverse.h"
#include "K2Nodes/K2Node_ForEachLoopWithDelay.h"
#include "K2Nodes/K2Node_ForEachSet.h"
#include "K2Nodes/K2Node_ForLoopWithDelay.h"
#include "K2Nodes/K2Node_ForLoopWithDelayReverse.h"
#include "K2Nodes/K2Node_WhileLoopWithDelay.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_AssignmentStatement.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_TemporaryVariable.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"

namespace
{
	class FExpansionTestCompilerContext : public FKismetCompilerContext
	{
	public:
		FExpansionTestCompilerContext(
			UBlueprint* Blueprint,
			FCompilerResultsLog& Results,
			const FKismetCompilerOptions& Options)
			: FKismetCompilerContext(Blueprint, Results, Options)
		{
			Schema = GetMutableDefault<UEdGraphSchema_K2>();
		}
	};

	template <typename NodeType>
	NodeType* AddTestNode(UEdGraph* Graph)
	{
		NodeType* Node = NewObject<NodeType>(Graph);
		Graph->AddNode(Node);
		Node->CreateNewGuid();
		Node->AllocateDefaultPins();
		return Node;
	}

	UBlueprint* CreateTestBlueprint(const TCHAR* BaseName, UEdGraph*& OutEventGraph)
	{
		const FName BlueprintName = MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), FName(BaseName));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			GetTransientPackage(),
			BlueprintName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			NAME_None);
		OutEventGraph = Blueprint ? FBlueprintEditorUtils::FindEventGraph(Blueprint) : nullptr;
		return Blueprint;
	}

	bool ConnectBreakFromLoopBody(
		FAutomationTestBase& Test,
		UEdGraph* Graph,
		UEdGraphPin* LoopBodyPin,
		UEdGraphPin* BreakPin,
		UK2Node_ExecutionSequence*& OutBodySequence)
	{
		OutBodySequence = AddTestNode<UK2Node_ExecutionSequence>(Graph);
		const UEdGraphSchema* Schema = Graph->GetSchema();
		return Test.TestTrue(TEXT("连接循环体到测试执行序列"),
			Schema && Schema->TryCreateConnection(LoopBodyPin, OutBodySequence->GetExecPin()))
			&& Test.TestTrue(TEXT("连接测试执行序列到 Break"),
				Schema->TryCreateConnection(OutBodySequence->GetThenPinGivenIndex(0), BreakPin));
	}

	bool ValidateExpandedBreakTopology(
		FAutomationTestBase& Test,
		const TCHAR* Context,
		UK2Node_ExecutionSequence* BodySequence)
	{
		UEdGraphPin* BodyExecPin = BodySequence ? BodySequence->GetExecPin() : nullptr;
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s: 测试循环体执行引脚"), Context), BodyExecPin)
			|| !Test.TestEqual(*FString::Printf(TEXT("%s: 循环体只有一个展开来源"), Context), BodyExecPin->LinkedTo.Num(), 1))
		{
			return false;
		}

		UK2Node_ExecutionSequence* ExpandedLoopSequence =
			Cast<UK2Node_ExecutionSequence>(BodyExecPin->LinkedTo[0]->GetOwningNode());
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s: 找到展开后的循环执行序列"), Context), ExpandedLoopSequence))
		{
			return false;
		}

		UEdGraphPin* BreakSourcePin = BodySequence->GetThenPinGivenIndex(0);
		if (!Test.TestEqual(*FString::Printf(TEXT("%s: Break 只有一个赋值目标"), Context), BreakSourcePin->LinkedTo.Num(), 1))
		{
			return false;
		}

		UK2Node_AssignmentStatement* BreakAssignment =
			Cast<UK2Node_AssignmentStatement>(BreakSourcePin->LinkedTo[0]->GetOwningNode());
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s: Break 展开为计数器赋值"), Context), BreakAssignment))
		{
			return false;
		}

		Test.TestEqual(*FString::Printf(TEXT("%s: Break 赋值不直接触发 Completed"), Context),
			BreakAssignment->GetThenPin()->LinkedTo.Num(), 0);

		UEdGraphPin* ContinuationPin = ExpandedLoopSequence->GetThenPinGivenIndex(1);
		if (!Test.TestEqual(*FString::Printf(TEXT("%s: 循环体后只有一个条件出口"), Context), ContinuationPin->LinkedTo.Num(), 1))
		{
			return false;
		}

		UK2Node_IfThenElse* PostBodyBranch = Cast<UK2Node_IfThenElse>(ContinuationPin->LinkedTo[0]->GetOwningNode());
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s: 循环体后重新检查循环条件"), Context), PostBodyBranch))
		{
			return false;
		}

		UEdGraphPin* CompletedExecPin = PostBodyBranch->GetElsePin()->LinkedTo.Num() == 1
			? PostBodyBranch->GetElsePin()->LinkedTo[0]
			: nullptr;
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s: 条件失效进入完成路径"), Context), CompletedExecPin))
		{
			return false;
		}

		Test.TestEqual(*FString::Printf(TEXT("%s: 自然结束与 Break 汇入同一完成入口"), Context),
			CompletedExecPin->LinkedTo.Num(), 2);
		return true;
	}

	bool ValidateSingleFlightGuard(
		FAutomationTestBase& Test,
		const TCHAR* Context,
		UEdGraph* Graph)
	{
		UK2Node_IfThenElse* EntryBranch = nullptr;
		UK2Node_AssignmentStatement* StartRunning = nullptr;

		for (UEdGraphNode* GraphNode : Graph->Nodes)
		{
			UK2Node_IfThenElse* CandidateBranch = Cast<UK2Node_IfThenElse>(GraphNode);
			if (!CandidateBranch
				|| CandidateBranch->GetThenPin()->LinkedTo.Num() != 0
				|| CandidateBranch->GetElsePin()->LinkedTo.Num() != 1)
			{
				continue;
			}

			UK2Node_AssignmentStatement* CandidateStart =
				Cast<UK2Node_AssignmentStatement>(CandidateBranch->GetElsePin()->LinkedTo[0]->GetOwningNode());
			if (CandidateStart && CandidateStart->GetValuePin()->DefaultValue == TEXT("true"))
			{
				EntryBranch = CandidateBranch;
				StartRunning = CandidateStart;
				break;
			}
		}

		if (!Test.TestNotNull(*FString::Printf(TEXT("%s: 找到单实例入口守卫"), Context), EntryBranch)
			|| !Test.TestNotNull(*FString::Printf(TEXT("%s: 首次进入时设置运行标记"), Context), StartRunning))
		{
			return false;
		}

		UEdGraphPin* RunningFlagPin = EntryBranch->GetConditionPin()->LinkedTo.Num() == 1
			? EntryBranch->GetConditionPin()->LinkedTo[0]
			: nullptr;
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s: 守卫读取共享运行标记"), Context), RunningFlagPin))
		{
			return false;
		}

		UK2Node_AssignmentStatement* FinishRunning = nullptr;
		for (UEdGraphPin* LinkedPin : RunningFlagPin->LinkedTo)
		{
			UK2Node_AssignmentStatement* Assignment =
				Cast<UK2Node_AssignmentStatement>(LinkedPin->GetOwningNode());
			if (Assignment && Assignment->GetVariablePin() == LinkedPin
				&& Assignment->GetValuePin()->DefaultValue == TEXT("false")
				&& Assignment->GetExecPin()->LinkedTo.Num() > 0)
			{
				FinishRunning = Assignment;
				break;
			}
		}

		return Test.TestNotNull(*FString::Printf(TEXT("%s: Completed前释放运行标记"), Context), FinishRunning);
	}

	bool ValidateZeroDelayBypass(
		FAutomationTestBase& Test,
		const TCHAR* Context,
		UEdGraph* Graph)
	{
		UK2Node_CallFunction* DelayNode = nullptr;
		UK2Node_CallFunction* DelayLessEqualZero = nullptr;

		for (UEdGraphNode* GraphNode : Graph->Nodes)
		{
			UK2Node_CallFunction* CallFunction = Cast<UK2Node_CallFunction>(GraphNode);
			if (!CallFunction)
			{
				continue;
			}

			if (CallFunction->GetFunctionName() == GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Delay))
			{
				DelayNode = CallFunction;
			}
			else if (CallFunction->GetFunctionName() == GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, LessEqual_DoubleDouble))
			{
				DelayLessEqualZero = CallFunction;
			}
		}

		if (!Test.TestNotNull(*FString::Printf(TEXT("%s: 找到正延迟节点"), Context), DelayNode)
			|| !Test.TestNotNull(*FString::Printf(TEXT("%s: 找到零延迟判定"), Context), DelayLessEqualZero))
		{
			return false;
		}

		UEdGraphPin* CompareToZeroPin = DelayLessEqualZero->FindPin(TEXT("B"), EGPD_Input);
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s: 零延迟比较引脚"), Context), CompareToZeroPin))
		{
			return false;
		}
		Test.TestEqual(*FString::Printf(TEXT("%s: Delay与0比较"), Context), CompareToZeroPin->DefaultValue, FString(TEXT("0.0")));

		UEdGraphPin* ComparisonResultPin = DelayLessEqualZero->GetReturnValuePin();
		UK2Node_IfThenElse* DelayBranch = ComparisonResultPin->LinkedTo.Num() == 1
			? Cast<UK2Node_IfThenElse>(ComparisonResultPin->LinkedTo[0]->GetOwningNode())
			: nullptr;
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s: 找到延迟分支"), Context), DelayBranch))
		{
			return false;
		}

		const bool bPositiveDelayUsesLatent = DelayBranch->GetElsePin()->LinkedTo.Num() == 1
			&& DelayBranch->GetElsePin()->LinkedTo[0] == DelayNode->GetExecPin();
		const bool bZeroDelayBypassesLatent = DelayBranch->GetThenPin()->LinkedTo.Num() == 1
			&& DelayBranch->GetThenPin()->LinkedTo[0] != DelayNode->GetExecPin();

		return Test.TestTrue(*FString::Printf(TEXT("%s: Delay>0才进入Latent"), Context), bPositiveDelayUsesLatent)
			&& Test.TestTrue(*FString::Printf(TEXT("%s: Delay<=0同步继续"), Context), bZeroDelayBypassesLatent);
	}

	bool ValidateWhileBreakTopology(
		FAutomationTestBase& Test,
		UK2Node_ExecutionSequence* BodySequence)
	{
		UEdGraphPin* BodyExecPin = BodySequence ? BodySequence->GetExecPin() : nullptr;
		if (!Test.TestNotNull(TEXT("While: 测试循环体执行引脚"), BodyExecPin)
			|| !Test.TestEqual(TEXT("While: 循环体只有一个展开来源"), BodyExecPin->LinkedTo.Num(), 1))
		{
			return false;
		}

		UK2Node_ExecutionSequence* ExpandedLoopSequence =
			Cast<UK2Node_ExecutionSequence>(BodyExecPin->LinkedTo[0]->GetOwningNode());
		if (!Test.TestNotNull(TEXT("While: 找到展开后的循环执行序列"), ExpandedLoopSequence))
		{
			return false;
		}

		UEdGraphPin* BreakSourcePin = BodySequence->GetThenPinGivenIndex(0);
		if (!Test.TestEqual(TEXT("While: Break只有一个赋值目标"), BreakSourcePin->LinkedTo.Num(), 1))
		{
			return false;
		}

		UK2Node_AssignmentStatement* BreakAssignment =
			Cast<UK2Node_AssignmentStatement>(BreakSourcePin->LinkedTo[0]->GetOwningNode());
		if (!Test.TestNotNull(TEXT("While: Break展开为标记赋值"), BreakAssignment))
		{
			return false;
		}
		Test.TestEqual(TEXT("While: Break赋值不直接触发Completed"), BreakAssignment->GetThenPin()->LinkedTo.Num(), 0);

		UEdGraphPin* ContinuationPin = ExpandedLoopSequence->GetThenPinGivenIndex(1);
		UK2Node_IfThenElse* PostBodyBreakGate = ContinuationPin->LinkedTo.Num() == 1
			? Cast<UK2Node_IfThenElse>(ContinuationPin->LinkedTo[0]->GetOwningNode())
			: nullptr;
		if (!Test.TestNotNull(TEXT("While: 循环体后检查Break标记"), PostBodyBreakGate))
		{
			return false;
		}

		UEdGraphPin* CompleteExecPin = PostBodyBreakGate->GetThenPin()->LinkedTo.Num() == 1
			? PostBodyBreakGate->GetThenPin()->LinkedTo[0]
			: nullptr;
		if (!Test.TestNotNull(TEXT("While: Break进入统一完成路径"), CompleteExecPin))
		{
			return false;
		}

		Test.TestEqual(TEXT("While: 自然结束与两个Break边界共用完成入口"), CompleteExecPin->LinkedTo.Num(), 3);
		Test.TestEqual(TEXT("While: 未Break时才进入延迟判定"), PostBodyBreakGate->GetElsePin()->LinkedTo.Num(), 1);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLoopBreakExpansionTest,
	"XTools.BlueprintExtensions.LoopBreakExpansion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLoopBreakExpansionTest::RunTest(const FString& Parameters)
{
	{
		UEdGraph* EventGraph = nullptr;
		UBlueprint* Blueprint = CreateTestBlueprint(TEXT("XToolsLoopBreakDelayTest"), EventGraph);
		if (!TestNotNull(TEXT("创建延迟循环测试蓝图"), Blueprint)
			|| !TestNotNull(TEXT("获取延迟循环事件图"), EventGraph))
		{
			return false;
		}

		UK2Node_ForLoopWithDelay* LoopNode = AddTestNode<UK2Node_ForLoopWithDelay>(EventGraph);
		UK2Node_ExecutionSequence* BodySequence = nullptr;
		if (!ConnectBreakFromLoopBody(*this, EventGraph, LoopNode->GetLoopBodyPin(), LoopNode->GetBreakPin(), BodySequence))
		{
			return false;
		}

		FCompilerResultsLog Results;
		FKismetCompilerOptions Options;
		FExpansionTestCompilerContext CompilerContext(Blueprint, Results, Options);
		LoopNode->ExpandNode(CompilerContext, EventGraph);
		TestEqual(TEXT("延迟循环展开无编译错误"), Results.NumErrors, 0);
		ValidateExpandedBreakTopology(*this, TEXT("延迟 ForLoop"), BodySequence);
		ValidateSingleFlightGuard(*this, TEXT("延迟 ForLoop"), EventGraph);
		ValidateZeroDelayBypass(*this, TEXT("延迟 ForLoop"), EventGraph);
	}

	{
		UEdGraph* EventGraph = nullptr;
		UBlueprint* Blueprint = CreateTestBlueprint(TEXT("XToolsLoopZeroDelayReverseTest"), EventGraph);
		if (!TestNotNull(TEXT("创建倒序延迟循环测试蓝图"), Blueprint)
			|| !TestNotNull(TEXT("获取倒序延迟循环事件图"), EventGraph))
		{
			return false;
		}

		UK2Node_ForLoopWithDelayReverse* LoopNode = AddTestNode<UK2Node_ForLoopWithDelayReverse>(EventGraph);
		FCompilerResultsLog Results;
		FKismetCompilerOptions Options;
		FExpansionTestCompilerContext CompilerContext(Blueprint, Results, Options);
		LoopNode->ExpandNode(CompilerContext, EventGraph);
		TestEqual(TEXT("倒序延迟循环展开无编译错误"), Results.NumErrors, 0);
		ValidateZeroDelayBypass(*this, TEXT("延迟倒序 ForLoop"), EventGraph);
	}

	{
		UEdGraph* EventGraph = nullptr;
		UBlueprint* Blueprint = CreateTestBlueprint(TEXT("XToolsLoopBreakWhileTest"), EventGraph);
		if (!TestNotNull(TEXT("创建延迟While测试蓝图"), Blueprint)
			|| !TestNotNull(TEXT("获取延迟While事件图"), EventGraph))
		{
			return false;
		}

		UK2Node_WhileLoopWithDelay* LoopNode = AddTestNode<UK2Node_WhileLoopWithDelay>(EventGraph);
		UK2Node_ExecutionSequence* BodySequence = nullptr;
		if (!ConnectBreakFromLoopBody(*this, EventGraph, LoopNode->GetLoopBodyPin(), LoopNode->GetBreakPin(), BodySequence))
		{
			return false;
		}

		FCompilerResultsLog Results;
		FKismetCompilerOptions Options;
		FExpansionTestCompilerContext CompilerContext(Blueprint, Results, Options);
		LoopNode->ExpandNode(CompilerContext, EventGraph);
		TestEqual(TEXT("延迟While展开无编译错误"), Results.NumErrors, 0);
		ValidateWhileBreakTopology(*this, BodySequence);
		ValidateSingleFlightGuard(*this, TEXT("延迟 While"), EventGraph);
		ValidateZeroDelayBypass(*this, TEXT("延迟 While"), EventGraph);
	}

	{
		UEdGraph* EventGraph = nullptr;
		UBlueprint* Blueprint = CreateTestBlueprint(TEXT("XToolsLoopZeroDelayForEachTest"), EventGraph);
		if (!TestNotNull(TEXT("创建延迟 ForEach 测试蓝图"), Blueprint)
			|| !TestNotNull(TEXT("获取延迟 ForEach 事件图"), EventGraph))
		{
			return false;
		}

		UK2Node_ForEachLoopWithDelay* LoopNode = AddTestNode<UK2Node_ForEachLoopWithDelay>(EventGraph);
		UK2Node_TemporaryVariable* ArraySource = AddTestNode<UK2Node_TemporaryVariable>(EventGraph);
		ArraySource->VariableType.PinCategory = UEdGraphSchema_K2::PC_Int;
		ArraySource->VariableType.ContainerType = EPinContainerType::Array;
		ArraySource->ReconstructNode();
		const UEdGraphSchema* Schema = EventGraph->GetSchema();
		if (!TestTrue(TEXT("连接延迟 ForEach 测试数组"),
			Schema && Schema->TryCreateConnection(ArraySource->GetVariablePin(), LoopNode->GetArrayPin())))
		{
			return false;
		}
		LoopNode->NotifyPinConnectionListChanged(LoopNode->GetArrayPin());

		FCompilerResultsLog Results;
		FKismetCompilerOptions Options;
		FExpansionTestCompilerContext CompilerContext(Blueprint, Results, Options);
		LoopNode->ExpandNode(CompilerContext, EventGraph);
		TestEqual(TEXT("延迟 ForEach 展开无编译错误"), Results.NumErrors, 0);
		ValidateZeroDelayBypass(*this, TEXT("延迟 ForEach"), EventGraph);
	}

	{
		UEdGraph* EventGraph = nullptr;
		UBlueprint* Blueprint = CreateTestBlueprint(TEXT("XToolsLoopZeroDelayForEachReverseTest"), EventGraph);
		if (!TestNotNull(TEXT("创建倒序延迟 ForEach 测试蓝图"), Blueprint)
			|| !TestNotNull(TEXT("获取倒序延迟 ForEach 事件图"), EventGraph))
		{
			return false;
		}

		UK2Node_ForEachArrayReverse* LoopNode = AddTestNode<UK2Node_ForEachArrayReverse>(EventGraph);
		UK2Node_TemporaryVariable* ArraySource = AddTestNode<UK2Node_TemporaryVariable>(EventGraph);
		ArraySource->VariableType.PinCategory = UEdGraphSchema_K2::PC_Int;
		ArraySource->VariableType.ContainerType = EPinContainerType::Array;
		ArraySource->ReconstructNode();
		const UEdGraphSchema* Schema = EventGraph->GetSchema();
		if (!TestTrue(TEXT("连接倒序延迟 ForEach 测试数组"),
			Schema && Schema->TryCreateConnection(ArraySource->GetVariablePin(), LoopNode->GetArrayPin())))
		{
			return false;
		}
		LoopNode->NotifyPinConnectionListChanged(LoopNode->GetArrayPin());

		FCompilerResultsLog Results;
		FKismetCompilerOptions Options;
		FExpansionTestCompilerContext CompilerContext(Blueprint, Results, Options);
		LoopNode->ExpandNode(CompilerContext, EventGraph);
		TestEqual(TEXT("倒序延迟 ForEach 展开无编译错误"), Results.NumErrors, 0);
		ValidateZeroDelayBypass(*this, TEXT("倒序延迟 ForEach"), EventGraph);
	}

	{
		UEdGraph* EventGraph = nullptr;
		UBlueprint* Blueprint = CreateTestBlueprint(TEXT("XToolsLoopBreakSetTest"), EventGraph);
		if (!TestNotNull(TEXT("创建 Set 循环测试蓝图"), Blueprint)
			|| !TestNotNull(TEXT("获取 Set 循环事件图"), EventGraph))
		{
			return false;
		}

		UK2Node_ForEachSet* LoopNode = AddTestNode<UK2Node_ForEachSet>(EventGraph);
		UK2Node_TemporaryVariable* SetSource = AddTestNode<UK2Node_TemporaryVariable>(EventGraph);
		SetSource->VariableType.PinCategory = UEdGraphSchema_K2::PC_Int;
		SetSource->VariableType.ContainerType = EPinContainerType::Set;
		SetSource->ReconstructNode();

		const UEdGraphSchema* Schema = EventGraph->GetSchema();
		if (!TestTrue(TEXT("连接 Set 测试输入"),
			Schema && Schema->TryCreateConnection(SetSource->GetVariablePin(), LoopNode->GetSetPin())))
		{
			return false;
		}
		LoopNode->NotifyPinConnectionListChanged(LoopNode->GetSetPin());

		UK2Node_ExecutionSequence* BodySequence = nullptr;
		if (!ConnectBreakFromLoopBody(*this, EventGraph, LoopNode->GetLoopBodyPin(), LoopNode->GetBreakPin(), BodySequence))
		{
			return false;
		}

		FCompilerResultsLog Results;
		FKismetCompilerOptions Options;
		FExpansionTestCompilerContext CompilerContext(Blueprint, Results, Options);
		LoopNode->ExpandNode(CompilerContext, EventGraph);
		TestEqual(TEXT("Set 循环展开无编译错误"), Results.NumErrors, 0);
		ValidateExpandedBreakTopology(*this, TEXT("ForEach Set"), BodySequence);
	}

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLoopSplitPinValueExpansionTest,
	"XTools.BlueprintExtensions.LoopSplitPinValueExpansion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLoopSplitPinValueExpansionTest::RunTest(const FString& Parameters)
{
	// 回归测试：结构体 Value 引脚被「分割结构体引脚」后，链接挂在子引脚上。
	// 若 ExpandNode 不先展开拆分引脚，MovePinLinksToIntermediate 只能迁移父引脚的空链接，
	// 子引脚链接会在收尾断链时丢失，下游静默读到默认值（如 Transform 全 0）。
	UEdGraph* EventGraph = nullptr;
	UBlueprint* Blueprint = CreateTestBlueprint(TEXT("XToolsLoopSplitPinValueTest"), EventGraph);
	if (!TestNotNull(TEXT("创建拆分引脚测试蓝图"), Blueprint)
		|| !TestNotNull(TEXT("获取拆分引脚测试事件图"), EventGraph))
	{
		return false;
	}

	UK2Node_ForEachLoopWithDelay* LoopNode = AddTestNode<UK2Node_ForEachLoopWithDelay>(EventGraph);

	// Transform 数组作为输入，使 Value 引脚传播为可拆分的结构体
	UK2Node_TemporaryVariable* ArraySource = AddTestNode<UK2Node_TemporaryVariable>(EventGraph);
	ArraySource->VariableType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	ArraySource->VariableType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
	ArraySource->VariableType.ContainerType = EPinContainerType::Array;
	ArraySource->ReconstructNode();

	const UEdGraphSchema* Schema = EventGraph->GetSchema();
	if (!TestTrue(TEXT("连接 Transform 测试数组"),
		Schema && Schema->TryCreateConnection(ArraySource->GetVariablePin(), LoopNode->GetArrayPin())))
	{
		return false;
	}
	LoopNode->NotifyPinConnectionListChanged(LoopNode->GetArrayPin());

	UEdGraphPin* ValuePin = LoopNode->GetValuePin();
	if (!TestEqual(TEXT("Value 引脚已传播为 Transform 结构体"),
		ValuePin->PinType.PinCategory, UEdGraphSchema_K2::PC_Struct))
	{
		return false;
	}

	// 模拟用户在节点上执行「分割结构体引脚」
	const UEdGraphSchema_K2* K2Schema = CastChecked<UEdGraphSchema_K2>(Schema);
	K2Schema->SplitPin(ValuePin, false);
	if (!TestTrue(TEXT("Value 引脚已拆分为子引脚"), ValuePin->SubPins.Num() > 0))
	{
		return false;
	}

	// 找到 Location 子引脚并连接一个消费端（取向量长度）
	UEdGraphPin* LocationSubPin = nullptr;
	for (UEdGraphPin* SubPin : ValuePin->SubPins)
	{
		if (SubPin && SubPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct
			&& SubPin->PinType.PinSubCategoryObject == TBaseStructure<FVector>::Get())
		{
			LocationSubPin = SubPin;
			break;
		}
	}
	if (!TestNotNull(TEXT("找到 Location 拆分引脚"), LocationSubPin))
	{
		return false;
	}

	UK2Node_CallFunction* Consumer = AddTestNode<UK2Node_CallFunction>(EventGraph);
	Consumer->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, VSize)));
	Consumer->AllocateDefaultPins();
	UEdGraphPin* ConsumerInputPin = Consumer->FindPinChecked(TEXT("A"), EGPD_Input);
	if (!TestTrue(TEXT("拆分引脚连接消费端"),
		Schema->TryCreateConnection(LocationSubPin, ConsumerInputPin)))
	{
		return false;
	}

	FCompilerResultsLog Results;
	FKismetCompilerOptions Options;
	FExpansionTestCompilerContext CompilerContext(Blueprint, Results, Options);
	LoopNode->ExpandNode(CompilerContext, EventGraph);
	TestEqual(TEXT("拆分引脚展开无编译错误"), Results.NumErrors, 0);

	// 核心断言：拆分引脚的链接必须在展开后存活，而不是被静默断开
	if (!TestEqual(TEXT("拆分引脚链接在展开后保留"), ConsumerInputPin->LinkedTo.Num(), 1))
	{
		return false;
	}

	// 链接应经由自动生成的 Break 节点回到数组 Get 调用的元素输出。
	// 注意：FTransform 走 MD_NativeBreakFunction 元数据，生成的是调用 BreakTransform 的
	// UK2Node_CallFunction 而非 UK2Node_BreakStruct，这里对两种形态都接受。
	UEdGraphNode* SplitExpandNode = ConsumerInputPin->LinkedTo[0]->GetOwningNode();
	UK2Node* SplitK2Node = Cast<UK2Node>(SplitExpandNode);
	const bool bIsBreakForm = Cast<UK2Node_BreakStruct>(SplitExpandNode) != nullptr
		|| Cast<UK2Node_CallFunction>(SplitExpandNode) != nullptr;
	if (!TestTrue(TEXT("拆分引脚展开为 Break 形态节点"), SplitK2Node != nullptr && bIsBreakForm))
	{
		return false;
	}

	UEdGraphPin* BreakInputPin = nullptr;
	for (UEdGraphPin* Pin : SplitK2Node->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct
			&& Pin->PinType.PinSubCategoryObject == TBaseStructure<FTransform>::Get())
		{
			BreakInputPin = Pin;
			break;
		}
	}
	if (!TestNotNull(TEXT("找到 Break 节点 Transform 输入"), BreakInputPin)
		|| !TestEqual(TEXT("Break 输入连接到数组元素输出"), BreakInputPin->LinkedTo.Num(), 1))
	{
		return false;
	}

	UK2Node_CallArrayFunction* GetArrayItem = Cast<UK2Node_CallArrayFunction>(BreakInputPin->LinkedTo[0]->GetOwningNode());
	TestNotNull(TEXT("数组元素来自 Array_Get 调用"), GetArrayItem);
	return !HasAnyErrors();
}

#endif
