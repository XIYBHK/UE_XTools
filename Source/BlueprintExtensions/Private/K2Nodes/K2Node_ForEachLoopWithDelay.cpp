#include "K2Nodes/K2Node_ForEachLoopWithDelay.h"

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

#define LOCTEXT_NAMESPACE "XTools_K2Node_ForEachLoopWithDelay"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region Helper

namespace ForEachLoopWithDelayHelper
{
	const FName ArrayPinName = FName("Array");
	const FName DelayPinName = FName("Delay");
	const FName LoopBodyPinName = FName("Loop Body");
	const FName ValuePinName = FName("Value");
	const FName IndexPinName = FName("Index");
	const FName BreakPinName = FName("Break");
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeAppearance

FText UK2Node_ForEachLoopWithDelay::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "带延迟的ForEachLoop");
}

FText UK2Node_ForEachLoopWithDelay::GetCompactNodeTitle() const
{
	return LOCTEXT("CompactNodeTitle", "FOREACH DELAY");
}

FText UK2Node_ForEachLoopWithDelay::GetTooltipText() const
{
	return LOCTEXT("TooltipText", "遍历数组中的每个元素，每次迭代之间等待指定的延迟时间");
}

FText UK2Node_ForEachLoopWithDelay::GetKeywords() const
{
	return LOCTEXT("Keywords", "foreach loop each delay 遍历 数组 循环 延迟 等待 for array");
}

FText UK2Node_ForEachLoopWithDelay::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "XTools|Blueprint Extensions|Loops");
}

FSlateIcon UK2Node_ForEachLoopWithDelay::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Macro.Loop_16x");
	return Icon;
}

