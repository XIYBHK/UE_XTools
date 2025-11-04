#include "K2Nodes/K2Node_MapAppend.h"

// 编辑器
#include "EdGraphSchema_K2.h"

// 蓝图系统
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "SPinTypeSelector.h"

// 编译器-ExpandNode相关
#include "KismetCompiler.h"

// 节点
#include "K2Node_CallFunction.h"
#include "K2Nodes/K2Node_ForEachMap.h"

// 功能库
#include "Kismet/BlueprintMapLibrary.h"

#define LOCTEXT_NAMESPACE "XTools_K2Node_MapAppend"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region Helper

namespace MapAppendHelper
{
	FName TargetMapPin = TEXT("TargetMap");
	FName SourceMapPin = TEXT("SourceMap");
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeAppearance

FText UK2Node_MapAppend::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "合并Map");
}

FText UK2Node_MapAppend::GetCompactNodeTitle() const
{
	return LOCTEXT("CompactNodeTitle", "合并");
}

FText UK2Node_MapAppend::GetTooltipText() const
{
	return LOCTEXT("TooltipText", "将源Map合并到目标Map\n如果键已存在则会被覆盖");
}

FText UK2Node_MapAppend::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "XTools|Blueprint Extensions|Map");
}

FSlateIcon UK2Node_MapAppend::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.PureFunction_16x");
	return Icon;
}

TSharedPtr<SWidget> UK2Node_MapAppend::CreateNodeImage() const
{
	return SPinTypeSelector::ConstructPinTypeImage(GetTargetMapPin());
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintCompile

void UK2Node_MapAppend::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	// 【最佳实践 3.1】：验证输入类型
	if(GetTargetMapPin()->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard || GetSourceMapPin()->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
	{
		// 【最佳实践 4.3】：使用LOCTEXT本地化
		CompilerContext.MessageLog.Error(*LOCTEXT("InvalidMapType", "Target map and source map pins must be of a valid type @@").ToString(), this);
		// 【最佳实践 3.1】：错误后必须调用BreakAllNodeLinks
		BreakAllNodeLinks();
		return;
	}

	// Map for each loop to add map elements
	UK2Node_ForEachMap* MapForEach = CompilerContext.SpawnIntermediateNode<UK2Node_ForEachMap>(this, SourceGraph);
	MapForEach->AllocateDefaultPins();
	MapForEach->GetMapPin()->PinType = GetTargetMapPin()->PinType;
	MapForEach->GetMapPin()->PinType.ContainerType = EPinContainerType::Map;
	MapForEach->GetKeyPin()->PinType = GetTargetMapPin()->PinType;
	MapForEach->GetKeyPin()->PinType.ContainerType = EPinContainerType::None;
	MapForEach->GetValuePin()->PinType = FEdGraphPinType::GetPinTypeForTerminalType(GetTargetMapPin()->PinType.PinValueType);
	MapForEach->GetValuePin()->PinType.ContainerType = EPinContainerType::None;

	// Map add function
	UK2Node_CallFunction* AddElement = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	AddElement->SetFromFunction(UBlueprintMapLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UBlueprintMapLibrary, Map_Add)));
	AddElement->AllocateDefaultPins();
	UEdGraphPin* AddElemTargetMapPin = AddElement->FindPinChecked(MapAppendHelper::TargetMapPin, EGPD_Input);
	AddElemTargetMapPin->PinType = GetTargetMapPin()->PinType;
	AddElemTargetMapPin->PinType.ContainerType = EPinContainerType::Map;
	UEdGraphPin* AddElemKeyPin = AddElement->FindPinChecked(TEXT("Key"), EGPD_Input);
	AddElemKeyPin->PinType = GetTargetMapPin()->PinType;
	AddElemKeyPin->PinType.ContainerType = EPinContainerType::None;
	UEdGraphPin* AddElemValuePin = AddElement->FindPinChecked(TEXT("Value"), EGPD_Input);
	AddElemValuePin->PinType = FEdGraphPinType::GetPinTypeForTerminalType(GetTargetMapPin()->PinType.PinValueType);
	AddElemValuePin->PinType.ContainerType = EPinContainerType::None;

	// Connect exec with map for each exec
	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *MapForEach->GetExecPin());

	// Connect the target map with the add element map pin
	CompilerContext.MovePinLinksToIntermediate(*GetTargetMapPin(), *AddElemTargetMapPin);
	
	// Connect source map with map for each map pin
	CompilerContext.MovePinLinksToIntermediate(*GetSourceMapPin(), *MapForEach->GetMapPin());
	
	// Connect then with map for each completed
	CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *MapForEach->GetCompletedPin());

	// Connect loop body with add elem exec
	GetSchema()->TryCreateConnection(AddElement->GetExecPin(), MapForEach->GetLoopBodyPin());

	// Connect get key return pin to add element key pin
	GetSchema()->TryCreateConnection(AddElement->FindPinChecked(TEXT("Key"), EGPD_Input), MapForEach->GetKeyPin());

	// Connect get value return pin to add element value pin
	GetSchema()->TryCreateConnection(AddElement->FindPinChecked(TEXT("Value"), EGPD_Input), MapForEach->GetValuePin());

	BreakAllNodeLinks();
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintSystem

void UK2Node_MapAppend::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	const UClass* Action = GetClass();

	if (ActionRegistrar.IsOpenForRegistration(Action))
	{
		UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(GetClass());
		check(Spawner != nullptr);

		ActionRegistrar.AddBlueprintAction(Action, Spawner);
	}	
}

