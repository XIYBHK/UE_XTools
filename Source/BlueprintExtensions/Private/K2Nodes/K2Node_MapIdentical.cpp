#include "K2Nodes/K2Node_MapIdentical.h"
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
#include "K2Node_CallFunction.h"

// 功能库
#include "Libraries/MapExtensionsLibrary.h"

#define LOCTEXT_NAMESPACE "XTools_K2Node_MapIdentical"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region Helper

namespace MapIdenticalHelper
{
	FName MapAPinName = TEXT("MapA");
	FName MapBPinName = TEXT("MapB");
	FName MapLibrary_MapPinName = TEXT("TargetMap");
	FName MapLibrary_KeysPinName = TEXT("Keys");
	FName MapLibrary_ValuesPinName = TEXT("Values");
	FName ArrayAPinValue = TEXT("ArrayA");
	FName ArrayBPinValue = TEXT("ArrayB");
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeAppearance

FText UK2Node_MapIdentical::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Map是否完全相同");
}

FText UK2Node_MapIdentical::GetCompactNodeTitle() const
{
	return LOCTEXT("CompactNodeTitle", "相同");
}

FText UK2Node_MapIdentical::GetTooltipText() const
{
	return LOCTEXT("TooltipText", "检查两个Map是否完全相同");
}

FText UK2Node_MapIdentical::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "XTools|Blueprint Extensions|Map");
}

FSlateIcon UK2Node_MapIdentical::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.PureFunction_16x");
	return Icon;
}

TSharedPtr<SWidget> UK2Node_MapIdentical::CreateNodeImage() const
{
	return SPinTypeSelector::ConstructPinTypeImage(GetMapAPin());
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintCompile

void UK2Node_MapIdentical::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	// 【UE 最佳实践】不调用 Super::ExpandNode()，因为基类会提前断开所有链接
	// Super::ExpandNode(CompilerContext, SourceGraph);

	// 【最佳实践 3.1】：验证输入类型
	if(GetMapAPin()->PinType != GetMapBPin()->PinType)
	{
		// 【修复】使用 Warning 避免触发 EdGraphNode.h:563 断言崩溃
		CompilerContext.MessageLog.Warning(*LOCTEXT("TypeMismatch", "MapA and MapB must be of the same type @@").ToString(), this);
		// 【最佳实践 3.1】：错误后必须调用BreakAllNodeLinks
		BreakAllNodeLinks();
		return;
	}

	// Map Identical
	UK2Node_CallFunction* MapIdentical = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	MapIdentical->SetFromFunction(UMapExtensionsLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UMapExtensionsLibrary, Map_Identical)));
	MapIdentical->AllocateDefaultPins();
	UEdGraphPin* MapAPin = MapIdentical->FindPinChecked(TEXT("MapA"), EGPD_Input);
	MapAPin->PinType = GetMapAPin()->PinType;
	MapAPin->PinType.ContainerType = EPinContainerType::Map;
	UEdGraphPin* KeysBPin = MapIdentical->FindPinChecked(TEXT("KeysB"), EGPD_Input);
	KeysBPin->PinType = GetMapAPin()->PinType;
	KeysBPin->PinType.ContainerType = EPinContainerType::Array;
	UEdGraphPin* ValueBPin = MapIdentical->FindPinChecked(TEXT("ValuesB"),EGPD_Input);
	ValueBPin->PinType = FEdGraphPinType::GetPinTypeForTerminalType(GetMapAPin()->PinType.PinValueType);
	ValueBPin->PinType.ContainerType = EPinContainerType::Array;
	
	// MapB GetKeys
	UK2Node_CallFunction* MapBGetKeys = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	MapBGetKeys->SetFromFunction(UMapExtensionsLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UMapExtensionsLibrary, Map_Keys)));
	MapBGetKeys->AllocateDefaultPins();
	GetKeysPin(MapBGetKeys)->PinType = GetMapBPin()->PinType;
	GetKeysPin(MapBGetKeys)->PinType.ContainerType = EPinContainerType::Array;

	// MapB GetValues
	UK2Node_CallFunction* MapBGetValues = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	MapBGetValues->SetFromFunction(UMapExtensionsLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UMapExtensionsLibrary, Map_Values)));
	MapBGetValues->AllocateDefaultPins();
	GetValuesPin(MapBGetValues)->PinType.PinCategory = GetMapBPin()->PinType.PinValueType.TerminalCategory;
	GetValuesPin(MapBGetValues)->PinType.PinSubCategory = GetMapBPin()->PinType.PinValueType.TerminalSubCategory;
	GetValuesPin(MapBGetValues)->PinType.PinSubCategoryObject = GetMapBPin()->PinType.PinValueType.TerminalSubCategoryObject;

	// Move MapA pin to map identical map A pin
	CompilerContext.MovePinLinksToIntermediate(*GetMapAPin(), *MapAPin);

	// Copy MapB to get keys target map pin
	CompilerContext.CopyPinLinksToIntermediate(*GetMapBPin(), *GetTargetMapPin(MapBGetKeys));

	// Copy MapB to get values target map pin
	CompilerContext.CopyPinLinksToIntermediate(*GetMapBPin(), *GetTargetMapPin(MapBGetValues));

	// Connect get map B keys return value to keys B pin of map identical internal 
	GetSchema()->TryCreateConnection(KeysBPin, MapBGetKeys->FindPinChecked(TEXT("Keys"), EGPD_Output));

	// Connect get map B values return value to values B pin of map identical internal 
	GetSchema()->TryCreateConnection(ValueBPin, MapBGetValues->FindPinChecked(TEXT("Values"), EGPD_Output));

	// Move the map identical return value to this function's return value
	CompilerContext.MovePinLinksToIntermediate(*GetReturnValuePin(), *MapIdentical->GetReturnValuePin());
	
	BreakAllNodeLinks();
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintSystem

void UK2Node_MapIdentical::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	K2NodeHelpers::RegisterNode<UK2Node_MapIdentical>(ActionRegistrar);
}

void UK2Node_MapIdentical::PostReconstructNode()
{
	Super::PostReconstructNode();

	if(GetMapAPin() && GetMapAPin()->LinkedTo.Num() > 0 && GetMapAPin()->LinkedTo[0]->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
	{
		GetMapAPin()->PinType = GetMapAPin()->LinkedTo[0]->PinType;
		GetMapAPin()->PinType.PinValueType = FEdGraphTerminalType(GetMapAPin()->PinType.PinValueType);

		GetMapBPin()->PinType = GetMapAPin()->LinkedTo[0]->PinType;
		GetMapBPin()->PinType.PinValueType = FEdGraphTerminalType(GetMapAPin()->PinType.PinValueType);
	}
	else if(GetMapBPin() && GetMapBPin()->LinkedTo.Num() > 0 && GetMapBPin()->LinkedTo[0]->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
	{
		GetMapBPin()->PinType = GetMapBPin()->LinkedTo[0]->PinType;
		GetMapBPin()->PinType.PinValueType = FEdGraphTerminalType(GetMapBPin()->PinType.PinValueType);

		GetMapAPin()->PinType = GetMapBPin()->LinkedTo[0]->PinType;
		GetMapAPin()->PinType.PinValueType = FEdGraphTerminalType(GetMapBPin()->PinType.PinValueType);
	}
}

void UK2Node_MapIdentical::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	if(Pin == GetMapAPin() || Pin == GetMapBPin())
	{
		if(GetMapAPin()->LinkedTo.Num() == 0 && GetMapBPin()->LinkedTo.Num() == 0)
		{
			GetMapAPin()->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
			GetMapAPin()->PinType.PinSubCategory = NAME_None;
			GetMapAPin()->PinType.PinSubCategoryObject = nullptr;
			GetMapAPin()->PinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
			GetMapAPin()->PinType.PinValueType.TerminalSubCategory = NAME_None;
			GetMapAPin()->PinType.PinValueType.TerminalSubCategoryObject = nullptr;
			
			GetMapBPin()->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
			GetMapBPin()->PinType.PinSubCategory = NAME_None;
			GetMapBPin()->PinType.PinSubCategoryObject = nullptr;
			GetMapBPin()->PinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
			GetMapBPin()->PinType.PinValueType.TerminalSubCategory = NAME_None;
			GetMapBPin()->PinType.PinValueType.TerminalSubCategoryObject = nullptr;
		}
		else
		{
			if(Pin == GetMapAPin())
			{
				if(GetMapAPin()->LinkedTo.Num() > 0 && GetMapAPin()->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard && GetMapAPin()->LinkedTo[0]->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
				{
					GetMapAPin()->PinType = GetMapAPin()->LinkedTo[0]->PinType;
					GetMapAPin()->PinType.PinValueType = FEdGraphTerminalType(GetMapAPin()->PinType.PinValueType);
					
					GetMapBPin()->PinType = GetMapAPin()->LinkedTo[0]->PinType;
					GetMapBPin()->PinType.PinValueType = FEdGraphTerminalType(GetMapAPin()->PinType.PinValueType);
				}
			}
			else if(Pin == GetMapBPin())
			{
				if(GetMapBPin()->LinkedTo.Num() > 0 && GetMapBPin()->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard && GetMapBPin()->LinkedTo[0]->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
				{
					GetMapBPin()->PinType = GetMapBPin()->LinkedTo[0]->PinType;
					GetMapBPin()->PinType.PinValueType = FEdGraphTerminalType(GetMapBPin()->PinType.PinValueType);
					
					GetMapAPin()->PinType = GetMapBPin()->LinkedTo[0]->PinType;
					GetMapAPin()->PinType.PinValueType = FEdGraphTerminalType(GetMapBPin()->PinType.PinValueType);
				}
			}
		}
		
		GetGraph()->NotifyGraphChanged();
	}
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region PinManagement

void UK2Node_MapIdentical::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	// Map pin params
	UEdGraphNode::FCreatePinParams PinParams;
	PinParams.ContainerType = EPinContainerType::Map;
	PinParams.ValueTerminalType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
	PinParams.ValueTerminalType.TerminalSubCategory = NAME_None;
	PinParams.ValueTerminalType.TerminalSubCategoryObject = nullptr;
	
	// MapA
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, MapIdenticalHelper::MapAPinName, PinParams);

	// MapB
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, MapIdenticalHelper::MapBPinName, PinParams);

	// Return type
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Boolean, UEdGraphSchema_K2::PN_ReturnValue);
}

