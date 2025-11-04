#include "K2Nodes/K2Node_ForLoop.h"

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

#define LOCTEXT_NAMESPACE "XTools_K2Node_ForLoop"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region Helper

namespace ForLoopHelper
{
    const FName FirstPinName = FName("FirstIndex");
    const FName LastPinName = FName("LastIndex");
    const FName LoopBodyPinName = FName("Loop Body");
    const FName IndexPinName = FName("Index");
    const FName BreakPinName = FName("Break");
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeAppearance

FText UK2Node_ForLoop::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return LOCTEXT("ForLoopTitle", "ForLoop");
}

FText UK2Node_ForLoop::GetCompactNodeTitle() const
{
    return LOCTEXT("ForLoopCompactNodeTitle", "FORLOOP");
}

FText UK2Node_ForLoop::GetTooltipText() const
{
    return LOCTEXT("ForLoopTooltipText", "在指定范围内循环执行");
}

FText UK2Node_ForLoop::GetKeywords() const
{
    return LOCTEXT("Keywords", "for loop 循环 for each 遍历 计数");
}

FText UK2Node_ForLoop::GetMenuCategory() const
{
    return LOCTEXT("ForLoopCategory", "XTools|Blueprint Extensions|Loops");
}

FSlateIcon UK2Node_ForLoop::GetIconAndTint(FLinearColor& OutColor) const
{
    static FSlateIcon Icon("EditorStyle", "GraphEditor.Macro.Loop_16x");
    return Icon;
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintCompile

void UK2Node_ForLoop::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    Super::ExpandNode(CompilerContext, SourceGraph);

    const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();

    bool bResult = true;
    // Create int Loop Counter
    UK2Node_TemporaryVariable* LoopCounterNode = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(this, SourceGraph);
    LoopCounterNode->VariableType.PinCategory = UEdGraphSchema_K2::PC_Int;
    LoopCounterNode->AllocateDefaultPins();
    UEdGraphPin* LoopCounterPin = LoopCounterNode->GetVariablePin();
    check(LoopCounterPin);

    // Initialise loop counter
    UK2Node_AssignmentStatement* LoopCounterInitialise = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
    LoopCounterInitialise->AllocateDefaultPins();
    CompilerContext.MovePinLinksToIntermediate(*GetFirstIndexPin(), *LoopCounterInitialise->GetValuePin());
    bResult &= Schema->TryCreateConnection(LoopCounterInitialise->GetVariablePin(), LoopCounterPin);
    UEdGraphPin* LoopCounterInitialiseExecPin = LoopCounterInitialise->GetExecPin();
    check(LoopCounterInitialiseExecPin);

    if (!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("InitCounterFailed", "Could not connect initialise loop counter node @@").ToString(), this);

    // Do loop branch
    UK2Node_IfThenElse* Branch = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
    Branch->AllocateDefaultPins();
    bResult &= Schema->TryCreateConnection(LoopCounterInitialise->GetThenPin(), Branch->GetExecPin());
    UEdGraphPin* BranchElsePin = Branch->GetElsePin();
    check(BranchElsePin);

    if (!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("BranchFailed", "Could not connect branch node @@").ToString(), this);

    // Do loop condition
    UK2Node_CallFunction* Condition = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    Condition->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, LessEqual_IntInt)));
    Condition->AllocateDefaultPins();
    bResult &= Schema->TryCreateConnection(Condition->GetReturnValuePin(), Branch->GetConditionPin());
    bResult &= Schema->TryCreateConnection(Condition->FindPinChecked(TEXT("A")), LoopCounterPin);
    CompilerContext.MovePinLinksToIntermediate(*GetLastIndexPin(), *Condition->FindPinChecked(TEXT("B")));

    if (!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("ConditionFailed", "Could not connect loop condition node @@").ToString(), this);

    // Break loop from set counter
    UK2Node_AssignmentStatement* LoopCounterBreak = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
    LoopCounterBreak->AllocateDefaultPins();
    CompilerContext.MovePinLinksToIntermediate(*GetLastIndexPin(), *LoopCounterBreak->GetValuePin());
    bResult &= Schema->TryCreateConnection(LoopCounterBreak->GetVariablePin(), LoopCounterPin);
    UEdGraphPin* LoopCounterBreakExecPin = LoopCounterBreak->GetExecPin();
    check(LoopCounterBreakExecPin);

    if (!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("BreakNodeFailed", "Could not set BreakNode from length node @@").ToString(), this);

    // Sequence
    UK2Node_ExecutionSequence* Sequence = CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(this, SourceGraph);
    Sequence->AllocateDefaultPins();
    bResult &= Schema->TryCreateConnection(Sequence->GetExecPin(), Branch->GetThenPin());
    UEdGraphPin* SequenceThen0Pin = Sequence->GetThenPinGivenIndex(0);
    UEdGraphPin* SequenceThen1Pin = Sequence->GetThenPinGivenIndex(1);
    check(SequenceThen0Pin);
    check(SequenceThen1Pin);

    if (!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("SequenceFailed", "Could not connect sequence node @@").ToString(), this);

    // Loop Counter increment
    UK2Node_CallFunction* Increment = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    Increment->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Add_IntInt)));
    Increment->AllocateDefaultPins();
    bResult &= Schema->TryCreateConnection(Increment->FindPinChecked(TEXT("A")), LoopCounterPin);
    Increment->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("1");

    if (!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("IncrementFailed", "Could not connect loop counter increment node @@").ToString(), this);

    // Loop Counter assigned
    UK2Node_AssignmentStatement* LoopCounterAssign = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
    LoopCounterAssign->AllocateDefaultPins();
    bResult &= Schema->TryCreateConnection(LoopCounterAssign->GetExecPin(), SequenceThen1Pin);
    bResult &= Schema->TryCreateConnection(LoopCounterAssign->GetVariablePin(), LoopCounterPin);
    bResult &= Schema->TryCreateConnection(LoopCounterAssign->GetValuePin(), Increment->GetReturnValuePin());
    bResult &= Schema->TryCreateConnection(LoopCounterAssign->GetThenPin(), Branch->GetExecPin());

    if (!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("AssignmentFailed", "Could not connect loop counter assignment node @@").ToString(), this);

    // Loop Last Counter assigned
    UK2Node_AssignmentStatement* LoopLastCounterAssign = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
    LoopLastCounterAssign->AllocateDefaultPins();
    bResult &= Schema->TryCreateConnection(LoopLastCounterAssign->GetVariablePin(), LoopCounterPin);
    bResult &= Schema->TryCreateConnection(LoopLastCounterAssign->GetValuePin(), Increment->GetReturnValuePin());
    bResult &= Schema->TryCreateConnection(LoopCounterBreak->GetThenPin(), LoopLastCounterAssign->GetExecPin());

    CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *LoopCounterInitialiseExecPin);
    CompilerContext.MovePinLinksToIntermediate(*GetLoopBodyPin(), *SequenceThen0Pin);
    CompilerContext.MovePinLinksToIntermediate(*GetCompletedPin(), *BranchElsePin);
    CompilerContext.MovePinLinksToIntermediate(*GetBreakPin(), *LoopCounterBreakExecPin);
    CompilerContext.MovePinLinksToIntermediate(*GetIndexPin(), *LoopCounterPin);

    BreakAllNodeLinks();
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintSystem

void UK2Node_ForLoop::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
    const UClass* Action = GetClass();

    if (ActionRegistrar.IsOpenForRegistration(Action))
    {
        UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(GetClass());
        check(Spawner != nullptr);

        ActionRegistrar.AddBlueprintAction(Action, Spawner);
    }
}

void UK2Node_ForLoop::PostReconstructNode()
{
    Super::PostReconstructNode();
}

void UK2Node_ForLoop::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
    Super::NotifyPinConnectionListChanged(Pin);
    GetGraph()->NotifyGraphChanged();
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region PinManagement

void UK2Node_ForLoop::AllocateDefaultPins()
{
    Super::AllocateDefaultPins();

    // Execute
    CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);

    // First Index
    CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, ForLoopHelper::FirstPinName);

    // Last Index
    CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, ForLoopHelper::LastPinName);

    // Break
    CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, ForLoopHelper::BreakPinName)->PinFriendlyName = FText::FromName(ForLoopHelper::BreakPinName);

    // Loop body
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, ForLoopHelper::LoopBodyPinName);

    // Index
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Int, ForLoopHelper::IndexPinName);

    // Completed
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then)->PinFriendlyName = FText::FromName(UEdGraphSchema_K2::PN_Completed);
}

bool UK2Node_ForLoop::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
    return Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason);
}

UEdGraphPin* UK2Node_ForLoop::GetLoopBodyPin() const
{
    return FindPinChecked(ForLoopHelper::LoopBodyPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForLoop::GetFirstIndexPin() const
{
    return FindPinChecked(ForLoopHelper::FirstPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForLoop::GetLastIndexPin() const
{
    return FindPinChecked(ForLoopHelper::LastPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForLoop::GetCompletedPin() const
{
    return FindPinChecked(UEdGraphSchema_K2::PN_Then, EGPD_Output);
}

UEdGraphPin* UK2Node_ForLoop::GetBreakPin() const
{
    return FindPinChecked(ForLoopHelper::BreakPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForLoop::GetIndexPin() const
{
    return FindPinChecked(ForLoopHelper::IndexPinName, EGPD_Output);
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#undef LOCTEXT_NAMESPACE
