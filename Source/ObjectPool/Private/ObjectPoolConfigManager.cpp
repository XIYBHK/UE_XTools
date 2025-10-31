/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#include "ObjectPoolConfigManager.h"
#include "ActorPool.h"
#include "ObjectPoolUtils.h"
#include "ObjectPool.h"

//  UE核心依赖
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"

//  日志和统计
DEFINE_LOG_CATEGORY(LogObjectPoolConfigManager);

#if STATS
DEFINE_STAT(STAT_ConfigManager_ValidateConfig);
DEFINE_STAT(STAT_ConfigManager_ApplyConfig);
#endif

//  构造函数和析构函数

FObjectPoolConfigManager::FObjectPoolConfigManager()
    : bIsInitialized(false)
{
    InitializeDefaultConfig();
    bIsInitialized = true;

    CONFIG_MANAGER_LOG(Log, TEXT("配置管理器已初始化"));
}

FObjectPoolConfigManager::~FObjectPoolConfigManager()
{
    if (bIsInitialized)
    {
        ClearAllConfigs();
        CONFIG_MANAGER_LOG(Log, TEXT("配置管理器已清理"));
    }
}

//  移动语义实现

FObjectPoolConfigManager::FObjectPoolConfigManager(FObjectPoolConfigManager&& Other) noexcept
    : ActorConfigs(MoveTemp(Other.ActorConfigs))
    , DefaultConfig(MoveTemp(Other.DefaultConfig))
    , bIsInitialized(Other.bIsInitialized)
{
    Other.bIsInitialized = false;
}

FObjectPoolConfigManager& FObjectPoolConfigManager::operator=(FObjectPoolConfigManager&& Other) noexcept
{
    if (this != &Other)
    {
        ClearAllConfigs();
        
        ActorConfigs = MoveTemp(Other.ActorConfigs);
        DefaultConfig = MoveTemp(Other.DefaultConfig);
        bIsInitialized = Other.bIsInitialized;
        
        Other.bIsInitialized = false;
    }
    return *this;
}

//  配置管理功能实现

bool FObjectPoolConfigManager::SetConfig(UClass* ActorClass, const FObjectPoolConfig& Config)
{
    if (!ValidateActorClass(ActorClass))
    {
        return false;
    }

    FString ErrorMessage;
    if (!ValidateConfig(Config, ErrorMessage))
    {
        CONFIG_MANAGER_LOG(Warning, TEXT("设置配置失败: %s, 错误: %s"), 
            *ActorClass->GetName(), *ErrorMessage);
        return false;
    }

    FScopeLock Lock(&ConfigLock);

    //  使用FindOrAdd避免重复添加时的问题
    FObjectPoolConfig& ExistingConfig = ActorConfigs.FindOrAdd(ActorClass);
    ExistingConfig = Config;

    CONFIG_MANAGER_LOG(Log, TEXT("设置配置: %s, 初始大小=%d, 硬限制=%d"),
        *ActorClass->GetName(), Config.InitialSize, Config.HardLimit);

    return true;
}

FObjectPoolConfig FObjectPoolConfigManager::GetConfig(UClass* ActorClass) const
{
    if (!ValidateActorClass(ActorClass))
    {
        return DefaultConfig;
    }

    FScopeLock Lock(&ConfigLock);

    if (const FObjectPoolConfig* Config = ActorConfigs.Find(ActorClass))
    {
        return *Config;
    }

    // 如果没有特定配置，生成推荐配置
    return GenerateRecommendedConfig(ActorClass);
}

bool FObjectPoolConfigManager::RemoveConfig(UClass* ActorClass)
{
    if (!ValidateActorClass(ActorClass))
    {
        return false;
    }

    FScopeLock Lock(&ConfigLock);
    bool bRemoved = ActorConfigs.Remove(ActorClass) > 0;

    if (bRemoved)
    {
        CONFIG_MANAGER_LOG(Log, TEXT("移除配置: %s"), *ActorClass->GetName());
    }

    return bRemoved;
}

bool FObjectPoolConfigManager::HasConfig(UClass* ActorClass) const
{
    if (!ValidateActorClass(ActorClass))
    {
        return false;
    }

    FScopeLock Lock(&ConfigLock);
    return ActorConfigs.Contains(ActorClass);
}

void FObjectPoolConfigManager::ClearAllConfigs()
{
    FScopeLock Lock(&ConfigLock);
    ActorConfigs.Empty();

    CONFIG_MANAGER_LOG(Log, TEXT("清空所有配置"));
}

//  默认配置管理实现

FObjectPoolConfig FObjectPoolConfigManager::GetDefaultConfig() const
{
    FScopeLock Lock(&ConfigLock);
    return DefaultConfig;
}

void FObjectPoolConfigManager::SetDefaultConfig(const FObjectPoolConfig& Config)
{
    FString ErrorMessage;
    if (!ValidateConfig(Config, ErrorMessage))
    {
        CONFIG_MANAGER_LOG(Warning, TEXT("设置默认配置失败: %s"), *ErrorMessage);
        return;
    }

    FScopeLock Lock(&ConfigLock);
    DefaultConfig = Config;

    CONFIG_MANAGER_LOG(Log, TEXT("设置默认配置: 初始大小=%d, 硬限制=%d"), 
        Config.InitialSize, Config.HardLimit);
}

