#include "K2Nodes/K2Node_ForEachArray.h"

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

#define LOCTEXT_NAMESPACE "XTools_K2Node_ForEachArray"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region Helper

namespace ForEachArrayHelper
{
	const FName ArrayPinName = FName("Array");
	const FName LoopBodyPinName = FName("Loop Body");
	const FName ValuePinName = FName("Value");
	const FName IndexPinName = FName("Index");
    const FName BreakPinName = FName("Break");
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeAppearance

FText UK2Node_ForEachArray::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "ForEachArray");
}

FText UK2Node_ForEachArray::GetCompactNodeTitle() const
{
	return LOCTEXT("CompactNodeTitle", "FOREACH");
}

FText UK2Node_ForEachArray::GetTooltipText() const
{
	return LOCTEXT("TooltipText", "遍历数组中的每个元素");
}

FText UK2Node_ForEachArray::GetKeywords() const
{
	return LOCTEXT("Keywords", "foreach loop each 遍历 数组 循环 for array");
}

FText UK2Node_ForEachArray::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "XTools|Blueprint Extensions|Loops");
}

FSlateIcon UK2Node_ForEachArray::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Macro.Loop_16x");
	return Icon;
}

TSharedPtr<SWidget> UK2Node_ForEachArray::CreateNodeImage() const
{
	return SPinTypeSelector::ConstructPinTypeImage(GetArrayPin());
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintCompile

void UK2Node_ForEachArray::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    Super::ExpandNode(CompilerContext, SourceGraph);

	// 【防御性编程】：额外的空指针检查（超越 UE 标准实践，防御硬件不稳定）
	// 注：虽然 FindPinChecked() 在 UE 中被认为安全，但在硬件不稳定时可能返回损坏指针
	UEdGraphPin* ArrayPin = GetArrayPin();
	if (!ArrayPin)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("ArrayPinNotFound", "Internal error: Array pin not found @@").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	// 【最佳实践 3.1】：验证必要连接
	if (ArrayPin->LinkedTo.Num() == 0)
	{
		// 【最佳实践 4.3】：使用LOCTEXT本地化
		CompilerContext.MessageLog.Error(*LOCTEXT("ArrayNotConnected", "Array pin must be connected @@").ToString(), this);
		// 【最佳实践 3.1】：错误后必须调用BreakAllNodeLinks
		BreakAllNodeLinks();
		return;
	}
	
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
    LoopCounterInitialise->GetValuePin()->DefaultValue = TEXT("0");
    bResult &= Schema->TryCreateConnection(LoopCounterPin, LoopCounterInitialise->GetVariablePin());
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
    Condition->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_IntInt)));
    Condition->AllocateDefaultPins();
    bResult &= Schema->TryCreateConnection(Condition->GetReturnValuePin(), Branch->GetConditionPin());
    bResult &= Schema->TryCreateConnection(Condition->FindPinChecked(TEXT("A")), LoopCounterPin);
    UEdGraphPin* LoopConditionBPin = Condition->FindPinChecked(TEXT("B"));
    check(LoopConditionBPin);

    if (!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("ConditionFailed", "Could not connect loop condition node @@").ToString(), this);

    // Length of Array 
    UK2Node_CallFunction* Length = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    Length->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Length)));
    Length->AllocateDefaultPins();
    UEdGraphPin* LengthTargetArrayPin = Length->FindPinChecked(TEXT("TargetArray"), EGPD_Input);
    LengthTargetArrayPin->PinType = GetArrayPin()->PinType;
    LengthTargetArrayPin->PinType.PinValueType = FEdGraphTerminalType(GetArrayPin()->PinType.PinValueType);
    bResult &= Schema->TryCreateConnection(LoopConditionBPin, Length->GetReturnValuePin());
    CompilerContext.CopyPinLinksToIntermediate(*GetArrayPin(), *LengthTargetArrayPin);
    Length->PostReconstructNode();

    if (!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("LengthFailed", "Could not connect length node @@").ToString(), this);

    // Break loop from set counter
    UK2Node_CallFunction* BreakLength = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    BreakLength->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Length)));
    BreakLength->AllocateDefaultPins();
    UEdGraphPin* BreakLengthTargetArrayPin = BreakLength->FindPinChecked(TEXT("TargetArray"), EGPD_Input);
    BreakLengthTargetArrayPin->PinType = GetArrayPin()->PinType;
    BreakLengthTargetArrayPin->PinType.PinValueType = FEdGraphTerminalType(GetArrayPin()->PinType.PinValueType);
    CompilerContext.CopyPinLinksToIntermediate(*GetArrayPin(), *BreakLengthTargetArrayPin);
    BreakLength->PostReconstructNode();

    UK2Node_AssignmentStatement* LoopCounterBreak = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
    LoopCounterBreak->AllocateDefaultPins();
    bResult &= Schema->TryCreateConnection(LoopCounterBreak->GetVariablePin(), LoopCounterPin);
    bResult &= Schema->TryCreateConnection(LoopCounterBreak->GetValuePin(), BreakLength->GetReturnValuePin());
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

    // Get value
    UK2Node_CallFunction* GetValue = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    GetValue->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Get)));
    GetValue->AllocateDefaultPins();
    UEdGraphPin* GetValueTargetArrayPin = GetValue->FindPinChecked(TEXT("TargetArray"), EGPD_Input);
    GetValueTargetArrayPin->PinType = GetArrayPin()->PinType;
    GetValueTargetArrayPin->PinType.PinValueType = FEdGraphTerminalType(GetArrayPin()->PinType.PinValueType);
    bResult &= Schema->TryCreateConnection(GetValue->FindPinChecked(TEXT("Index")), LoopCounterPin);
    CompilerContext.CopyPinLinksToIntermediate(*GetArrayPin(), *GetValueTargetArrayPin);
    UEdGraphPin* ValuePin = GetValue->FindPinChecked(TEXT("Item"));
    ValuePin->PinType = GetValuePin()->PinType;
    check(ValuePin);
    GetValue->PostReconstructNode();

    if (!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("GetValueFailed", "Could not connect get array value node @@").ToString(), this);

    CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *LoopCounterInitialiseExecPin);
    CompilerContext.MovePinLinksToIntermediate(*GetLoopBodyPin(), *SequenceThen0Pin);
    CompilerContext.MovePinLinksToIntermediate(*GetCompletedPin(), *BranchElsePin);
    CompilerContext.MovePinLinksToIntermediate(*GetBreakPin(), *LoopCounterBreakExecPin);
    CompilerContext.MovePinLinksToIntermediate(*GetValuePin(), *ValuePin);
    CompilerContext.MovePinLinksToIntermediate(*GetIndexPin(), *LoopCounterPin);

    BreakAllNodeLinks();
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintSystem

void UK2Node_ForEachArray::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	const UClass* Action = GetClass();

	if (ActionRegistrar.IsOpenForRegistration(Action))
	{
		UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(GetClass());
		check(Spawner != nullptr);

		ActionRegistrar.AddBlueprintAction(Action, Spawner);
	}
}

void UK2Node_ForEachArray::PostReconstructNode()
{
	Super::PostReconstructNode();

	// 【修复】仅在有连接时才传播类型，避免重载时丢失已序列化的类型信息
	// 参考 UE 源码 K2Node_GetArrayItem::PostReconstructNode 实现
	UEdGraphPin* ArrayPin = GetArrayPin();
	UEdGraphPin* ValuePin = GetValuePin();
	
	if (ArrayPin->LinkedTo.Num() > 0 || ValuePin->LinkedTo.Num() > 0)
	{
		PropagatePinType();
	}
	else
	{
		// 【额外修复】无连接时，如果一个引脚有确定类型，另一个是Wildcard，则同步类型
		// 这可以自动修复已损坏的蓝图资产
		bool bArrayIsWildcard = (ArrayPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);
		bool bValueIsWildcard = (ValuePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);
		
		if (!bArrayIsWildcard && bValueIsWildcard)
		{
			ValuePin->PinType = ArrayPin->PinType;
			ValuePin->PinType.ContainerType = EPinContainerType::None;
			GetGraph()->NotifyGraphChanged();
		}
		else if (bArrayIsWildcard && !bValueIsWildcard)
		{
			ArrayPin->PinType = ValuePin->PinType;
			ArrayPin->PinType.ContainerType = EPinContainerType::Array;
			GetGraph()->NotifyGraphChanged();
		}
	}
}

