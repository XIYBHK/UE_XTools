#include "K2Nodes/K2Node_WhileLoopWithDelay.h"
#include "K2Nodes/K2NodeHelpers.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"

#include "K2Node_AssignmentStatement.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_TemporaryVariable.h"

#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

#define LOCTEXT_NAMESPACE "XTools_K2Node_WhileLoopWithDelay"

namespace WhileLoopWithDelayHelper
{
const FName ConditionPinName = FName("Condition");
const FName DelayPinName = FName("Delay");
const FName LoopBodyPinName = FName("Loop Body");
const FName BreakPinName = FName("Break");
}

#pragma region NodeAppearance

FText UK2Node_WhileLoopWithDelay::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "带延迟的WhileLoop");
}

FText UK2Node_WhileLoopWithDelay::GetCompactNodeTitle() const
{
	return LOCTEXT("CompactNodeTitle", "WHILE\nDELAY");
}

FText UK2Node_WhileLoopWithDelay::GetTooltipText() const
{
	return LOCTEXT(
		"TooltipText",
		"Condition为True时按间隔启动循环体\n\n- Delay为0或负数时在当前帧同步继续\n- 零延迟且Condition持续为True可能触发无限循环保护\n- 支持Break中断循环\n- 运行期间再次触发同一节点会被忽略\n- 不会等待循环体中的Latent或异步节点完成");
}

FText UK2Node_WhileLoopWithDelay::GetKeywords() const
{
	return LOCTEXT("Keywords", "while loop delay latent 循环 延迟 等待 条件 break");
}

FText UK2Node_WhileLoopWithDelay::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "XTools|Blueprint Extensions|Loops");
}

FSlateIcon UK2Node_WhileLoopWithDelay::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Macro.Loop_16x");
	return Icon;
}

#pragma endregion

#pragma region BlueprintCompile

void UK2Node_WhileLoopWithDelay::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	// 先展开拆分引脚：结构体引脚被分割（Split Struct Pin）后链接挂在子引脚上，
	// 不展开则 MovePinLinksToIntermediate 迁移不到链接，下游会静默读到默认值
	Super::ExpandNode(CompilerContext, SourceGraph);

	if (!K2NodeHelpers::IsLatentNodeGraphCompatible(CompilerContext, this, SourceGraph))
	{
		CompilerContext.MessageLog.Error(
			*LOCTEXT("LatentGraphOnly", "@@ 是带延迟的 Latent 节点，只能放在事件图中，不能放在蓝图函数或宏图中").ToString(),
			this);
		BreakAllNodeLinks();
		return;
	}

	if (!K2NodeHelpers::BeginExpandNode(
		CompilerContext,
		this,
		{GetExecPin(), GetConditionPin(), GetDelayPin(), GetLoopBodyPin(), GetBreakPin(), GetCompletedPin()},
		LOCTEXT("MissingPins", "@@ 节点引脚不完整")))
	{
		return;
	}

	const K2NodeHelpers::FSingleFlightExecutionGuard ExecutionGuard =
		K2NodeHelpers::CreateSingleFlightExecutionGuard(CompilerContext, this, SourceGraph);

	UK2Node_TemporaryVariable* BreakFlagNode = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(this, SourceGraph);
	BreakFlagNode->VariableType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	BreakFlagNode->AllocateDefaultPins();
	UEdGraphPin* BreakFlagPin = BreakFlagNode->GetVariablePin();

	UK2Node_AssignmentStatement* InitBreakFlag = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	InitBreakFlag->AllocateDefaultPins();
	InitBreakFlag->GetValuePin()->DefaultValue = TEXT("false");
	K2NodeHelpers::TryConnect(CompilerContext, BreakFlagPin, InitBreakFlag->GetVariablePin());
	K2NodeHelpers::TryConnect(CompilerContext, ExecutionGuard.StartThenPin, InitBreakFlag->GetExecPin());

	UK2Node_IfThenElse* ConditionBranch = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
	ConditionBranch->AllocateDefaultPins();
	K2NodeHelpers::TryConnect(CompilerContext, InitBreakFlag->GetThenPin(), ConditionBranch->GetExecPin());

	UK2Node_ExecutionSequence* LoopSequence = CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(this, SourceGraph);
	LoopSequence->AllocateDefaultPins();
	K2NodeHelpers::TryConnect(CompilerContext, ConditionBranch->GetThenPin(), LoopSequence->GetExecPin());

	UK2Node_IfThenElse* PostBodyBreakGate = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
	PostBodyBreakGate->AllocateDefaultPins();
	K2NodeHelpers::TryConnect(CompilerContext, LoopSequence->GetThenPinGivenIndex(1), PostBodyBreakGate->GetExecPin());
	K2NodeHelpers::TryConnect(CompilerContext, BreakFlagPin, PostBodyBreakGate->GetConditionPin());

	UK2Node_CallFunction* DelayNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	DelayNode->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Delay)));
	DelayNode->AllocateDefaultPins();

	UK2Node_CallFunction* DelayLessEqualZero = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	DelayLessEqualZero->SetFromFunction(
		UKismetMathLibrary::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, LessEqual_DoubleDouble)));
	DelayLessEqualZero->AllocateDefaultPins();
	DelayLessEqualZero->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("0.0");

	UK2Node_IfThenElse* DelayBranch = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
	DelayBranch->AllocateDefaultPins();
	K2NodeHelpers::TryConnect(CompilerContext, PostBodyBreakGate->GetElsePin(), DelayBranch->GetExecPin());
	K2NodeHelpers::TryConnect(CompilerContext, DelayLessEqualZero->GetReturnValuePin(), DelayBranch->GetConditionPin());
	K2NodeHelpers::TryConnect(CompilerContext, DelayBranch->GetThenPin(), ConditionBranch->GetExecPin());
	K2NodeHelpers::TryConnect(CompilerContext, DelayBranch->GetElsePin(), DelayNode->GetExecPin());

	UK2Node_IfThenElse* BreakGate = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
	BreakGate->AllocateDefaultPins();
	K2NodeHelpers::TryConnect(CompilerContext, DelayNode->GetThenPin(), BreakGate->GetExecPin());
	K2NodeHelpers::TryConnect(CompilerContext, BreakFlagPin, BreakGate->GetConditionPin());
	K2NodeHelpers::TryConnect(CompilerContext, BreakGate->GetElsePin(), ConditionBranch->GetExecPin());

	UK2Node_ExecutionSequence* CompleteSequence = CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(this, SourceGraph);
	CompleteSequence->AllocateDefaultPins();
	K2NodeHelpers::TryConnect(CompilerContext, ConditionBranch->GetElsePin(), CompleteSequence->GetExecPin());
	K2NodeHelpers::TryConnect(CompilerContext, PostBodyBreakGate->GetThenPin(), CompleteSequence->GetExecPin());
	K2NodeHelpers::TryConnect(CompilerContext, BreakGate->GetThenPin(), CompleteSequence->GetExecPin());
	K2NodeHelpers::TryConnect(CompilerContext, CompleteSequence->GetThenPinGivenIndex(0), ExecutionGuard.FinishExecPin);

	UK2Node_AssignmentStatement* BreakFlagAssign = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	BreakFlagAssign->AllocateDefaultPins();
	BreakFlagAssign->GetValuePin()->DefaultValue = TEXT("true");
	K2NodeHelpers::TryConnect(CompilerContext, BreakFlagPin, BreakFlagAssign->GetVariablePin());

	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *ExecutionGuard.EntryExecPin);
	CompilerContext.MovePinLinksToIntermediate(*GetBreakPin(), *BreakFlagAssign->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*GetConditionPin(), *ConditionBranch->GetConditionPin());
	CompilerContext.CopyPinLinksToIntermediate(*GetDelayPin(), *DelayLessEqualZero->FindPinChecked(TEXT("A")));
	CompilerContext.MovePinLinksToIntermediate(*GetDelayPin(), *DelayNode->FindPinChecked(TEXT("Duration")));
	CompilerContext.MovePinLinksToIntermediate(*GetLoopBodyPin(), *LoopSequence->GetThenPinGivenIndex(0));
	CompilerContext.MovePinLinksToIntermediate(*GetCompletedPin(), *ExecutionGuard.FinishThenPin);

	K2NodeHelpers::EndExpandNode(this);
}

