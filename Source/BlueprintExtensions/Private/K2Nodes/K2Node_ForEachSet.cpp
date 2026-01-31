#include "K2Nodes/K2Node_ForEachSet.h"
#include "K2Nodes/K2NodeHelpers.h"

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
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/BlueprintSetLibrary.h"


#define LOCTEXT_NAMESPACE "XTools_K2Node_ForEachSet"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region Helper

namespace ForEachSetHelper
{
	const FName SetPinName = FName("Set");
	const FName LoopBodyPinName = FName("Loop Body");
	const FName ValuePinName = FName("Value");
	const FName IndexPinName = FName("Index");
    const FName BreakPinName = FName("Break");
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeAppearance

FText UK2Node_ForEachSet::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("ForEachSetTitle", "ForEachSet");
}

FText UK2Node_ForEachSet::GetCompactNodeTitle() const
{
	return LOCTEXT("ForEachSetCompactNodeTitle", "FOREACH");
}

FText UK2Node_ForEachSet::GetTooltipText() const
{
	return LOCTEXT("ForEachSetTooltipText", "遍历Set中的每个元素");
}

FText UK2Node_ForEachSet::GetKeywords() const
{
	return LOCTEXT("Keywords", "foreach loop each 遍历 循环 set 集合 for");
}

FText UK2Node_ForEachSet::GetMenuCategory() const
{
	return LOCTEXT("ForEachSetCategory", "XTools|Blueprint Extensions|Loops");
}

FSlateIcon UK2Node_ForEachSet::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Macro.Loop_16x");
	return Icon;
}

