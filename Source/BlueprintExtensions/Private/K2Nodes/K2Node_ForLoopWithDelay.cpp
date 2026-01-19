#include "K2Nodes/K2Node_ForLoopWithDelay.h"
#include "K2Nodes/K2NodeHelpers.h"

// 编辑器
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"

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

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region Helper

namespace ForLoopWithDelayHelper {
const FName FirstPinName = FName("FirstIndex");
const FName LastPinName = FName("LastIndex");
const FName DelayPinName = FName("Delay");
const FName LoopBodyPinName = FName("Loop Body");
const FName IndexPinName = FName("Index");
const FName BreakPinName = FName("Break");
} // namespace ForLoopWithDelayHelper

#pragma endregion

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeAppearance

FText UK2Node_ForLoopWithDelay::GetNodeTitle(
    ENodeTitleType::Type TitleType) const {
  return LOCTEXT("NodeTitle", "带延迟的ForLoop");
}

FText UK2Node_ForLoopWithDelay::GetCompactNodeTitle() const {
  return LOCTEXT("CompactNodeTitle", "FORLOOP\nDELAY");
}

FText UK2Node_ForLoopWithDelay::GetTooltipText() const {
  return LOCTEXT(
      "TooltipText",
      "在指定范围内循环执行\n\n- 支持延迟：每次迭代之间可设置等待时间\n- "
      "支持Break中断循环\n- 适用于需要顺序计数的场景");
}

FText UK2Node_ForLoopWithDelay::GetKeywords() const {
  return LOCTEXT("Keywords",
                 "for loop delay 循环 延迟 等待 for each 遍历 计数");
}

FText UK2Node_ForLoopWithDelay::GetMenuCategory() const {
  return LOCTEXT("MenuCategory", "XTools|Blueprint Extensions|Loops");
}

FSlateIcon
UK2Node_ForLoopWithDelay::GetIconAndTint(FLinearColor &OutColor) const {
  static FSlateIcon Icon("EditorStyle", "GraphEditor.Macro.Loop_16x");
  return Icon;
}

#pragma endregion

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintCompile

void UK2Node_ForLoopWithDelay::ExpandNode(
    FKismetCompilerContext &CompilerContext, UEdGraph *SourceGraph) {
  // 【参考 K2Node_SmartSort 实现模式】
  // 不调用 Super::ExpandNode()，因为基类会提前断开所有链接

  // 编译时检查：确保必要引脚有效
  if (!GetExecPin() || !GetFirstIndexPin() || !GetLastIndexPin() ||
      !GetDelayPin()) {
    CompilerContext.MessageLog.Error(
        *LOCTEXT("MissingPins", "@@ 节点引脚不完整").ToString(), this);
    return;
  }

  // 1. 创建循环计数器临时变量
  UK2Node_TemporaryVariable *LoopCounterNode =
      CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(
          this, SourceGraph);
  LoopCounterNode->VariableType.PinCategory = UEdGraphSchema_K2::PC_Int;
  LoopCounterNode->AllocateDefaultPins();
  UEdGraphPin *LoopCounterPin = LoopCounterNode->GetVariablePin();

  // 2. 初始化循环计数器
  UK2Node_AssignmentStatement *LoopCounterInit =
      CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(
          this, SourceGraph);
  LoopCounterInit->AllocateDefaultPins();
  CompilerContext.GetSchema()->TryCreateConnection(
      LoopCounterPin, LoopCounterInit->GetVariablePin());

  // 3. 创建分支节点
  UK2Node_IfThenElse *Branch =
      CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this,
                                                                SourceGraph);
  Branch->AllocateDefaultPins();
  CompilerContext.GetSchema()->TryCreateConnection(
      LoopCounterInit->GetThenPin(), Branch->GetExecPin());

  // 4. 创建循环条件（计数器 <= 最后索引）
  UK2Node_CallFunction *Condition =
      CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,
                                                                  SourceGraph);
  Condition->SetFromFunction(
      UKismetMathLibrary::StaticClass()->FindFunctionByName(
          GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, LessEqual_IntInt)));
  Condition->AllocateDefaultPins();
  CompilerContext.GetSchema()->TryCreateConnection(
      Condition->GetReturnValuePin(), Branch->GetConditionPin());
  CompilerContext.GetSchema()->TryCreateConnection(
      Condition->FindPinChecked(TEXT("A")), LoopCounterPin);

  // 5. 创建延迟节点
  UK2Node_CallFunction *DelayNode =
      CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,
                                                                  SourceGraph);
  DelayNode->SetFromFunction(
      UKismetSystemLibrary::StaticClass()->FindFunctionByName(
          GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Delay)));
  DelayNode->AllocateDefaultPins();
  CompilerContext.GetSchema()->TryCreateConnection(Branch->GetThenPin(),
                                                   DelayNode->GetExecPin());

  // 6. 创建执行序列（循环体 -> 递增）
  UK2Node_ExecutionSequence *Sequence =
      CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(
          this, SourceGraph);
  Sequence->AllocateDefaultPins();
  CompilerContext.GetSchema()->TryCreateConnection(DelayNode->GetThenPin(),
                                                   Sequence->GetExecPin());

  // 7. 创建递增节点
  UK2Node_CallFunction *Increment =
      CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,
                                                                  SourceGraph);
  Increment->SetFromFunction(
      UKismetMathLibrary::StaticClass()->FindFunctionByName(
          GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Add_IntInt)));
  Increment->AllocateDefaultPins();
  CompilerContext.GetSchema()->TryCreateConnection(
      Increment->FindPinChecked(TEXT("A")), LoopCounterPin);
  Increment->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("1");

  // 8. 创建赋值节点（递增后的值）
  UK2Node_AssignmentStatement *LoopCounterAssign =
      CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(
          this, SourceGraph);
  LoopCounterAssign->AllocateDefaultPins();
  CompilerContext.GetSchema()->TryCreateConnection(
      LoopCounterAssign->GetExecPin(), Sequence->GetThenPinGivenIndex(1));
  CompilerContext.GetSchema()->TryCreateConnection(
      LoopCounterAssign->GetVariablePin(), LoopCounterPin);
  CompilerContext.GetSchema()->TryCreateConnection(
      LoopCounterAssign->GetValuePin(), Increment->GetReturnValuePin());
  CompilerContext.GetSchema()->TryCreateConnection(
      LoopCounterAssign->GetThenPin(), Branch->GetExecPin()); // 循环回到分支

  // 9. Break 功能：创建 LastIndex+1 来跳出循环
  UK2Node_CallFunction *BreakValue =
      CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,
                                                                  SourceGraph);
  BreakValue->SetFromFunction(
      UKismetMathLibrary::StaticClass()->FindFunctionByName(
          GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Add_IntInt)));
  BreakValue->AllocateDefaultPins();
  CompilerContext.GetSchema()->TryCreateConnection(
      BreakValue->FindPinChecked(TEXT("A")),
      Condition->FindPinChecked(TEXT("B")));
  BreakValue->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("1");

  UK2Node_AssignmentStatement *LoopCounterBreak =
      CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(
          this, SourceGraph);
  LoopCounterBreak->AllocateDefaultPins();
  CompilerContext.GetSchema()->TryCreateConnection(
      LoopCounterBreak->GetVariablePin(), LoopCounterPin);
  CompilerContext.GetSchema()->TryCreateConnection(
      LoopCounterBreak->GetValuePin(), BreakValue->GetReturnValuePin());
  CompilerContext.GetSchema()->TryCreateConnection(
      LoopCounterBreak->GetThenPin(), Branch->GetElsePin());

  // 10. 最后统一移动所有外部连接（参考智能排序模式）
  CompilerContext.MovePinLinksToIntermediate(*GetExecPin(),
                                             *LoopCounterInit->GetExecPin());
  CompilerContext.MovePinLinksToIntermediate(*GetFirstIndexPin(),
                                             *LoopCounterInit->GetValuePin());
  CompilerContext.MovePinLinksToIntermediate(
      *GetLastIndexPin(), *Condition->FindPinChecked(TEXT("B")));
  CompilerContext.MovePinLinksToIntermediate(
      *GetDelayPin(), *DelayNode->FindPinChecked(TEXT("Duration")));
  CompilerContext.MovePinLinksToIntermediate(
      *GetLoopBodyPin(), *Sequence->GetThenPinGivenIndex(0));
  CompilerContext.MovePinLinksToIntermediate(*GetCompletedPin(),
                                             *Branch->GetElsePin());
  CompilerContext.MovePinLinksToIntermediate(*GetIndexPin(), *LoopCounterPin);

  // Break 引脚连接到专门的 Break 赋值节点
  if (UEdGraphPin *BreakPin = GetBreakPin()) {
    CompilerContext.MovePinLinksToIntermediate(*BreakPin,
                                               *LoopCounterBreak->GetExecPin());
  }

  // 11. 断开原节点所有链接
  BreakAllNodeLinks();
}