#pragma endregion

#pragma region BlueprintSystem

void UK2Node_WhileLoopWithDelay::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	K2NodeHelpers::RegisterNode<UK2Node_WhileLoopWithDelay>(ActionRegistrar);
}

void UK2Node_WhileLoopWithDelay::PostReconstructNode()
{
	Super::PostReconstructNode();
}

bool UK2Node_WhileLoopWithDelay::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	return K2NodeHelpers::IsLatentGraphCompatible(TargetGraph) && Super::IsCompatibleWithGraph(TargetGraph);
}

#pragma endregion

#pragma region PinManagement

void UK2Node_WhileLoopWithDelay::AllocateDefaultPins()
{
	using namespace WhileLoopWithDelayHelper;

	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);

	UEdGraphPin* ConditionPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Boolean, ConditionPinName);
	ConditionPin->DefaultValue = TEXT("true");
	ConditionPin->PinToolTip = LOCTEXT("ConditionTooltip", "循环条件。每次循环体执行后会重新检查此条件").ToString();

	UEdGraphPin* DelayPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float, DelayPinName);
	DelayPin->DefaultValue = TEXT("0.1");
	DelayPin->PinToolTip = LOCTEXT("DelayTooltip", "每次循环体执行后的等待时间，单位为秒。0或负数表示在当前帧同步继续").ToString();

	UEdGraphPin* LoopBodyPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, LoopBodyPinName);
	LoopBodyPin->PinToolTip = LOCTEXT("LoopBodyTooltip", "循环体：Condition为True时启动；不会等待其中的Latent或异步节点完成").ToString();

	UEdGraphPin* BreakPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, BreakPinName);
	BreakPin->PinToolTip = LOCTEXT("BreakTooltip", "在当前循环边界中断并执行Completed").ToString();

	UEdGraphPin* CompletedPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	CompletedPin->PinFriendlyName = LOCTEXT("CompletedPinName", "Completed");
	CompletedPin->PinToolTip = LOCTEXT("CompletedTooltip", "Condition为False或Break触发时执行").ToString();
}

UEdGraphPin* UK2Node_WhileLoopWithDelay::GetConditionPin() const
{
	return FindPinChecked(WhileLoopWithDelayHelper::ConditionPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_WhileLoopWithDelay::GetDelayPin() const
{
	return FindPinChecked(WhileLoopWithDelayHelper::DelayPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_WhileLoopWithDelay::GetLoopBodyPin() const
{
	return FindPinChecked(WhileLoopWithDelayHelper::LoopBodyPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_WhileLoopWithDelay::GetBreakPin() const
{
	return FindPin(WhileLoopWithDelayHelper::BreakPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_WhileLoopWithDelay::GetCompletedPin() const
{
	return FindPinChecked(UEdGraphSchema_K2::PN_Then, EGPD_Output);
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
