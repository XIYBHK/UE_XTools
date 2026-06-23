#include "K2Nodes/K2Node_ForEachMap.h"
#include "K2Nodes/K2NodeHelpers.h"
#include "K2NodePinTypeHelpers.h"

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
#include "K2Node_CallArrayFunction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_TemporaryVariable.h"

// 功能库
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet/BlueprintMapLibrary.h"


#define LOCTEXT_NAMESPACE "XTools_K2Node_MapForEach"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region Helper

namespace ForEachMapHelper
{
	const FName LoopBodyPinName = FName("Loop Body") ;
	const FName BreakPinName = FName("Break");
	const FName MapPinName = FName("Map");
	const FName KeyPinName = FName("Key");
	const FName ValuePinName = FName("Value");
	const FName IndexPinName = FName("Index");

	/** 从源引脚复制类型到目标引脚（基础类型） */
	void CopyPinType(UEdGraphPin* DestPin, const UEdGraphPin* SourcePin)
	{
		DestPin->PinType.PinCategory = SourcePin->PinType.PinCategory;
		DestPin->PinType.PinSubCategory = SourcePin->PinType.PinSubCategory;
		DestPin->PinType.PinSubCategoryObject = SourcePin->PinType.PinSubCategoryObject;
	}

	/** 从源引脚复制 Key 类型到 Map 引脚 */
	void CopyKeyTypeToMapPin(UEdGraphPin* MapPin, const UEdGraphPin* SourcePin)
	{
		MapPin->PinType.PinCategory = SourcePin->PinType.PinCategory;
		MapPin->PinType.PinSubCategory = SourcePin->PinType.PinSubCategory;
		MapPin->PinType.PinSubCategoryObject = SourcePin->PinType.PinSubCategoryObject;
	}

	/** 从源引脚复制 Value 类型到 Map 引脚的 PinValueType */
	void CopyValueTypeToMapPin(UEdGraphPin* MapPin, const UEdGraphPin* SourcePin)
	{
		MapPin->PinType.PinValueType.TerminalCategory = SourcePin->PinType.PinCategory;
		MapPin->PinType.PinValueType.TerminalSubCategory = SourcePin->PinType.PinSubCategory;
		MapPin->PinType.PinValueType.TerminalSubCategoryObject = SourcePin->PinType.PinSubCategoryObject;
	}

}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeAppearance

FText UK2Node_ForEachMap::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "ForEachMap");
}

FText UK2Node_ForEachMap::GetCompactNodeTitle() const
{
	return LOCTEXT("CompactNodeTitle", "FOREACH");
}

FText UK2Node_ForEachMap::GetTooltipText() const
{
	return LOCTEXT("TooltipText", "遍历Map中的每个键值对");
}

FText UK2Node_ForEachMap::GetKeywords() const
{
	return LOCTEXT("Keywords", "foreach loop each map 遍历 字典 循环 键值对 for");
}

FText UK2Node_ForEachMap::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "XTools|Blueprint Extensions|Loops");
}

FSlateIcon UK2Node_ForEachMap::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Macro.Loop_16x");
	return Icon;
}