TSharedPtr<SWidget> UK2Node_ForEachLoopWithDelay::CreateNodeImage() const
{
	return SPinTypeSelector::ConstructPinTypeImage(GetArrayPin());
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintCompile

void UK2Node_ForEachLoopWithDelay::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	// 【参考 K2Node_SmartSort 实现模式】
	// 不调用 Super::ExpandNode()，因为基类会提前断开所有链接

	// 验证数组引脚连接
	UEdGraphPin* ArrayPin = GetArrayPin();
	if (!ArrayPin || ArrayPin->LinkedTo.Num() == 0)
	{
		CompilerContext.MessageLog.Warning(*LOCTEXT("ArrayNotConnected", "Array pin must be connected @@").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	// 1. 创建循环计数器临时变量
	UK2Node_TemporaryVariable* LoopCounterNode = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(this, SourceGraph);
	LoopCounterNode->VariableType.PinCategory = UEdGraphSchema_K2::PC_Int;
	LoopCounterNode->AllocateDefaultPins();
	UEdGraphPin* LoopCounterPin = LoopCounterNode->GetVariablePin();

	// 2. 初始化循环计数器为0
	UK2Node_AssignmentStatement* LoopCounterInit = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	LoopCounterInit->AllocateDefaultPins();
	LoopCounterInit->GetValuePin()->DefaultValue = TEXT("0");
	CompilerContext.GetSchema()->TryCreateConnection(LoopCounterPin, LoopCounterInit->GetVariablePin());

	// 3. 创建分支节点
	UK2Node_IfThenElse* Branch = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
	Branch->AllocateDefaultPins();
	CompilerContext.GetSchema()->TryCreateConnection(LoopCounterInit->GetThenPin(), Branch->GetExecPin());

	// 4. 创建循环条件（计数器 < 数组长度）
	UK2Node_CallFunction* Condition = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	Condition->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_IntInt)));
	Condition->AllocateDefaultPins();
	CompilerContext.GetSchema()->TryCreateConnection(Condition->GetReturnValuePin(), Branch->GetConditionPin());
	CompilerContext.GetSchema()->TryCreateConnection(Condition->FindPinChecked(TEXT("A")), LoopCounterPin);

	// 5. 获取数组长度
	UK2Node_CallFunction* Length = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	Length->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Length)));
	Length->AllocateDefaultPins();
	UEdGraphPin* LengthTargetArrayPin = Length->FindPinChecked(TEXT("TargetArray"), EGPD_Input);
	LengthTargetArrayPin->PinType = GetArrayPin()->PinType;
	LengthTargetArrayPin->PinType.PinValueType = FEdGraphTerminalType(GetArrayPin()->PinType.PinValueType);
	CompilerContext.GetSchema()->TryCreateConnection(Condition->FindPinChecked(TEXT("B")), Length->GetReturnValuePin());
	CompilerContext.CopyPinLinksToIntermediate(*GetArrayPin(), *LengthTargetArrayPin);
	Length->PostReconstructNode();

	// 6. 创建延迟节点
	UK2Node_CallFunction* DelayNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	DelayNode->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Delay)));
	DelayNode->AllocateDefaultPins();
	CompilerContext.GetSchema()->TryCreateConnection(Branch->GetThenPin(), DelayNode->GetExecPin());

	// 7. 创建执行序列（循环体 -> 递增）
	UK2Node_ExecutionSequence* Sequence = CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(this, SourceGraph);
	Sequence->AllocateDefaultPins();
	CompilerContext.GetSchema()->TryCreateConnection(DelayNode->GetThenPin(), Sequence->GetExecPin());

	// 8. 创建递增节点
	UK2Node_CallFunction* Increment = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	Increment->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Add_IntInt)));
	Increment->AllocateDefaultPins();
	CompilerContext.GetSchema()->TryCreateConnection(Increment->FindPinChecked(TEXT("A")), LoopCounterPin);
	Increment->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("1");

	// 9. 创建赋值节点（递增后的值）
	UK2Node_AssignmentStatement* LoopCounterAssign = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	LoopCounterAssign->AllocateDefaultPins();
	CompilerContext.GetSchema()->TryCreateConnection(LoopCounterAssign->GetExecPin(), Sequence->GetThenPinGivenIndex(1));
	CompilerContext.GetSchema()->TryCreateConnection(LoopCounterAssign->GetVariablePin(), LoopCounterPin);
	CompilerContext.GetSchema()->TryCreateConnection(LoopCounterAssign->GetValuePin(), Increment->GetReturnValuePin());
	CompilerContext.GetSchema()->TryCreateConnection(LoopCounterAssign->GetThenPin(), Branch->GetExecPin());  // 循环回到分支

	// 10. Break 功能：设置计数器为数组长度以跳出循环
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
	CompilerContext.GetSchema()->TryCreateConnection(LoopCounterBreak->GetVariablePin(), LoopCounterPin);
	CompilerContext.GetSchema()->TryCreateConnection(LoopCounterBreak->GetValuePin(), BreakLength->GetReturnValuePin());

	// 11. 获取数组元素
	UK2Node_CallFunction* GetElement = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	GetElement->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Get)));
	GetElement->AllocateDefaultPins();
	UEdGraphPin* GetTargetArrayPin = GetElement->FindPinChecked(TEXT("TargetArray"), EGPD_Input);
	GetTargetArrayPin->PinType = GetArrayPin()->PinType;
	GetTargetArrayPin->PinType.PinValueType = FEdGraphTerminalType(GetArrayPin()->PinType.PinValueType);
	CompilerContext.CopyPinLinksToIntermediate(*GetArrayPin(), *GetTargetArrayPin);
	CompilerContext.GetSchema()->TryCreateConnection(GetElement->FindPinChecked(TEXT("Index")), LoopCounterPin);
	UEdGraphPin* ValuePin = GetElement->FindPinChecked(TEXT("Item"));
	ValuePin->PinType = GetValuePin()->PinType;
	GetElement->PostReconstructNode();

	// 12. 最后统一移动所有外部连接（参考智能排序模式）
	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *LoopCounterInit->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*GetDelayPin(), *DelayNode->FindPinChecked(TEXT("Duration")));
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

void UK2Node_ForEachLoopWithDelay::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

