// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Templates/SharedPointer.h"
#include "Containers/Map.h"
#include "UObject/GCObject.h"

// ✅ 对象池模块依赖
#include "ObjectPoolTypes.h"
#include "ActorPool.h"

// ✅ 生成的头文件必须放在最后
#include "ObjectPoolSubsystem.generated.h"

// ✅ 前向声明
class FActorPool;
class FActorStateResetter;
class FObjectPoolConfigManager;
class FObjectPoolDebugManager;

/**
 * 对象池子系统 - 全局Actor对象池管理器
 *
 * @deprecated 此类已被UObjectPoolSubsystemSimplified替代，将在未来版本中移除。
 *             请使用UObjectPoolSubsystemSimplified以获得更好的性能和更简洁的API。
 *             迁移指南：所有API保持兼容，只需通过UObjectPoolLibrary访问即可自动使用新实现。
 *
 * 设计理念：
 * - 基于UGameInstanceSubsystem，提供全局访问
 * - 管理多个Actor类型的对象池
 * - 提供蓝图友好的极简API接口
 * - 支持自动池创建和生命周期管理
 *
 * 核心功能：
 * - RegisterActorClass: 注册Actor类到对象池
 * - SpawnActorFromPool: 从池获取Actor（永不失败）
 * - ReturnActorToPool: 将Actor归还到池
 *
 * 线程安全：
 * - 子系统级别的操作是线程安全的
 * - 底层FActorPool使用FRWLock保证并发安全
 */
UCLASS(BlueprintType, meta = (
    DisplayName = "对象池子系统（已废弃）",
    ToolTip = "已废弃：此类已被UObjectPoolSubsystemSimplified替代。请使用UObjectPoolLibrary访问新的简化实现以获得更好的性能。",
    Keywords = "对象池,Actor池,性能优化,内存管理,复用,池化,已废弃,deprecated",
    Category = "XTools|性能优化|已废弃",
    ShortToolTip = "已废弃的Actor对象池管理器"))
