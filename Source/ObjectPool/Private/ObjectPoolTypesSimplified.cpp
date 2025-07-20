// Fill out your copyright notice in the Description page of Project Settings.

#include "ObjectPoolTypesSimplified.h"
#include "ObjectPool.h"

// 注意：转换函数的实现将在后续任务中添加，避免循环依赖

// ✅ 为简化类型提供一些实用的静态方法

/**
 * FObjectPoolConfigSimplified 的静态工厂方法
 */
namespace ObjectPoolConfigFactory
{
    /**
     * 创建子弹类型的默认配置
     */
    FObjectPoolConfigSimplified CreateBulletConfig(TSubclassOf<AActor> BulletClass)
    {
        return FObjectPoolConfigSimplified(BulletClass, 50, 200);
    }

    /**
     * 创建敌人类型的默认配置
     */
    FObjectPoolConfigSimplified CreateEnemyConfig(TSubclassOf<AActor> EnemyClass)
    {
        return FObjectPoolConfigSimplified(EnemyClass, 20, 100);
    }

    /**
     * 创建特效类型的默认配置
     */
    FObjectPoolConfigSimplified CreateEffectConfig(TSubclassOf<AActor> EffectClass)
    {
        return FObjectPoolConfigSimplified(EffectClass, 15, 50);
    }

    /**
     * 创建拾取物类型的默认配置
     */
    FObjectPoolConfigSimplified CreatePickupConfig(TSubclassOf<AActor> PickupClass)
    {
        return FObjectPoolConfigSimplified(PickupClass, 10, 30);
    }
}

/**
 * FObjectPoolStatsSimplified 的实用方法
 */
namespace ObjectPoolStatsUtils
{
    /**
     * 检查池是否健康
     */
    bool IsPoolHealthy(const FObjectPoolStatsSimplified& Stats)
    {
        // 基本健康检查
        if (Stats.HitRate < 0.3f && Stats.TotalCreated > 10)
        {
            // 命中率过低
            return false;
        }
        
        if (Stats.CurrentAvailable > Stats.TotalCreated * 0.8f && Stats.TotalCreated > 20)
        {
            // 太多未使用的对象
            return false;
        }
        
        return true;
    }

    /**
     * 获取池的健康状态描述
     */
    FString GetHealthDescription(const FObjectPoolStatsSimplified& Stats)
    {
        if (IsPoolHealthy(Stats))
        {
            return TEXT("健康");
        }
        
        TArray<FString> Issues;
        
        if (Stats.HitRate < 0.3f && Stats.TotalCreated > 10)
        {
            Issues.Add(FString::Printf(TEXT("命中率过低(%.1f%%)"), Stats.HitRate * 100.0f));
        }
        
        if (Stats.CurrentAvailable > Stats.TotalCreated * 0.8f && Stats.TotalCreated > 20)
        {
            Issues.Add(TEXT("过多未使用对象"));
        }
        
        return FString::Join(Issues, TEXT(", "));
    }

    /**
     * 生成性能建议
     */
    TArray<FString> GetPerformanceSuggestions(const FObjectPoolStatsSimplified& Stats)
    {
        TArray<FString> Suggestions;
        
        if (Stats.HitRate < 0.5f && Stats.TotalCreated > 10)
        {
            Suggestions.Add(TEXT("建议增加初始池大小以提高命中率"));
        }
        
        if (Stats.CurrentAvailable > Stats.TotalCreated * 0.7f && Stats.TotalCreated > 20)
        {
            Suggestions.Add(TEXT("建议启用自动收缩以减少内存使用"));
        }
        
        if (Stats.PoolSize > 100)
        {
            Suggestions.Add(TEXT("池大小较大，考虑分析使用模式"));
        }
        
        if (Stats.TotalCreated == Stats.CurrentActive && Stats.CurrentAvailable == 0)
        {
            Suggestions.Add(TEXT("池可能过小，考虑增加最大限制"));
        }
        
        return Suggestions;
    }
}

