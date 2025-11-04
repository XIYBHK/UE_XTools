#include "K2Nodes/K2Node_ForEachMap.h"

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

struct ForEachMapExpandNodeHelper
{
	UEdGraphPin* StartLoopExecInPin;
	UEdGraphPin* InsideLoopExecOutPin;
	UEdGraphPin* LoopCompleteOutExecPin;

	UEdGraphPin* ArrayIndexOutPin;
	UEdGraphPin* LoopCounterOutPin;
	// for(LoopCounter = 0; LoopCounter < LoopCounterLimit; ++LoopCounter)
	UEdGraphPin* LoopCounterLimitInPin;

	ForEachMapExpandNodeHelper()
		: StartLoopExecInPin(nullptr)
		, InsideLoopExecOutPin(nullptr)
		, LoopCompleteOutExecPin(nullptr)
		, ArrayIndexOutPin(nullptr)
		, LoopCounterOutPin(nullptr)
		, LoopCounterLimitInPin(nullptr)
	{ }

	bool BuildLoop(UK2Node* Node, FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
	{
		const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
		check(Node && SourceGraph && Schema);

		bool bResult = true;
    
		// TODO: 实现循环构建逻辑
    
		return bResult;
	}
};

namespace ForEachMapHelper
{
	const FName LoopBodyPinName = FName("Loop Body") ;
	const FName BreakPinName = FName("Loop Body") ;
	const FName MapPinName = FName("Map");
	const FName KeyPinName = FName("Key");
	const FName ValuePinName = FName("Value");
	const FName IndexPinName = FName("Index");
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
	Super::ExpandNode(CompilerContext, SourceGraph);

	// 【最佳实践 3.1】：验证必要连接
	UEdGraphPin* MapPin = GetMapPin();
	if (MapPin->LinkedTo.Num() == 0)
	{
		// 【最佳实践 4.3】：使用LOCTEXT本地化
		CompilerContext.MessageLog.Error(*LOCTEXT("MapNotConnected", "Map pin must be connected @@").ToString(), this);
		// 【最佳实践 3.1】：错误后必须调用BreakAllNodeLinks
		BreakAllNodeLinks();
		return;
	}

	// 【最佳实践 3.1】：验证类型有效性
	if (MapPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard ||
		MapPin->PinType.PinValueType.TerminalCategory == UEdGraphSchema_K2::PC_Wildcard)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("InvalidMapType", "Map Key and Value types must be valid @@").ToString(), this);
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

	if(!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("InitCounterFailed", "Could not connect initialise loop counter node @@").ToString(), this);
	
	// Do loop branch
	UK2Node_IfThenElse* Branch = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
	Branch->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(LoopCounterInitialise->GetThenPin(), Branch->GetExecPin());
	UEdGraphPin* BranchElsePin = Branch->GetElsePin();
	check(BranchElsePin);

	if(!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("BranchFailed", "Could not connect branch node @@").ToString(), this);
	
