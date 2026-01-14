/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "K2NodePinTypeHelpers.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"

void FK2NodePinTypeHelpers::ResetPinToWildcard(UEdGraphPin* Pin, EPinContainerType ContainerType)
{
	if (!Pin)
	{
		return;
	}

	Pin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
	Pin->PinType.PinSubCategory = NAME_None;
	Pin->PinType.PinSubCategoryObject = nullptr;
	Pin->PinType.ContainerType = ContainerType;
}

void FK2NodePinTypeHelpers::ResetMapPinToWildcard(UEdGraphPin* MapPin)
{
	if (!MapPin)
	{
		return;
	}

	MapPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
	MapPin->PinType.PinSubCategory = NAME_None;
	MapPin->PinType.PinSubCategoryObject = nullptr;
	MapPin->PinType.ContainerType = EPinContainerType::Map;

	// 重置 Map 的 Value 类型
	FEdGraphTerminalType WildcardTerminal;
	WildcardTerminal.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
	WildcardTerminal.TerminalSubCategory = NAME_None;
	WildcardTerminal.TerminalSubCategoryObject = nullptr;
	MapPin->PinType.PinValueType = WildcardTerminal;
}

bool FK2NodePinTypeHelpers::GetMapKeyType(const UEdGraphPin* MapPin, FEdGraphPinType& OutKeyType)
{
	if (!MapPin || MapPin->PinType.ContainerType != EPinContainerType::Map)
	{
		return false;
	}

	// 如果 Map 引脚有连接，从连接获取类型
	if (MapPin->LinkedTo.Num() > 0)
	{
		const UEdGraphPin* ConnectedPin = MapPin->LinkedTo[0];
		if (ConnectedPin->PinType.ContainerType == EPinContainerType::Map &&
			ConnectedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
		{
			OutKeyType.PinCategory = ConnectedPin->PinType.PinCategory;
			OutKeyType.PinSubCategory = ConnectedPin->PinType.PinSubCategory;
			OutKeyType.PinSubCategoryObject = ConnectedPin->PinType.PinSubCategoryObject;
			OutKeyType.ContainerType = EPinContainerType::None;
			return true;
		}
	}

	// 从 Map 引脚本身获取类型
	if (MapPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
	{
		OutKeyType.PinCategory = MapPin->PinType.PinCategory;
		OutKeyType.PinSubCategory = MapPin->PinType.PinSubCategory;
		OutKeyType.PinSubCategoryObject = MapPin->PinType.PinSubCategoryObject;
		OutKeyType.ContainerType = EPinContainerType::None;
		return true;
	}

	return false;
}

bool FK2NodePinTypeHelpers::GetMapKeyTypeFromStructProperty(
	const UScriptStruct* StructType,
	FEdGraphPinType& OutKeyType,
	const UEdGraphSchema_K2* Schema)
{
	if (!StructType || !Schema)
	{
		return false;
	}

	for (TFieldIterator<FProperty> PropIt(StructType); PropIt; ++PropIt)
	{
		if (FMapProperty* MapProperty = CastField<FMapProperty>(*PropIt))
		{
			Schema->ConvertPropertyToPinType(MapProperty->KeyProp, OutKeyType);
			return true;
		}
	}

	return false;
}

bool FK2NodePinTypeHelpers::GetMapValueTypeFromStructProperty(
	const UScriptStruct* StructType,
	FEdGraphPinType& OutValueType,
	const UEdGraphSchema_K2* Schema)
{
	if (!StructType || !Schema)
	{
		return false;
	}

	for (TFieldIterator<FProperty> PropIt(StructType); PropIt; ++PropIt)
	{
		if (FMapProperty* MapProperty = CastField<FMapProperty>(*PropIt))
		{
			Schema->ConvertPropertyToPinType(MapProperty->ValueProp, OutValueType);
			return true;
		}
	}

	return false;
}

bool FK2NodePinTypeHelpers::GetArrayElementTypeFromStructProperty(
	const UScriptStruct* StructType,
	FEdGraphPinType& OutElementType,
	const UEdGraphSchema_K2* Schema)
{
	if (!StructType || !Schema)
	{
		return false;
	}

	for (TFieldIterator<FProperty> PropIt(StructType); PropIt; ++PropIt)
	{
		if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(*PropIt))
		{
			Schema->ConvertPropertyToPinType(ArrayProperty->Inner, OutElementType);
			return true;
		}
	}

	return false;
}

bool FK2NodePinTypeHelpers::GetSetElementTypeFromStructProperty(
	const UScriptStruct* StructType,
	FEdGraphPinType& OutElementType,
	const UEdGraphSchema_K2* Schema)
{
	if (!StructType || !Schema)
	{
		return false;
	}

	for (TFieldIterator<FProperty> PropIt(StructType); PropIt; ++PropIt)
	{
		if (FSetProperty* SetProperty = CastField<FSetProperty>(*PropIt))
		{
			Schema->ConvertPropertyToPinType(SetProperty->ElementProp, OutElementType);
			return true;
		}
	}

	return false;
}

bool FK2NodePinTypeHelpers::ValidateMapValueIsStruct(const UEdGraphPin* MapPin, FString* OutReason)
{
	if (!MapPin || MapPin->PinType.ContainerType != EPinContainerType::Map)
	{
		if (OutReason)
		{
			*OutReason = TEXT("引脚不是 Map 类型");
		}
		return false;
	}

	if (MapPin->PinType.PinValueType.TerminalCategory != UEdGraphSchema_K2::PC_Struct)
	{
		if (OutReason)
		{
			*OutReason = TEXT("Map 的 Value 必须是结构体类型");
		}
		return false;
	}

	return true;
}

bool FK2NodePinTypeHelpers::ValidateStructHasSinglePropertyOfType(
	const UScriptStruct* StructType,
	const FFieldClass* PropertyClass,
	FString* OutReason)
{
	if (!StructType)
	{
		if (OutReason)
		{
			*OutReason = TEXT("结构体类型无效");
		}
		return false;
	}

	FProperty* FirstProperty = StructType->PropertyLink;
	if (!FirstProperty)
	{
		if (OutReason)
		{
			*OutReason = TEXT("结构体必须包含一个成员变量");
		}
		return false;
	}

	if (FirstProperty->Next != nullptr)
	{
		if (OutReason)
		{
			*OutReason = TEXT("结构体只能包含一个成员变量");
		}
		return false;
	}

	if (!FirstProperty->IsA(PropertyClass))
	{
		if (OutReason)
		{
			*OutReason = FString::Printf(TEXT("结构体的成员必须是 %s 类型"), *PropertyClass->GetName());
		}
		return false;
	}

	return true;
}
