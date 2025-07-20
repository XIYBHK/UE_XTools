// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 对象池迁移配置
 * 
 * 这个文件定义了新旧对象池系统并存的配置选项，
 * 支持渐进式迁移和A/B测试。
 * 
 * 设计原则：
 * - 编译时开关：通过宏定义控制使用哪个实现
 * - 运行时切换：支持动态切换用于测试
 * - 向后兼容：确保现有代码无需修改
 * - 渐进迁移：支持逐步迁移不同模块
 */

// ✅ 编译时配置宏

/**
 * 主要的实现选择开关
 * 
 * OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION:
 * - 0: 使用原始实现（默认，向后兼容）
 * - 1: 使用简化实现（新的高性能实现）
 * - 2: 使用混合模式（运行时选择）
 */
#ifndef OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION
    #define OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION 2  // 默认使用混合模式
#endif

/**
 * 子系统实现选择
 * 控制UObjectPoolLibrary使用哪个子系统实现
 */
#ifndef OBJECTPOOL_LIBRARY_USE_SIMPLIFIED
    #if OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 1
        #define OBJECTPOOL_LIBRARY_USE_SIMPLIFIED 1
    #elif OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 0
        #define OBJECTPOOL_LIBRARY_USE_SIMPLIFIED 0
    #else
        #define OBJECTPOOL_LIBRARY_USE_SIMPLIFIED 2  // 运行时选择
    #endif
#endif

/**
 * 迁移验证开关
 * 启用时会进行额外的验证和日志记录
 */
#ifndef OBJECTPOOL_ENABLE_MIGRATION_VALIDATION
    #define OBJECTPOOL_ENABLE_MIGRATION_VALIDATION 1
#endif

/**
 * 性能监控开关
 * 启用时会收集详细的性能数据用于对比
 */
#ifndef OBJECTPOOL_ENABLE_PERFORMANCE_MONITORING
    #define OBJECTPOOL_ENABLE_PERFORMANCE_MONITORING 1
#endif

/**
 * A/B测试支持
 * 启用时支持运行时在新旧实现间切换
 */
#ifndef OBJECTPOOL_ENABLE_AB_TESTING
    #define OBJECTPOOL_ENABLE_AB_TESTING 1
#endif

// ✅ 功能特性开关

/**
 * 启用迁移日志
 * 记录迁移过程中的详细信息
 */
#ifndef OBJECTPOOL_ENABLE_MIGRATION_LOGGING
    #define OBJECTPOOL_ENABLE_MIGRATION_LOGGING 1
#endif

/**
 * 启用兼容性检查
 * 在运行时验证新旧实现的行为一致性
 */
#ifndef OBJECTPOOL_ENABLE_COMPATIBILITY_CHECKS
    #define OBJECTPOOL_ENABLE_COMPATIBILITY_CHECKS 1
#endif

/**
 * 启用性能对比
 * 同时运行新旧实现并对比性能
 */
#ifndef OBJECTPOOL_ENABLE_PERFORMANCE_COMPARISON
    #define OBJECTPOOL_ENABLE_PERFORMANCE_COMPARISON 0  // 默认关闭，仅用于测试
#endif

// ✅ 实现选择辅助宏

/**
 * 检查是否使用简化实现
 */
#define OBJECTPOOL_IS_USING_SIMPLIFIED() \
    (OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 1 || \
     (OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 2 && FObjectPoolMigrationManager::IsUsingSimplifiedImplementation()))

/**
 * 检查是否使用原始实现
 */
#define OBJECTPOOL_IS_USING_ORIGINAL() \
    (OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 0 || \
     (OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 2 && !FObjectPoolMigrationManager::IsUsingSimplifiedImplementation()))

/**
 * 检查是否启用混合模式
 */
#define OBJECTPOOL_IS_MIXED_MODE() \
    (OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 2)

// ✅ 条件编译辅助宏

/**
 * 简化实现相关代码
 */
#if OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION != 0
    #define OBJECTPOOL_SIMPLIFIED_CODE(code) code
#else
    #define OBJECTPOOL_SIMPLIFIED_CODE(code)
#endif

/**
 * 原始实现相关代码
 */
#if OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION != 1
    #define OBJECTPOOL_ORIGINAL_CODE(code) code
#else
    #define OBJECTPOOL_ORIGINAL_CODE(code)
#endif

/**
 * 混合模式相关代码
 */
#if OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 2
    #define OBJECTPOOL_MIXED_MODE_CODE(code) code
#else
    #define OBJECTPOOL_MIXED_MODE_CODE(code)
#endif