UEdGraphPin* UK2Node_MapIdentical::GetMapAPin() const
{
	return FindPinChecked(MapIdenticalHelper::MapAPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_MapIdentical::GetMapBPin() const
{
	return FindPinChecked(MapIdenticalHelper::MapBPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_MapIdentical::GetReturnValuePin() const
{
	return FindPinChecked(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output);
}

UEdGraphPin* UK2Node_MapIdentical::GetTargetMapPin(const UK2Node_CallFunction* Function) const
{
	return Function->FindPinChecked(MapIdenticalHelper::MapLibrary_MapPinName, EGPD_Input);
}

UEdGraphPin* UK2Node_MapIdentical::GetKeysPin(const UK2Node_CallFunction* Function) const
{
	return Function->FindPinChecked(MapIdenticalHelper::MapLibrary_KeysPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_MapIdentical::GetValuesPin(const UK2Node_CallFunction* Function) const
{
	return Function->FindPinChecked(MapIdenticalHelper::MapLibrary_ValuesPinName, EGPD_Output);
}

UEdGraphPin* UK2Node_MapIdentical::GetArrayAPin(const UK2Node_CallFunction* Function) const
{
	return Function->FindPinChecked(MapIdenticalHelper::ArrayAPinValue, EGPD_Input);
}

UEdGraphPin* UK2Node_MapIdentical::GetArrayBPin(const UK2Node_CallFunction* Function) const
{
	return Function->FindPinChecked(MapIdenticalHelper::ArrayBPinValue, EGPD_Input);
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#undef LOCTEXT_NAMESPACE
