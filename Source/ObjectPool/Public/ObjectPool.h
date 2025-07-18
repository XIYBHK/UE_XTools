// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * ObjectPool模块 - 基于UE最佳实践的Actor对象池系统
 * 
 * 设计理念：
 * - 基于UE内置FObjectPool基础设施，不重新实现
 * - 提供蓝图友好的极简API接口
 * - 支持Actor生命周期管理和自动状态重置
 * - 遵循UE线程安全和性能优化最佳实践
 * 
 * 核心特性：
 * - 极简API：仅3个核心函数 (RegisterActorClass, SpawnActorFromPool, ReturnActorToPool)
 * - 永不失败：自动回退机制，池为空时自动创建新Actor
 * - 生命周期接口：支持OnPoolActorCreated, OnPoolActorActivated, OnReturnToPool事件
 * - 线程安全：使用FRWLock读写锁优化并发访问
 * - 性能监控：内置统计和分析功能
 */
class FObjectPoolModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    
    /**
     * 检查模块是否已加载
     * @return true if the module is loaded
     */
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("ObjectPool");
    }
    
    /**
     * 获取模块实例
     * @return Module instance
     */
    static FObjectPoolModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FObjectPoolModule>("ObjectPool");
    }

private:
    /** 模块启动时的初始化 */
    void InitializeModule();
    
    /** 模块关闭时的清理 */
    void CleanupModule();
    
    /** 注册控制台命令 */
    void RegisterConsoleCommands();
    
    /** 注销控制台命令 */
    void UnregisterConsoleCommands();

private:
    /** 控制台命令句柄 */
    TArray<struct IConsoleCommand*> ConsoleCommands;
    
    /** 模块是否已初始化 */
    bool bIsInitialized = false;
};

// ✅ 模块日志类别定义
DECLARE_LOG_CATEGORY_EXTERN(LogObjectPool, Log, All);

// ✅ 性能统计宏定义
#if !OBJECTPOOL_SHIPPING
    #define OBJECTPOOL_STAT(StatName) SCOPE_CYCLE_COUNTER(STAT_##StatName)
    #define OBJECTPOOL_LOG(Verbosity, Format, ...) UE_LOG(LogObjectPool, Verbosity, Format, ##__VA_ARGS__)
#else
    #define OBJECTPOOL_STAT(StatName)
    #define OBJECTPOOL_LOG(Verbosity, Format, ...)
#endif

// ✅ 模块API宏定义
#ifndef OBJECTPOOL_API
    #define OBJECTPOOL_API DLLIMPORT
#endif