TSharedPtr<SWidget> UK2Node_ForEachSet::CreateNodeImage() const
{
	return SPinTypeSelector::ConstructPinTypeImage(GetSetPin());
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintCompile

void UK2Node_ForEachSet::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    // 【参考 K2Node_SmartSort 实现模式】
    // 不调用 Super::ExpandNode()，因为基类会提前断开所有链接

    // 验证 Set 引脚连接
    UEdGraphPin* SetPin = GetSetPin();
    if (!SetPin || SetPin->LinkedTo.Num() == 0)
    {
        CompilerContext.MessageLog.Warning(*LOCTEXT("SetNotConnected", "Set pin must be connected @@").ToString(), this);
        BreakAllNodeLinks();
        return;
    }

    // 1. Set 转 Array
    UK2Node_CallFunction* ToArrayFun = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    ToArrayFun->SetFromFunction(UBlueprintSetLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UBlueprintSetLibrary, Set_ToArray)));
    ToArrayFun->AllocateDefaultPins();
    UEdGraphPin* ToArrayFunPin = ToArrayFun->FindPinChecked(TEXT("A"), EGPD_Input);
    ToArrayFunPin->PinType = GetSetPin()->PinType;
    ToArrayFunPin->PinType.PinValueType = FEdGraphTerminalType(GetSetPin()->PinType.PinValueType);
    CompilerContext.CopyPinLinksToIntermediate(*GetSetPin(), *ToArrayFunPin);
    UEdGraphPin* ToArrayFunValuePin = ToArrayFun->FindPinChecked(TEXT("Result"));
    ToArrayFun->PostReconstructNode();

    // 2. 创建循环计数器临时变量
    UK2Node_TemporaryVariable* LoopCounterNode = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(this, SourceGraph);
    LoopCounterNode->VariableType.PinCategory = UEdGraphSchema_K2::PC_Int;
    LoopCounterNode->AllocateDefaultPins();
    UEdGraphPin* LoopCounterPin = LoopCounterNode->GetVariablePin();

    // 3. 初始化循环计数器为0
    UK2Node_AssignmentStatement* LoopCounterInitialise = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
    LoopCounterInitialise->AllocateDefaultPins();
    LoopCounterInitialise->GetValuePin()->DefaultValue = TEXT("0");
    CompilerContext.GetSchema()->TryCreateConnection(LoopCounterPin, LoopCounterInitialise->GetVariablePin());
    CompilerContext.GetSchema()->TryCreateConnection(ToArrayFun->GetThenPin(), LoopCounterInitialise->GetExecPin());

    // 4. 创建分支节点
    UK2Node_IfThenElse* Branch = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
    Branch->AllocateDefaultPins();
    CompilerContext.GetSchema()->TryCreateConnection(LoopCounterInitialise->GetThenPin(), Branch->GetExecPin());

    // 5. 创建循环条件（计数器 < Set长度）
    UK2Node_CallFunction* Condition = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    Condition->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_IntInt)));
    Condition->AllocateDefaultPins();
    CompilerContext.GetSchema()->TryCreateConnection(Condition->GetReturnValuePin(), Branch->GetConditionPin());
    CompilerContext.GetSchema()->TryCreateConnection(Condition->FindPinChecked(TEXT("A")), LoopCounterPin);

    // 6. 获取Set长度
    UK2Node_CallFunction* Length = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    Length->SetFromFunction(UBlueprintSetLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UBlueprintSetLibrary, Set_Length)));
    Length->AllocateDefaultPins();
    UEdGraphPin* LengthTargetArrayPin = Length->FindPinChecked(TEXT("TargetSet"), EGPD_Input);
    LengthTargetArrayPin->PinType = GetSetPin()->PinType;
    LengthTargetArrayPin->PinType.PinValueType = FEdGraphTerminalType(GetSetPin()->PinType.PinValueType);
    CompilerContext.GetSchema()->TryCreateConnection(Condition->FindPinChecked(TEXT("B")), Length->GetReturnValuePin());
    CompilerContext.CopyPinLinksToIntermediate(*GetSetPin(), *LengthTargetArrayPin);
    Length->PostReconstructNode();

    // 7. Break 功能：设置计数器为Set长度以跳出循环
    UK2Node_CallFunction* BreakLength = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    BreakLength->SetFromFunction(UBlueprintSetLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UBlueprintSetLibrary, Set_Length)));
    BreakLength->AllocateDefaultPins();
    UEdGraphPin* BreakLengthTargetArrayPin = BreakLength->FindPinChecked(TEXT("TargetSet"), EGPD_Input);
    BreakLengthTargetArrayPin->PinType = GetSetPin()->PinType;
    BreakLengthTargetArrayPin->PinType.PinValueType = FEdGraphTerminalType(GetSetPin()->PinType.PinValueType);
    CompilerContext.CopyPinLinksToIntermediate(*GetSetPin(), *BreakLengthTargetArrayPin);
    BreakLength->PostReconstructNode();

    UK2Node_AssignmentStatement* LoopCounterBreak = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
    LoopCounterBreak->AllocateDefaultPins();
    CompilerContext.GetSchema()->TryCreateConnection(LoopCounterBreak->GetVariablePin(), LoopCounterPin);
    CompilerContext.GetSchema()->TryCreateConnection(LoopCounterBreak->GetValuePin(), BreakLength->GetReturnValuePin());
    // 【修复】连接 Break 的 Then 引脚到 Branch 的 Else，使 Break 后执行流继续到 Completed
    CompilerContext.GetSchema()->TryCreateConnection(LoopCounterBreak->GetThenPin(), Branch->GetElsePin());

    // 8. 创建执行序列（循环体 -> 递增）
    UK2Node_ExecutionSequence* Sequence = CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(this, SourceGraph);
    Sequence->AllocateDefaultPins();
    CompilerContext.GetSchema()->TryCreateConnection(Sequence->GetExecPin(), Branch->GetThenPin());

    // 9. 创建递增节点
    UK2Node_CallFunction* Increment = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    Increment->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Add_IntInt)));
    Increment->AllocateDefaultPins();
    CompilerContext.GetSchema()->TryCreateConnection(Increment->FindPinChecked(TEXT("A")), LoopCounterPin);
    Increment->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("1");

    // 10. 创建赋值节点（递增后的值）
    UK2Node_AssignmentStatement* LoopCounterAssign = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
    LoopCounterAssign->AllocateDefaultPins();
    CompilerContext.GetSchema()->TryCreateConnection(LoopCounterAssign->GetExecPin(), Sequence->GetThenPinGivenIndex(1));
    CompilerContext.GetSchema()->TryCreateConnection(LoopCounterAssign->GetVariablePin(), LoopCounterPin);
    CompilerContext.GetSchema()->TryCreateConnection(LoopCounterAssign->GetValuePin(), Increment->GetReturnValuePin());
    CompilerContext.GetSchema()->TryCreateConnection(LoopCounterAssign->GetThenPin(), Branch->GetExecPin());  // 循环回到分支

    // 11. 获取数组元素
    UK2Node_CallFunction* GetValue = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    GetValue->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Get)));
    GetValue->AllocateDefaultPins();
    UEdGraphPin* GetValueTargetArrayPin = GetValue->FindPinChecked(TEXT("TargetArray"), EGPD_Input);
    GetValueTargetArrayPin->PinType = ToArrayFunValuePin->PinType;
    GetValueTargetArrayPin->PinType.PinValueType = FEdGraphTerminalType(ToArrayFunValuePin->PinType.PinValueType);
    CompilerContext.GetSchema()->TryCreateConnection(GetValue->FindPinChecked(TEXT("Index")), LoopCounterPin);
    CompilerContext.GetSchema()->TryCreateConnection(GetValueTargetArrayPin, ToArrayFunValuePin);
    UEdGraphPin* ValuePin = GetValue->FindPinChecked(TEXT("Item"));
    ValuePin->PinType = GetValuePin()->PinType;
    GetValue->PostReconstructNode();

    // 12. 最后统一移动所有外部连接（参考智能排序模式）
    CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *ToArrayFun->GetExecPin());
    CompilerContext.MovePinLinksToIntermediate(*GetLoopBodyPin(), *Sequence->GetThenPinGivenIndex(0));
    CompilerContext.MovePinLinksToIntermediate(*GetCompletedPin(), *Branch->GetElsePin());
    CompilerContext.MovePinLinksToIntermediate(*GetBreakPin(), *LoopCounterBreak->GetExecPin());
    CompilerContext.MovePinLinksToIntermediate(*GetValuePin(), *ValuePin);
    CompilerContext.MovePinLinksToIntermediate(*GetIndexPin(), *LoopCounterPin);

    // 13. 断开原节点所有链接
    BreakAllNodeLinks();
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintSystem

