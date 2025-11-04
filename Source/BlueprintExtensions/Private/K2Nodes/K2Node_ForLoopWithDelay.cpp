#include "K2Nodes/K2Node_ForLoopWithDelay.h"

// 编辑器
#include "EdGraphSchema_K2.h"

// 蓝图系统
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "SPinTypeSelector.h"

// 编译器-ExpandNode相关
#include "KismetCompiler.h"

// 节点
#include "K2Node_AssignmentStatement.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_TemporaryVariable.h"

// 功能库
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

#define LOCTEXT_NAMESPACE "XTools_K2Node_ForLoopWithDelay"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region Helper

namespace ForLoopWithDelayHelper
{
	const FName FirstPinName = FName("FirstIndex");
	const FName LastPinName = FName("LastIndex");
	const FName DelayPinName = FName("Delay");
	const FName LoopBodyPinName = FName("Loop Body");
	const FName IndexPinName = FName("Index");
	const FName BreakPinName = FName("Break");
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeAppearance

FText UK2Node_ForLoopWithDelay::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "带延迟的ForLoop");
}

FText UK2Node_ForLoopWithDelay::GetCompactNodeTitle() const
{
	return LOCTEXT("CompactNodeTitle", "FORLOOP DELAY");
}

FText UK2Node_ForLoopWithDelay::GetTooltipText() const
{
	return LOCTEXT("TooltipText", "在指定范围内循环执行，每次迭代之间等待指定的延迟时间");
}

FText UK2Node_ForLoopWithDelay::GetKeywords() const
{
	return LOCTEXT("Keywords", "for loop delay 循环 延迟 等待 for each 遍历 计数");
}

FText UK2Node_ForLoopWithDelay::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "XTools|Blueprint Extensions|Loops");
}

FSlateIcon UK2Node_ForLoopWithDelay::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Macro.Loop_16x");
	return Icon;
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintCompile

void UK2Node_ForLoopWithDelay::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();

	bool bResult = true;

	// 创建循环计数器临时变量
	UK2Node_TemporaryVariable* LoopCounterNode = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(this, SourceGraph);
	LoopCounterNode->VariableType.PinCategory = UEdGraphSchema_K2::PC_Int;
	LoopCounterNode->AllocateDefaultPins();
	UEdGraphPin* LoopCounterPin = LoopCounterNode->GetVariablePin();
	check(LoopCounterPin);

	// 初始化循环计数器
	UK2Node_AssignmentStatement* LoopCounterInitialise = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	LoopCounterInitialise->AllocateDefaultPins();
	CompilerContext.MovePinLinksToIntermediate(*GetFirstIndexPin(), *LoopCounterInitialise->GetValuePin());
	bResult &= Schema->TryCreateConnection(LoopCounterInitialise->GetVariablePin(), LoopCounterPin);
	UEdGraphPin* LoopCounterInitialiseExecPin = LoopCounterInitialise->GetExecPin();
	check(LoopCounterInitialiseExecPin);

	if (!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("InitCounterFailed", "Could not connect initialise loop counter node @@").ToString(), this);

	// 创建分支节点
	UK2Node_IfThenElse* Branch = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
	Branch->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(LoopCounterInitialise->GetThenPin(), Branch->GetExecPin());
	UEdGraphPin* BranchThenPin = Branch->GetThenPin();
	UEdGraphPin* BranchElsePin = Branch->GetElsePin();
	check(BranchThenPin && BranchElsePin);

	if (!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("BranchFailed", "Could not connect branch node @@").ToString(), this);

	// 创建循环条件（小于等于）
	UK2Node_CallFunction* Condition = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	Condition->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, LessEqual_IntInt)));
	Condition->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(Condition->GetReturnValuePin(), Branch->GetConditionPin());
	bResult &= Schema->TryCreateConnection(Condition->FindPinChecked(TEXT("A")), LoopCounterPin);
	UEdGraphPin* LoopConditionBPin = Condition->FindPinChecked(TEXT("B"));
	CompilerContext.MovePinLinksToIntermediate(*GetLastIndexPin(), *LoopConditionBPin);
	check(LoopConditionBPin);

	if (!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("ConditionFailed", "Could not connect loop condition node @@").ToString(), this);

	// 创建延迟节点
	UK2Node_CallFunction* DelayNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	DelayNode->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Delay)));
	DelayNode->AllocateDefaultPins();
	
	// 连接延迟时间
	UEdGraphPin* DelayDurationPin = DelayNode->FindPinChecked(TEXT("Duration"));
	CompilerContext.MovePinLinksToIntermediate(*GetDelayPin(), *DelayDurationPin);

	// 连接 WorldContext
	UEdGraphPin* DelayWorldContextPin = DelayNode->FindPinChecked(TEXT("WorldContextObject"));
	CompilerContext.CopyPinLinksToIntermediate(*FindPinChecked(UEdGraphSchema_K2::PN_Self), *DelayWorldContextPin);

	// 将循环体执行引脚连接到延迟节点的输入
	CompilerContext.MovePinLinksToIntermediate(*GetLoopBodyPin(), *DelayNode->GetExecPin());
	
	// 创建递增节点
	UK2Node_CallFunction* Increment = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	Increment->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Add_IntInt)));
	Increment->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(Increment->FindPinChecked(TEXT("A")), LoopCounterPin);
	Increment->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("1");

	if (!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("IncrementFailed", "Could not connect increment node @@").ToString(), this);

	// 创建赋值节点（递增后的值）
	UK2Node_AssignmentStatement* LoopCounterAssignment = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	LoopCounterAssignment->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(LoopCounterAssignment->GetVariablePin(), LoopCounterPin);
	bResult &= Schema->TryCreateConnection(LoopCounterAssignment->GetValuePin(), Increment->GetReturnValuePin());

	if (!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("AssignmentFailed", "Could not connect assignment node @@").ToString(), this);

	// 延迟后连接到赋值，然后赋值连接回分支形成循环
	bResult &= Schema->TryCreateConnection(DelayNode->GetThenPin(), LoopCounterAssignment->GetExecPin());
	bResult &= Schema->TryCreateConnection(LoopCounterAssignment->GetThenPin(), Branch->GetExecPin());

	// 连接输入执行引脚到初始化
	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *LoopCounterInitialise->GetExecPin());

	// 连接Then引脚到循环体
	bResult &= Schema->TryCreateConnection(BranchThenPin, DelayNode->GetExecPin());

	// 连接完成引脚
	CompilerContext.MovePinLinksToIntermediate(*GetCompletedPin(), *BranchElsePin);

	// 连接Break引脚（如果有的话）
	if (UEdGraphPin* BreakPin = GetBreakPin())
	{
		if (BreakPin->LinkedTo.Num() > 0)
		{
			CompilerContext.MovePinLinksToIntermediate(*BreakPin, *BranchElsePin);
		}
	}

	// 连接Index输出引脚
	CompilerContext.MovePinLinksToIntermediate(*GetIndexPin(), *LoopCounterPin);

	// 断开原节点的所有链接
	BreakAllNodeLinks();
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintSystem

void UK2Node_ForLoopWithDelay::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

void UK2Node_ForLoopWithDelay::PostReconstructNode()
{
	Super::PostReconstructNode();
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region PinManagement

void UK2Node_ForLoopWithDelay::AllocateDefaultPins()
{
	using namespace ForLoopWithDelayHelper;

	// 输入执行引脚
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);

	// FirstIndex 输入
	UEdGraphPin* FirstPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, FirstPinName);
	FirstPin->DefaultValue = TEXT("0");
	FirstPin->PinToolTip = LOCTEXT("FirstIndexTooltip", "起始索引").ToString();

	// LastIndex 输入
	UEdGraphPin* LastPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, LastPinName);
	LastPin->DefaultValue = TEXT("10");
	LastPin->PinToolTip = LOCTEXT("LastIndexTooltip", "结束索引").ToString();

	// Delay 输入
	UEdGraphPin* DelayPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float, DelayPinName);
	DelayPin->DefaultValue = TEXT("0.1");
	DelayPin->PinToolTip = LOCTEXT("DelayTooltip", "每次循环之间的延迟时间（秒）").ToString();

	// LoopBody 输出执行引脚
	UEdGraphPin* LoopBodyPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, LoopBodyPinName);
	LoopBodyPin->PinToolTip = LOCTEXT("LoopBodyTooltip", "循环体：每次迭代时执行").ToString();

	// Index 输出
	UEdGraphPin* IndexPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Int, IndexPinName);
	IndexPin->PinToolTip = LOCTEXT("IndexTooltip", "当前循环索引").ToString();

	// Break 输入执行引脚（可选）
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, BreakPinName);

	// Completed 输出执行引脚
	UEdGraphPin* CompletedPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	CompletedPin->PinFriendlyName = LOCTEXT("CompletedPinName", "Completed");
	CompletedPin->PinToolTip = LOCTEXT("CompletedTooltip", "循环完成时执行").ToString();
}

UEdGraphPin* UK2Node_ForLoopWithDelay::GetFirstIndexPin() const
{
	return FindPinChecked(ForLoopWithDelayHelper::FirstPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForLoopWithDelay::GetLastIndexPin() const
{
	return FindPinChecked(ForLoopWithDelayHelper::LastPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForLoopWithDelay::GetDelayPin() const
{
	return FindPinChecked(ForLoopWithDelayHelper::DelayPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForLoopWithDelay::GetLoopBodyPin() const
{
	return FindPinChecked(ForLoopWithDelayHelper::LoopBodyPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForLoopWithDelay::GetBreakPin() const
{
	return FindPin(ForLoopWithDelayHelper::BreakPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForLoopWithDelay::GetCompletedPin() const
{
	return FindPinChecked(UEdGraphSchema_K2::PN_Then, EGPD_Output);
}

UEdGraphPin* UK2Node_ForLoopWithDelay::GetIndexPin() const
{
	return FindPinChecked(ForLoopWithDelayHelper::IndexPinName, EGPD_Output);
}

#pragma endregion

#undef LOCTEXT_NAMESPACE

