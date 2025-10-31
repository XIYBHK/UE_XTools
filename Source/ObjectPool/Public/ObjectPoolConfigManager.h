/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

//  对象池模块依赖
#include "ObjectPoolTypes.h"

// 前向声明
class FActorPool;

/**
 * 对象池配置管理器
 * 
 * 设计原则：
 * - 单一职责：专注于配置的管理、验证和应用
 * - 轻量级：不依赖UObject系统，纯C++实现
 * - 类型安全：提供强类型的配置接口
 * - 简单易用：提供直观的配置管理API
 * 
 * 职责：
 * - 配置验证和应用
 * - 默认配置管理
 * - 配置推荐和优化
 */
class OBJECTPOOL_API FObjectPoolConfigManager
{
public:
    /**
     * 配置策略枚举
     */
    enum class EConfigStrategy : uint8
    {
        Default,        // 使用默认配置
        PerformanceFirst, // 性能优先配置
        MemoryFirst,    // 内存优先配置
        Balanced,       // 平衡配置
        Custom          // 自定义配置
    };

public:
    /**
     * 构造函数
     */
    FObjectPoolConfigManager();

    /** 析构函数 */
    ~FObjectPoolConfigManager();

    //  禁用拷贝构造和赋值
    FObjectPoolConfigManager(const FObjectPoolConfigManager&) = delete;
    FObjectPoolConfigManager& operator=(const FObjectPoolConfigManager&) = delete;

    //  支持移动语义
    FObjectPoolConfigManager(FObjectPoolConfigManager&& Other) noexcept;
    FObjectPoolConfigManager& operator=(FObjectPoolConfigManager&& Other) noexcept;

public:
    //  配置管理功能

    /**
     * 设置指定Actor类的配置
     * @param ActorClass Actor类
     * @param Config 配置信息
     * @return 是否设置成功
     */
    bool SetConfig(UClass* ActorClass, const FObjectPoolConfig& Config);

    /**
     * 获取指定Actor类的配置
     * @param ActorClass Actor类
     * @return 配置信息
     */
    FObjectPoolConfig GetConfig(UClass* ActorClass) const;

    /**
     * 移除指定Actor类的配置
     * @param ActorClass Actor类
     * @return 是否移除成功
     */
    bool RemoveConfig(UClass* ActorClass);

    /**
     * 检查是否有指定Actor类的配置
     * @param ActorClass Actor类
     * @return 是否存在配置
     */
    bool HasConfig(UClass* ActorClass) const;

    /**
     * 清空所有配置
     */
    void ClearAllConfigs();

    //  默认配置管理

    /**
     * 获取默认配置
     * @return 默认配置
     */
    FObjectPoolConfig GetDefaultConfig() const;

    /**
     * 设置默认配置
     * @param Config 默认配置
     */
    void SetDefaultConfig(const FObjectPoolConfig& Config);

    /**
     * 为指定Actor类生成推荐配置
     * @param ActorClass Actor类
     * @param Strategy 配置策略
     * @return 推荐配置
     */
    FObjectPoolConfig GenerateRecommendedConfig(UClass* ActorClass, EConfigStrategy Strategy = EConfigStrategy::Balanced) const;

    //  配置验证

    /**
     * 验证配置是否有效
     * @param Config 要验证的配置
     * @param OutErrorMessage 错误信息输出
     * @return 是否有效
     */
    bool ValidateConfig(const FObjectPoolConfig& Config, FString& OutErrorMessage) const;

    /**
     * 修复无效的配置
     * @param Config 要修复的配置
     * @return 修复后的配置
     */
    FObjectPoolConfig FixInvalidConfig(const FObjectPoolConfig& Config) const;

    //  配置应用

    /**
     * 将配置应用到池
     * @param Pool 目标池
     * @param Config 要应用的配置
     * @return 是否应用成功
     */
    bool ApplyConfigToPool(FActorPool& Pool, const FObjectPoolConfig& Config) const;

    /**
     * 从池中提取当前配置
     * @param Pool 源池
     * @return 提取的配置
     */
    FObjectPoolConfig ExtractConfigFromPool(const FActorPool& Pool) const;

    //  统计和分析

    /**
     * 获取配置统计信息
     * @return 统计信息字符串
     */
    FString GetConfigStats() const;

    /**
     * 分析配置使用情况
     * @return 分析报告
     */
    FString AnalyzeConfigUsage() const;

private:
    //  内部数据成员

    /** 配置存储：Actor类 -> 配置 */
    TMap<UClass*, FObjectPoolConfig> ActorConfigs;

    /** 默认配置 */
    FObjectPoolConfig DefaultConfig;

    /** 配置访问锁 */
    mutable FCriticalSection ConfigLock;

    /** 是否已初始化 */
    bool bIsInitialized;

private:
    //  内部辅助方法

    /**
     * 初始化默认配置
     */
    void InitializeDefaultConfig();

    /**
     * 根据Actor类生成基础配置
     * @param ActorClass Actor类
     * @return 基础配置
     */
    FObjectPoolConfig GenerateBaseConfig(UClass* ActorClass) const;

    /**
     * 应用配置策略
     * @param BaseConfig 基础配置
     * @param Strategy 策略
     * @return 应用策略后的配置
     */
    FObjectPoolConfig ApplyStrategy(const FObjectPoolConfig& BaseConfig, EConfigStrategy Strategy) const;

    /**
     * 验证Actor类是否有效
     * @param ActorClass Actor类
     * @return 是否有效
     */
    bool ValidateActorClass(UClass* ActorClass) const;

    /**
     * 获取配置策略名称
     * @param Strategy 策略枚举
     * @return 策略名称
     */
    static FString GetStrategyName(EConfigStrategy Strategy);

    //  常量定义

    /** 默认初始池大小 */
    static constexpr int32 DEFAULT_INITIAL_SIZE = 10;

    /** 默认硬限制 */
    static constexpr int32 DEFAULT_HARD_LIMIT = 100;

    /** 性能优先策略的初始大小 */
    static constexpr int32 PERFORMANCE_INITIAL_SIZE = 20;

    /** 内存优先策略的硬限制 */
    static constexpr int32 MEMORY_HARD_LIMIT = 50;
};

/**
 * 配置管理器的日志宏
 */
DECLARE_LOG_CATEGORY_EXTERN(LogObjectPoolConfigManager, Log, All);

#define CONFIG_MANAGER_LOG(Verbosity, Format, ...) \
    UE_LOG(LogObjectPoolConfigManager, Verbosity, Format, ##__VA_ARGS__)

/**
 * 配置管理器的性能统计宏
 */
#if STATS
DECLARE_CYCLE_STAT_EXTERN(TEXT("ConfigManager_ValidateConfig"), STAT_ConfigManager_ValidateConfig, STATGROUP_Game, OBJECTPOOL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("ConfigManager_ApplyConfig"), STAT_ConfigManager_ApplyConfig, STATGROUP_Game, OBJECTPOOL_API);
#endif