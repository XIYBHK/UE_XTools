// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "HAL/CriticalSection.h"

// ✅ 对象池模块依赖
#include "ObjectPoolTypes.h"

/**
 * Actor状态重置管理器
 * 
 * 职责：
 * - 提供完整的Actor状态重置功能
 * - 支持各种组件类型的状态重置
 * - 可配置的重置策略
 * - 性能优化的批量重置
 * 
 * 设计原则：
 * - 单一职责：只负责Actor状态重置
 * - 通用性：不仅服务对象池，也可供其他系统使用
 * - 可扩展：支持自定义重置策略
 * - 高性能：优化的重置算法
 */
class OBJECTPOOL_API FActorStateResetter
{
public:
    /**
     * 构造函数
     */
    FActorStateResetter();

    /** 析构函数 */
    ~FActorStateResetter();

    // ✅ 核心重置接口

    /**
     * 完整重置Actor状态
     * @param Actor 目标Actor
     * @param SpawnTransform 新的Transform
     * @param ResetConfig 重置配置
     * @return 重置是否成功
     */
    bool ResetActorState(AActor* Actor, const FTransform& SpawnTransform, const FActorResetConfig& ResetConfig = FActorResetConfig());

    /**
     * 批量重置Actor状态
     * @param Actors Actor数组
     * @param Transforms Transform数组（可选，如果为空则保持当前Transform）
     * @param ResetConfig 重置配置
     * @return 成功重置的数量
     */
    int32 BatchResetActorStates(const TArray<AActor*>& Actors, const TArray<FTransform>& Transforms = TArray<FTransform>(), const FActorResetConfig& ResetConfig = FActorResetConfig());

    /**
     * 重置Actor到池化状态（归还时调用）
     * @param Actor 目标Actor
     * @return 重置是否成功
     */
    bool ResetActorForPooling(AActor* Actor);

    /**
     * 激活Actor从池化状态（获取时调用）
     * @param Actor 目标Actor
     * @param SpawnTransform 激活时的Transform
     * @return 激活是否成功
     */
    bool ActivateActorFromPool(AActor* Actor, const FTransform& SpawnTransform);

    // ✅ 分类重置方法

    /**
     * 重置基本Actor属性
     * @param Actor 目标Actor
     * @param bResetTransform 是否重置Transform
     * @param NewTransform 新的Transform（如果重置）
     */
    void ResetBasicProperties(AActor* Actor, bool bResetTransform = true, const FTransform& NewTransform = FTransform::Identity);

    /**
     * 重置物理状态
     * @param Actor 目标Actor
     */
    void ResetPhysicsState(AActor* Actor);

    /**
     * 重置所有组件状态
     * @param Actor 目标Actor
     * @param ResetConfig 重置配置
     */
    void ResetComponentStates(AActor* Actor, const FActorResetConfig& ResetConfig);

    /**
     * 清理定时器和事件绑定
     * @param Actor 目标Actor
     */
    void ClearTimersAndEvents(AActor* Actor);

    /**
     * 重置AI相关状态
     * @param Actor 目标Actor
     */
    void ResetAIState(AActor* Actor);

    /**
     * 重置动画状态
     * @param Actor 目标Actor
     */
    void ResetAnimationState(AActor* Actor);

    /**
     * 重置音频状态
     * @param Actor 目标Actor
     */
    void ResetAudioState(AActor* Actor);

    /**
     * 重置粒子系统状态
     * @param Actor 目标Actor
     */
    void ResetParticleState(AActor* Actor);

    /**
     * 重置网络相关状态
     * @param Actor 目标Actor
     */
    void ResetNetworkState(AActor* Actor);

    // ✅ 配置和统计

    /**
     * 设置默认重置配置
     * @param Config 新的默认配置
     */
    void SetDefaultResetConfig(const FActorResetConfig& Config);

    /**
     * 获取重置统计信息
     * @return 统计数据
     */
    FActorResetStats GetResetStats() const;

    /**
     * 重置统计信息
     */
    void ResetStats();

    /**
     * 注册自定义组件重置器
     * @param ComponentClass 组件类型
     * @param ResetFunction 重置函数
     */
    void RegisterCustomComponentResetter(UClass* ComponentClass, TFunction<void(UActorComponent*)> ResetFunction);

    /**
     * 注销自定义组件重置器
     * @param ComponentClass 组件类型
     */
    void UnregisterCustomComponentResetter(UClass* ComponentClass);

private:
    // ✅ 内部实现方法

    /**
     * 重置特定类型的组件
     * @param Component 目标组件
     * @param ResetConfig 重置配置
     */
    void ResetSingleComponent(UActorComponent* Component, const FActorResetConfig& ResetConfig);

    /**
     * 检查Actor是否需要重置
     * @param Actor 目标Actor
     * @return true if reset is needed
     */
    bool ShouldResetActor(AActor* Actor) const;

    /**
     * 更新重置统计
     * @param bSuccess 是否成功
     * @param ResetTimeMs 重置耗时
     */
    void UpdateResetStats(bool bSuccess, float ResetTimeMs);

    /**
     * 安全执行重置操作
     * @param ResetFunction 重置函数
     * @param Context 上下文描述
     * @return 是否成功
     */
    bool SafeExecuteReset(TFunction<void()> ResetFunction, const FString& Context);

private:
    /** 默认重置配置 */
    FActorResetConfig DefaultConfig;

    /** 重置统计信息 */
    mutable FActorResetStats ResetStatsData;

    /** 自定义组件重置器映射 */
    TMap<UClass*, TFunction<void(UActorComponent*)>> CustomComponentResetters;

    /** 线程安全锁 */
    mutable FCriticalSection ResetterLock;

    /** 性能监控 */
    struct FPerformanceMetrics
    {
        double TotalResetTimeMs = 0.0;
        int32 ResetCount = 0;
        double AverageResetTimeMs = 0.0;
        double MaxResetTimeMs = 0.0;
        double MinResetTimeMs = DBL_MAX;
    } PerformanceMetrics;
};