void UK2Node_ForEachSet::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	K2NodeHelpers::RegisterNode<UK2Node_ForEachSet>(ActionRegistrar);
}

void UK2Node_ForEachSet::PostReconstructNode()
{
	Super::PostReconstructNode();

	// 【修复】仅在有连接时才传播类型，避免重载时丢失已序列化的类型信息
	// 参考 UE 源码 K2Node_GetArrayItem::PostReconstructNode 实现
	UEdGraphPin* SetPin = GetSetPin();
	UEdGraphPin* ValuePin = GetValuePin();
	
	if (SetPin->LinkedTo.Num() > 0 || ValuePin->LinkedTo.Num() > 0)
	{
		PropagatePinType();
	}
	else
	{
		// 【额外修复】无连接时，如果一个引脚有确定类型，另一个是Wildcard，则同步类型
		bool bSetIsWildcard = (SetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);
		bool bValueIsWildcard = (ValuePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);
		
		if (!bSetIsWildcard && bValueIsWildcard)
		{
			ValuePin->PinType = SetPin->PinType;
			ValuePin->PinType.ContainerType = EPinContainerType::None;
			GetGraph()->NotifyGraphChanged();
		}
		else if (bSetIsWildcard && !bValueIsWildcard)
		{
			SetPin->PinType = ValuePin->PinType;
			SetPin->PinType.ContainerType = EPinContainerType::Set;
			GetGraph()->NotifyGraphChanged();
		}
	}
}

