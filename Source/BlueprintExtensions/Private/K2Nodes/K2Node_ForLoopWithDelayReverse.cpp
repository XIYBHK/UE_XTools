#include "K2Nodes/K2Node_ForLoopWithDelayReverse.h"
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

#define LOCTEXT_NAMESPACE "XTools_K2Node_ForLoopWithDelayReverse"

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region Helper

namespace ForLoopWithDelayReverseHelper {
const FName FirstPinName = FName("FirstIndex");
const FName LastPinName = FName("LastIndex");
const FName DelayPinName = FName("Delay");
const FName LoopBodyPinName = FName("Loop Body");
const FName IndexPinName = FName("Index");
const FName BreakPinName = FName("Break");
} // namespace ForLoopWithDelayReverseHelper

#pragma endregion

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeAppearance

FText UK2Node_ForLoopWithDelayReverse::GetNodeTitle(
    ENodeTitleType::Type TitleType) const {
  return LOCTEXT("NodeTitle", "带延迟的倒序ForLoop");
}

FText UK2Node_ForLoopWithDelayReverse::GetCompactNodeTitle() const {
  return LOCTEXT("CompactNodeTitle", "FORLOOP\nDELAY REV");
}

FText UK2Node_ForLoopWithDelayReverse::GetTooltipText() const {
  return LOCTEXT("TooltipText",
                 "从 LastIndex 递减到 FirstIndex 循环执行\n\n- "
                 "支持延迟：每次迭代之间可设置等待时间\n- 支持Break中断循环\n- "
                 "适用于需要倒序计数的场景");
}

FText UK2Node_ForLoopWithDelayReverse::GetKeywords() const {
  return LOCTEXT("Keywords",
                 "for loop delay reverse 循环 延迟 等待 倒序 反向 递减 计数");
}

FText UK2Node_ForLoopWithDelayReverse::GetMenuCategory() const {
  return LOCTEXT("MenuCategory", "XTools|Blueprint Extensions|Loops");
}

FSlateIcon
UK2Node_ForLoopWithDelayReverse::GetIconAndTint(FLinearColor &OutColor) const {
  static FSlateIcon Icon("EditorStyle", "GraphEditor.Macro.Loop_16x");
  return Icon;
}

#pragma endregion

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintCompile

void UK2Node_ForLoopWithDelayReverse::ExpandNode(
    FKismetCompilerContext &CompilerContext, UEdGraph *SourceGraph) {
  // 【参考 K2Node_ForLoopWithDelay 实现模式，修改为倒序】
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

  // 2. 初始化循环计数器（从 LastIndex 开始）
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

  // 4. 创建循环条件（计数器 >= FirstIndex，倒序使用 >=）
  UK2Node_CallFunction *Condition =
      CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,
                                                                  SourceGraph);
  Condition->SetFromFunction(
      UKismetMathLibrary::StaticClass()->FindFunctionByName(
          GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, GreaterEqual_IntInt)));
  Condition->AllocateDefaultPins();
  CompilerContext.GetSchema()->TryCreateConnection(
      Condition->GetReturnValuePin(), Branch->GetConditionPin());
  CompilerContext.GetSchema()->TryCreateConnection(
      Condition->FindPinChecked(TEXT("A")), LoopCounterPin);

  // 5. 创建执行序列（循环体 -> 延迟路径）
  // 【修复】先执行循环体，再延迟，避免初次进入时延迟
  UK2Node_ExecutionSequence *Sequence =
      CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(
          this, SourceGraph);
  Sequence->AllocateDefaultPins();
  CompilerContext.GetSchema()->TryCreateConnection(Branch->GetThenPin(),
                                                   Sequence->GetExecPin());

  // 6. 创建延迟节点
  UK2Node_CallFunction *DelayNode =
      CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,
                                                                  SourceGraph);
  DelayNode->SetFromFunction(
      UKismetSystemLibrary::StaticClass()->FindFunctionByName(
          GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Delay)));
  DelayNode->AllocateDefaultPins();
  // 【修复】延迟节点连接到 Sequence 的第二个输出，先执行循环体再延迟
  CompilerContext.GetSchema()->TryCreateConnection(
      Sequence->GetThenPinGivenIndex(1), DelayNode->GetExecPin());

  // 7. 创建递减节点（倒序使用减法）
  UK2Node_CallFunction *Decrement =
      CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,
                                                                  SourceGraph);
  Decrement->SetFromFunction(
      UKismetMathLibrary::StaticClass()->FindFunctionByName(
          GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Subtract_IntInt)));
  Decrement->AllocateDefaultPins();
  CompilerContext.GetSchema()->TryCreateConnection(
      Decrement->FindPinChecked(TEXT("A")), LoopCounterPin);
  Decrement->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("1");

  // 8. 创建赋值节点（递减后的值）
  UK2Node_AssignmentStatement *LoopCounterAssign =
      CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(
          this, SourceGraph);
  LoopCounterAssign->AllocateDefaultPins();
  // 【修复】递减逻辑连接到 Delay->Then，确保顺序为：循环体 → 延迟 → 递减
  CompilerContext.GetSchema()->TryCreateConnection(
      LoopCounterAssign->GetExecPin(), DelayNode->GetThenPin());
  CompilerContext.GetSchema()->TryCreateConnection(
      LoopCounterAssign->GetVariablePin(), LoopCounterPin);
  CompilerContext.GetSchema()->TryCreateConnection(
      LoopCounterAssign->GetValuePin(), Decrement->GetReturnValuePin());
  CompilerContext.GetSchema()->TryCreateConnection(
      LoopCounterAssign->GetThenPin(), Branch->GetExecPin()); // 循环回到分支

  // 9. Break 功能：创建 FirstIndex-1 来跳出循环
  UK2Node_CallFunction *BreakValue =
      CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,
                                                                  SourceGraph);
  BreakValue->SetFromFunction(
      UKismetMathLibrary::StaticClass()->FindFunctionByName(
          GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Subtract_IntInt)));
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
  CompilerContext.MovePinLinksToIntermediate(
      *GetLastIndexPin(),
      *LoopCounterInit->GetValuePin()); // 倒序：从 LastIndex 开始
  CompilerContext.MovePinLinksToIntermediate(
      *GetFirstIndexPin(),
      *Condition->FindPinChecked(TEXT("B"))); // 倒序：到 FirstIndex 结束
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

