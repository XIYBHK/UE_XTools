#include "K2Nodes/K2Node_ForEachArrayReverse.h"
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
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

#define LOCTEXT_NAMESPACE "XTools_K2Node_ForEachArrayReverse"

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region Helper

namespace ForEachArrayReverseHelper {
const FName ArrayPinName = FName("Array");
const FName DelayPinName = FName("Delay");
const FName LoopBodyPinName = FName("Loop Body");
const FName ValuePinName = FName("Value");
const FName IndexPinName = FName("Index");
const FName BreakPinName = FName("Break");
} // namespace ForEachArrayReverseHelper

#pragma endregion

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeAppearance

FText UK2Node_ForEachArrayReverse::GetNodeTitle(
    ENodeTitleType::Type TitleType) const {
  return LOCTEXT("ForEachArrayReverseTitle", "倒序遍历数组");
}

FText UK2Node_ForEachArrayReverse::GetCompactNodeTitle() const {
  return LOCTEXT("ForEachArrayReverseCompactNodeTitle", "FOREACH\nDELAY REV");
}

FText UK2Node_ForEachArrayReverse::GetTooltipText() const {
  return LOCTEXT("ForEachArrayReverseToolTip",
                 "从后向前遍历数组中的每个元素\n\n- "
                 "支持延迟：每次迭代之间可设置等待时间\n- 支持Break中断循环\n- "
                 "适用于需要倒序处理数组的场景");
}

FText UK2Node_ForEachArrayReverse::GetKeywords() const {
  return LOCTEXT("Keywords", "foreach loop each reverse delay 遍历 数组 循环 "
                             "倒序 反向 延迟 for array");
}

FText UK2Node_ForEachArrayReverse::GetMenuCategory() const {
  return LOCTEXT("ForEachArrayReverseCategory",
                 "XTools|Blueprint Extensions|Loops");
}

FSlateIcon
UK2Node_ForEachArrayReverse::GetIconAndTint(FLinearColor &OutColor) const {
  static FSlateIcon Icon("EditorStyle", "GraphEditor.Macro.Loop_16x");
  return Icon;
}

TSharedPtr<SWidget> UK2Node_ForEachArrayReverse::CreateNodeImage() const {
  return SPinTypeSelector::ConstructPinTypeImage(GetArrayPin());
}

#pragma endregion

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintCompile

void UK2Node_ForEachArrayReverse::ExpandNode(
    FKismetCompilerContext &CompilerContext, UEdGraph *SourceGraph) {
  // 【参考 K2Node_SmartSort 实现模式】
  // 不调用 Super::ExpandNode()，因为基类会提前断开所有链接
  if (!K2NodeHelpers::BeginExpandNode(
          CompilerContext, this,
          {GetExecPin(), GetArrayPin(), GetDelayPin(), GetLoopBodyPin(),
           GetValuePin(), GetIndexPin(), GetBreakPin(), GetCompletedPin()},
          LOCTEXT("MissingPins", "@@ 节点引脚不完整"))) {
    return;
  }

  // 验证数组引脚连接
  UEdGraphPin *ArrayPin = GetArrayPin();
  if (!ArrayPin || ArrayPin->LinkedTo.Num() == 0) {
    CompilerContext.MessageLog.Warning(
        *LOCTEXT("ArrayNotConnected", "Array pin must be connected @@")
             .ToString(),
        this);
    BreakAllNodeLinks();
    return;
  }

  // 1. 创建循环计数器临时变量
  UK2Node_TemporaryVariable *LoopCounterNode =
      CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(
          this, SourceGraph);
  LoopCounterNode->VariableType.PinCategory = UEdGraphSchema_K2::PC_Int;
  LoopCounterNode->AllocateDefaultPins();
  UEdGraphPin *LoopCounterPin = LoopCounterNode->GetVariablePin();

  // 2. 创建零值临时变量（用于条件比较）
  UK2Node_TemporaryVariable *LoopCounterZeroNode =
      CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(
          this, SourceGraph);
  LoopCounterZeroNode->VariableType.PinCategory = UEdGraphSchema_K2::PC_Int;
  LoopCounterZeroNode->AllocateDefaultPins();
  UEdGraphPin *LoopCounterZeroPin = LoopCounterZeroNode->GetVariablePin();

  // 3. 获取数组长度
  UK2Node_CallFunction *Length =
      CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,
                                                                  SourceGraph);
  Length->SetFromFunction(
      UKismetArrayLibrary::StaticClass()->FindFunctionByName(
          GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Length)));
  Length->AllocateDefaultPins();
  UEdGraphPin *LengthTargetArrayPin =
      Length->FindPinChecked(TEXT("TargetArray"), EGPD_Input);
  LengthTargetArrayPin->PinType = GetArrayPin()->PinType;
  LengthTargetArrayPin->PinType.PinValueType =
      FEdGraphTerminalType(GetArrayPin()->PinType.PinValueType);
  CompilerContext.CopyPinLinksToIntermediate(*GetArrayPin(),
                                             *LengthTargetArrayPin);
  Length->PostReconstructNode();

  // 4. 设置循环计数器为数组长度
  UK2Node_AssignmentStatement *LoopCounterSet =
      CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(
          this, SourceGraph);
  LoopCounterSet->AllocateDefaultPins();
  K2NodeHelpers::TryConnect(CompilerContext, 
      LoopCounterSet->GetVariablePin(), LoopCounterPin);
  K2NodeHelpers::TryConnect(CompilerContext, 
      LoopCounterSet->GetValuePin(), Length->GetReturnValuePin());

  // 5. 计数器减1（初始化为 length-1）
  UK2Node_CallFunction *IncrementInit =
      CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,
                                                                  SourceGraph);
  IncrementInit->SetFromFunction(
      UKismetMathLibrary::StaticClass()->FindFunctionByName(
          GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Add_IntInt)));
  IncrementInit->AllocateDefaultPins();
  K2NodeHelpers::TryConnect(CompilerContext, 
      IncrementInit->FindPinChecked(TEXT("A")), LoopCounterPin);
  IncrementInit->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("-1");

  // 6. 设置初始化后的计数器值
  UK2Node_AssignmentStatement *LoopCounterSetInit =
      CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(
          this, SourceGraph);
  LoopCounterSetInit->AllocateDefaultPins();
  K2NodeHelpers::TryConnect(CompilerContext, 
      LoopCounterSetInit->GetVariablePin(), LoopCounterPin);
  K2NodeHelpers::TryConnect(CompilerContext, 
      LoopCounterSetInit->GetValuePin(), IncrementInit->GetReturnValuePin());
  K2NodeHelpers::TryConnect(CompilerContext, 
      LoopCounterSet->GetThenPin(), LoopCounterSetInit->GetExecPin());

  // 7. 创建分支节点
  UK2Node_IfThenElse *Branch =
      CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this,
                                                                SourceGraph);
  Branch->AllocateDefaultPins();
  K2NodeHelpers::TryConnect(CompilerContext, 
      LoopCounterSetInit->GetThenPin(), Branch->GetExecPin());

  // 8. 创建循环条件（计数器 >= 0）
  UK2Node_CallFunction *Condition =
      CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,
                                                                  SourceGraph);
  Condition->SetFromFunction(
      UKismetMathLibrary::StaticClass()->FindFunctionByName(
          GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, GreaterEqual_IntInt)));
  Condition->AllocateDefaultPins();
  K2NodeHelpers::TryConnect(CompilerContext, 
      Condition->GetReturnValuePin(), Branch->GetConditionPin());
  K2NodeHelpers::TryConnect(CompilerContext, 
      Condition->FindPinChecked(TEXT("A")), LoopCounterPin);
  K2NodeHelpers::TryConnect(CompilerContext, 
      Condition->FindPinChecked(TEXT("B")), LoopCounterZeroPin);

  // 9. Break 功能：设置计数器为-1以跳出循环
  UK2Node_AssignmentStatement *LoopCounterBreak =
      CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(
          this, SourceGraph);
  LoopCounterBreak->AllocateDefaultPins();
  LoopCounterBreak->GetValuePin()->DefaultValue = TEXT("-1");
  K2NodeHelpers::TryConnect(CompilerContext, 
      LoopCounterPin, LoopCounterBreak->GetVariablePin());
  // 【修复】连接 Break 的 Then 引脚到 Branch 的 Else，使 Break 后执行流继续到 Completed
  K2NodeHelpers::TryConnect(CompilerContext, 
      LoopCounterBreak->GetThenPin(), Branch->GetElsePin());

  // 10. 创建执行序列（循环体 -> 延迟路径）
  // 【修复】先执行循环体，再延迟，避免初次进入时延迟
  UK2Node_ExecutionSequence *Sequence =
      CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(
          this, SourceGraph);
  Sequence->AllocateDefaultPins();
  K2NodeHelpers::TryConnect(CompilerContext, Branch->GetThenPin(),
                                                   Sequence->GetExecPin());

  // 11. 创建延迟节点
  UK2Node_CallFunction *DelayNode =
      CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,
                                                                  SourceGraph);
  DelayNode->SetFromFunction(
      UKismetSystemLibrary::StaticClass()->FindFunctionByName(
          GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Delay)));
  DelayNode->AllocateDefaultPins();
  // Delay<=0 时等同于无延迟：直接进入递减分支；仅 Delay>0 走 Delay 节点
  UK2Node_CallFunction *DelayLessEqualZero =
      CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,
                                                                  SourceGraph);
  DelayLessEqualZero->SetFromFunction(
      UKismetMathLibrary::StaticClass()->FindFunctionByName(
          GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, LessEqual_FloatFloat)));
  DelayLessEqualZero->AllocateDefaultPins();

  UK2Node_IfThenElse *DelayBranch =
      CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this,
                                                                SourceGraph);
  DelayBranch->AllocateDefaultPins();
  K2NodeHelpers::TryConnect(CompilerContext, 
      Sequence->GetThenPinGivenIndex(1), DelayBranch->GetExecPin());
  K2NodeHelpers::TryConnect(CompilerContext, 
      DelayLessEqualZero->GetReturnValuePin(), DelayBranch->GetConditionPin());
  K2NodeHelpers::TryConnect(CompilerContext, 
      DelayLessEqualZero->FindPinChecked(TEXT("A")), GetDelayPin());
  DelayLessEqualZero->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("0.0");
  K2NodeHelpers::TryConnect(CompilerContext, DelayBranch->GetElsePin(),
                                                   DelayNode->GetExecPin());

  // 12. 创建递减节点
  UK2Node_CallFunction *Increment =
      CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,
                                                                  SourceGraph);
  Increment->SetFromFunction(
      UKismetMathLibrary::StaticClass()->FindFunctionByName(
          GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Add_IntInt)));
  Increment->AllocateDefaultPins();
  K2NodeHelpers::TryConnect(CompilerContext, 
      Increment->FindPinChecked(TEXT("A")), LoopCounterPin);
  Increment->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("-1");

  // 12. 创建赋值节点（递减后的值）
  UK2Node_AssignmentStatement *LoopCounterAssignNoDelay =
      CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(
          this, SourceGraph);
  LoopCounterAssignNoDelay->AllocateDefaultPins();
  // Delay<=0：直接递减
  K2NodeHelpers::TryConnect(CompilerContext, 
      LoopCounterAssignNoDelay->GetExecPin(), DelayBranch->GetThenPin());
  K2NodeHelpers::TryConnect(CompilerContext, 
      LoopCounterAssignNoDelay->GetVariablePin(), LoopCounterPin);
  K2NodeHelpers::TryConnect(CompilerContext, 
      LoopCounterAssignNoDelay->GetValuePin(), Increment->GetReturnValuePin());
  K2NodeHelpers::TryConnect(CompilerContext, 
      LoopCounterAssignNoDelay->GetThenPin(), Branch->GetExecPin()); // 循环回到分支

  UK2Node_AssignmentStatement *LoopCounterAssignWithDelay =
      CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(
          this, SourceGraph);
  LoopCounterAssignWithDelay->AllocateDefaultPins();
  // Delay>0：延迟后递减
  K2NodeHelpers::TryConnect(CompilerContext, 
      LoopCounterAssignWithDelay->GetExecPin(), DelayNode->GetThenPin());
  K2NodeHelpers::TryConnect(CompilerContext, 
      LoopCounterAssignWithDelay->GetVariablePin(), LoopCounterPin);
  K2NodeHelpers::TryConnect(CompilerContext, 
      LoopCounterAssignWithDelay->GetValuePin(), Increment->GetReturnValuePin());
  K2NodeHelpers::TryConnect(CompilerContext, 
      LoopCounterAssignWithDelay->GetThenPin(), Branch->GetExecPin()); // 循环回到分支

  // 13. 获取数组元素
  UK2Node_CallFunction *GetValue =
      CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,
                                                                  SourceGraph);
  GetValue->SetFromFunction(
      UKismetArrayLibrary::StaticClass()->FindFunctionByName(
          GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Get)));
  GetValue->AllocateDefaultPins();
  UEdGraphPin *GetValueTargetArrayPin =
      GetValue->FindPinChecked(TEXT("TargetArray"), EGPD_Input);
  GetValueTargetArrayPin->PinType = GetArrayPin()->PinType;
  GetValueTargetArrayPin->PinType.PinValueType =
      FEdGraphTerminalType(GetArrayPin()->PinType.PinValueType);
  K2NodeHelpers::TryConnect(CompilerContext, 
      GetValue->FindPinChecked(TEXT("Index")), LoopCounterPin);
  CompilerContext.CopyPinLinksToIntermediate(*GetArrayPin(),
                                             *GetValueTargetArrayPin);
  UEdGraphPin *ValuePin =
      K2NodeHelpers::ReconstructAndFindPin(GetValue, TEXT("Item"));
  check(ValuePin);
  ValuePin->PinType = GetValuePin()->PinType;

  // 14. 最后统一移动所有外部连接（参考智能排序模式）
  CompilerContext.MovePinLinksToIntermediate(*GetExecPin(),
                                             *LoopCounterSet->GetExecPin());
  CompilerContext.MovePinLinksToIntermediate(
      *GetDelayPin(), *DelayNode->FindPinChecked(TEXT("Duration")));
  CompilerContext.MovePinLinksToIntermediate(
      *GetLoopBodyPin(), *Sequence->GetThenPinGivenIndex(0));
  CompilerContext.MovePinLinksToIntermediate(*GetCompletedPin(),
                                             *Branch->GetElsePin());
  CompilerContext.MovePinLinksToIntermediate(*GetBreakPin(),
                                             *LoopCounterBreak->GetExecPin());
  CompilerContext.MovePinLinksToIntermediate(*GetValuePin(), *ValuePin);
  CompilerContext.MovePinLinksToIntermediate(*GetIndexPin(), *LoopCounterPin);

  // 15. 断开原节点所有链接
  K2NodeHelpers::EndExpandNode(this);
}

