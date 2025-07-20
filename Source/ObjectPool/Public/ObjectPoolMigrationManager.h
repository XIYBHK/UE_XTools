// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "HAL/CriticalSection.h"

// ✅ 迁移配置依赖
#include "ObjectPoolMigrationConfig.h"

// ✅ 对象池依赖
#include "ObjectPoolTypesSimplified.h"

// ✅ 向后兼容性支持（废弃）
#if OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION != 1
#include "ObjectPoolTypes.h"
#endif

// 前向声明
class UObjectPoolSubsystem;
class UObjectPoolSubsystemSimplified;
class AActor;

/**
 * 对象池迁移管理器
 * 
 * 设计原则：
 * - 单例模式：全局唯一的迁移管理器
 * - 线程安全：支持多线程环境下的安全切换
 * - 状态管理：跟踪迁移状态和统计信息
 * - 验证支持：提供新旧实现的行为验证
 * 
 * 职责：
 * - 运行时实现切换
 * - 迁移状态管理
 * - 性能数据收集
 * - 兼容性验证
 * - A/B测试支持
 */
class OBJECTPOOL_API FObjectPoolMigrationManager
{
public:
    /**
     * 实现类型枚举
     */
    enum class EImplementationType : uint8
    {
        Original,       // 原始实现
        Simplified,     // 简化实现
        Auto            // 自动选择（基于配置）
    };

    /**
     * 迁移状态枚举
     */
    enum class EMigrationState : uint8
    {
        NotStarted,     // 未开始迁移
        InProgress,     // 迁移进行中
        Completed,      // 迁移完成
        RolledBack,     // 已回滚
        Testing         // A/B测试中
    };

    /**
     * 迁移统计信息
     */
    struct FMigrationStats
    {
        /** 迁移开始时间 */
        double MigrationStartTime;

        /** 迁移完成时间 */
        double MigrationEndTime;

        /** 使用原始实现的调用次数 */
        int32 OriginalImplementationCalls;

        /** 使用简化实现的调用次数 */
        int32 SimplifiedImplementationCalls;

        /** 兼容性检查通过次数 */
        int32 CompatibilityChecksPassed;

        /** 兼容性检查失败次数 */
        int32 CompatibilityChecksFailed;

        /** 性能对比次数 */
        int32 PerformanceComparisons;

        /** 平均性能提升（百分比） */
        float AveragePerformanceImprovement;

        FMigrationStats()
            : MigrationStartTime(0.0)
            , MigrationEndTime(0.0)
            , OriginalImplementationCalls(0)
            , SimplifiedImplementationCalls(0)
            , CompatibilityChecksPassed(0)
            , CompatibilityChecksFailed(0)
            , PerformanceComparisons(0)
            , AveragePerformanceImprovement(0.0f)
        {
        }
    };

    /**
     * 性能对比结果
     */
    struct FPerformanceComparisonResult
    {
        /** 原始实现耗时 */
        double OriginalTime;

        /** 简化实现耗时 */
        double SimplifiedTime;

        /** 性能提升百分比 */
        float ImprovementPercentage;

        /** 操作类型 */
        FString OperationType;

        /** 测试时间 */
        double TestTime;

        FPerformanceComparisonResult()
            : OriginalTime(0.0)
            , SimplifiedTime(0.0)
            , ImprovementPercentage(0.0f)
            , TestTime(0.0)
        {
        }
    };

public:
    /**
     * 获取单例实例
     */
    static FObjectPoolMigrationManager& Get();

    /**
     * 销毁单例实例
     */
    static void Shutdown();

private:
    /**
     * 私有构造函数（单例模式）
     */
    FObjectPoolMigrationManager();

    /**
     * 私有析构函数
     */
    ~FObjectPoolMigrationManager();

    // 禁用拷贝构造和赋值
    FObjectPoolMigrationManager(const FObjectPoolMigrationManager&) = delete;
    FObjectPoolMigrationManager& operator=(const FObjectPoolMigrationManager&) = delete;

public:
    // ✅ 实现选择和切换

    /**
     * 检查当前是否使用简化实现
     */
    static bool IsUsingSimplifiedImplementation();

    /**
     * 设置当前使用的实现类型
     * @param ImplementationType 实现类型
     * @return 是否设置成功
     */
    bool SetImplementationType(EImplementationType ImplementationType);

    /**
     * 获取当前实现类型
     */
    EImplementationType GetCurrentImplementationType() const;

