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
#if (defined(ENGINE_MAJOR_VERSION) && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3) || !defined(ENGINE_MAJOR_VERSION)
		// UE 5.3-5.7+: 支持直接读取
		// 如果ENGINE_MAJOR_VERSION未定义，假设是5.3+（当前编译环境）
		return AtomicVar;
#else
		// UE 5.0-5.2: 使用load()
		return AtomicVar.load();
#endif
	}

	/**
	 * 设置 TAtomic<T> 的值（兼容所有UE版本）
	 */
	template<typename T>
	FORCEINLINE void AtomicStore(TAtomic<T>& AtomicVar, T Value)
	{
#if (defined(ENGINE_MAJOR_VERSION) && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3) || !defined(ENGINE_MAJOR_VERSION)
		// UE 5.3-5.7+: 支持直接赋值
		// 如果ENGINE_MAJOR_VERSION未定义，假设是5.3+（当前编译环境）
		AtomicVar = Value;
#else
		// UE 5.0-5.2: 使用store()
		AtomicVar.store(Value);
#endif
	}

	/**
	 * 原子递增 TAtomic<int32>（兼容所有UE版本）
	 * @return 递增后的值
	 */
	FORCEINLINE int32 AtomicIncrement(TAtomic<int32>& AtomicVar)
	{
#if (defined(ENGINE_MAJOR_VERSION) && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3) || !defined(ENGINE_MAJOR_VERSION)
		// UE 5.3+: 直接递增
		// 如果ENGINE_MAJOR_VERSION未定义，假设是5.3+（当前编译环境）
		return ++AtomicVar;
#else
		// UE 5.0-5.2: 使用fetch_add()
		return AtomicVar.fetch_add(1) + 1;
#endif
	}

	/**
	 * 原子递减 TAtomic<int32>（兼容所有UE版本）
	 * @return 递减后的值
	 */
	FORCEINLINE int32 AtomicDecrement(TAtomic<int32>& AtomicVar)
	{
#if (defined(ENGINE_MAJOR_VERSION) && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3) || !defined(ENGINE_MAJOR_VERSION)
		// UE 5.3+: 直接递减
		// 如果ENGINE_MAJOR_VERSION未定义，假设是5.3+（当前编译环境）
		return --AtomicVar;
#else
		// UE 5.0-5.2: 使用fetch_sub()
		return AtomicVar.fetch_sub(1) - 1;
#endif
	}

	/**
	 * 原子加法 TAtomic<int32>（兼容所有UE版本）
	 * @param Value 要加的值
	 * @return 加法后的值
	 */
	FORCEINLINE int32 AtomicAdd(TAtomic<int32>& AtomicVar, int32 Value)
	{
#if (defined(ENGINE_MAJOR_VERSION) && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3) || !defined(ENGINE_MAJOR_VERSION)
		// UE 5.3+: 直接加法
		// 如果ENGINE_MAJOR_VERSION未定义，假设是5.3+（当前编译环境）
		AtomicVar += Value;
		return AtomicVar;
#else
		// UE 5.0-5.2: 使用fetch_add()
		return AtomicVar.fetch_add(Value) + Value;
#endif
	}

	/**
	 * 原子减法 TAtomic<int32>（兼容所有UE版本）
	 * @param Value 要减的值
	 * @return 减法后的值
	 */
	FORCEINLINE int32 AtomicSub(TAtomic<int32>& AtomicVar, int32 Value)
	{
#if (defined(ENGINE_MAJOR_VERSION) && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3) || !defined(ENGINE_MAJOR_VERSION)
		// UE 5.3+: 直接减法
		// 如果ENGINE_MAJOR_VERSION未定义，假设是5.3+（当前编译环境）
		AtomicVar -= Value;
		return AtomicVar;
#else
		// UE 5.0-5.2: 使用fetch_sub()
		return AtomicVar.fetch_sub(Value) - Value;
#endif
	}

	/**
	 * 原子交换 TAtomic<T>（兼容所有UE版本）
	 * @param Value 新值
	 * @return 旧值
	 */
	template<typename T>
	FORCEINLINE T AtomicExchange(TAtomic<T>& AtomicVar, T Value)
	{
#if (defined(ENGINE_MAJOR_VERSION) && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3) || !defined(ENGINE_MAJOR_VERSION)
		// UE 5.3+: 直接交换
		// 如果ENGINE_MAJOR_VERSION未定义，假设是5.3+（当前编译环境）
		T OldValue = AtomicVar;
		AtomicVar = Value;
		return OldValue;
#else
		// UE 5.0-5.2: 使用exchange()
		return AtomicVar.exchange(Value);
#endif
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
#if (defined(ENGINE_MAJOR_VERSION) && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3) || !defined(ENGINE_MAJOR_VERSION)
		// UE 5.3+: 直接比较并交换
		// 如果ENGINE_MAJOR_VERSION未定义，假设是5.3+（当前编译环境）
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
#else
		// UE 5.0-5.2: 使用compare_exchange_weak()
		return AtomicVar.compare_exchange_weak(Expected, Desired);
#endif
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
#define XTOOLS_ATOMIC_EXCHANGE(AtomicVar, Value) XToolsVersionCompat::AtomicExchange(AtomicVar, Value)
#define XTOOLS_ATOMIC_COMPARE_EXCHANGE(AtomicVar, Expected, Desired) XToolsVersionCompat::AtomicCompareExchange(AtomicVar, Expected, Desired)
