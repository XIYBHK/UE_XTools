// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Field/FieldSystemObjects.h"
#include "Field/FieldSystemTypes.h"
#include "XFieldSystemLibrary.generated.h"

class AXFieldSystemActor;

/**
 * XFieldSystemLibrary
 * 
 * 提供Field System相关的蓝图辅助函数
 */
UCLASS()
class FIELDSYSTEMEXTENSIONS_API UXFieldSystemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================================================
	// 筛选器创建
	// ============================================================================

	/**
	 * 创建基础的MetaDataFilter
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|Field System|Filter", 
		meta = (DisplayName = "创建基础筛选器"))
	static UFieldSystemMetaDataFilter* CreateBasicFilter(
		UPARAM(DisplayName = "对象类型") TEnumAsByte<EFieldObjectType> ObjectType = EFieldObjectType::Field_Object_All,
		UPARAM(DisplayName = "状态类型") TEnumAsByte<EFieldFilterType> FilterType = EFieldFilterType::Field_Filter_All,
		UPARAM(DisplayName = "位置类型") TEnumAsByte<EFieldPositionType> PositionType = EFieldPositionType::Field_Position_CenterOfMass
	);

	/**
	 * 创建排除角色的筛选器
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|Field System|Filter", 
		meta = (DisplayName = "创建排除角色筛选器"))
	static UFieldSystemMetaDataFilter* CreateExcludeCharacterFilter();

	/**
	 * 创建只影响破碎的筛选器
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|Field System|Filter", 
		meta = (DisplayName = "创建破碎专用筛选器"))
	static UFieldSystemMetaDataFilter* CreateDestructionOnlyFilter();

	// ============================================================================
	// Field System Actor辅助
	// ============================================================================

	/**
	 * 获取或创建XFieldSystemActor的筛选器
	 * 如果Actor已有缓存的筛选器则返回，否则创建新的
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|Field System|Filter", 
		meta = (DisplayName = "获取或创建Actor筛选器"))
	static UFieldSystemMetaDataFilter* GetOrCreateActorFilter(
		UPARAM(DisplayName = "Field Actor") AXFieldSystemActor* Actor);
};