TSharedPtr<SWidget> UK2Node_ForEachMap::CreateNodeImage() const
{
	return SPinTypeSelector::ConstructPinTypeImage(GetMapPin());
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintCompile

void UK2Node_ForEachMap::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	// 【参考 K2Node_SmartSort 实现模式】
	// 不调用 Super::ExpandNode()，因为基类会提前断开所有链接
	if (!K2NodeHelpers::BeginExpandNode(
			CompilerContext, this,
			{GetExecPin(), GetMapPin(), GetLoopBodyPin(), GetBreakPin(), GetKeyPin(), GetValuePin(), GetIndexPin(), GetCompletedPin()},
			LOCTEXT("MissingPins", "@@ 节点引脚不完整")))
	{
		return;
	}

	// 验证 Map 引脚连接
	UEdGraphPin* MapPin = GetMapPin();
	if (!MapPin || MapPin->LinkedTo.Num() == 0)
	{
		CompilerContext.MessageLog.Warning(*LOCTEXT("MapNotConnected", "Map pin must be connected @@").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	// 验证类型有效性
	if (MapPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard ||
		MapPin->PinType.PinValueType.TerminalCategory == UEdGraphSchema_K2::PC_Wildcard)
	{
		CompilerContext.MessageLog.Warning(*LOCTEXT("InvalidMapType", "Map Key and Value types must be valid @@").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	// 1. 创建循环计数器临时变量
	UK2Node_TemporaryVariable* LoopCounterNode = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(this, SourceGraph);
	LoopCounterNode->VariableType.PinCategory = UEdGraphSchema_K2::PC_Int;
	LoopCounterNode->AllocateDefaultPins();
	UEdGraphPin* LoopCounterPin = LoopCounterNode->GetVariablePin();

	// 2. 进入循环前生成 Key/Value 快照，避免循环体修改 Map 后索引漂移
	UK2Node_CallFunction* MapKeys = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	MapKeys->SetFromFunction(UBlueprintMapLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UBlueprintMapLibrary, Map_Keys)));
	MapKeys->AllocateDefaultPins();
	UEdGraphPin* MapKeysTargetMapPin = MapKeys->FindPinChecked(TEXT("TargetMap"), EGPD_Input);
	MapKeysTargetMapPin->PinType = GetMapPin()->PinType;
	MapKeysTargetMapPin->PinType.PinValueType = FEdGraphTerminalType(GetMapPin()->PinType.PinValueType);
	CompilerContext.CopyPinLinksToIntermediate(*GetMapPin(), *MapKeysTargetMapPin);
	MapKeys->PinConnectionListChanged(MapKeysTargetMapPin);
	UEdGraphPin* KeysArrayPin = K2NodeHelpers::ReconstructAndFindPin(MapKeys, TEXT("Keys"), EGPD_Output);
	if (!ensureMsgf(KeysArrayPin, TEXT("ForEachMap: 找不到 Keys 引脚，中间节点重建失败")))
	{
		CompilerContext.MessageLog.Warning(*LOCTEXT("ForEachMap_NoKeysPin", "警告：[ForEachMap] 节点 @@ 找不到 Keys 引脚，展开中止。").ToString(), this);
		BreakAllNodeLinks();
		return;
	}
	KeysArrayPin->PinType = GetKeyPin()->PinType;
	KeysArrayPin->PinType.ContainerType = EPinContainerType::Array;
	MapKeysTargetMapPin = MapKeys->FindPinChecked(TEXT("TargetMap"), EGPD_Input);
	MapKeysTargetMapPin->PinType = GetMapPin()->PinType;
	MapKeysTargetMapPin->PinType.PinValueType = FEdGraphTerminalType(GetMapPin()->PinType.PinValueType);

	UK2Node_CallFunction* MapValues = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	MapValues->SetFromFunction(UBlueprintMapLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UBlueprintMapLibrary, Map_Values)));
	MapValues->AllocateDefaultPins();
	UEdGraphPin* MapValuesTargetMapPin = MapValues->FindPinChecked(TEXT("TargetMap"), EGPD_Input);
	MapValuesTargetMapPin->PinType = GetMapPin()->PinType;
	MapValuesTargetMapPin->PinType.PinValueType = FEdGraphTerminalType(GetMapPin()->PinType.PinValueType);
	CompilerContext.CopyPinLinksToIntermediate(*GetMapPin(), *MapValuesTargetMapPin);
	MapValues->PinConnectionListChanged(MapValuesTargetMapPin);
	UEdGraphPin* ValuesArrayPin = K2NodeHelpers::ReconstructAndFindPin(MapValues, TEXT("Values"), EGPD_Output);
	if (!ensureMsgf(ValuesArrayPin, TEXT("ForEachMap: 找不到 Values 引脚，中间节点重建失败")))
	{
		CompilerContext.MessageLog.Warning(*LOCTEXT("ForEachMap_NoValuesPin", "警告：[ForEachMap] 节点 @@ 找不到 Values 引脚，展开中止。").ToString(), this);
		BreakAllNodeLinks();
		return;
	}
	ValuesArrayPin->PinType = GetValuePin()->PinType;
	ValuesArrayPin->PinType.ContainerType = EPinContainerType::Array;
	MapValuesTargetMapPin = MapValues->FindPinChecked(TEXT("TargetMap"), EGPD_Input);
	MapValuesTargetMapPin->PinType = GetMapPin()->PinType;
	MapValuesTargetMapPin->PinType.PinValueType = FEdGraphTerminalType(GetMapPin()->PinType.PinValueType);

	K2NodeHelpers::TryConnect(CompilerContext, MapKeys->GetThenPin(), MapValues->GetExecPin());

	// 3. 初始化循环计数器为0
	UK2Node_AssignmentStatement* LoopCounterInitialise = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	LoopCounterInitialise->AllocateDefaultPins();
	LoopCounterInitialise->GetValuePin()->DefaultValue = TEXT("0");
	K2NodeHelpers::TryConnect(CompilerContext, LoopCounterPin, LoopCounterInitialise->GetVariablePin());
	K2NodeHelpers::TryConnect(CompilerContext, MapValues->GetThenPin(), LoopCounterInitialise->GetExecPin());

	// 4. 创建分支节点
	UK2Node_IfThenElse* Branch = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
	Branch->AllocateDefaultPins();
	K2NodeHelpers::TryConnect(CompilerContext, LoopCounterInitialise->GetThenPin(), Branch->GetExecPin());

	// 5. 创建循环条件（计数器 < Keys快照长度）
	UK2Node_CallFunction* Condition = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	Condition->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_IntInt)));
	Condition->AllocateDefaultPins();
	K2NodeHelpers::TryConnect(CompilerContext, Condition->GetReturnValuePin(), Branch->GetConditionPin());
	K2NodeHelpers::TryConnect(CompilerContext, Condition->FindPinChecked(TEXT("A")), LoopCounterPin);

	// 6. 获取 Keys 快照长度
	UK2Node_CallArrayFunction* Length = CompilerContext.SpawnIntermediateNode<UK2Node_CallArrayFunction>(this, SourceGraph);
	Length->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Length)));
	Length->AllocateDefaultPins();
	UEdGraphPin* LengthTargetArrayPin = Length->GetTargetArrayPin();
	LengthTargetArrayPin->PinType = KeysArrayPin->PinType;
	LengthTargetArrayPin->PinType.PinValueType = FEdGraphTerminalType(KeysArrayPin->PinType.PinValueType);
	K2NodeHelpers::TryConnect(CompilerContext, Condition->FindPinChecked(TEXT("B")), Length->GetReturnValuePin());
	K2NodeHelpers::TryConnect(CompilerContext, LengthTargetArrayPin, KeysArrayPin);
	Length->PinConnectionListChanged(LengthTargetArrayPin);

	// 7. Break 功能：设置计数器为 Keys 快照长度以跳出循环
	UK2Node_CallArrayFunction* BreakLength = CompilerContext.SpawnIntermediateNode<UK2Node_CallArrayFunction>(this, SourceGraph);
	BreakLength->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Length)));
	BreakLength->AllocateDefaultPins();
	UEdGraphPin* BreakLengthTargetArrayPin = BreakLength->GetTargetArrayPin();
	BreakLengthTargetArrayPin->PinType = KeysArrayPin->PinType;
	BreakLengthTargetArrayPin->PinType.PinValueType = FEdGraphTerminalType(KeysArrayPin->PinType.PinValueType);
	K2NodeHelpers::TryConnect(CompilerContext, BreakLengthTargetArrayPin, KeysArrayPin);
	BreakLength->PinConnectionListChanged(BreakLengthTargetArrayPin);

	UK2Node_AssignmentStatement* LoopCounterBreak = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	LoopCounterBreak->AllocateDefaultPins();
	K2NodeHelpers::TryConnect(CompilerContext, LoopCounterBreak->GetVariablePin(), LoopCounterPin);
	K2NodeHelpers::TryConnect(CompilerContext, LoopCounterBreak->GetValuePin(), BreakLength->GetReturnValuePin());

	UK2Node_ExecutionSequence* CompleteSequence = CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(this, SourceGraph);
	CompleteSequence->AllocateDefaultPins();
	K2NodeHelpers::TryConnect(CompilerContext, Branch->GetElsePin(), CompleteSequence->GetExecPin());
	K2NodeHelpers::TryConnect(CompilerContext, LoopCounterBreak->GetThenPin(), CompleteSequence->GetExecPin());

	// 8. 创建执行序列（循环体 -> 递增）
	UK2Node_ExecutionSequence* Sequence = CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(this, SourceGraph);
	Sequence->AllocateDefaultPins();
	K2NodeHelpers::TryConnect(CompilerContext, Sequence->GetExecPin(), Branch->GetThenPin());

	// 9. 创建递增节点
	UK2Node_CallFunction* Increment = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	Increment->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Add_IntInt)));
	Increment->AllocateDefaultPins();
	K2NodeHelpers::TryConnect(CompilerContext, Increment->FindPinChecked(TEXT("A")), LoopCounterPin);
	Increment->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("1");

	// 10. 创建赋值节点（递增后的值）
	UK2Node_AssignmentStatement* LoopCounterAssign = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	LoopCounterAssign->AllocateDefaultPins();
	K2NodeHelpers::TryConnect(CompilerContext, LoopCounterAssign->GetExecPin(), Sequence->GetThenPinGivenIndex(1));
	K2NodeHelpers::TryConnect(CompilerContext, LoopCounterAssign->GetVariablePin(), LoopCounterPin);
	K2NodeHelpers::TryConnect(CompilerContext, LoopCounterAssign->GetValuePin(), Increment->GetReturnValuePin());
	K2NodeHelpers::TryConnect(CompilerContext, LoopCounterAssign->GetThenPin(), Branch->GetExecPin());  // 循环回到分支

	// 11. 从 Keys 快照获取 Key
	UK2Node_CallArrayFunction* GetKey = CompilerContext.SpawnIntermediateNode<UK2Node_CallArrayFunction>(this, SourceGraph);
	GetKey->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Get)));
	GetKey->AllocateDefaultPins();
	UEdGraphPin* GetKeyTargetArrayPin = GetKey->GetTargetArrayPin();
	GetKeyTargetArrayPin->PinType = KeysArrayPin->PinType;
	GetKeyTargetArrayPin->PinType.PinValueType = FEdGraphTerminalType(KeysArrayPin->PinType.PinValueType);
	K2NodeHelpers::TryConnect(CompilerContext, GetKeyTargetArrayPin, KeysArrayPin);
	GetKey->PinConnectionListChanged(GetKeyTargetArrayPin);
	K2NodeHelpers::TryConnect(CompilerContext, GetKey->FindPinChecked(TEXT("Index")), LoopCounterPin);
	UEdGraphPin* KeyPin = GetKey->FindPin(TEXT("Item"), EGPD_Output);
	if (!ensureMsgf(KeyPin, TEXT("ForEachMap: 找不到 Key 引脚，中间节点重建失败")))
	{
		CompilerContext.MessageLog.Warning(*LOCTEXT("ForEachMap_NoKeyPin", "警告：[ForEachMap] 节点 @@ 找不到 Key 引脚，展开中止。").ToString(), this);
		BreakAllNodeLinks();
		return;
	}
	KeyPin->PinType = GetKeyPin()->PinType;

	// 12. 从 Values 快照获取 Value
	UK2Node_CallArrayFunction* GetValue = CompilerContext.SpawnIntermediateNode<UK2Node_CallArrayFunction>(this, SourceGraph);
	GetValue->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Get)));
	GetValue->AllocateDefaultPins();
	UEdGraphPin* GetValueTargetArrayPin = GetValue->GetTargetArrayPin();
	GetValueTargetArrayPin->PinType = ValuesArrayPin->PinType;
	GetValueTargetArrayPin->PinType.PinValueType = FEdGraphTerminalType(ValuesArrayPin->PinType.PinValueType);
	K2NodeHelpers::TryConnect(CompilerContext, GetValueTargetArrayPin, ValuesArrayPin);
	GetValue->PinConnectionListChanged(GetValueTargetArrayPin);
	K2NodeHelpers::TryConnect(CompilerContext, GetValue->FindPinChecked(TEXT("Index")), LoopCounterPin);
	UEdGraphPin* ValuePin = GetValue->FindPin(TEXT("Item"), EGPD_Output);
	if (!ensureMsgf(ValuePin, TEXT("ForEachMap: 找不到 Value 引脚，中间节点重建失败")))
	{
		CompilerContext.MessageLog.Warning(*LOCTEXT("ForEachMap_NoValuePin", "警告：[ForEachMap] 节点 @@ 找不到 Value 引脚，展开中止。").ToString(), this);
		BreakAllNodeLinks();
		return;
	}
	ValuePin->PinType = GetValuePin()->PinType;

	// 13. 最后统一移动所有外部连接（参考智能排序模式）
	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *MapKeys->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*GetLoopBodyPin(), *Sequence->GetThenPinGivenIndex(0));
	CompilerContext.MovePinLinksToIntermediate(*GetBreakPin(), *LoopCounterBreak->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*GetCompletedPin(), *CompleteSequence->GetThenPinGivenIndex(0));
	CompilerContext.MovePinLinksToIntermediate(*GetKeyPin(), *KeyPin);
	CompilerContext.MovePinLinksToIntermediate(*GetValuePin(), *ValuePin);
	CompilerContext.MovePinLinksToIntermediate(*GetIndexPin(), *LoopCounterPin);

	// 14. 断开原节点所有链接
	K2NodeHelpers::EndExpandNode(this);
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintSystem

void UK2Node_ForEachMap::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	K2NodeHelpers::RegisterNode<UK2Node_ForEachMap>(ActionRegistrar);
}

void UK2Node_ForEachMap::PostReconstructNode()
{
	Super::PostReconstructNode();

	// 【修复】仅在有连接时才传播类型，避免重载时丢失已序列化的类型信息
	// 参考 UE 源码 K2Node_GetArrayItem::PostReconstructNode 实现
	UEdGraphPin* MapPin = GetMapPin();
	UEdGraphPin* KeyPin = GetKeyPin();
	UEdGraphPin* ValuePin = GetValuePin();
	
	if (MapPin->LinkedTo.Num() > 0 || KeyPin->LinkedTo.Num() > 0 || ValuePin->LinkedTo.Num() > 0)
	{
		PropagatePinType();
	}
	else
	{
		// 【额外修复】无连接时，尝试从已确定类型的引脚同步到Wildcard引脚
		bool bMapKeyIsWildcard = (MapPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);
		bool bMapValueIsWildcard = (MapPin->PinType.PinValueType.TerminalCategory == UEdGraphSchema_K2::PC_Wildcard);
		bool bKeyIsWildcard = (KeyPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);
		bool bValueIsWildcard = (ValuePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);
		
		// 如果有任何引脚有确定类型，尝试同步
		if (!bKeyIsWildcard || !bValueIsWildcard)
		{
			if (!bKeyIsWildcard && (bMapKeyIsWildcard || bMapValueIsWildcard))
			{
				MapPin->PinType.PinCategory = KeyPin->PinType.PinCategory;
				MapPin->PinType.PinSubCategory = KeyPin->PinType.PinSubCategory;
				MapPin->PinType.PinSubCategoryObject = KeyPin->PinType.PinSubCategoryObject;
				MapPin->PinType.ContainerType = EPinContainerType::Map;
			}
			if (!bValueIsWildcard && bMapValueIsWildcard)
			{
				MapPin->PinType.PinValueType.TerminalCategory = ValuePin->PinType.PinCategory;
				MapPin->PinType.PinValueType.TerminalSubCategory = ValuePin->PinType.PinSubCategory;
				MapPin->PinType.PinValueType.TerminalSubCategoryObject = ValuePin->PinType.PinSubCategoryObject;
			}
			GetGraph()->NotifyGraphChanged();
		}
		else if (!bMapKeyIsWildcard || !bMapValueIsWildcard)
		{
			if (!bMapKeyIsWildcard && bKeyIsWildcard)
			{
				KeyPin->PinType.PinCategory = MapPin->PinType.PinCategory;
				KeyPin->PinType.PinSubCategory = MapPin->PinType.PinSubCategory;
				KeyPin->PinType.PinSubCategoryObject = MapPin->PinType.PinSubCategoryObject;
			}
			if (!bMapValueIsWildcard && bValueIsWildcard)
			{
				ValuePin->PinType.PinCategory = MapPin->PinType.PinValueType.TerminalCategory;
				ValuePin->PinType.PinSubCategory = MapPin->PinType.PinValueType.TerminalSubCategory;
				ValuePin->PinType.PinSubCategoryObject = MapPin->PinType.PinValueType.TerminalSubCategoryObject;
			}
			GetGraph()->NotifyGraphChanged();
		}
	}
}