void UK2Node_ForEachArray::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

    if (Pin == GetArrayPin() || Pin == GetValuePin())
	{
		PropagatePinType();
	}
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region PinManagement

void UK2Node_ForEachArray::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	// Execute
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	// Map 
	UEdGraphNode::FCreatePinParams PinParams;
	PinParams.ContainerType = EPinContainerType::Array;
	PinParams.ValueTerminalType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
	PinParams.ValueTerminalType.TerminalSubCategory = NAME_None;
	PinParams.ValueTerminalType.TerminalSubCategoryObject = nullptr;
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, ForEachArrayHelper::ArrayPinName, PinParams);

	// Break
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, ForEachArrayHelper::BreakPinName)->PinFriendlyName = FText::FromName(ForEachArrayHelper::BreakPinName);

	// Loop body
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, ForEachArrayHelper::LoopBodyPinName);

	// Value
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, ForEachArrayHelper::ValuePinName);

	// Index
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Int, ForEachArrayHelper::IndexPinName);

	// Completed
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then)->PinFriendlyName = FText::FromName(UEdGraphSchema_K2::PN_Completed);
}

bool UK2Node_ForEachArray::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	return Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason);
}

UEdGraphPin* UK2Node_ForEachArray::GetLoopBodyPin() const
{
	return FindPinChecked(ForEachArrayHelper::LoopBodyPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForEachArray::GetBreakPin() const
{
	return FindPinChecked(ForEachArrayHelper::BreakPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForEachArray::GetCompletedPin() const
{
	return FindPinChecked(UEdGraphSchema_K2::PN_Then, EGPD_Output);
}

UEdGraphPin* UK2Node_ForEachArray::GetArrayPin() const
{
	return FindPinChecked(ForEachArrayHelper::ArrayPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForEachArray::GetValuePin() const
{
	return FindPinChecked(ForEachArrayHelper::ValuePinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForEachArray::GetIndexPin() const
{
	return FindPinChecked(ForEachArrayHelper::IndexPinName, EGPD_Output);
}

void UK2Node_ForEachArray::PropagatePinType() const
{
    bool bNotifyGraphChanged = false;
    UEdGraphPin* ArrayPin = GetArrayPin();
    UEdGraphPin* ValuePin = GetValuePin();
    
    // 【修复】无连接的情况：仅在引脚当前为Wildcard时才重置
    // 这样可以保留加载时已序列化的类型信息
    if (ArrayPin->LinkedTo.Num() == 0 && ValuePin->LinkedTo.Num() == 0)
    {
        // 只有在引脚类型确实是Wildcard时才重置
        bool bArrayIsWildcard = (ArrayPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);
        bool bValueIsWildcard = (ValuePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);
        
        if (!bArrayIsWildcard || !bValueIsWildcard)
        {
            // 引脚已有确定的类型（从序列化数据恢复），保留它
            return;
        }

        // 重置为Wildcard
        ArrayPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
        ArrayPin->PinType.PinSubCategory = NAME_None;
        ArrayPin->PinType.PinSubCategoryObject = nullptr;
        ArrayPin->PinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
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
    else if (ArrayPin->LinkedTo.Num() > 0 && ValuePin->LinkedTo.Num() == 0)
    {
    	UEdGraphPin* LinkedPin = ArrayPin->LinkedTo[0];
    	if (LinkedPin->PinType.ContainerType == EPinContainerType::Array &&
    		LinkedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
    	{
    		ArrayPin->PinType.PinCategory = LinkedPin->PinType.PinCategory;
    		ArrayPin->PinType.PinSubCategory = LinkedPin->PinType.PinSubCategory;
    		ArrayPin->PinType.PinSubCategoryObject = LinkedPin->PinType.PinSubCategoryObject;
    		ArrayPin->PinType.ContainerType = EPinContainerType::Array;
        
    		ValuePin->PinType.PinCategory = LinkedPin->PinType.PinCategory;
    		ValuePin->PinType.PinSubCategory = LinkedPin->PinType.PinSubCategory;
    		ValuePin->PinType.PinSubCategoryObject = LinkedPin->PinType.PinSubCategoryObject;
        
    		bNotifyGraphChanged = true;
    	}
    }

	// 只有 Value 引脚有连接
    else if (ArrayPin->LinkedTo.Num() == 0 && ValuePin->LinkedTo.Num() > 0)
    {
    	UEdGraphPin* LinkedPin = ValuePin->LinkedTo[0];
    	if (LinkedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
    	{
    		ArrayPin->PinType.PinCategory = LinkedPin->PinType.PinCategory;
    		ArrayPin->PinType.PinSubCategory = LinkedPin->PinType.PinSubCategory;
    		ArrayPin->PinType.PinSubCategoryObject = LinkedPin->PinType.PinSubCategoryObject;
    		ArrayPin->PinType.ContainerType = EPinContainerType::Array;
    
    		ValuePin->PinType.PinCategory = LinkedPin->PinType.PinCategory;
    		ValuePin->PinType.PinSubCategory = LinkedPin->PinType.PinSubCategory;
    		ValuePin->PinType.PinSubCategoryObject = LinkedPin->PinType.PinSubCategoryObject;
    
    		bNotifyGraphChanged = true;
    	}
    }
	
    // 【修复】两个引脚都有连接：尝试从连接推断类型
    else if (ArrayPin->LinkedTo.Num() > 0 && ValuePin->LinkedTo.Num() > 0)
    {
    	UEdGraphPin* ArrayLinkedPin = ArrayPin->LinkedTo[0];
    	
    	// 优先从Array连接推断
    	if (ArrayLinkedPin->PinType.ContainerType == EPinContainerType::Array &&
    		ArrayLinkedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
    	{
    		ArrayPin->PinType.PinCategory = ArrayLinkedPin->PinType.PinCategory;
    		ArrayPin->PinType.PinSubCategory = ArrayLinkedPin->PinType.PinSubCategory;
    		ArrayPin->PinType.PinSubCategoryObject = ArrayLinkedPin->PinType.PinSubCategoryObject;
    		ArrayPin->PinType.ContainerType = EPinContainerType::Array;
    		
    		ValuePin->PinType.PinCategory = ArrayLinkedPin->PinType.PinCategory;
    		ValuePin->PinType.PinSubCategory = ArrayLinkedPin->PinType.PinSubCategory;
    		ValuePin->PinType.PinSubCategoryObject = ArrayLinkedPin->PinType.PinSubCategoryObject;
    		
    		bNotifyGraphChanged = true;
    	}
    	// Array连接是Wildcard，尝试从Value连接推断
    	else
    	{
    		UEdGraphPin* ValueLinkedPin = ValuePin->LinkedTo[0];
    		if (ValueLinkedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
    		{
    			ArrayPin->PinType.PinCategory = ValueLinkedPin->PinType.PinCategory;
    			ArrayPin->PinType.PinSubCategory = ValueLinkedPin->PinType.PinSubCategory;
    			ArrayPin->PinType.PinSubCategoryObject = ValueLinkedPin->PinType.PinSubCategoryObject;
    			ArrayPin->PinType.ContainerType = EPinContainerType::Array;
    			
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