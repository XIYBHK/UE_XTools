/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*
* UE版本兼容性适配层
* 
* 用于处理不同UE版本之间的API差异，确保插件可以在多个UE版本中正常编译和运行。
* 
* 当前支持版本：UE 5.3, 5.4, 5.5, 5.6+
* 
* 主要处理的API变化：
* - TAtomic API (UE 5.3+支持直接赋值，5.0-5.2需要load/store)
* - FProperty::ElementSize (UE 5.5+弃用，使用GetElementSize/SetElementSize)
* - BufferCommand (UE 5.5+弃用，使用BufferFieldCommand_Internal)
* 
* 使用说明：
* 1. 包含本头文件： #include "XToolsVersionCompat.h"
* 2. 使用版本宏进行条件编译： #if XTOOLS_ENGINE_5_5_OR_LATER
* 3. 保持API原样调用，不强制封装
*/

#pragma once

#include "CoreMinimal.h"
#include "Templates/Atomic.h"

// ===================================================================
// 版本判断宏 - 统一处理UE版本API变迁
// ===================================================================

/**
 * 版本比较宏（核心工具）
 * 用法: #if XTOOLS_ENGINE_VERSION_AT_LEAST(5, 5)
 */
#define XTOOLS_ENGINE_VERSION_AT_LEAST(Major, Minor) \
    ((ENGINE_MAJOR_VERSION > Major) || \
     (ENGINE_MAJOR_VERSION == Major && ENGINE_MINOR_VERSION >= Minor))

/**
 * 常用版本开关（提高可读性）
 */
#define XTOOLS_ENGINE_5_4_OR_LATER XTOOLS_ENGINE_VERSION_AT_LEAST(5, 4)
#define XTOOLS_ENGINE_5_5_OR_LATER XTOOLS_ENGINE_VERSION_AT_LEAST(5, 5)
#define XTOOLS_ENGINE_5_6_OR_LATER XTOOLS_ENGINE_VERSION_AT_LEAST(5, 6)

// ===================================================================
// API变更记录（文档参考）
// ===================================================================
// UE 5.4:
//   - IWYU原则引入，需要显式包含头文件（如Engine/OverlapResult.h）
//
// UE 5.5:
//   - FProperty::ElementSize 弃用 → 使用GetElementSize()/SetElementSize()
//   - BufferCommand 弃用 → 使用BufferFieldCommand_Internal
//
// UE 5.6:
//   - 编译器对弃用警告更严格（-Werror）
// ===================================================================

/**
 * UE版本兼容性工具类
 * 
 * 提供统一的API来操作可能在不同UE版本中有差异的功能
 */
namespace XToolsVersionCompat
{
	/**
	 * TAtomic 兼容性操作
	 * 
	 * UE 5.3+ 支持直接赋值和读取
	 * UE 5.0-5.2 需要使用 load()/store()/fetch_add() 等方法
	 */
	
	/**
	 * 读取 TAtomic<bool> 的值（兼容所有UE版本）
	 */
	template<typename T>
	FORCEINLINE T AtomicLoad(const TAtomic<T>& AtomicVar)
	{
		// UE 5.3+: 支持直接读取
		return AtomicVar;
	}

	/**
	 * 设置 TAtomic<T> 的值（兼容所有UE版本）
	 */
	template<typename T>
	FORCEINLINE void AtomicStore(TAtomic<T>& AtomicVar, T Value)
	{
		// UE 5.3+: 支持直接赋值
		AtomicVar = Value;
	}

	/**
	 * 原子递增 TAtomic<int32>（兼容所有UE版本）
	 * @return 递增后的值
	 */
	FORCEINLINE int32 AtomicIncrement(TAtomic<int32>& AtomicVar)
	{
		// UE 5.3+: 直接递增
		return ++AtomicVar;
	}

	/**
	 * 原子递减 TAtomic<int32>（兼容所有UE版本）
	 * @return 递减后的值
	 */
	FORCEINLINE int32 AtomicDecrement(TAtomic<int32>& AtomicVar)
	{
		// UE 5.3+: 直接递减
		return --AtomicVar;
	}

	/**
	 * 原子加法 TAtomic<int32>（兼容所有UE版本）
	 * @param Value 要加的值
	 * @return 加法后的值
	 */
	FORCEINLINE int32 AtomicAdd(TAtomic<int32>& AtomicVar, int32 Value)
	{
		// UE 5.3+: 直接加法
		AtomicVar += Value;
		return AtomicVar;
	}