#pragma endregion

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintSystem

void UK2Node_ForEachArrayReverse::GetMenuActions(
    FBlueprintActionDatabaseRegistrar &ActionRegistrar) const {
  K2NodeHelpers::RegisterNode<UK2Node_ForEachArrayReverse>(ActionRegistrar);
}

void UK2Node_ForEachArrayReverse::PostReconstructNode() {
  Super::PostReconstructNode();

  // 【修复】仅在有连接时才传播类型，避免重载时丢失已序列化的类型信息
  // 参考 UE 源码 K2Node_GetArrayItem::PostReconstructNode 实现
  UEdGraphPin *ArrayPin = GetArrayPin();
  UEdGraphPin *ValuePin = GetValuePin();

  if (ArrayPin->LinkedTo.Num() > 0 || ValuePin->LinkedTo.Num() > 0) {
    PropagatePinType();
  } else {
    // 【额外修复】无连接时，如果一个引脚有确定类型，另一个是Wildcard，则同步类型
    bool bArrayIsWildcard =
        (ArrayPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);
    bool bValueIsWildcard =
        (ValuePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);

    if (!bArrayIsWildcard && bValueIsWildcard) {
      ValuePin->PinType = ArrayPin->PinType;
      ValuePin->PinType.ContainerType = EPinContainerType::None;
      GetGraph()->NotifyGraphChanged();
    } else if (bArrayIsWildcard && !bValueIsWildcard) {
      ArrayPin->PinType = ValuePin->PinType;
      ArrayPin->PinType.ContainerType = EPinContainerType::Array;
      GetGraph()->NotifyGraphChanged();
    }
  }
}