/**
 * 调试信息的实用方法
 */
namespace ObjectPoolDebugUtils
{
    /**
     * 创建基本的调试信息
     */
    FObjectPoolDebugInfoSimplified CreateDebugInfo(const FString& PoolName, const FObjectPoolStatsSimplified& Stats)
    {
        FObjectPoolDebugInfoSimplified DebugInfo;
        DebugInfo.PoolName = PoolName;
        DebugInfo.Stats = Stats;
        
        // 检查健康状态
        if (!ObjectPoolStatsUtils::IsPoolHealthy(Stats))
        {
            DebugInfo.bIsHealthy = false;
            DebugInfo.AddWarning(ObjectPoolStatsUtils::GetHealthDescription(Stats));
        }
        
        // 添加性能建议作为警告
        TArray<FString> Suggestions = ObjectPoolStatsUtils::GetPerformanceSuggestions(Stats);
        for (const FString& Suggestion : Suggestions)
        {
            DebugInfo.AddWarning(Suggestion);
        }
        
        return DebugInfo;
    }

    /**
     * 格式化调试信息为字符串
     */
    FString FormatDebugInfo(const FObjectPoolDebugInfoSimplified& DebugInfo)
    {
        FString Result = FString::Printf(TEXT("=== 对象池调试信息: %s ===\n"), *DebugInfo.PoolName);
        Result += FString::Printf(TEXT("状态: %s\n"), DebugInfo.bIsHealthy ? TEXT("健康") : TEXT("需要注意"));
        Result += FString::Printf(TEXT("统计: %s\n"), *DebugInfo.Stats.ToString());
        
        if (DebugInfo.Warnings.Num() > 0)
        {
            Result += TEXT("警告/建议:\n");
            for (int32 i = 0; i < DebugInfo.Warnings.Num(); i++)
            {
                Result += FString::Printf(TEXT("  %d. %s\n"), i + 1, *DebugInfo.Warnings[i]);
            }
        }
        
        return Result;
    }
}

/**
 * 全局的简化类型验证函数
 */
namespace ObjectPoolValidation
{
    /**
     * 验证配置的有效性
     */
    bool ValidateConfig(const FObjectPoolConfigSimplified& Config, FString& OutErrorMessage)
    {
        if (!Config.ActorClass)
        {
            OutErrorMessage = TEXT("Actor类不能为空");
            return false;
        }
        
        if (Config.InitialSize <= 0)
        {
            OutErrorMessage = TEXT("初始大小必须大于0");
            return false;
        }
        
        if (Config.HardLimit > 0 && Config.HardLimit < Config.InitialSize)
        {
            OutErrorMessage = TEXT("硬限制不能小于初始大小");
            return false;
        }
        
        if (Config.InitialSize > 1000)
        {
            OutErrorMessage = TEXT("初始大小过大，建议不超过1000");
            return false;
        }
        
        return true;
    }

    /**
     * 验证统计数据的一致性
     */
    bool ValidateStats(const FObjectPoolStatsSimplified& Stats, FString& OutErrorMessage)
    {
        if (Stats.CurrentActive < 0 || Stats.CurrentAvailable < 0 || Stats.TotalCreated < 0)
        {
            OutErrorMessage = TEXT("统计数据不能为负数");
            return false;
        }
        
        if (Stats.PoolSize != Stats.CurrentActive + Stats.CurrentAvailable)
        {
            OutErrorMessage = TEXT("池大小与活跃+可用数量不匹配");
            return false;
        }
        
        if (Stats.HitRate < 0.0f || Stats.HitRate > 1.0f)
        {
            OutErrorMessage = TEXT("命中率必须在0-1之间");
            return false;
        }
        
        if (Stats.TotalCreated < Stats.PoolSize)
        {
            OutErrorMessage = TEXT("总创建数不能小于池大小");
            return false;
        }
        
        return true;
    }
}