	// Do loop condition
	UK2Node_CallFunction* Condition = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph); 
	Condition->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_IntInt)));
	Condition->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(Condition->GetReturnValuePin(), Branch->GetConditionPin());
	bResult &= Schema->TryCreateConnection(Condition->FindPinChecked(TEXT("A")), LoopCounterPin);
	UEdGraphPin* LoopConditionBPin = Condition->FindPinChecked(TEXT("B"));
	check(LoopConditionBPin);

	if(!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("ConditionFailed", "Could not connect loop condition node @@").ToString(), this);

	// Length of map 
	UK2Node_CallFunction* Length = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	Length->SetFromFunction(UBlueprintMapLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UBlueprintMapLibrary, Map_Length)));
	Length->AllocateDefaultPins();
	UEdGraphPin* LengthTargetMapPin = Length->FindPinChecked(TEXT("TargetMap"), EGPD_Input);
	LengthTargetMapPin->PinType= GetMapPin()->PinType;
	LengthTargetMapPin->PinType.PinValueType = FEdGraphTerminalType(GetMapPin()->PinType.PinValueType);
	bResult &= Schema->TryCreateConnection(LoopConditionBPin, Length->GetReturnValuePin());
	CompilerContext.CopyPinLinksToIntermediate(*GetMapPin(), *LengthTargetMapPin);
	Length->PostReconstructNode();

	if(!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("LengthFailed", "Could not connect length node @@").ToString(), this);
	
	// Break 相关节点
	UK2Node_CallFunction* BreakLength = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	BreakLength->SetFromFunction(UBlueprintMapLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UBlueprintMapLibrary, Map_Length)));
	BreakLength->AllocateDefaultPins();
	UEdGraphPin* BreakLengthTargetMapPin = BreakLength->FindPinChecked(TEXT("TargetMap"), EGPD_Input);
	BreakLengthTargetMapPin->PinType = GetMapPin()->PinType;
	BreakLengthTargetMapPin->PinType.PinValueType = FEdGraphTerminalType(GetMapPin()->PinType.PinValueType);
	CompilerContext.CopyPinLinksToIntermediate(*GetMapPin(), *BreakLengthTargetMapPin);
	BreakLength->PostReconstructNode();

	// 创建赋值节点，执行 Break 时设置循环计数器为 Map 长度
	UK2Node_AssignmentStatement* LoopCounterBreak = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	LoopCounterBreak->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(LoopCounterBreak->GetVariablePin(), LoopCounterPin);
	bResult &= Schema->TryCreateConnection(LoopCounterBreak->GetValuePin(), BreakLength->GetReturnValuePin());
	UEdGraphPin* LoopCounterBreakExecPin = LoopCounterBreak->GetExecPin();
	check(LoopCounterBreakExecPin);
	if(!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("BreakFailed", "Could not connect break nodes @@").ToString(), this);
	
	// Sequence
	UK2Node_ExecutionSequence* Sequence = CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(this, SourceGraph);
	Sequence->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(Sequence->GetExecPin(), Branch->GetThenPin());
	UEdGraphPin* SequenceThen0Pin = Sequence->GetThenPinGivenIndex(0);
	UEdGraphPin* SequenceThen1Pin = Sequence->GetThenPinGivenIndex(1);
	check(SequenceThen0Pin);
	check(SequenceThen1Pin);

	if(!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("SequenceFailed", "Could not connect sequence node @@").ToString(), this);
	
	// Loop Counter increment
	UK2Node_CallFunction* Increment = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph); 
	Increment->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Add_IntInt)));
	Increment->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(Increment->FindPinChecked(TEXT("A")), LoopCounterPin);
	Increment->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("1");

	if(!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("IncrementFailed", "Could not connect loop counter increment node @@").ToString(), this);
	
	// Loop Counter assigned
	UK2Node_AssignmentStatement* LoopCounterAssign = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	LoopCounterAssign->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(LoopCounterAssign->GetExecPin(), SequenceThen1Pin);
	bResult &= Schema->TryCreateConnection(LoopCounterAssign->GetVariablePin(), LoopCounterPin);
	bResult &= Schema->TryCreateConnection(LoopCounterAssign->GetValuePin(), Increment->GetReturnValuePin());
	bResult &= Schema->TryCreateConnection(LoopCounterAssign->GetThenPin(), Branch->GetExecPin());
	
	if(!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("AssignmentFailed", "Could not connect loop counter assignment node @@").ToString(), this);

	// Get key
	UK2Node_CallFunction* GetKey =CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	GetKey->SetFromFunction(UMapExtensionsLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UMapExtensionsLibrary, Map_GetKey)));
	GetKey->AllocateDefaultPins();
	UEdGraphPin* GetKeyTargetMapPin = GetKey->FindPinChecked(TEXT("TargetMap"), EGPD_Input);
	GetKeyTargetMapPin->PinType = GetMapPin()->PinType;
	GetKeyTargetMapPin->PinType.PinValueType = FEdGraphTerminalType(GetMapPin()->PinType.PinValueType);
	CompilerContext.CopyPinLinksToIntermediate(*GetMapPin(), *GetKeyTargetMapPin);
	bResult &= Schema->TryCreateConnection(GetKey->FindPinChecked(TEXT("Index")), LoopCounterPin);
	UEdGraphPin* KeyPin = GetKey->FindPinChecked(TEXT("Key"));
	KeyPin->PinType = GetKeyPin()->PinType;
	check(KeyPin);
	GetKey->PostReconstructNode();
	
	if(!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("GetKeyFailed", "Could not connect get map key node @@").ToString(), this);

	// Get value
	UK2Node_CallFunction* GetValue = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	GetValue->SetFromFunction(UMapExtensionsLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UMapExtensionsLibrary, Map_GetValue)));
	GetValue->AllocateDefaultPins();
	UEdGraphPin* GetValueTargetMapPin = GetValue->FindPinChecked(TEXT("TargetMap"), EGPD_Input);
	GetValueTargetMapPin->PinType = GetMapPin()->PinType;
	GetValueTargetMapPin->PinType.PinValueType = FEdGraphTerminalType(GetMapPin()->PinType.PinValueType);
	bResult &= Schema->TryCreateConnection(GetValue->FindPinChecked(TEXT("Index")), LoopCounterPin);
	CompilerContext.CopyPinLinksToIntermediate(*GetMapPin(), *GetValueTargetMapPin);
	UEdGraphPin* ValuePin = GetValue->FindPinChecked(TEXT("Value"));
	ValuePin->PinType = GetValuePin()->PinType;
	check(ValuePin);
	GetValue->PostReconstructNode();
	
	if(!bResult) CompilerContext.MessageLog.Error(*LOCTEXT("GetValueFailed", "Could not connect get map value node @@").ToString(), this);

	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *LoopCounterInitialiseExecPin);
	CompilerContext.MovePinLinksToIntermediate(*GetLoopBodyPin(), *SequenceThen0Pin);
	CompilerContext.MovePinLinksToIntermediate(*GetBreakPin(), *LoopCounterBreakExecPin);
	CompilerContext.MovePinLinksToIntermediate(*GetCompletedPin(), *BranchElsePin);
	CompilerContext.MovePinLinksToIntermediate(*GetKeyPin(), *KeyPin);
	CompilerContext.MovePinLinksToIntermediate(*GetValuePin(), *ValuePin);
	CompilerContext.MovePinLinksToIntermediate(*GetIndexPin(), *LoopCounterPin);
	
	BreakAllNodeLinks();
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintSystem

void UK2Node_ForEachMap::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	const UClass* Action = GetClass();

	if (ActionRegistrar.IsOpenForRegistration(Action))
	{
		UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(GetClass());
		check(Spawner != nullptr);

		ActionRegistrar.AddBlueprintAction(Action, Spawner);
	}	
}

void UK2Node_ForEachMap::PostReconstructNode()
{
	Super::PostReconstructNode();

	PropagatePinType();
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

	// 第一种情况：Map引脚无连接
	if (MapPin->LinkedTo.Num() == 0)
	{
		// 初始化 Map 引脚为 Wildcard
		MapPin->PinType.ContainerType = EPinContainerType::Map;
		MapPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
		MapPin->PinType.PinSubCategory = NAME_None;
		MapPin->PinType.PinSubCategoryObject = nullptr;
		MapPin->PinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
		MapPin->PinType.PinValueType.TerminalSubCategory = NAME_None;
		MapPin->PinType.PinValueType.TerminalSubCategoryObject = nullptr;

		// 处理 Key 引脚
		if (KeyPin->LinkedTo.Num() > 0)
		{
			const UEdGraphPin* ConnectedKeyPin = KeyPin->LinkedTo[0];
			MapPin->PinType.PinCategory = ConnectedKeyPin->PinType.PinCategory;
			MapPin->PinType.PinSubCategory = ConnectedKeyPin->PinType.PinSubCategory;
			MapPin->PinType.PinSubCategoryObject = ConnectedKeyPin->PinType.PinSubCategoryObject;
			KeyPin->PinType = ConnectedKeyPin->PinType;
		}
		else
		{
			// 重置未连接的 Key 引脚
			KeyPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
			KeyPin->PinType.PinSubCategory = NAME_None;
			KeyPin->PinType.PinSubCategoryObject = nullptr;
		}

		// 处理 Value 引脚
		if (ValuePin->LinkedTo.Num() > 0)
		{
			const UEdGraphPin* ConnectedValuePin = ValuePin->LinkedTo[0];
			MapPin->PinType.PinValueType.TerminalCategory = ConnectedValuePin->PinType.PinCategory;
			MapPin->PinType.PinValueType.TerminalSubCategory = ConnectedValuePin->PinType.PinSubCategory;
			MapPin->PinType.PinValueType.TerminalSubCategoryObject = ConnectedValuePin->PinType.PinSubCategoryObject;
			ValuePin->PinType = ConnectedValuePin->PinType;
		}
		else
		{
			// 重置未连接的 Value 引脚
			ValuePin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
			ValuePin->PinType.PinSubCategory = NAME_None;
			ValuePin->PinType.PinSubCategoryObject = nullptr;
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
	            // 使用已连接的 Key 引脚类型
	            const UEdGraphPin* ConnectedKeyPin = KeyPin->LinkedTo[0];
	            MapPin->PinType.PinCategory = ConnectedKeyPin->PinType.PinCategory;
	            MapPin->PinType.PinSubCategory = ConnectedKeyPin->PinType.PinSubCategory;
	            MapPin->PinType.PinSubCategoryObject = ConnectedKeyPin->PinType.PinSubCategoryObject;
	            KeyPin->PinType = ConnectedKeyPin->PinType;
	            bShouldUpdate = true;
	        }
	        else if (ConnectedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
	        {
	        	// 使用外部 Map 的 Key 类型（而不是整个 Map 的类型）
	        	MapPin->PinType.PinCategory = ConnectedPin->PinType.PinCategory;
	        	MapPin->PinType.PinSubCategory = ConnectedPin->PinType.PinSubCategory;
	        	MapPin->PinType.PinSubCategoryObject = ConnectedPin->PinType.PinSubCategoryObject;
    
	        	KeyPin->PinType.PinCategory = ConnectedPin->PinType.PinCategory;
	        	KeyPin->PinType.PinSubCategory = ConnectedPin->PinType.PinSubCategory;
	        	KeyPin->PinType.PinSubCategoryObject = ConnectedPin->PinType.PinSubCategoryObject;
	        	bShouldUpdate = true;
	        }

	        // 处理 Value 类型
	        if (ValuePin->LinkedTo.Num() > 0 && ValuePin->LinkedTo[0]->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
	        {
	            // 使用已连接的 Value 引脚类型
	            const UEdGraphPin* ConnectedValuePin = ValuePin->LinkedTo[0];
	            MapPin->PinType.PinValueType.TerminalCategory = ConnectedValuePin->PinType.PinCategory;
	            MapPin->PinType.PinValueType.TerminalSubCategory = ConnectedValuePin->PinType.PinSubCategory;
	            MapPin->PinType.PinValueType.TerminalSubCategoryObject = ConnectedValuePin->PinType.PinSubCategoryObject;
	            ValuePin->PinType = ConnectedValuePin->PinType;
	            bShouldUpdate = true;
	        }
	        else if (ConnectedPin->PinType.PinValueType.TerminalCategory != UEdGraphSchema_K2::PC_Wildcard)
	        {
	            // 使用外部 Map 的 Value 类型
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