void UK2Node_ForEachArrayReverse::NotifyPinConnectionListChanged(
    UEdGraphPin *Pin) {
  Super::NotifyPinConnectionListChanged(Pin);

  if (Pin == GetArrayPin() || Pin == GetValuePin()) {
    PropagatePinType();
  }
}

bool UK2Node_ForEachArrayReverse::IsCompatibleWithGraph(
    const UEdGraph *TargetGraph) const {
  return K2NodeHelpers::IsEventGraphCompatible(TargetGraph) &&
         Super::IsCompatibleWithGraph(TargetGraph);
}

#pragma endregion

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region PinManagement

void UK2Node_ForEachArrayReverse::AllocateDefaultPins() {
  // 【修复】移除 Super::AllocateDefaultPins() 调用，避免引脚重复创建
  // 参考 ForEachLoopWithDelay.cpp 的正确实现

  // Execute
  CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec,
            UEdGraphSchema_K2::PN_Execute);

  // Array
  UEdGraphNode::FCreatePinParams PinParams;
  PinParams.ContainerType = EPinContainerType::Array;
  PinParams.ValueTerminalType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
  PinParams.ValueTerminalType.TerminalSubCategory = NAME_None;
  PinParams.ValueTerminalType.TerminalSubCategoryObject = nullptr;
  CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard,
            ForEachArrayReverseHelper::ArrayPinName, PinParams);

  // Delay
  UEdGraphPin *DelayPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real,
                                    UEdGraphSchema_K2::PC_Float,
                                    ForEachArrayReverseHelper::DelayPinName);
  DelayPin->DefaultValue = TEXT("0.0");
  DelayPin->PinToolTip =
      LOCTEXT("DelayTooltip", "每次循环之间的延迟时间(秒)，0表示无延迟")
          .ToString();

  // Break
  CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec,
            ForEachArrayReverseHelper::BreakPinName)
      ->PinFriendlyName =
      FText::FromName(ForEachArrayReverseHelper::BreakPinName);

  // Loop body
  CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec,
            ForEachArrayReverseHelper::LoopBodyPinName);

  // Value
  CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard,
            ForEachArrayReverseHelper::ValuePinName);

  // Index
  CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Int,
            ForEachArrayReverseHelper::IndexPinName);

  // Completed
  CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then)
      ->PinFriendlyName = FText::FromName(UEdGraphSchema_K2::PN_Completed);
}

