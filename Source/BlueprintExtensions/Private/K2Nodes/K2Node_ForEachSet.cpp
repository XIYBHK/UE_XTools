#include "K2Nodes/K2Node_ForEachSet.h"

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
    Super::ExpandNode(CompilerContext, SourceGraph);

	// 【最佳实践 3.1】：验证必要连接
	UEdGraphPin* SetPin = GetSetPin();
	if (SetPin->LinkedTo.Num() == 0)
	{
		// 【最佳实践 4.3】：使用LOCTEXT本地化
		CompilerContext.MessageLog.Error(*LOCTEXT("SetNotConnected", "Set pin must be connected @@").ToString(), this);
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

    // Length of Set 
    UK2Node_CallFunction* Length = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    Length->SetFromFunction(UBlueprintSetLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UBlueprintSetLibrary, Set_Length)));
    Length->AllocateDefaultPins();
    UEdGraphPin* LengthTargetArrayPin = Length->FindPinChecked(TEXT("TargetSet"), EGPD_Input);
    LengthTargetArrayPin->PinType = GetSetPin()->PinType;
    LengthTargetArrayPin->PinType.PinValueType = FEdGraphTerminalType(GetSetPin()->PinType.PinValueType);
    bResult &= Schema->TryCreateConnection(LoopConditionBPin, Length->GetReturnValuePin());
    CompilerContext.CopyPinLinksToIntermediate(*GetSetPin(), *LengthTargetArrayPin);
    Length->PostReconstructNode();

    if (!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("LengthFailed", "Could not connect length node @@").ToString(), this);

    // Break loop from set counter
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

    // Set to Array
    UK2Node_CallFunction* ToArrayFun = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    ToArrayFun->SetFromFunction(UBlueprintSetLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UBlueprintSetLibrary, Set_ToArray)));
    ToArrayFun->AllocateDefaultPins();
    UEdGraphPin* ToArrayFunPin = ToArrayFun->FindPinChecked(TEXT("A"), EGPD_Input);
    ToArrayFunPin->PinType = GetSetPin()->PinType;
    ToArrayFunPin->PinType.PinValueType = FEdGraphTerminalType(GetSetPin()->PinType.PinValueType);
    CompilerContext.CopyPinLinksToIntermediate(*GetSetPin(), *ToArrayFunPin);
    UEdGraphPin* ToArrayFunValuePin = ToArrayFun->FindPinChecked(TEXT("Result"));
    check(ToArrayFunValuePin);
    ToArrayFun->PostReconstructNode();
    bResult &= Schema->TryCreateConnection(ToArrayFun->GetThenPin(), LoopCounterInitialiseExecPin);

    if (!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("SetToArrayFailed", "Could not connect Set to Array node @@").ToString(), this);

    // Get value
    UK2Node_CallFunction* GetValue = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    GetValue->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Get)));
    GetValue->AllocateDefaultPins();
    UEdGraphPin* GetValueTargetArrayPin = GetValue->FindPinChecked(TEXT("TargetArray"), EGPD_Input);
    GetValueTargetArrayPin->PinType = ToArrayFunValuePin->PinType;
    GetValueTargetArrayPin->PinType.PinValueType = FEdGraphTerminalType(ToArrayFunValuePin->PinType.PinValueType);
    bResult &= Schema->TryCreateConnection(GetValue->FindPinChecked(TEXT("Index")), LoopCounterPin);
    bResult &= Schema->TryCreateConnection(GetValueTargetArrayPin, ToArrayFunValuePin);
    UEdGraphPin* ValuePin = GetValue->FindPinChecked(TEXT("Item"));
    ValuePin->PinType = GetValuePin()->PinType;
    check(ValuePin);
    GetValue->PostReconstructNode();

    if (!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("GetValueFailed", "Could not connect get Set value node @@").ToString(), this);

    CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *(ToArrayFun->GetExecPin()));
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

