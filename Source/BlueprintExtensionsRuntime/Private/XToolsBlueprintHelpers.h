/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/UnrealType.h"
#include "XToolsVersionCompat.h"

/**
 * XTools 蓝图扩展辅助函数命名空间
 * 提供各 Library 类共享的底层算法实现
 */
namespace XToolsBlueprintHelpers
{
	/**
	 * 获取有效的 World 指针
	 * @param WorldContextObject 世界上下文对象
	 * @return 有效 World 指针，无效返回 nullptr
	 */
	FORCEINLINE UWorld* GetValidWorld(const UObject* WorldContextObject)
	{
		if (!WorldContextObject)
		{
			return nullptr;
		}

		return GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	}

	/**
	 * 清理枚举名称（移除空格和下划线）
	 * @param Input 原始字符串
	 * @return 清理后的字符串
	 */
	FORCEINLINE FString CleanEnumName(const FString& Input)
	{
		return Input.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT(""));
	}

	/**
	 * 获取枚举的显示名称列表（排除与枚举名相同的项）
	 * @param EnumPtr 枚举指针
	 * @return 显示名称数组
	 */
	inline TArray<FName> GetEnumDisplayNames(UEnum* EnumPtr)
	{
		TArray<FName> Keys;
		if (EnumPtr)
		{
			for (int32 i = 0; i < EnumPtr->NumEnums(); ++i)
			{
				const FString DisplayName = EnumPtr->GetDisplayNameTextByIndex(i).ToString();
				const FString EnumName = EnumPtr->GetNameStringByIndex(i);

				// 如果清理后的显示名和枚举名不同，则添加
				if (CleanEnumName(DisplayName) != CleanEnumName(EnumName))
				{
					Keys.Add(FName(*DisplayName));
				}
			}
		}
		return Keys;
	}

	/**
	 * 构建枚举名称到字符串的映射
	 * @param EnumPtr 枚举指针
	 * @return 名称到字符串的映射
	 */
	inline TMap<FName, FString> BuildEnumNameMap(UEnum* EnumPtr)
	{
		TMap<FName, FString> NameMap;
		if (EnumPtr)
		{
			for (int32 i = 0; i < EnumPtr->NumEnums(); ++i)
			{
				const FString DisplayName = EnumPtr->GetDisplayNameTextByIndex(i).ToString();
				const FString EnumName = EnumPtr->GetNameStringByIndex(i);

				NameMap.Add(FName(*DisplayName), EnumName);
			}
		}
		return NameMap;
	}

	/**
	 * RAII 风格的属性存储管理器
	 * 自动管理属性的内存分配、初始化和清理
	 */
	struct FScopedPropertyStorage
	{
		void* ValuePtr;
		FProperty* Property;
		int32 AllocatedSize;

		explicit FScopedPropertyStorage(FProperty* InProperty)
			: ValuePtr(nullptr)
			, Property(InProperty)
			, AllocatedSize(0)
		{
			if (Property)
			{
				AllocatedSize = XTOOLS_GET_ELEMENT_SIZE(Property) * Property->ArrayDim;
				ValuePtr = FMemory_Alloca(AllocatedSize);
				if (ValuePtr)
				{
					Property->InitializeValue(ValuePtr);
				}
			}
		}

		~FScopedPropertyStorage()
		{
			if (ValuePtr && Property)
			{
				Property->DestroyValue(ValuePtr);
			}
		}

		void* Get() const { return ValuePtr; }
		int32 GetSize() const { return AllocatedSize; }
		bool IsValid() const { return ValuePtr != nullptr && Property != nullptr; }
	};

	/**
	 * 检查属性指针是否兼容（类型相同且大小匹配）
	 * @param TargetProperty 目标属性
	 * @param SourceProperty 源属性
	 * @param SourceAddress 源地址
	 * @param SourceSize 源大小
	 * @return 是否兼容
	 */
	FORCEINLINE bool IsCompatiblePropertyPtr(
		const FProperty* TargetProperty,
		const FProperty* SourceProperty,
		const void* SourceAddress,
		int32 SourceSize)
	{
		if (!TargetProperty || !SourceProperty)
		{
			return false;
		}

		const FFieldClass* TargetClass = TargetProperty->GetClass();
		const FFieldClass* SourceClass = SourceProperty->GetClass();

		return SourceAddress != nullptr &&
			SourceSize == XTOOLS_GET_ELEMENT_SIZE(TargetProperty) * TargetProperty->ArrayDim &&
			(SourceClass->IsChildOf(TargetClass) || TargetClass->IsChildOf(SourceClass));
	}

	/**
	 * 模板化的枚举缓存获取器
	 * @param Cache 缓存映射
	 * @param Key 键
	 * @param StaticEnum 静态枚举
	 * @return 缓存的枚举值
	 */
	template<typename TEnum>
	inline TEnum GetCachedEnum(TMap<FString, TEnum>& Cache, const FString& Key, UEnum* StaticEnum)
	{
		if (const TEnum* CachedValue = Cache.Find(Key))
		{
			return *CachedValue;
		}

		const int64 EnumValue = StaticEnum->GetValueByName(FName(*Key));
		const TEnum Result = static_cast<TEnum>(EnumValue);
		Cache.Add(Key, Result);
		return Result;
	}
}