#pragma endregion

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintSystem

void UK2Node_ForLoopWithDelay::GetMenuActions(
    FBlueprintActionDatabaseRegistrar &ActionRegistrar) const {
  K2NodeHelpers::RegisterNode<UK2Node_ForLoopWithDelay>(ActionRegistrar);
}

void UK2Node_ForLoopWithDelay::PostReconstructNode() {
  Super::PostReconstructNode();
}

bool UK2Node_ForLoopWithDelay::IsCompatibleWithGraph(
    const UEdGraph *TargetGraph) const {
  return K2NodeHelpers::IsEventGraphCompatible(TargetGraph) &&
         Super::IsCompatibleWithGraph(TargetGraph);
}

#pragma endregion

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region PinManagement

void UK2Node_ForLoopWithDelay::AllocateDefaultPins() {
  using namespace ForLoopWithDelayHelper;

  // 输入执行引脚
  CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec,
            UEdGraphSchema_K2::PN_Execute);

  // FirstIndex 输入
  UEdGraphPin *FirstPin =
      CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, FirstPinName);
  FirstPin->DefaultValue = TEXT("0");
  FirstPin->PinToolTip =
      LOCTEXT("FirstIndexTooltip", "起始索引（从此值开始递增）").ToString();

  // LastIndex 输入
  UEdGraphPin *LastPin =
      CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, LastPinName);
  LastPin->DefaultValue = TEXT("10");
  LastPin->PinToolTip =
      LOCTEXT("LastIndexTooltip", "结束索引（递增到此值后停止，包含此值）")
          .ToString();

  // Delay 输入
  UEdGraphPin *DelayPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real,
                                    UEdGraphSchema_K2::PC_Float, DelayPinName);
  DelayPin->DefaultValue = TEXT("0.1");
  DelayPin->PinToolTip =
      LOCTEXT("DelayTooltip",
              "每次循环之间的延迟时间，单位为秒\n0表示无延迟（但仍会延迟一帧）")
          .ToString();

  // LoopBody 输出执行引脚
  UEdGraphPin *LoopBodyPin =
      CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, LoopBodyPinName);
  LoopBodyPin->PinToolTip =
      LOCTEXT("LoopBodyTooltip", "循环体：每次迭代时执行").ToString();

  // Index 输出
  UEdGraphPin *IndexPin =
      CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Int, IndexPinName);
  IndexPin->PinToolTip = LOCTEXT("IndexTooltip", "当前循环索引").ToString();

  // Break 输入执行引脚（可选）
  CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, BreakPinName);

  // Completed 输出执行引脚
  UEdGraphPin *CompletedPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec,
                                        UEdGraphSchema_K2::PN_Then);
  CompletedPin->PinFriendlyName = LOCTEXT("CompletedPinName", "Completed");
  CompletedPin->PinToolTip =
      LOCTEXT("CompletedTooltip", "循环完成时执行").ToString();
}

UEdGraphPin *UK2Node_ForLoopWithDelay::GetFirstIndexPin() const {
  return FindPinChecked(ForLoopWithDelayHelper::FirstPinName, EGPD_Input);
}

UEdGraphPin *UK2Node_ForLoopWithDelay::GetLastIndexPin() const {
  return FindPinChecked(ForLoopWithDelayHelper::LastPinName, EGPD_Input);
}

UEdGraphPin *UK2Node_ForLoopWithDelay::GetDelayPin() const {
  return FindPinChecked(ForLoopWithDelayHelper::DelayPinName, EGPD_Input);
}

UEdGraphPin *UK2Node_ForLoopWithDelay::GetLoopBodyPin() const {
  return FindPinChecked(ForLoopWithDelayHelper::LoopBodyPinName, EGPD_Output);
}

UEdGraphPin *UK2Node_ForLoopWithDelay::GetBreakPin() const {
  return FindPin(ForLoopWithDelayHelper::BreakPinName, EGPD_Input);
}

UEdGraphPin *UK2Node_ForLoopWithDelay::GetCompletedPin() const {
  return FindPinChecked(UEdGraphSchema_K2::PN_Then, EGPD_Output);
}

UEdGraphPin *UK2Node_ForLoopWithDelay::GetIndexPin() const {
  return FindPinChecked(ForLoopWithDelayHelper::IndexPinName, EGPD_Output);
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