bool UK2Node_ForEachArrayReverse::IsConnectionDisallowed(
    const UEdGraphPin *MyPin, const UEdGraphPin *OtherPin,
    FString &OutReason) const {
  // 【修复】添加数组连接验证，参考 ForEachLoopWithDelay.cpp 实现
  if (MyPin == GetArrayPin() &&
      MyPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard) {
    if (OtherPin->PinType.ContainerType != EPinContainerType::Array) {
      OutReason =
          LOCTEXT("MustConnectArray", "Must connect to an array").ToString();
      return true;
    }
  }

  return Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason);
}

UEdGraphPin *UK2Node_ForEachArrayReverse::GetLoopBodyPin() const {
  return FindPinChecked(ForEachArrayReverseHelper::LoopBodyPinName,
                        EGPD_Output);
}

UEdGraphPin *UK2Node_ForEachArrayReverse::GetArrayPin() const {
  return FindPinChecked(ForEachArrayReverseHelper::ArrayPinName, EGPD_Input);
}

UEdGraphPin *UK2Node_ForEachArrayReverse::GetDelayPin() const {
  return FindPinChecked(ForEachArrayReverseHelper::DelayPinName, EGPD_Input);
}

UEdGraphPin *UK2Node_ForEachArrayReverse::GetValuePin() const {
  return FindPinChecked(ForEachArrayReverseHelper::ValuePinName, EGPD_Output);
}