void UK2Node_ForLoopWithDelayReverse::GetMenuActions(
    FBlueprintActionDatabaseRegistrar &ActionRegistrar) const {
  K2NodeHelpers::RegisterNode<UK2Node_ForLoopWithDelayReverse>(ActionRegistrar);
}

void UK2Node_ForLoopWithDelayReverse::PostReconstructNode() {
  Super::PostReconstructNode();
}

bool UK2Node_ForLoopWithDelayReverse::IsCompatibleWithGraph(
    const UEdGraph *TargetGraph) const {
  return K2NodeHelpers::IsEventGraphCompatible(TargetGraph) &&
         Super::IsCompatibleWithGraph(TargetGraph);
}

#pragma endregion

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region PinManagement

void UK2Node_ForLoopWithDelayReverse::AllocateDefaultPins() {
  using namespace ForLoopWithDelayReverseHelper;

  // 输入执行引脚
  CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec,
            UEdGraphSchema_K2::PN_Execute);

  // LastIndex 输入（倒序的起始值）- 先创建，显示在上方
  UEdGraphPin *LastPin =
      CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, LastPinName);
  LastPin->DefaultValue = TEXT("10");
  LastPin->PinToolTip =
      LOCTEXT("LastIndexTooltip", "起始索引（从此值开始递减）").ToString();

  // FirstIndex 输入（倒序的结束值）- 后创建，显示在下方
  UEdGraphPin *FirstPin =
      CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, FirstPinName);
  FirstPin->DefaultValue = TEXT("0");
  FirstPin->PinToolTip =
      LOCTEXT("FirstIndexTooltip", "结束索引（递减到此值后停止，包含此值）")
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
  IndexPin->PinToolTip =
      LOCTEXT("IndexTooltip", "当前循环索引（递减）").ToString();

  // Break 输入执行引脚（可选）
  CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, BreakPinName);

  // Completed 输出执行引脚
  UEdGraphPin *CompletedPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec,
                                        UEdGraphSchema_K2::PN_Then);
  CompletedPin->PinFriendlyName = LOCTEXT("CompletedPinName", "Completed");
  CompletedPin->PinToolTip =
      LOCTEXT("CompletedTooltip", "循环完成时执行").ToString();
}

UEdGraphPin *UK2Node_ForLoopWithDelayReverse::GetFirstIndexPin() const {
  return FindPinChecked(ForLoopWithDelayReverseHelper::FirstPinName,
                        EGPD_Input);
}

UEdGraphPin *UK2Node_ForLoopWithDelayReverse::GetLastIndexPin() const {
  return FindPinChecked(ForLoopWithDelayReverseHelper::LastPinName, EGPD_Input);
}

UEdGraphPin *UK2Node_ForLoopWithDelayReverse::GetDelayPin() const {
  return FindPinChecked(ForLoopWithDelayReverseHelper::DelayPinName,
                        EGPD_Input);
}

UEdGraphPin *UK2Node_ForLoopWithDelayReverse::GetLoopBodyPin() const {
  return FindPinChecked(ForLoopWithDelayReverseHelper::LoopBodyPinName,
                        EGPD_Output);
}

UEdGraphPin *UK2Node_ForLoopWithDelayReverse::GetBreakPin() const {
  return FindPin(ForLoopWithDelayReverseHelper::BreakPinName, EGPD_Input);
}

UEdGraphPin *UK2Node_ForLoopWithDelayReverse::GetCompletedPin() const {
  return FindPinChecked(UEdGraphSchema_K2::PN_Then, EGPD_Output);
}

UEdGraphPin *UK2Node_ForLoopWithDelayReverse::GetIndexPin() const {
  return FindPinChecked(ForLoopWithDelayReverseHelper::IndexPinName,
                        EGPD_Output);
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