void UK2Node_MapAppend::PostReconstructNode()
{
	Super::PostReconstructNode();
	
	if(GetTargetMapPin() && GetTargetMapPin()->LinkedTo.Num() > 0 && GetTargetMapPin()->LinkedTo[0]->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
	{
		GetTargetMapPin()->PinType = GetTargetMapPin()->LinkedTo[0]->PinType;
		GetTargetMapPin()->PinType.PinValueType = FEdGraphTerminalType(GetTargetMapPin()->PinType.PinValueType);

		GetSourceMapPin()->PinType = GetTargetMapPin()->LinkedTo[0]->PinType;
		GetSourceMapPin()->PinType.PinValueType = FEdGraphTerminalType(GetTargetMapPin()->PinType.PinValueType);
	}
	else if(GetSourceMapPin() && GetSourceMapPin()->LinkedTo.Num() > 0 && GetSourceMapPin()->LinkedTo[0]->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
	{
		GetSourceMapPin()->PinType = GetSourceMapPin()->LinkedTo[0]->PinType;
		GetSourceMapPin()->PinType.PinValueType = FEdGraphTerminalType(GetSourceMapPin()->PinType.PinValueType);

		GetTargetMapPin()->PinType = GetSourceMapPin()->LinkedTo[0]->PinType;
		GetTargetMapPin()->PinType.PinValueType = FEdGraphTerminalType(GetSourceMapPin()->PinType.PinValueType);
	}
}

void UK2Node_MapAppend::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	
	if(Pin == GetTargetMapPin() || Pin == GetSourceMapPin())
	{
		if(GetTargetMapPin()->LinkedTo.Num() == 0 && GetSourceMapPin()->LinkedTo.Num() == 0)
		{
			GetTargetMapPin()->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
			GetTargetMapPin()->PinType.PinSubCategory = NAME_None;
			GetTargetMapPin()->PinType.PinSubCategoryObject = nullptr;
			GetTargetMapPin()->PinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
			GetTargetMapPin()->PinType.PinValueType.TerminalSubCategory = NAME_None;
			GetTargetMapPin()->PinType.PinValueType.TerminalSubCategoryObject = nullptr;
			
			GetSourceMapPin()->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
			GetSourceMapPin()->PinType.PinSubCategory = NAME_None;
			GetSourceMapPin()->PinType.PinSubCategoryObject = nullptr;
			GetSourceMapPin()->PinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
			GetSourceMapPin()->PinType.PinValueType.TerminalSubCategory = NAME_None;
			GetSourceMapPin()->PinType.PinValueType.TerminalSubCategoryObject = nullptr;
		}
		else
		{
			if(Pin == GetTargetMapPin())
			{
				if(GetTargetMapPin()->LinkedTo.Num() > 0 && GetTargetMapPin()->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard && GetTargetMapPin()->LinkedTo[0]->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
				{
					GetTargetMapPin()->PinType = GetTargetMapPin()->LinkedTo[0]->PinType;
					GetTargetMapPin()->PinType.PinValueType = FEdGraphTerminalType(GetTargetMapPin()->PinType.PinValueType);
					
					GetSourceMapPin()->PinType = GetTargetMapPin()->LinkedTo[0]->PinType;
					GetSourceMapPin()->PinType.PinValueType = FEdGraphTerminalType(GetTargetMapPin()->PinType.PinValueType);
				}
			}
			else if(Pin == GetSourceMapPin())
			{
				if(GetSourceMapPin()->LinkedTo.Num() > 0 && GetSourceMapPin()->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard && GetSourceMapPin()->LinkedTo[0]->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
				{
					GetSourceMapPin()->PinType = GetSourceMapPin()->LinkedTo[0]->PinType;
					GetSourceMapPin()->PinType.PinValueType = FEdGraphTerminalType(GetSourceMapPin()->PinType.PinValueType);
					
					GetTargetMapPin()->PinType = GetSourceMapPin()->LinkedTo[0]->PinType;
					GetTargetMapPin()->PinType.PinValueType = FEdGraphTerminalType(GetSourceMapPin()->PinType.PinValueType);
				}
			}
		}
		
		GetGraph()->NotifyGraphChanged();
	}
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region PinManagement

void UK2Node_MapAppend::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	// Map pin params
	UEdGraphNode::FCreatePinParams PinParams;
	PinParams.ContainerType = EPinContainerType::Map;
	PinParams.ValueTerminalType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
	PinParams.ValueTerminalType.TerminalSubCategory = NAME_None;
	PinParams.ValueTerminalType.TerminalSubCategoryObject = nullptr;

	// Exec pin
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);

	// Target map pin
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, MapAppendHelper::TargetMapPin, PinParams);

	// Source map pin
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, MapAppendHelper::SourceMapPin, PinParams);

	// Then pin
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
}

UEdGraphPin* UK2Node_MapAppend::GetTargetMapPin() const
{
	return FindPinChecked(MapAppendHelper::TargetMapPin, EGPD_Input);
}

UEdGraphPin* UK2Node_MapAppend::GetSourceMapPin() const
{
	return FindPinChecked(MapAppendHelper::SourceMapPin, EGPD_Input);
}

UEdGraphPin* UK2Node_MapAppend::GetThenPin() const
{
	return FindPinChecked(UEdGraphSchema_K2::PN_Then, EGPD_Output);
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#undef LOCTEXT_NAMESPACE