void UK2Node_ForEachLoopWithDelay::PostReconstructNode()
{
	Super::PostReconstructNode();
	
	// 【修复】仅在有连接时才传播类型，避免重载时丢失已序列化的类型信息
	// 参考 UE 源码 K2Node_GetArrayItem::PostReconstructNode 实现
	UEdGraphPin* ArrayPin = GetArrayPin();
	UEdGraphPin* ValuePin = GetValuePin();
	
	if (ArrayPin && ValuePin)
	{
		if (ArrayPin->LinkedTo.Num() > 0 || ValuePin->LinkedTo.Num() > 0)
		{
			PropagatePinType();
		}
		else
		{
			// 【额外修复】无连接时，如果一个引脚有确定类型，另一个是Wildcard，则同步类型
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
}

bool UK2Node_ForEachLoopWithDelay::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	// Delay 节点需要支持 Latent 操作的图
	const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	return Blueprint && FBlueprintEditorUtils::DoesSupportEventGraphs(Blueprint) && Super::IsCompatibleWithGraph(TargetGraph);
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region PinManagement

void UK2Node_ForEachLoopWithDelay::AllocateDefaultPins()
{
	using namespace ForEachLoopWithDelayHelper;

	// 输入执行引脚
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);

	// Array 输入
	UEdGraphPin* ArrayPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, ArrayPinName);
	ArrayPin->PinType.ContainerType = EPinContainerType::Array;
	ArrayPin->PinToolTip = LOCTEXT("ArrayTooltip", "要遍历的数组").ToString();

	// Delay 输入
	UEdGraphPin* DelayPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float, DelayPinName);
	DelayPin->DefaultValue = TEXT("0.1");
	DelayPin->PinToolTip = LOCTEXT("DelayTooltip", "每次循环之间的延迟时间（秒）").ToString();

	// LoopBody 输出执行引脚
	UEdGraphPin* LoopBodyPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, LoopBodyPinName);
	LoopBodyPin->PinToolTip = LOCTEXT("LoopBodyTooltip", "循环体：每次迭代时执行").ToString();

	// Value 输出
	UEdGraphPin* ValuePin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, ValuePinName);
	ValuePin->PinToolTip = LOCTEXT("ValueTooltip", "当前数组元素").ToString();

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

void UK2Node_ForEachLoopWithDelay::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	if (Pin == GetArrayPin())
	{
		PropagatePinType();
	}
}

bool UK2Node_ForEachLoopWithDelay::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	if (MyPin == GetArrayPin() && MyPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
	{
		if (OtherPin->PinType.ContainerType != EPinContainerType::Array)
		{
			OutReason = LOCTEXT("MustConnectArray", "Must connect to an array").ToString();
			return true;
		}
	}

	return Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason);
}

UEdGraphPin* UK2Node_ForEachLoopWithDelay::GetArrayPin() const
{
	return FindPinChecked(ForEachLoopWithDelayHelper::ArrayPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForEachLoopWithDelay::GetDelayPin() const
{
	return FindPinChecked(ForEachLoopWithDelayHelper::DelayPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForEachLoopWithDelay::GetLoopBodyPin() const
{
	return FindPinChecked(ForEachLoopWithDelayHelper::LoopBodyPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForEachLoopWithDelay::GetBreakPin() const
{
	return FindPin(ForEachLoopWithDelayHelper::BreakPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForEachLoopWithDelay::GetCompletedPin() const
{
	return FindPinChecked(UEdGraphSchema_K2::PN_Then, EGPD_Output);
}

UEdGraphPin* UK2Node_ForEachLoopWithDelay::GetValuePin() const
{
	return FindPinChecked(ForEachLoopWithDelayHelper::ValuePinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForEachLoopWithDelay::GetIndexPin() const
{
	return FindPinChecked(ForEachLoopWithDelayHelper::IndexPinName, EGPD_Output);
}

void UK2Node_ForEachLoopWithDelay::PropagatePinType() const
{
	bool bNotifyGraphChanged = false;
	UEdGraphPin* ArrayPin = GetArrayPin();
	UEdGraphPin* ValuePin = GetValuePin();

	if (!ArrayPin || !ValuePin)
	{
		return;
	}

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

#undef LOCTEXT_NAMESPACE