void UK2Node_ForEachMap::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

		PropagatePinType();
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region PinManagement

void UK2Node_ForEachMap::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	// Execute
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);

	// Map 
	UEdGraphNode::FCreatePinParams PinParams;
	PinParams.ContainerType = EPinContainerType::Map;
	PinParams.ValueTerminalType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
	PinParams.ValueTerminalType.TerminalSubCategory = NAME_None;
	PinParams.ValueTerminalType.TerminalSubCategoryObject = nullptr;
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, ForEachMapHelper::MapPinName, PinParams);

	// Loop body
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, ForEachMapHelper::LoopBodyPinName);

	// Break
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, ForEachMapHelper::BreakPinName)->PinFriendlyName = FText::FromName(ForEachMapHelper::BreakPinName);
	
	// Key
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, ForEachMapHelper::KeyPinName);

	// Value
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, ForEachMapHelper::ValuePinName);

	// Index
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Int, ForEachMapHelper::IndexPinName);

	// Completed
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then)->PinFriendlyName = FText::FromName(UEdGraphSchema_K2::PN_Completed);
}

bool UK2Node_ForEachMap::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
    if (Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason))
    {
        return true;
    }

    // 只检查 Map 引脚的连接
    if (MyPin == GetMapPin() || (OtherPin->PinType.ContainerType == EPinContainerType::Map && OtherPin->Direction != MyPin->Direction))
    {
        const UEdGraphPin* MapPin = (MyPin == GetMapPin()) ? MyPin : OtherPin;
        const UEdGraphPin* OtherMapPin = (MyPin == GetMapPin()) ? OtherPin : MyPin;

        // 确保另一个引脚也是 Map 类型
        if (OtherMapPin->PinType.ContainerType != EPinContainerType::Map)
        {
            OutReason = TEXT("目标引脚必须是 Map 类型");
            return true;
        }

        // 检查 Key 类型是否匹配（如果不是 Wildcard）
        if (MapPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard &&
            OtherMapPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard &&
            (MapPin->PinType.PinCategory != OtherMapPin->PinType.PinCategory ||
             MapPin->PinType.PinSubCategory != OtherMapPin->PinType.PinSubCategory ||
             MapPin->PinType.PinSubCategoryObject != OtherMapPin->PinType.PinSubCategoryObject))
        {
            OutReason = TEXT("Map 的 Key 类型不匹配");
            return true;
        }

        // 检查 Value 类型是否匹配（如果不是 Wildcard）
        if (MapPin->PinType.PinValueType.TerminalCategory != UEdGraphSchema_K2::PC_Wildcard &&
            OtherMapPin->PinType.PinValueType.TerminalCategory != UEdGraphSchema_K2::PC_Wildcard &&
            (MapPin->PinType.PinValueType.TerminalCategory != OtherMapPin->PinType.PinValueType.TerminalCategory ||
             MapPin->PinType.PinValueType.TerminalSubCategory != OtherMapPin->PinType.PinValueType.TerminalSubCategory ||
             MapPin->PinType.PinValueType.TerminalSubCategoryObject != OtherMapPin->PinType.PinValueType.TerminalSubCategoryObject))
        {
            OutReason = TEXT("Map 的 Value 类型不匹配");
            return true;
        }
    }

    return false;
}

