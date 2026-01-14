/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"

class UK2Node;
class UEdGraphSchema_K2;

/**
 * K2Node 引脚类型传播辅助类
 *
 * 提供通用的引脚类型推断和传播功能，减少重复代码
 *
 * 设计原则：
 * - 类型安全：自动处理 Wildcard 类型推断
 * - 容器支持：支持 Array、Set、Map 容器类型
 * - 结构体支持：支持从结构体属性提取类型
 * - 通知机制：可选的图表变更通知
 */
struct FK2NodePinTypeHelpers
{
	/**
	 * 重置引脚为 Wildcard 类型
	 * @param Pin 要重置的引脚
	 * @param ContainerType 容器类型（默认 None）
	 */
	static void ResetPinToWildcard(
		UEdGraphPin* Pin,
		EPinContainerType ContainerType = EPinContainerType::None
	);

	/**
	 * 重置 Map 引脚为 Wildcard 类型（包括 Key 和 Value）
	 * @param MapPin 要重置的 Map 引脚
	 */
	static void ResetMapPinToWildcard(UEdGraphPin* MapPin);

	/**
	 * 从 Map 引脚获取 Key 类型
	 * @param MapPin Map 引脚
	 * @param OutKeyType 输出的 Key 类型
	 * @return 成功获取返回 true
	 */
	static bool GetMapKeyType(
		const UEdGraphPin* MapPin,
		FEdGraphPinType& OutKeyType
	);

	/**
	 * 从结构体的 Map 属性获取 Key 类型
	 * @param StructType 结构体类型
	 * @param OutKeyType 输出的 Key 类型
	 * @param Schema K2 Schema（用于类型转换）
	 * @return 成功获取返回 true
	 */
	static bool GetMapKeyTypeFromStructProperty(
		const UScriptStruct* StructType,
		FEdGraphPinType& OutKeyType,
		const UEdGraphSchema_K2* Schema
	);

	/**
	 * 从结构体的 Map 属性获取 Value 类型
	 * @param StructType 结构体类型
	 * @param OutValueType 输出的 Value 类型
	 * @param Schema K2 Schema（用于类型转换）
	 * @return 成功获取返回 true
	 */
	static bool GetMapValueTypeFromStructProperty(
		const UScriptStruct* StructType,
		FEdGraphPinType& OutValueType,
		const UEdGraphSchema_K2* Schema
	);

	/**
	 * 从结构体的 Array 属性获取元素类型
	 * @param StructType 结构体类型
	 * @param OutElementType 输出的元素类型
	 * @param Schema K2 Schema（用于类型转换）
	 * @return 成功获取返回 true
	 */
	static bool GetArrayElementTypeFromStructProperty(
		const UScriptStruct* StructType,
		FEdGraphPinType& OutElementType,
		const UEdGraphSchema_K2* Schema
	);

	/**
	 * 从结构体的 Set 属性获取元素类型
	 * @param StructType 结构体类型
	 * @param OutElementType 输出的元素类型
	 * @param Schema K2 Schema（用于类型转换）
	 * @return 成功获取返回 true
	 */
	static bool GetSetElementTypeFromStructProperty(
		const UScriptStruct* StructType,
		FEdGraphPinType& OutElementType,
		const UEdGraphSchema_K2* Schema
	);

	/**
	 * 验证 Map 引脚的 Value 是否为结构体类型
	 * @param MapPin Map 引脚
	 * @param OutReason 失败原因（可选）
	 * @return 验证通过返回 true
	 */
	static bool ValidateMapValueIsStruct(
		const UEdGraphPin* MapPin,
		FString* OutReason = nullptr
	);

	/**
	 * 验证结构体是否只包含一个指定类型的属性
	 * @param StructType 结构体类型
	 * @param PropertyClass 属性类型（如 FMapProperty、FArrayProperty）
	 * @param OutReason 失败原因（可选）
	 * @return 验证通过返回 true
	 */
	static bool ValidateStructHasSinglePropertyOfType(
		const UScriptStruct* StructType,
		const FFieldClass* PropertyClass,
		FString* OutReason = nullptr
	);

};