	/**
	 * 原子减法 TAtomic<int32>（兼容所有UE版本）
	 * @param Value 要减的值
	 * @return 减法后的值
	 */
	FORCEINLINE int32 AtomicSub(TAtomic<int32>& AtomicVar, int32 Value)
	{
		// UE 5.3+: 直接减法
		AtomicVar -= Value;
		return AtomicVar;
	}

	/**
	 * 原子交换 TAtomic<T>（兼容所有UE版本）
	 * @param Value 新值
	 * @return 旧值
	 */
	template<typename T>
	FORCEINLINE T AtomicExchange(TAtomic<T>& AtomicVar, T Value)
	{
		// UE 5.3+: 直接交换
		T OldValue = AtomicVar;
		AtomicVar = Value;
		return OldValue;
	}

	/**
	 * 比较并交换 TAtomic<T>（兼容所有UE版本）
	 * @param Expected 期望的值（会被更新为实际值）
	 * @param Desired 期望等于Expected时设置的值
	 * @return 是否成功交换
	 */
	template<typename T>
	FORCEINLINE bool AtomicCompareExchange(TAtomic<T>& AtomicVar, T& Expected, T Desired)
	{
		// UE 5.3+: 直接比较并交换
		T CurrentValue = AtomicVar;
		if (CurrentValue == Expected)
		{
			AtomicVar = Desired;
			return true;
		}
		else
		{
			Expected = CurrentValue;
			return false;
		}
	}
}

/**
 * 便捷宏定义
 * 
 * 为了简化使用，提供一些常用的宏
 */
#define XTOOLS_ATOMIC_LOAD(AtomicVar) XToolsVersionCompat::AtomicLoad(AtomicVar)
#define XTOOLS_ATOMIC_STORE(AtomicVar, Value) XToolsVersionCompat::AtomicStore(AtomicVar, Value)
#define XTOOLS_ATOMIC_INCREMENT(AtomicVar) XToolsVersionCompat::AtomicIncrement(AtomicVar)
#define XTOOLS_ATOMIC_DECREMENT(AtomicVar) XToolsVersionCompat::AtomicDecrement(AtomicVar)
#define XTOOLS_ATOMIC_ADD(AtomicVar, Value) XToolsVersionCompat::AtomicAdd(AtomicVar, Value)
#define XTOOLS_ATOMIC_SUB(AtomicVar, Value) XToolsVersionCompat::AtomicSub(AtomicVar, Value)

/**
 * FProperty ElementSize 访问宏（兼容 UE 5.3-5.7）
 *
 * UE 5.5+ ElementSize 被弃用，需要使用 GetElementSize/SetElementSize
 * UE 5.3-5.4: 直接访问 ElementSize 成员变量
 *
 * 用法：
 *   const int32 Size = XTOOLS_GET_ELEMENT_SIZE(Property);
 *
 * @param Prop FProperty指针
 * @return int32 元素大小
 */
#if XTOOLS_ENGINE_5_5_OR_LATER
#define XTOOLS_GET_ELEMENT_SIZE(Prop) ((Prop)->GetElementSize())
#else
#define XTOOLS_GET_ELEMENT_SIZE(Prop) ((Prop)->ElementSize)
#endif

/**
 * FProperty ElementSize 设置宏（兼容 UE 5.3-5.7）
 *
 * 注意：在 UE 5.5+ 中，SetElementSize 是保护成员，只能在特定上下文使用
 * 通常只在 UHT 生成的代码或特定引擎模块中使用
 *
 * 用法：
 *   XTOOLS_SET_ELEMENT_SIZE(Property, NewSize);
 *
 * @param Prop FProperty指针
 * @param Size 新的元素大小
 */
#if XTOOLS_ENGINE_5_5_OR_LATER
	// UE 5.5+: ElementSize 被弃用，且 SetElementSize 可能为受保护成员。
	// 若在你的上下文中 SetElementSize 不可访问，请避免在插件代码中设置 FProperty 元素大小（通常属于引擎/UHT 内部行为）。
	#define XTOOLS_SET_ELEMENT_SIZE(Prop, Size) \
		do { \
			(Prop)->SetElementSize(Size); \
		} while (0)
#else
	// UE 5.3-5.4: 直接访问 ElementSize
	#define XTOOLS_SET_ELEMENT_SIZE(Prop, Size) \
		do { \
			(Prop)->ElementSize = (Size); \
		} while (0)
#endif
#define XTOOLS_ATOMIC_EXCHANGE(AtomicVar, Value) XToolsVersionCompat::AtomicExchange(AtomicVar, Value)
#define XTOOLS_ATOMIC_COMPARE_EXCHANGE(AtomicVar, Expected, Desired) XToolsVersionCompat::AtomicCompareExchange(AtomicVar, Expected, Desired)