UEdGraphPin *UK2Node_ForEachArrayReverse::GetCompletedPin() const {
  return FindPinChecked(UEdGraphSchema_K2::PN_Then, EGPD_Output);
}

UEdGraphPin *UK2Node_ForEachArrayReverse::GetBreakPin() const {
  return FindPinChecked(ForEachArrayReverseHelper::BreakPinName, EGPD_Input);
}

UEdGraphPin *UK2Node_ForEachArrayReverse::GetIndexPin() const {
  return FindPinChecked(ForEachArrayReverseHelper::IndexPinName, EGPD_Output);
}

void UK2Node_ForEachArrayReverse::PropagatePinType() const {
  bool bNotifyGraphChanged = false;
  UEdGraphPin *ArrayPin = GetArrayPin();
  UEdGraphPin *ValuePin = GetValuePin();

  // 【修复】无连接的情况：仅在引脚当前为Wildcard时才重置
  // 这样可以保留加载时已序列化的类型信息
  if (ArrayPin->LinkedTo.Num() == 0 && ValuePin->LinkedTo.Num() == 0) {
    // 只有在引脚类型确实是Wildcard时才重置
    bool bArrayIsWildcard =
        (ArrayPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);
    bool bValueIsWildcard =
        (ValuePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);

    if (!bArrayIsWildcard || !bValueIsWildcard) {
      // 引脚已有确定的类型（从序列化数据恢复），保留它
      // 这样重启编辑器后不会丢失类型信息
      return;
    }

    // 重置为Wildcard
    ArrayPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
    ArrayPin->PinType.PinSubCategory = NAME_None;
    ArrayPin->PinType.PinSubCategoryObject = nullptr;
    ArrayPin->PinType.PinValueType.TerminalCategory =
        UEdGraphSchema_K2::PC_Wildcard;
    ArrayPin->PinType.PinValueType.TerminalSubCategory = NAME_None;
    ArrayPin->PinType.PinValueType.TerminalSubCategoryObject = nullptr;
    ArrayPin->BreakAllPinLinks(true);

    ValuePin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
    ValuePin->PinType.PinSubCategory = NAME_None;
    ValuePin->PinType.PinSubCategoryObject = nullptr;
    ValuePin->BreakAllPinLinks(true);

    bNotifyGraphChanged = true;
  }

  // 只有 Array 引脚有连接
  else if (ArrayPin->LinkedTo.Num() > 0 && ValuePin->LinkedTo.Num() == 0) {
    UEdGraphPin *LinkedPin = ArrayPin->LinkedTo[0];
    if (LinkedPin->PinType.ContainerType == EPinContainerType::Array &&
        LinkedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard) {
      ArrayPin->PinType.PinCategory = LinkedPin->PinType.PinCategory;
      ArrayPin->PinType.PinSubCategory = LinkedPin->PinType.PinSubCategory;
      ArrayPin->PinType.PinSubCategoryObject =
          LinkedPin->PinType.PinSubCategoryObject;
      ArrayPin->PinType.ContainerType = EPinContainerType::Array;

      ValuePin->PinType.PinCategory = LinkedPin->PinType.PinCategory;
      ValuePin->PinType.PinSubCategory = LinkedPin->PinType.PinSubCategory;
      ValuePin->PinType.PinSubCategoryObject =
          LinkedPin->PinType.PinSubCategoryObject;

      bNotifyGraphChanged = true;
    }
  }

  // 只有 Value 引脚有连接
  else if (ArrayPin->LinkedTo.Num() == 0 && ValuePin->LinkedTo.Num() > 0) {
    UEdGraphPin *LinkedPin = ValuePin->LinkedTo[0];
    if (LinkedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard) {
      ArrayPin->PinType.PinCategory = LinkedPin->PinType.PinCategory;
      ArrayPin->PinType.PinSubCategory = LinkedPin->PinType.PinSubCategory;
      ArrayPin->PinType.PinSubCategoryObject =
          LinkedPin->PinType.PinSubCategoryObject;
      ArrayPin->PinType.ContainerType = EPinContainerType::Array;

      ValuePin->PinType.PinCategory = LinkedPin->PinType.PinCategory;
      ValuePin->PinType.PinSubCategory = LinkedPin->PinType.PinSubCategory;
      ValuePin->PinType.PinSubCategoryObject =
          LinkedPin->PinType.PinSubCategoryObject;

      bNotifyGraphChanged = true;
    }
  }

  // 【修复】两个引脚都有连接：尝试从连接推断类型
  else if (ArrayPin->LinkedTo.Num() > 0 && ValuePin->LinkedTo.Num() > 0) {
    UEdGraphPin *ArrayLinkedPin = ArrayPin->LinkedTo[0];

    // 优先从Array连接推断
    if (ArrayLinkedPin->PinType.ContainerType == EPinContainerType::Array &&
        ArrayLinkedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard) {
      ArrayPin->PinType.PinCategory = ArrayLinkedPin->PinType.PinCategory;
      ArrayPin->PinType.PinSubCategory = ArrayLinkedPin->PinType.PinSubCategory;
      ArrayPin->PinType.PinSubCategoryObject =
          ArrayLinkedPin->PinType.PinSubCategoryObject;
      ArrayPin->PinType.ContainerType = EPinContainerType::Array;

      ValuePin->PinType.PinCategory = ArrayLinkedPin->PinType.PinCategory;
      ValuePin->PinType.PinSubCategory = ArrayLinkedPin->PinType.PinSubCategory;
      ValuePin->PinType.PinSubCategoryObject =
          ArrayLinkedPin->PinType.PinSubCategoryObject;

      bNotifyGraphChanged = true;
    }
    // Array连接是Wildcard，尝试从Value连接推断
    else {
      UEdGraphPin *ValueLinkedPin = ValuePin->LinkedTo[0];
      if (ValueLinkedPin->PinType.PinCategory !=
          UEdGraphSchema_K2::PC_Wildcard) {
        ArrayPin->PinType.PinCategory = ValueLinkedPin->PinType.PinCategory;
        ArrayPin->PinType.PinSubCategory =
            ValueLinkedPin->PinType.PinSubCategory;
        ArrayPin->PinType.PinSubCategoryObject =
            ValueLinkedPin->PinType.PinSubCategoryObject;
        ArrayPin->PinType.ContainerType = EPinContainerType::Array;

        ValuePin->PinType.PinCategory = ValueLinkedPin->PinType.PinCategory;
        ValuePin->PinType.PinSubCategory =
            ValueLinkedPin->PinType.PinSubCategory;
        ValuePin->PinType.PinSubCategoryObject =
            ValueLinkedPin->PinType.PinSubCategoryObject;

        bNotifyGraphChanged = true;
      }
    }
  }

  if (bNotifyGraphChanged) {
    GetGraph()->NotifyGraphChanged();
  }
}

#pragma endregion

// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#undef LOCTEXT_NAMESPACE