    /**
     * 切换到简化实现
     * @return 是否切换成功
     */
    bool SwitchToSimplifiedImplementation();

    /**
     * 切换到原始实现
     * @return 是否切换成功
     */
    bool SwitchToOriginalImplementation();

    /**
     * 切换实现类型
     * @return 是否切换成功
     */
    bool ToggleImplementation();

    // ✅ 迁移状态管理

    /**
     * 开始迁移过程
     */
    void StartMigration();

    /**
     * 完成迁移过程
     */
    void CompleteMigration();

    /**
     * 回滚迁移
     */
    void RollbackMigration();

    /**
     * 获取迁移状态
     */
    EMigrationState GetMigrationState() const;

    /**
     * 检查是否正在迁移中
     */
    bool IsMigrationInProgress() const;

    // ✅ 统计和监控

    /**
     * 获取迁移统计信息
     */
    FMigrationStats GetMigrationStats() const;

    /**
     * 重置统计信息
     */
    void ResetStats();

    /**
     * 记录实现调用
     * @param ImplementationType 实现类型
     */
    void RecordImplementationCall(EImplementationType ImplementationType);

    /**
     * 记录兼容性检查结果
     * @param bPassed 是否通过
     */
    void RecordCompatibilityCheck(bool bPassed);

    /**
     * 记录性能对比结果
     * @param Result 对比结果
     */
    void RecordPerformanceComparison(const FPerformanceComparisonResult& Result);

    // ✅ 验证和测试

    /**
     * 启用A/B测试模式
     * @param TestRatio 使用简化实现的比例（0.0-1.0）
     */
    void EnableABTesting(float TestRatio = 0.5f);

    /**
     * 禁用A/B测试模式
     */
    void DisableABTesting();

    /**
     * 检查是否启用A/B测试
     */
    bool IsABTestingEnabled() const;

    /**
     * 获取A/B测试的实现选择
     * @return 应该使用的实现类型
     */
    EImplementationType GetABTestImplementation() const;

    /**
     * 验证两个实现的行为一致性
     * @param ActorClass Actor类
     * @param TestCount 测试次数
     * @return 验证结果
     */
    bool ValidateImplementationConsistency(UClass* ActorClass, int32 TestCount = 10);

    // ✅ 配置和信息

    /**
     * 获取配置摘要
     */
    FString GetConfigurationSummary() const;

    /**
     * 获取迁移报告
     */
    FString GenerateMigrationReport() const;

    /**
     * 获取性能对比报告
     */
    FString GeneratePerformanceReport() const;

    /**
     * 检查迁移配置是否有效
     */
    bool IsConfigurationValid() const;

private:
    // ✅ 内部数据成员

    /** 当前实现类型 */
    EImplementationType CurrentImplementationType;

    /** 迁移状态 */
    EMigrationState MigrationState;

    /** 迁移统计信息 */
    FMigrationStats Stats;

    /** 性能对比结果历史 */
    TArray<FPerformanceComparisonResult> PerformanceHistory;

    /** 是否启用A/B测试 */
    bool bABTestingEnabled;

    /** A/B测试比例 */
    float ABTestRatio;

    /** 线程安全锁 */
    mutable FCriticalSection MigrationLock;

    /** 单例实例 */
    static FObjectPoolMigrationManager* Instance;

private:
    // ✅ 内部辅助方法

    /**
     * 初始化迁移管理器
     */
    void Initialize();

    /**
     * 清理迁移管理器
     */
    void Cleanup();

    /**
     * 更新性能统计
     */
    void UpdatePerformanceStats();

    /**
     * 验证配置一致性
     */
    bool ValidateConfiguration() const;

    /**
     * 获取实现类型名称
     */
    static FString GetImplementationTypeName(EImplementationType Type);

    /**
     * 获取迁移状态名称
     */
    static FString GetMigrationStateName(EMigrationState State);
};

// 注意：日志分类已在ObjectPoolMigrationConfig.h中声明

/**
 * 迁移管理器的性能统计宏
 */
#if STATS
DECLARE_CYCLE_STAT_EXTERN(TEXT("MigrationManager_SwitchImplementation"), STAT_MigrationManager_SwitchImplementation, STATGROUP_Game, OBJECTPOOL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("MigrationManager_ValidateConsistency"), STAT_MigrationManager_ValidateConsistency, STATGROUP_Game, OBJECTPOOL_API);
#endif
