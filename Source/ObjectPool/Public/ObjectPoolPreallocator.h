// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include <atomic>

// ✅ 对象池模块依赖
#include "ObjectPoolTypes.h"



// ✅ 前向声明
class FActorPool;

/**
 * 智能预分配管理器
 * 
 * 职责：
 * - 管理对象池的智能预分配策略
 * - 执行渐进式、预测性预分配
 * - 监控内存使用和性能指标
 * - 提供动态调整建议
 * 
 * 设计原则：
 * - 单一职责：只负责预分配相关功能
 * - 松耦合：通过接口与FActorPool交互
 * - 可配置：支持多种预分配策略
 * - 线程安全：支持多线程环境
 */
class OBJECTPOOL_API FObjectPoolPreallocator
{
public:
    /**
     * 构造函数
     * @param InOwnerPool 所属的对象池
     */
    explicit FObjectPoolPreallocator(FActorPool* InOwnerPool);

    /** 析构函数 */
    ~FObjectPoolPreallocator();

    // ✅ 核心预分配接口

    /**
     * 启动智能预分配
     * @param World 世界上下文
     * @param Config 预分配配置
     * @return 是否成功启动
     */
    bool StartPreallocation(UWorld* World, const FObjectPoolConfig& Config);

    /**
     * 停止预分配
     */
    void StopPreallocation();

    /**
     * 更新预分配进度（每帧调用）
     * @param DeltaTime 帧时间间隔
     */
    void Tick(float DeltaTime);

    /**
     * 获取预分配统计信息
     * @return 统计数据
     */
    FObjectPoolPreallocationStats GetStats() const;

    /**
     * 是否正在预分配
     * @return true if preallocation is active
     */
    bool IsPreallocating() const { return bIsActive.load(); }

    // ✅ 动态调整接口

    /**
     * 检查是否需要动态调整
     * @param CurrentUsage 当前使用情况
     * @return 调整建议
     */
    struct FAdjustmentRecommendation
    {
        bool bShouldAdjust = false;
        bool bShouldExpand = false;
        int32 RecommendedSize = 0;
        FString Reason;
    };
    FAdjustmentRecommendation CheckAdjustmentNeeded(const FObjectPoolStats& CurrentUsage) const;

    /**
     * 记录使用模式（用于预测）
     * @param UsedCount 当前使用数量
     */
    void RecordUsagePattern(int32 UsedCount);

    /**
     * 预测下次需要的数量
     * @return 预测数量
     */
    int32 PredictRequiredCount() const;

    // ✅ 内存管理接口

    /**
     * 检查内存预算
     * @param EstimatedMemoryUsage 预估内存使用量
     * @return 是否在预算范围内
     */
    bool CheckMemoryBudget(int64 EstimatedMemoryUsage) const;

    /**
     * 计算单个Actor的估算内存大小
     * @param ActorClass Actor类型
     * @return 估算内存大小（字节）
     */
    int32 EstimateActorMemorySize(UClass* ActorClass) const;

private:
    // ✅ 预分配策略实现

    /**
     * 立即预分配策略
     * @param World 世界上下文
     * @param Count 分配数量
     */
    void ExecuteImmediatePreallocation(UWorld* World, int32 Count);

    /**
     * 渐进式预分配策略
     * @param World 世界上下文
     * @param DeltaTime 帧时间
     */
    void ExecuteProgressivePreallocation(UWorld* World, float DeltaTime);

    /**
     * 预测性预分配策略
     * @param World 世界上下文
     */
    void ExecutePredictivePreallocation(UWorld* World);

    /**
     * 自适应预分配策略
     * @param World 世界上下文
     * @param DeltaTime 帧时间
     */
    void ExecuteAdaptivePreallocation(UWorld* World, float DeltaTime);

    // ✅ 辅助方法

    /**
     * 创建单个Actor到池中
     * @param World 世界上下文
     * @return 是否成功创建
     */
    bool CreateSingleActor(UWorld* World);

    /**
     * 更新统计信息
     */
    void UpdateStats();

    /**
     * 检查是否应该继续预分配
     * @return true if should continue
     */
    bool ShouldContinuePreallocation() const;

private:
    /** 所属的对象池 */
    FActorPool* OwnerPool;

    /** 预分配配置 */
    FObjectPoolConfig Config;

    /** 统计信息 */
    mutable FObjectPoolPreallocationStats Stats;

    /** 是否激活 */
    std::atomic<bool> bIsActive{false};

    /** 当前进度 */
    std::atomic<int32> CurrentProgress{0};

    /** 累计时间 */
    float AccumulatedTime = 0.0f;

    /** 使用模式历史 */
    TArray<int32> UsageHistory;

    /** 最大历史记录数 */
    static constexpr int32 MaxHistorySize = 100;

    /** 线程安全锁 */
    mutable FCriticalSection PreallocatorLock;

    /** 上次调整时间 */
    double LastAdjustmentTime = 0.0;

    /** 性能监控 */
    struct FPerformanceMetrics
    {
        double AverageCreationTimeMs = 0.0;
        int32 CreationCount = 0;
        double TotalCreationTimeMs = 0.0;
    } PerformanceMetrics;
};