void UK2Node_ForEachSet::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

    if (Pin == GetSetPin() || Pin == GetValuePin())
	{
		PropagatePinType();
	}
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region PinManagement

void UK2Node_ForEachSet::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	// Execute
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);

	// Set 
	UEdGraphNode::FCreatePinParams PinParams;
	PinParams.ContainerType = EPinContainerType::Set;
	PinParams.ValueTerminalType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
	PinParams.ValueTerminalType.TerminalSubCategory = NAME_None;
	PinParams.ValueTerminalType.TerminalSubCategoryObject = nullptr;
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, ForEachSetHelper::SetPinName, PinParams);

    // Break
    CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, ForEachSetHelper::BreakPinName)->PinFriendlyName = FText::FromName(ForEachSetHelper::BreakPinName);

	// Loop body
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, ForEachSetHelper::LoopBodyPinName);

	// Value
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, ForEachSetHelper::ValuePinName);

	// Index
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Int, ForEachSetHelper::IndexPinName);

	// Completed
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then)->PinFriendlyName = FText::FromName(UEdGraphSchema_K2::PN_Completed);
}

bool UK2Node_ForEachSet::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	return Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason);
}

UEdGraphPin* UK2Node_ForEachSet::GetLoopBodyPin() const
{
    return FindPinChecked(ForEachSetHelper::LoopBodyPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForEachSet::GetSetPin() const
{
    return FindPinChecked(ForEachSetHelper::SetPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForEachSet::GetValuePin() const
{
    return FindPinChecked(ForEachSetHelper::ValuePinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForEachSet::GetCompletedPin() const
{
    return FindPinChecked(UEdGraphSchema_K2::PN_Then, EGPD_Output);
}

UEdGraphPin* UK2Node_ForEachSet::GetBreakPin() const
{
    return FindPinChecked(ForEachSetHelper::BreakPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForEachSet::GetIndexPin() const
{
    return FindPinChecked(ForEachSetHelper::IndexPinName, EGPD_Output);
}

void UK2Node_ForEachSet::PropagatePinType() const
{
    bool bNotifyGraphChanged = false;
    UEdGraphPin* SetPin = GetSetPin();
    UEdGraphPin* ValuePin = GetValuePin();
    
    // 【修复】无连接的情况：仅在引脚当前为Wildcard时才重置
    // 这样可以保留加载时已序列化的类型信息
    if (SetPin->LinkedTo.Num() == 0 && ValuePin->LinkedTo.Num() == 0)
    {
        // 只有在引脚类型确实是Wildcard时才重置
        bool bSetIsWildcard = (SetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);
        bool bValueIsWildcard = (ValuePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);
        
        if (!bSetIsWildcard || !bValueIsWildcard)
        {
            // 引脚已有确定的类型（从序列化数据恢复），保留它
            return;
        }

        // 重置为Wildcard
        SetPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
        SetPin->PinType.PinSubCategory = NAME_None;
        SetPin->PinType.PinSubCategoryObject = nullptr;
        SetPin->PinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
        SetPin->PinType.PinValueType.TerminalSubCategory = NAME_None;
        SetPin->PinType.PinValueType.TerminalSubCategoryObject = nullptr;
        SetPin->BreakAllPinLinks(true);

        ValuePin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
        ValuePin->PinType.PinSubCategory = NAME_None;
        ValuePin->PinType.PinSubCategoryObject = nullptr;
        ValuePin->BreakAllPinLinks(true);

        bNotifyGraphChanged = true;
    }
    
    // 只有 Set 引脚有连接
    else if (SetPin->LinkedTo.Num() > 0 && ValuePin->LinkedTo.Num() == 0)
    {
    	UEdGraphPin* LinkedPin = SetPin->LinkedTo[0];
        if (LinkedPin->PinType.ContainerType == EPinContainerType::Set &&
        	LinkedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
        {
            SetPin->PinType.PinCategory = LinkedPin->PinType.PinCategory;
            SetPin->PinType.PinSubCategory = LinkedPin->PinType.PinSubCategory;
            SetPin->PinType.PinSubCategoryObject = LinkedPin->PinType.PinSubCategoryObject;
            SetPin->PinType.ContainerType = EPinContainerType::Set;
            
            ValuePin->PinType.PinCategory = LinkedPin->PinType.PinCategory;
            ValuePin->PinType.PinSubCategory = LinkedPin->PinType.PinSubCategory;
            ValuePin->PinType.PinSubCategoryObject = LinkedPin->PinType.PinSubCategoryObject;
            
            bNotifyGraphChanged = true;
        }
    }

    // 只有 Value 引脚有连接
    else if (SetPin->LinkedTo.Num() == 0 && ValuePin->LinkedTo.Num() > 0)
    {
    	UEdGraphPin* LinkedPin = ValuePin->LinkedTo[0];
    	if (LinkedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
    	{
    		SetPin->PinType.PinCategory = LinkedPin->PinType.PinCategory;
    		SetPin->PinType.PinSubCategory = LinkedPin->PinType.PinSubCategory;
    		SetPin->PinType.PinSubCategoryObject = LinkedPin->PinType.PinSubCategoryObject;
    		SetPin->PinType.ContainerType = EPinContainerType::Set;
        
    		ValuePin->PinType.PinCategory = LinkedPin->PinType.PinCategory;
    		ValuePin->PinType.PinSubCategory = LinkedPin->PinType.PinSubCategory;
    		ValuePin->PinType.PinSubCategoryObject = LinkedPin->PinType.PinSubCategoryObject;
        
    		bNotifyGraphChanged = true;
    	}
    }
    
    // 【修复】两个引脚都有连接：尝试从连接推断类型
    else if (SetPin->LinkedTo.Num() > 0 && ValuePin->LinkedTo.Num() > 0)
    {
    	UEdGraphPin* SetLinkedPin = SetPin->LinkedTo[0];
    	
    	// 优先从Set连接推断
    	if (SetLinkedPin->PinType.ContainerType == EPinContainerType::Set &&
    		SetLinkedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
    	{
    		SetPin->PinType.PinCategory = SetLinkedPin->PinType.PinCategory;
    		SetPin->PinType.PinSubCategory = SetLinkedPin->PinType.PinSubCategory;
    		SetPin->PinType.PinSubCategoryObject = SetLinkedPin->PinType.PinSubCategoryObject;
    		SetPin->PinType.ContainerType = EPinContainerType::Set;
    		
    		ValuePin->PinType.PinCategory = SetLinkedPin->PinType.PinCategory;
    		ValuePin->PinType.PinSubCategory = SetLinkedPin->PinType.PinSubCategory;
    		ValuePin->PinType.PinSubCategoryObject = SetLinkedPin->PinType.PinSubCategoryObject;
    		
    		bNotifyGraphChanged = true;
    	}
    	// Set连接是Wildcard，尝试从Value连接推断
    	else
    	{
    		UEdGraphPin* ValueLinkedPin = ValuePin->LinkedTo[0];
    		if (ValueLinkedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
    		{
    			SetPin->PinType.PinCategory = ValueLinkedPin->PinType.PinCategory;
    			SetPin->PinType.PinSubCategory = ValueLinkedPin->PinType.PinSubCategory;
    			SetPin->PinType.PinSubCategoryObject = ValueLinkedPin->PinType.PinSubCategoryObject;
    			SetPin->PinType.ContainerType = EPinContainerType::Set;
    			
    			ValuePin->PinType.PinCategory = ValueLinkedPin->PinType.PinCategory;
    			ValuePin->PinType.PinSubCategory = ValueLinkedPin->PinType.PinSubCategory;
    			ValuePin->PinType.PinSubCategoryObject = ValueLinkedPin->PinType.PinSubCategoryObject;
    			
    			bNotifyGraphChanged = true;
    		}
    	}
    }
    
    if (bNotifyGraphChanged)
    {
        GetGraph()->NotifyGraphChanged();
    }
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#undef LOCTEXT_NAMESPACE