class OBJECTPOOL_API UObjectPoolSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** USubsystem interface */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;



    /** 性能统计和调试功能 */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池|调试", meta = (
        DisplayName = "输出性能统计",
        ToolTip = "输出所有对象池的性能统计信息到日志，用于调试和性能分析",
        Keywords = "性能,统计,调试,日志,分析",
        CallInEditor = "true"))
    void LogPerformanceStats();

    UFUNCTION(BlueprintCallable, Category = "XTools|对象池|调试", meta = (
        DisplayName = "输出内存使用情况",
        ToolTip = "输出对象池的内存使用情况到日志",
        Keywords = "内存,使用,统计,调试"))
    void LogMemoryUsage();

    UFUNCTION(BlueprintCallable, Category = "XTools|对象池|调试", meta = (
        DisplayName = "启用性能监控",
        ToolTip = "启用定期性能监控，每隔指定秒数输出统计信息",
        Keywords = "性能,监控,定时,自动"))
    void EnablePerformanceMonitoring(float IntervalSeconds = 60.0f);

    UFUNCTION(BlueprintCallable, Category = "XTools|对象池|调试", meta = (
        DisplayName = "禁用性能监控",
        ToolTip = "禁用定期性能监控",
        Keywords = "性能,监控,停止"))
    void DisablePerformanceMonitoring();

    /** 配置管理功能 - 利用已有的配置系统 */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池|配置", meta = (
        DisplayName = "应用配置模板",
        ToolTip = "应用预设的配置模板到对象池系统",
        Keywords = "配置,模板,应用,设置"))
    bool ApplyConfigTemplate(const FString& TemplateName);

    UFUNCTION(BlueprintCallable, Category = "XTools|对象池|配置", meta = (
        DisplayName = "获取可用模板",
        ToolTip = "获取所有可用的配置模板名称列表",
        Keywords = "配置,模板,列表"))
    TArray<FString> GetAvailableConfigTemplates() const;

    UFUNCTION(BlueprintCallable, Category = "XTools|对象池|配置", meta = (
        DisplayName = "重置为默认配置",
        ToolTip = "将对象池配置重置为默认设置",
        Keywords = "配置,重置,默认"))
    void ResetToDefaultConfig();

    /** 调试工具功能 - 基于已有的统计系统 */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池|调试", meta = (
        DisplayName = "设置调试模式",
        ToolTip = "设置对象池的调试显示模式",
        Keywords = "调试,模式,显示"))
    void SetDebugMode(int32 DebugMode);

    UFUNCTION(BlueprintCallable, Category = "XTools|对象池|调试", meta = (
        DisplayName = "获取调试摘要",
        ToolTip = "获取对象池系统的调试摘要信息",
        Keywords = "调试,摘要,统计"))
    FString GetDebugSummary();

    UFUNCTION(BlueprintCallable, Category = "XTools|对象池|调试", meta = (
        DisplayName = "检测性能热点",
        ToolTip = "检测对象池使用中的性能热点和问题",
        Keywords = "热点,性能,检测,分析"))
    TArray<FString> DetectPerformanceHotspots();

    /**
     * 获取对象池子系统实例
     * @param WorldContext 世界上下文对象
     * @return 对象池子系统实例
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "获取对象池子系统",
        ToolTip = "获取全局对象池管理器实例。这是访问对象池功能的入口点，支持编辑器和运行时调用。",
        Keywords = "获取,子系统,对象池,管理器",
        ShortToolTip = "获取对象池管理器",
        CallInEditor = "true",
        CompactNodeTitle = "获取对象池"))
    static UObjectPoolSubsystem* Get(const UObject* WorldContext);

    /**
     * 智能获取World实例（公有方法，供其他模块使用）
     * @return 有效的World指针，如果无法获取则返回nullptr
     */
    UWorld* GetValidWorld() const;

    /**
     * 智能获取子系统实例（无需WorldContext）
     * 使用引擎的全局上下文自动查找合适的子系统实例
     * @return 子系统实例，失败返回nullptr
     */
    static UObjectPoolSubsystem* GetGlobal();

    /**
     * 全局静态方法：智能获取World实例
     * 供其他模块使用，无需手动获取子系统实例
     * @param WorldContext 可选的世界上下文，如果为空则使用全局查找
     * @return 有效的World指针，失败返回nullptr
     */
    static UWorld* GetValidWorldStatic(const UObject* WorldContext = nullptr);

    // ✅ Actor状态重置公共API

    /**
     * 重置Actor状态（公共API）
     * @param Actor 目标Actor
     * @param SpawnTransform 新的Transform
     * @param ResetConfig 重置配置
     * @return 重置是否成功
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "重置Actor状态",
        ToolTip = "重置Actor到指定状态，清理所有临时数据。可用于对象池或其他需要重置Actor的场景。",
        Keywords = "重置,状态,Actor,清理",
        ShortToolTip = "重置Actor状态",
        CallInEditor = "true"))
    bool ResetActorState(AActor* Actor, const FTransform& SpawnTransform, const FActorResetConfig& ResetConfig);

    /**
     * 批量重置Actor状态（公共API）
     * @param Actors Actor数组
     * @param ResetConfig 重置配置
     * @return 成功重置的数量
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "批量重置Actor状态",
        ToolTip = "批量重置多个Actor的状态，提高性能。适合同时重置大量Actor的场景。",
        Keywords = "批量,重置,状态,Actor,性能",
        ShortToolTip = "批量重置Actor状态"))
    int32 BatchResetActorStates(const TArray<AActor*>& Actors, const FActorResetConfig& ResetConfig);

    /**
     * 获取Actor重置统计信息
     * @return 重置统计数据
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "获取重置统计",
        ToolTip = "获取Actor状态重置操作的统计信息，包括成功率、耗时等性能指标。",
        Keywords = "统计,重置,性能,监控",
        ShortToolTip = "获取重置统计"))
    FActorResetStats GetActorResetStats() const;

    // ✅ 极简核心API - 仅3个核心函数

    /**
     * 注册Actor类到对象池
     * @param ActorClass 要池化的Actor类
     * @param InitialSize 初始池大小
     * @param HardLimit 池的最大限制 (0表示无限制)
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "注册Actor类",
        ToolTip = "为指定Actor类创建对象池，设置初始大小和最大限制。建议在游戏开始时注册常用的Actor类型以获得最佳性能。",
        Keywords = "注册,Actor类,对象池,初始化,配置",
        ShortToolTip = "注册Actor类到对象池",
        CompactNodeTitle = "注册Actor类",
        AdvancedDisplay = "HardLimit"))
    void RegisterActorClass(
        UPARAM(DisplayName = "Actor类") TSubclassOf<AActor> ActorClass,
        UPARAM(DisplayName = "初始大小") int32 InitialSize = 10,
        UPARAM(DisplayName = "硬限制") int32 HardLimit = 0);

    /**
     * 从对象池获取Actor
     * @param ActorClass Actor类型
     * @param SpawnTransform 生成位置和旋转
     * @return 池化的Actor实例，永不返回null (自动回退到正常生成)
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "从池获取Actor",
        ToolTip = "从对象池获取Actor实例。采用智能回退机制：优先从池获取→池空时自动创建→失败时回退到默认类型，确保永不失败。自动调用生命周期事件。",
        Keywords = "获取,生成,Actor,对象池,永不失败,智能回退",
        ShortToolTip = "从对象池获取Actor（永不失败）",
        CompactNodeTitle = "池获取Actor",
        BlueprintInternalUseOnly = "false"))
    AActor* SpawnActorFromPool(
        UPARAM(DisplayName = "Actor类") TSubclassOf<AActor> ActorClass,
        UPARAM(DisplayName = "生成Transform") const FTransform& SpawnTransform);

    /**
     * 将Actor归还到对象池
     * @param Actor 要归还的Actor
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "归还Actor到池",
        ToolTip = "将Actor归还到对象池以供重复使用。自动重置Actor状态（隐藏、禁用碰撞、停止Tick），调用生命周期事件，确保下次使用时状态正确。",
        Keywords = "归还,回收,Actor,对象池,重置状态,生命周期",
        ShortToolTip = "归还Actor到对象池",
        CompactNodeTitle = "归还Actor",
        BlueprintInternalUseOnly = "false"))
    void ReturnActorToPool(UPARAM(DisplayName = "Actor") AActor* Actor);

    // ✅ 可选的高级功能

    /**
     * 预热对象池
     * @param ActorClass Actor类型
     * @param Count 预创建的数量
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "预热对象池",
        ToolTip = "预先创建指定数量的Actor到池中"))
    void PrewarmPool(TSubclassOf<AActor> ActorClass, int32 Count);

    /**
     * 获取池统计信息
     * @param ActorClass Actor类型
     * @return 池的统计信息
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "获取池统计信息",
        ToolTip = "获取指定Actor类型的对象池使用统计"))
    FObjectPoolStats GetPoolStats(TSubclassOf<AActor> ActorClass);

    /**
     * 清空指定对象池
     * @param ActorClass Actor类型
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "清空对象池",
        ToolTip = "清空指定Actor类型的对象池，销毁所有池化Actor"))
    void ClearPool(TSubclassOf<AActor> ActorClass);

    /**
     * 清空所有对象池
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "清空所有对象池",
        ToolTip = "清空所有对象池，销毁所有池化Actor"))
    void ClearAllPools();

    /**
     * 获取所有池的统计信息
     * @return 所有池的统计信息数组
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "获取所有池统计",
        ToolTip = "获取所有对象池的统计信息"))
    TArray<FObjectPoolStats> GetAllPoolStats();

    /**
     * 检查Actor类是否已注册
     * @param ActorClass Actor类型
     * @return true if registered
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "检查类是否已注册",
        ToolTip = "检查指定Actor类是否已经注册到对象池"))
    bool IsActorClassRegistered(TSubclassOf<AActor> ActorClass);

    /**
     * 验证所有对象池的完整性
     * @return true if all pools are valid
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|对象池", meta = (
        DisplayName = "验证池完整性",
        ToolTip = "验证所有对象池的完整性和状态"))
    bool ValidateAllPools();

private:
    /**
     * 获取或创建指定Actor类的对象池
     * @param ActorClass Actor类型
     * @return 对象池指针
     */
    TSharedPtr<FActorPool> GetOrCreatePool(UClass* ActorClass);

    /**
     * 清理无效的对象池
     */
    void CleanupInvalidPools();

    /**
     * 初始化默认对象池
     */
    void InitializeDefaultPools();

    /**
     * 验证Actor类是否有效
     * @param ActorClass 要验证的Actor类
     * @return true if valid
     */
    bool ValidateActorClass(TSubclassOf<AActor> ActorClass) const;

    /**
     * 安全销毁Actor
     * @param Actor 要销毁的Actor
     */
    void SafeDestroyActor(AActor* Actor) const;

    /**
     * 多级回退机制：尝试从对象池获取Actor
     * @param ActorClass Actor类
     * @param SpawnTransform 生成Transform
     * @param World 世界实例
     * @return 从池获取的Actor，失败返回nullptr
     */
    AActor* TryGetFromPool(UClass* ActorClass, const FTransform& SpawnTransform, UWorld* World);

    /**
     * 多级回退机制：尝试直接创建Actor
     * @param ActorClass Actor类
     * @param SpawnTransform 生成Transform
     * @param World 世界实例
     * @return 新创建的Actor，失败返回nullptr
     */
    AActor* TryCreateDirectly(UClass* ActorClass, const FTransform& SpawnTransform, UWorld* World);

    /**
     * 多级回退机制：最后的回退策略
     * @param SpawnTransform 生成Transform
     * @param World 世界实例
     * @return 默认Actor实例
     */
    AActor* FallbackToDefault(const FTransform& SpawnTransform, UWorld* World);

    /**
     * 验证生成的Actor是否有效
     * @param Actor 要验证的Actor
     * @param ExpectedClass 期望的类型
     * @return true if valid
     */
    bool ValidateSpawnedActor(AActor* Actor, UClass* ExpectedClass) const;

private:
    /** 对象池映射表 - 从Actor类到对象池的映射 */
    TMap<UClass*, TSharedPtr<FActorPool>> ActorPools;

    /** 池访问锁 */
    mutable FCriticalSection PoolsLock;

    /** 子系统是否已初始化 */
    bool bIsInitialized;

    /** 默认池配置 */
    TArray<FObjectPoolConfig> DefaultPoolConfigs;

    /** 生命周期事件配置 */
    FObjectPoolLifecycleConfig LifecycleConfig;

    /** Actor状态重置管理器（单一职责，独立功能） */
    TSharedPtr<FActorStateResetter> StateResetter;

    /** 配置管理器（单一职责，独立功能） */
    TSharedPtr<FObjectPoolConfigManager> ConfigManager;

    /** 调试管理器（单一职责，独立功能） */
    TSharedPtr<FObjectPoolDebugManager> DebugManager;

    /** 性能监控定时器句柄 */
    FTimerHandle PerformanceMonitoringTimer;
};
