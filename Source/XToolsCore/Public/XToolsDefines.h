/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"

// XToolsCore 的日志类别在 XToolsCore.cpp 中定义，这里做前向声明以保证本头文件自洽。
//（允许重复声明 extern）
DECLARE_LOG_CATEGORY_EXTERN(LogXToolsCore, Log, All);

// Plugin version
#define XTOOLS_VERSION_MAJOR 1
#define XTOOLS_VERSION_MINOR 9
#define XTOOLS_VERSION_PATCH 3

// Debugging
#if !UE_BUILD_SHIPPING
#define XTOOLS_DEBUG 1
#else
#define XTOOLS_DEBUG 0
#endif

// Platform specific macros
#if PLATFORM_WINDOWS
#define XTOOLS_WINDOWS 1
#else
#define XTOOLS_WINDOWS 0
#endif

// Feature toggles
#define XTOOLS_FEATURE_PARENT_FINDER 1
#define XTOOLS_FEATURE_DEBUG_DRAWING 1

// Parent finder settings
static constexpr int32 XTOOLS_MAX_PARENT_DEPTH = 100;

//=============================================================================
// 防御性编程宏 - 用于提升硬件不稳定环境下的代码鲁棒性
//=============================================================================

/**
 * 安全执行宏：检查指针有效后执行代码
 * 用法：XTOOLS_SAFE_EXECUTE(MyPtr, MyPtr->DoSomething())
 */
#define XTOOLS_SAFE_EXECUTE(Ptr, Code) \
	do { \
		if (Ptr) { \
			Code; \
		} else { \
			UE_LOG(LogXToolsCore, Warning, TEXT("XTOOLS_SAFE_EXECUTE: %s 为空 (%s:%d)"), \
				TEXT(#Ptr), TEXT(__FILE__), __LINE__); \
		} \
	} while(0)

/**
 * UObject 有效性检查宏：检查 UObject 有效性后继续，否则返回
 * 用法：XTOOLS_CHECK_VALID(MyActor, );  // void 函数
 *       XTOOLS_CHECK_VALID(MyActor, false);  // 返回 false
 */
#define XTOOLS_CHECK_VALID(Obj, ReturnValue) \
	if (!IsValid(Obj)) { \
		UE_LOG(LogXToolsCore, Warning, TEXT("XTOOLS_CHECK_VALID: %s 无效 (%s:%d)"), \
			TEXT(#Obj), TEXT(__FILE__), __LINE__); \
		return ReturnValue; \
	}

/**
 * UObject 有效性检查宏（静默版）：检查 UObject 有效性后继续，否则静默返回
 * 用法：XTOOLS_CHECK_VALID_SILENT(MyActor, );
 */
#define XTOOLS_CHECK_VALID_SILENT(Obj, ReturnValue) \
	if (!IsValid(Obj)) { \
		return ReturnValue; \
	}

/**
 * 数组边界检查宏：检查数组索引是否有效
 * 用法：XTOOLS_CHECK_ARRAY_INDEX(MyArray, Index, false);
 */
#define XTOOLS_CHECK_ARRAY_INDEX(Array, Index, ReturnValue) \
	if (!Array.IsValidIndex(Index)) { \
		UE_LOG(LogXToolsCore, Error, TEXT("XTOOLS_CHECK_ARRAY_INDEX: %s[%d] 越界，数组长度=%d (%s:%d)"), \
			TEXT(#Array), Index, Array.Num(), TEXT(__FILE__), __LINE__); \
		return ReturnValue; \
	}

/**
 * 指针非空断言宏（仅 Debug/Development 构建）
 * 用法：XTOOLS_ASSERT_PTR(MyPtr);
 */
#if !UE_BUILD_SHIPPING
#define XTOOLS_ASSERT_PTR(Ptr) \
	checkf(Ptr != nullptr, TEXT("XTOOLS_ASSERT_PTR: %s 不应为空 (%s:%d)"), \
		TEXT(#Ptr), TEXT(__FILE__), __LINE__)
#else
#define XTOOLS_ASSERT_PTR(Ptr)
#endif

/**
 * 条件日志宏：仅在条件为真时记录日志
 * 用法：XTOOLS_COND_LOG(bShouldLog, Warning, TEXT("Something happened"));
 */
#define XTOOLS_COND_LOG(Condition, Verbosity, Format, ...) \
	if (Condition) { \
		UE_LOG(LogXToolsCore, Verbosity, Format, ##__VA_ARGS__); \
	}
