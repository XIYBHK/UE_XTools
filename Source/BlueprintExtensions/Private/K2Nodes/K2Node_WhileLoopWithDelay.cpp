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
		"条件为True时循环执行\n\n- 每次执行循环体后等待Delay秒，再重新检查Condition\n- Delay为0或负数时也会通过Latent流程到下一次更新，避免同帧无限循环\n- 支持Break中断循环\n- 只能放在事件图中，不能放在蓝图函数或宏图中");
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
	if (!K2NodeHelpers::IsLatentGraphCompatible(SourceGraph))
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

	UK2Node_TemporaryVariable* BreakFlagNode = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(this, SourceGraph);
	BreakFlagNode->VariableType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	BreakFlagNode->AllocateDefaultPins();
	UEdGraphPin* BreakFlagPin = BreakFlagNode->GetVariablePin();

	UK2Node_AssignmentStatement* InitBreakFlag = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	InitBreakFlag->AllocateDefaultPins();
	InitBreakFlag->GetValuePin()->DefaultValue = TEXT("false");
	K2NodeHelpers::TryConnect(CompilerContext, BreakFlagPin, InitBreakFlag->GetVariablePin());

	UK2Node_IfThenElse* ConditionBranch = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
	ConditionBranch->AllocateDefaultPins();
	K2NodeHelpers::TryConnect(CompilerContext, InitBreakFlag->GetThenPin(), ConditionBranch->GetExecPin());

	UK2Node_ExecutionSequence* LoopSequence = CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(this, SourceGraph);
	LoopSequence->AllocateDefaultPins();
	K2NodeHelpers::TryConnect(CompilerContext, ConditionBranch->GetThenPin(), LoopSequence->GetExecPin());

	UK2Node_CallFunction* DelayNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	DelayNode->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Delay)));
	DelayNode->AllocateDefaultPins();
	K2NodeHelpers::TryConnect(CompilerContext, LoopSequence->GetThenPinGivenIndex(1), DelayNode->GetExecPin());

	UK2Node_IfThenElse* BreakGate = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
	BreakGate->AllocateDefaultPins();
	K2NodeHelpers::TryConnect(CompilerContext, DelayNode->GetThenPin(), BreakGate->GetExecPin());
	K2NodeHelpers::TryConnect(CompilerContext, BreakFlagPin, BreakGate->GetConditionPin());
	K2NodeHelpers::TryConnect(CompilerContext, BreakGate->GetElsePin(), ConditionBranch->GetExecPin());

	UK2Node_ExecutionSequence* CompleteSequence = CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(this, SourceGraph);
	CompleteSequence->AllocateDefaultPins();
	K2NodeHelpers::TryConnect(CompilerContext, ConditionBranch->GetElsePin(), CompleteSequence->GetExecPin());

	UK2Node_AssignmentStatement* BreakFlagAssign = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	BreakFlagAssign->AllocateDefaultPins();
	BreakFlagAssign->GetValuePin()->DefaultValue = TEXT("true");
	K2NodeHelpers::TryConnect(CompilerContext, BreakFlagPin, BreakFlagAssign->GetVariablePin());
	K2NodeHelpers::TryConnect(CompilerContext, BreakFlagAssign->GetThenPin(), CompleteSequence->GetExecPin());

	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *InitBreakFlag->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*GetBreakPin(), *BreakFlagAssign->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*GetConditionPin(), *ConditionBranch->GetConditionPin());
	CompilerContext.MovePinLinksToIntermediate(*GetDelayPin(), *DelayNode->FindPinChecked(TEXT("Duration")));
	CompilerContext.MovePinLinksToIntermediate(*GetLoopBodyPin(), *LoopSequence->GetThenPinGivenIndex(0));
	CompilerContext.MovePinLinksToIntermediate(*GetCompletedPin(), *CompleteSequence->GetThenPinGivenIndex(0));

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
	DelayPin->PinToolTip = LOCTEXT("DelayTooltip", "每次循环体执行后的等待时间，单位为秒。0或负数也会通过Latent流程进入下一次更新").ToString();

	UEdGraphPin* LoopBodyPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, LoopBodyPinName);
	LoopBodyPin->PinToolTip = LOCTEXT("LoopBodyTooltip", "循环体：Condition为True时执行").ToString();

	UEdGraphPin* BreakPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, BreakPinName);
	BreakPin->PinToolTip = LOCTEXT("BreakTooltip", "中断循环并立即执行Completed").ToString();

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