/**
 * 迁移验证相关代码
 */
#if OBJECTPOOL_ENABLE_MIGRATION_VALIDATION
    #define OBJECTPOOL_MIGRATION_VALIDATION_CODE(code) code
#else
    #define OBJECTPOOL_MIGRATION_VALIDATION_CODE(code)
#endif

/**
 * 性能监控相关代码
 */
#if OBJECTPOOL_ENABLE_PERFORMANCE_MONITORING
    #define OBJECTPOOL_PERFORMANCE_MONITORING_CODE(code) code
#else
    #define OBJECTPOOL_PERFORMANCE_MONITORING_CODE(code)
#endif

// ✅ 日志宏定义

/**
 * 迁移相关日志
 */
#if OBJECTPOOL_ENABLE_MIGRATION_LOGGING
    #define OBJECTPOOL_MIGRATION_LOG(Verbosity, Format, ...) \
        UE_LOG(LogObjectPoolMigration, Verbosity, Format, ##__VA_ARGS__)
#else
    #define OBJECTPOOL_MIGRATION_LOG(Verbosity, Format, ...)
#endif

/**
 * 兼容性检查日志
 */
#if OBJECTPOOL_ENABLE_COMPATIBILITY_CHECKS
    #define OBJECTPOOL_COMPATIBILITY_LOG(Verbosity, Format, ...) \
        UE_LOG(LogObjectPoolCompatibility, Verbosity, Format, ##__VA_ARGS__)
#else
    #define OBJECTPOOL_COMPATIBILITY_LOG(Verbosity, Format, ...)
#endif

// ✅ 前向声明
class FObjectPoolMigrationManager;

// ✅ 日志分类声明
DECLARE_LOG_CATEGORY_EXTERN(LogObjectPoolMigration, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogObjectPoolCompatibility, Log, All);

// ✅ 编译时验证

/**
 * 编译时配置验证
 */
#if OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION < 0 || OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION > 2
    #error "OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION must be 0, 1, or 2"
#endif

#if OBJECTPOOL_LIBRARY_USE_SIMPLIFIED < 0 || OBJECTPOOL_LIBRARY_USE_SIMPLIFIED > 2
    #error "OBJECTPOOL_LIBRARY_USE_SIMPLIFIED must be 0, 1, or 2"
#endif

// ✅ 版本信息

/**
 * 迁移配置版本
 */
#define OBJECTPOOL_MIGRATION_CONFIG_VERSION_MAJOR 1
#define OBJECTPOOL_MIGRATION_CONFIG_VERSION_MINOR 0
#define OBJECTPOOL_MIGRATION_CONFIG_VERSION_PATCH 0

/**
 * 迁移配置版本字符串
 */
#define OBJECTPOOL_MIGRATION_CONFIG_VERSION_STRING \
    PREPROCESSOR_TO_STRING(OBJECTPOOL_MIGRATION_CONFIG_VERSION_MAJOR) "." \
    PREPROCESSOR_TO_STRING(OBJECTPOOL_MIGRATION_CONFIG_VERSION_MINOR) "." \
    PREPROCESSOR_TO_STRING(OBJECTPOOL_MIGRATION_CONFIG_VERSION_PATCH)

// ✅ 配置摘要宏

/**
 * 获取当前配置摘要
 */
#define OBJECTPOOL_GET_CONFIG_SUMMARY() \
    FString::Printf(TEXT("ObjectPool Migration Config v%s - Implementation: %s, Library: %s, Validation: %s"), \
        TEXT(OBJECTPOOL_MIGRATION_CONFIG_VERSION_STRING), \
        (OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 0) ? TEXT("Original") : \
        (OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 1) ? TEXT("Simplified") : TEXT("Mixed"), \
        (OBJECTPOOL_LIBRARY_USE_SIMPLIFIED == 0) ? TEXT("Original") : \
        (OBJECTPOOL_LIBRARY_USE_SIMPLIFIED == 1) ? TEXT("Simplified") : TEXT("Runtime"), \
        OBJECTPOOL_ENABLE_MIGRATION_VALIDATION ? TEXT("Enabled") : TEXT("Disabled"))

/**
 * 静态断言：确保配置的一致性
 */
#if OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 1 && OBJECTPOOL_LIBRARY_USE_SIMPLIFIED == 0
    #error "Inconsistent configuration: Cannot use original library with simplified implementation only"
#endif

#if OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 0 && OBJECTPOOL_LIBRARY_USE_SIMPLIFIED == 1
    #error "Inconsistent configuration: Cannot use simplified library with original implementation only"
#endif
