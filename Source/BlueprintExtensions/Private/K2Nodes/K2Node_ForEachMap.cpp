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
#include "K2Node_CallFunction.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_TemporaryVariable.h"

// 功能库
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/BlueprintMapLibrary.h"
#include "Libraries/MapExtensionsLibrary.h"


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

	// 2. 初始化循环计数器为0
	UK2Node_AssignmentStatement* LoopCounterInitialise = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	LoopCounterInitialise->AllocateDefaultPins();
	LoopCounterInitialise->GetValuePin()->DefaultValue = TEXT("0");
	CompilerContext.GetSchema()->TryCreateConnection(LoopCounterPin, LoopCounterInitialise->GetVariablePin());

	// 3. 创建分支节点
	UK2Node_IfThenElse* Branch = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
	Branch->AllocateDefaultPins();
	CompilerContext.GetSchema()->TryCreateConnection(LoopCounterInitialise->GetThenPin(), Branch->GetExecPin());

	// 4. 创建循环条件（计数器 < Map长度）
	UK2Node_CallFunction* Condition = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	Condition->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_IntInt)));
	Condition->AllocateDefaultPins();
	CompilerContext.GetSchema()->TryCreateConnection(Condition->GetReturnValuePin(), Branch->GetConditionPin());
	CompilerContext.GetSchema()->TryCreateConnection(Condition->FindPinChecked(TEXT("A")), LoopCounterPin);

	// 5. 获取Map长度
	UK2Node_CallFunction* Length = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	Length->SetFromFunction(UBlueprintMapLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UBlueprintMapLibrary, Map_Length)));
	Length->AllocateDefaultPins();
	UEdGraphPin* LengthTargetMapPin = Length->FindPinChecked(TEXT("TargetMap"), EGPD_Input);
	LengthTargetMapPin->PinType = GetMapPin()->PinType;
	LengthTargetMapPin->PinType.PinValueType = FEdGraphTerminalType(GetMapPin()->PinType.PinValueType);
	CompilerContext.GetSchema()->TryCreateConnection(Condition->FindPinChecked(TEXT("B")), Length->GetReturnValuePin());
	CompilerContext.CopyPinLinksToIntermediate(*GetMapPin(), *LengthTargetMapPin);
	Length->PostReconstructNode();

	// 6. Break 功能：设置计数器为Map长度以跳出循环
	UK2Node_CallFunction* BreakLength = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	BreakLength->SetFromFunction(UBlueprintMapLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UBlueprintMapLibrary, Map_Length)));
	BreakLength->AllocateDefaultPins();
	UEdGraphPin* BreakLengthTargetMapPin = BreakLength->FindPinChecked(TEXT("TargetMap"), EGPD_Input);
	BreakLengthTargetMapPin->PinType = GetMapPin()->PinType;
	BreakLengthTargetMapPin->PinType.PinValueType = FEdGraphTerminalType(GetMapPin()->PinType.PinValueType);
	CompilerContext.CopyPinLinksToIntermediate(*GetMapPin(), *BreakLengthTargetMapPin);
	BreakLength->PostReconstructNode();

	UK2Node_AssignmentStatement* LoopCounterBreak = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	LoopCounterBreak->AllocateDefaultPins();
	CompilerContext.GetSchema()->TryCreateConnection(LoopCounterBreak->GetVariablePin(), LoopCounterPin);
	CompilerContext.GetSchema()->TryCreateConnection(LoopCounterBreak->GetValuePin(), BreakLength->GetReturnValuePin());

	// 7. 创建执行序列（循环体 -> 递增）
	UK2Node_ExecutionSequence* Sequence = CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(this, SourceGraph);
	Sequence->AllocateDefaultPins();
	CompilerContext.GetSchema()->TryCreateConnection(Sequence->GetExecPin(), Branch->GetThenPin());

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

	// 10. 获取Key
	UK2Node_CallFunction* GetKey = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	GetKey->SetFromFunction(UMapExtensionsLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UMapExtensionsLibrary, Map_GetKey)));
	GetKey->AllocateDefaultPins();
	UEdGraphPin* GetKeyTargetMapPin = GetKey->FindPinChecked(TEXT("TargetMap"), EGPD_Input);
	GetKeyTargetMapPin->PinType = GetMapPin()->PinType;
	GetKeyTargetMapPin->PinType.PinValueType = FEdGraphTerminalType(GetMapPin()->PinType.PinValueType);
	CompilerContext.CopyPinLinksToIntermediate(*GetMapPin(), *GetKeyTargetMapPin);
	CompilerContext.GetSchema()->TryCreateConnection(GetKey->FindPinChecked(TEXT("Index")), LoopCounterPin);
	UEdGraphPin* KeyPin = GetKey->FindPinChecked(TEXT("Key"));
	KeyPin->PinType = GetKeyPin()->PinType;
	GetKey->PostReconstructNode();

	// 11. 获取Value
	UK2Node_CallFunction* GetValue = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	GetValue->SetFromFunction(UMapExtensionsLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UMapExtensionsLibrary, Map_GetValue)));
	GetValue->AllocateDefaultPins();
	UEdGraphPin* GetValueTargetMapPin = GetValue->FindPinChecked(TEXT("TargetMap"), EGPD_Input);
	GetValueTargetMapPin->PinType = GetMapPin()->PinType;
	GetValueTargetMapPin->PinType.PinValueType = FEdGraphTerminalType(GetMapPin()->PinType.PinValueType);
	CompilerContext.GetSchema()->TryCreateConnection(GetValue->FindPinChecked(TEXT("Index")), LoopCounterPin);
	CompilerContext.CopyPinLinksToIntermediate(*GetMapPin(), *GetValueTargetMapPin);
	UEdGraphPin* ValuePin = GetValue->FindPinChecked(TEXT("Value"));
	ValuePin->PinType = GetValuePin()->PinType;
	GetValue->PostReconstructNode();

	// 12. 最后统一移动所有外部连接（参考智能排序模式）
	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *LoopCounterInitialise->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*GetLoopBodyPin(), *Sequence->GetThenPinGivenIndex(0));
	CompilerContext.MovePinLinksToIntermediate(*GetBreakPin(), *LoopCounterBreak->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*GetCompletedPin(), *Branch->GetElsePin());
	CompilerContext.MovePinLinksToIntermediate(*GetKeyPin(), *KeyPin);
	CompilerContext.MovePinLinksToIntermediate(*GetValuePin(), *ValuePin);
	CompilerContext.MovePinLinksToIntermediate(*GetIndexPin(), *LoopCounterPin);

	// 13. 断开原节点所有链接
	BreakAllNodeLinks();
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