UEdGraphPin* UK2Node_ForEachMap::GetLoopBodyPin() const
{
	return FindPinChecked(ForEachMapHelper::LoopBodyPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForEachMap::GetBreakPin() const
{
	return FindPinChecked(ForEachMapHelper::BreakPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForEachMap::GetCompletedPin() const
{
	return FindPinChecked(UEdGraphSchema_K2::PN_Then, EGPD_Output);
}

UEdGraphPin* UK2Node_ForEachMap::GetMapPin() const
{
	return FindPinChecked(ForEachMapHelper::MapPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_ForEachMap::GetKeyPin() const
{
	return FindPinChecked(ForEachMapHelper::KeyPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForEachMap::GetValuePin() const
{
	return FindPinChecked(ForEachMapHelper::ValuePinName, EGPD_Output);
}

UEdGraphPin* UK2Node_ForEachMap::GetIndexPin() const
{
	return FindPinChecked(ForEachMapHelper::IndexPinName, EGPD_Output);
}

void UK2Node_ForEachMap::PropagatePinType() const
{
    bool bNotifyGraphChanged = false;
    UEdGraphPin* MapPin = GetMapPin();
    UEdGraphPin* KeyPin = GetKeyPin();
    UEdGraphPin* ValuePin = GetValuePin();

	// 【修复】第一种情况：Map引脚无连接
	if (MapPin->LinkedTo.Num() == 0)
	{
		// 检查Key和Value引脚是否都无连接
		bool bAllPinsDisconnected = (KeyPin->LinkedTo.Num() == 0 && ValuePin->LinkedTo.Num() == 0);

		if (bAllPinsDisconnected)
		{
			// 检查是否已有确定的类型（从序列化数据恢复）
			bool bMapKeyIsWildcard = (MapPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);
			bool bMapValueIsWildcard = (MapPin->PinType.PinValueType.TerminalCategory == UEdGraphSchema_K2::PC_Wildcard);
			bool bKeyIsWildcard = (KeyPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);
			bool bValueIsWildcard = (ValuePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);

			if (!bMapKeyIsWildcard || !bMapValueIsWildcard || !bKeyIsWildcard || !bValueIsWildcard)
			{
				// 引脚已有确定的类型（从序列化数据恢复），保留它
				return;
			}
		}

		// 初始化 Map 引脚为 Wildcard
		FK2NodePinTypeHelpers::ResetMapPinToWildcard(MapPin);

		// 处理 Key 引脚
		if (KeyPin->LinkedTo.Num() > 0)
		{
			const UEdGraphPin* ConnectedKeyPin = KeyPin->LinkedTo[0];
			ForEachMapHelper::CopyKeyTypeToMapPin(MapPin, ConnectedKeyPin);
			KeyPin->PinType = ConnectedKeyPin->PinType;
		}
		else
		{
			FK2NodePinTypeHelpers::ResetPinToWildcard(KeyPin);
		}

		// 处理 Value 引脚
		if (ValuePin->LinkedTo.Num() > 0)
		{
			const UEdGraphPin* ConnectedValuePin = ValuePin->LinkedTo[0];
			ForEachMapHelper::CopyValueTypeToMapPin(MapPin, ConnectedValuePin);
			ValuePin->PinType = ConnectedValuePin->PinType;
		}
		else
		{
			FK2NodePinTypeHelpers::ResetPinToWildcard(ValuePin);
		}

		bNotifyGraphChanged = true;
	}

	// 第二种情况：Map引脚有连接且需要更新类型
	else if (MapPin->LinkedTo.Num() > 0)
	{
	    const UEdGraphPin* ConnectedPin = MapPin->LinkedTo[0];

	    if (ConnectedPin->PinType.ContainerType == EPinContainerType::Map)
	    {
	        bool bShouldUpdate = false;

	        // 更新 Map 引脚的容器类型
	        if (MapPin->PinType.ContainerType != EPinContainerType::Map)
	        {
	            MapPin->PinType.ContainerType = EPinContainerType::Map;
	            bShouldUpdate = true;
	        }

	        // 处理 Key 类型
	        if (KeyPin->LinkedTo.Num() > 0 && KeyPin->LinkedTo[0]->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
	        {
	            const UEdGraphPin* ConnectedKeyPin = KeyPin->LinkedTo[0];
	            ForEachMapHelper::CopyKeyTypeToMapPin(MapPin, ConnectedKeyPin);
	            KeyPin->PinType = ConnectedKeyPin->PinType;
	            bShouldUpdate = true;
	        }
	        else if (ConnectedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
	        {
	        	ForEachMapHelper::CopyKeyTypeToMapPin(MapPin, ConnectedPin);
	        	ForEachMapHelper::CopyPinType(KeyPin, ConnectedPin);
	        	bShouldUpdate = true;
	        }

	        // 处理 Value 类型
	        if (ValuePin->LinkedTo.Num() > 0 && ValuePin->LinkedTo[0]->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
	        {
	            const UEdGraphPin* ConnectedValuePin = ValuePin->LinkedTo[0];
	            ForEachMapHelper::CopyValueTypeToMapPin(MapPin, ConnectedValuePin);
	            ValuePin->PinType = ConnectedValuePin->PinType;
	            bShouldUpdate = true;
	        }
	        else if (ConnectedPin->PinType.PinValueType.TerminalCategory != UEdGraphSchema_K2::PC_Wildcard)
	        {
	            const FEdGraphPinType ValuePinType = FEdGraphPinType::GetPinTypeForTerminalType(ConnectedPin->PinType.PinValueType);
	            MapPin->PinType.PinValueType = ConnectedPin->PinType.PinValueType;
	            ValuePin->PinType = ValuePinType;
	            bShouldUpdate = true;
	        }

	        if (bShouldUpdate)
	        {
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