FObjectPoolConfig FObjectPoolConfigManager::GenerateRecommendedConfig(UClass* ActorClass, EConfigStrategy Strategy) const
{
    if (!ValidateActorClass(ActorClass))
    {
        return DefaultConfig;
    }

    // 生成基础配置
    FObjectPoolConfig BaseConfig = GenerateBaseConfig(ActorClass);

    // 应用策略
    return ApplyStrategy(BaseConfig, Strategy);
}

//  配置验证实现

bool FObjectPoolConfigManager::ValidateConfig(const FObjectPoolConfig& Config, FString& OutErrorMessage) const
{
    SCOPE_CYCLE_COUNTER(STAT_ConfigManager_ValidateConfig);

    OutErrorMessage.Empty();

    // 验证Actor类
    if (!IsValid(Config.ActorClass))
    {
        OutErrorMessage = TEXT("Actor类无效");
        return false;
    }

    if (!Config.ActorClass->IsChildOf<AActor>())
    {
        OutErrorMessage = TEXT("类不是Actor的子类");
        return false;
    }

    // 验证大小参数
    if (Config.InitialSize < 0)
    {
        OutErrorMessage = TEXT("初始大小不能为负数");
        return false;
    }

    if (Config.HardLimit < 0)
    {
        OutErrorMessage = TEXT("硬限制不能为负数");
        return false;
    }

    if (Config.HardLimit > 0 && Config.InitialSize > Config.HardLimit)
    {
        OutErrorMessage = TEXT("初始大小不能大于硬限制");
        return false;
    }

    // 验证合理性
    if (Config.InitialSize > 1000)
    {
        OutErrorMessage = TEXT("初始大小过大（>1000）");
        return false;
    }

    if (Config.HardLimit > 10000)
    {
        OutErrorMessage = TEXT("硬限制过大（>10000）");
        return false;
    }

    return true;
}

FObjectPoolConfig FObjectPoolConfigManager::FixInvalidConfig(const FObjectPoolConfig& Config) const
{
    FObjectPoolConfig FixedConfig = Config;

    // 修复Actor类
    if (!IsValid(FixedConfig.ActorClass) || !FixedConfig.ActorClass->IsChildOf<AActor>())
    {
        FixedConfig.ActorClass = AActor::StaticClass();
    }

    // 修复大小参数
    if (FixedConfig.InitialSize < 0)
    {
        FixedConfig.InitialSize = DEFAULT_INITIAL_SIZE;
    }

    if (FixedConfig.HardLimit < 0)
    {
        FixedConfig.HardLimit = DEFAULT_HARD_LIMIT;
    }

    // 修复大小关系
    if (FixedConfig.HardLimit > 0 && FixedConfig.InitialSize > FixedConfig.HardLimit)
    {
        FixedConfig.InitialSize = FixedConfig.HardLimit;
    }

    // 修复过大的值
    if (FixedConfig.InitialSize > 1000)
    {
        FixedConfig.InitialSize = 1000;
    }

    if (FixedConfig.HardLimit > 10000)
    {
        FixedConfig.HardLimit = 10000;
    }

    CONFIG_MANAGER_LOG(Log, TEXT("修复配置: %s"), 
        FixedConfig.ActorClass ? *FixedConfig.ActorClass->GetName() : TEXT("Unknown"));

    return FixedConfig;
}

//  配置应用实现

bool FObjectPoolConfigManager::ApplyConfigToPool(FActorPool& Pool, const FObjectPoolConfig& Config) const
{
    SCOPE_CYCLE_COUNTER(STAT_ConfigManager_ApplyConfig);

    FString ErrorMessage;
    if (!ValidateConfig(Config, ErrorMessage))
    {
        CONFIG_MANAGER_LOG(Warning, TEXT("应用配置失败: %s"), *ErrorMessage);
        return false;
    }

    // 应用最大大小限制
    if (Config.HardLimit > 0)
    {
        Pool.SetMaxSize(Config.HardLimit);
    }

    CONFIG_MANAGER_LOG(Log, TEXT("应用配置到池: %s"), 
        Config.ActorClass ? *Config.ActorClass->GetName() : TEXT("Unknown"));

    return true;
}

FObjectPoolConfig FObjectPoolConfigManager::ExtractConfigFromPool(const FActorPool& Pool) const
{
    FObjectPoolConfig Config;
    Config.ActorClass = Pool.GetActorClass();
    Config.InitialSize = Pool.GetPoolSize(); // 当前大小作为初始大小
    Config.HardLimit = 0; // 无法从池中获取硬限制

    return Config;
}

//  统计和分析实现

FString FObjectPoolConfigManager::GetConfigStats() const
{
    FScopeLock Lock(&ConfigLock);

    return FString::Printf(TEXT(
        "=== 配置管理器统计 ===\n"
        "已配置Actor类数量: %d\n"
        "默认初始大小: %d\n"
        "默认硬限制: %d\n"
    ),
        ActorConfigs.Num(),
        DefaultConfig.InitialSize,
        DefaultConfig.HardLimit
    );
}