void UK2Node_ForEachSet::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	const UClass* Action = GetClass();

	if (ActionRegistrar.IsOpenForRegistration(Action))
	{
		UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(GetClass());
		check(Spawner != nullptr);

		ActionRegistrar.AddBlueprintAction(Action, Spawner);
	}
}

void UK2Node_ForEachSet::PostReconstructNode()
{
	Super::PostReconstructNode();

	PropagatePinType();
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
    
    // 无连接的情况：重置为Wildcard并断开连接
    if (SetPin->LinkedTo.Num() == 0 && ValuePin->LinkedTo.Num() == 0)
    {
        // 重置 Set 引脚
        SetPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
        SetPin->PinType.PinSubCategory = NAME_None;
        SetPin->PinType.PinSubCategoryObject = nullptr;
        SetPin->PinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
        SetPin->PinType.PinValueType.TerminalSubCategory = NAME_None;
        SetPin->PinType.PinValueType.TerminalSubCategoryObject = nullptr;
        SetPin->BreakAllPinLinks(true);

        // 重置 Value 引脚
        ValuePin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
        ValuePin->PinType.PinSubCategory = NAME_None;
        ValuePin->PinType.PinSubCategoryObject = nullptr;
        ValuePin->BreakAllPinLinks(true);

        bNotifyGraphChanged = true;
    }
    
    // 只有 Set 引脚有连接：同时更新 Set 和 Value 引脚
    else if (SetPin->LinkedTo.Num() > 0 && ValuePin->LinkedTo.Num() == 0)
    {
        if (SetPin->LinkedTo[0]->PinType.ContainerType == EPinContainerType::Set)
        {
            // 更新 Set 引脚
            SetPin->PinType.PinCategory = SetPin->LinkedTo[0]->PinType.PinCategory;
            SetPin->PinType.PinSubCategory = SetPin->LinkedTo[0]->PinType.PinSubCategory;
            SetPin->PinType.PinSubCategoryObject = SetPin->LinkedTo[0]->PinType.PinSubCategoryObject;
            SetPin->PinType.ContainerType = EPinContainerType::Set;
            
            // 更新 Value 引脚
            ValuePin->PinType.PinCategory = SetPin->LinkedTo[0]->PinType.PinCategory;
            ValuePin->PinType.PinSubCategory = SetPin->LinkedTo[0]->PinType.PinSubCategory;
            ValuePin->PinType.PinSubCategoryObject = SetPin->LinkedTo[0]->PinType.PinSubCategoryObject;
            
            bNotifyGraphChanged = true;
        }
    }

    // 只有 Value 引脚有连接：同时更新 Set 和 Value 引脚
    else if (SetPin->LinkedTo.Num() == 0 && ValuePin->LinkedTo.Num() > 0)
    {
        // 更新 Set 引脚
        SetPin->PinType.PinCategory = ValuePin->LinkedTo[0]->PinType.PinCategory;
        SetPin->PinType.PinSubCategory = ValuePin->LinkedTo[0]->PinType.PinSubCategory;
        SetPin->PinType.PinSubCategoryObject = ValuePin->LinkedTo[0]->PinType.PinSubCategoryObject;
        SetPin->PinType.ContainerType = EPinContainerType::Set;
        
        // 更新 Value 引脚
        ValuePin->PinType.PinCategory = ValuePin->LinkedTo[0]->PinType.PinCategory;
        ValuePin->PinType.PinSubCategory = ValuePin->LinkedTo[0]->PinType.PinSubCategory;
        ValuePin->PinType.PinSubCategoryObject = ValuePin->LinkedTo[0]->PinType.PinSubCategoryObject;
        
        bNotifyGraphChanged = true;
    }
    
    // 两个引脚都有连接：不做处理
    if (bNotifyGraphChanged)
    {
        GetGraph()->NotifyGraphChanged();
    }
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#undef LOCTEXT_NAMESPACE