FString FObjectPoolConfigManager::AnalyzeConfigUsage() const
{
    FScopeLock Lock(&ConfigLock);

    if (ActorConfigs.Num() == 0)
    {
        return TEXT("暂无配置数据可分析");
    }

    int32 TotalInitialSize = 0;
    int32 TotalHardLimit = 0;
    int32 ValidConfigs = 0;

    for (const auto& ConfigPair : ActorConfigs)
    {
        const FObjectPoolConfig& Config = ConfigPair.Value;
        TotalInitialSize += Config.InitialSize;
        TotalHardLimit += Config.HardLimit;
        ++ValidConfigs;
    }

    float AvgInitialSize = ValidConfigs > 0 ? static_cast<float>(TotalInitialSize) / ValidConfigs : 0.0f;
    float AvgHardLimit = ValidConfigs > 0 ? static_cast<float>(TotalHardLimit) / ValidConfigs : 0.0f;

    return FString::Printf(TEXT(
        "=== 配置使用分析 ===\n"
        "配置数量: %d\n"
        "平均初始大小: %.1f\n"
        "平均硬限制: %.1f\n"
        "总预计初始对象数: %d\n"
        "总预计最大对象数: %d\n"
    ),
        ValidConfigs,
        AvgInitialSize,
        AvgHardLimit,
        TotalInitialSize,
        TotalHardLimit
    );
}

//  内部辅助方法实现

void FObjectPoolConfigManager::InitializeDefaultConfig()
{
    DefaultConfig.ActorClass = AActor::StaticClass();
    DefaultConfig.InitialSize = DEFAULT_INITIAL_SIZE;
    DefaultConfig.HardLimit = DEFAULT_HARD_LIMIT;

    CONFIG_MANAGER_LOG(Log, TEXT("初始化默认配置: 初始大小=%d, 硬限制=%d"),
        DefaultConfig.InitialSize, DefaultConfig.HardLimit);
}

FObjectPoolConfig FObjectPoolConfigManager::GenerateBaseConfig(UClass* ActorClass) const
{
    FObjectPoolConfig Config;
    Config.ActorClass = ActorClass;

    // 根据Actor类型调整基础配置
    if (ActorClass->IsChildOf<ACharacter>())
    {
        // Character通常更复杂，使用较小的池
        Config.InitialSize = 5;
        Config.HardLimit = 50;
    }
    else if (ActorClass->IsChildOf<APawn>())
    {
        // Pawn中等复杂度
        Config.InitialSize = 8;
        Config.HardLimit = 80;
    }
    else
    {
        // 普通Actor使用默认配置
        Config.InitialSize = DEFAULT_INITIAL_SIZE;
        Config.HardLimit = DEFAULT_HARD_LIMIT;
    }

    return Config;
}

FObjectPoolConfig FObjectPoolConfigManager::ApplyStrategy(const FObjectPoolConfig& BaseConfig, EConfigStrategy Strategy) const
{
    FObjectPoolConfig Config = BaseConfig;

    switch (Strategy)
    {
    case EConfigStrategy::PerformanceFirst:
        // 性能优先：更大的初始池，更高的限制
        Config.InitialSize = FMath::Max(Config.InitialSize * 2, PERFORMANCE_INITIAL_SIZE);
        Config.HardLimit = FMath::Max(Config.HardLimit * 2, 200);
        break;

    case EConfigStrategy::MemoryFirst:
        // 内存优先：更小的池
        Config.InitialSize = FMath::Max(Config.InitialSize / 2, 2);
        Config.HardLimit = FMath::Min(Config.HardLimit, MEMORY_HARD_LIMIT);
        break;

    case EConfigStrategy::Balanced:
        // 平衡策略：轻微调整
        Config.InitialSize = FMath::Max(Config.InitialSize, 5);
        Config.HardLimit = FMath::Max(Config.HardLimit, 50);
        break;

    case EConfigStrategy::Default:
    case EConfigStrategy::Custom:
    default:
        // 保持基础配置不变
        break;
    }

    return Config;
}

bool FObjectPoolConfigManager::ValidateActorClass(UClass* ActorClass) const
{
    if (!IsValid(ActorClass))
    {
        return false;
    }

    if (!ActorClass->IsChildOf<AActor>())
    {
        CONFIG_MANAGER_LOG(Warning, TEXT("类不是Actor的子类: %s"), *ActorClass->GetName());
        return false;
    }

    return true;
}

FString FObjectPoolConfigManager::GetStrategyName(EConfigStrategy Strategy)
{
    switch (Strategy)
    {
    case EConfigStrategy::Default:
        return TEXT("默认策略");
    case EConfigStrategy::PerformanceFirst:
        return TEXT("性能优先");
    case EConfigStrategy::MemoryFirst:
        return TEXT("内存优先");
    case EConfigStrategy::Balanced:
        return TEXT("平衡策略");
    case EConfigStrategy::Custom:
        return TEXT("自定义策略");
    default:
        return TEXT("未知策略");
    }
}