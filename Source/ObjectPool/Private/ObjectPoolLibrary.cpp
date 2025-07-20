// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 遵循IWYU原则的头文件包含
#include "ObjectPoolLibrary.h"

// ✅ UE核心依赖
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

// ✅ 对象池模块依赖
#include "ObjectPool.h"
#include "ObjectPoolSubsystem.h"
#include "ObjectPoolSubsystemSimplified.h"
#include "ObjectPoolMigrationManager.h"

bool UObjectPoolLibrary::RegisterActorClass(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, int32 InitialSize, int32 HardLimit)
{
    // ✅ 优先使用简化子系统
    UObjectPoolSubsystemSimplified* SimplifiedSubsystem = GetSimplifiedSubsystemSafe(WorldContext);
    if (SimplifiedSubsystem)
    {
        // 使用简化子系统的配置方法
        FObjectPoolConfigSimplified Config;
        Config.ActorClass = ActorClass;
        Config.InitialSize = InitialSize;
        Config.HardLimit = HardLimit;

        bool bSuccess = SimplifiedSubsystem->SetPoolConfig(ActorClass, Config);

        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::RegisterActorClass (Simplified): %s, 结果: %s"),
            ActorClass ? *ActorClass->GetName() : TEXT("Invalid"),
            bSuccess ? TEXT("成功") : TEXT("失败"));

        return bSuccess;
    }

    // ✅ 回退到原始子系统
    UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::RegisterActorClass: 无法获取对象池子系统"));
        return false;
    }

    // ✅ 调用子系统方法（子系统内部已有完整的错误处理）
    Subsystem->RegisterActorClass(ActorClass, InitialSize, HardLimit);

    // ✅ 验证注册是否成功
    bool bSuccess = Subsystem->IsActorClassRegistered(ActorClass);

    OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::RegisterActorClass: %s, 结果: %s"),
        ActorClass ? *ActorClass->GetName() : TEXT("Invalid"),
        bSuccess ? TEXT("成功") : TEXT("失败"));

    return bSuccess;
}

AActor* UObjectPoolLibrary::SpawnActorFromPool(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform)
{
    // ✅ 优先使用简化子系统
    UObjectPoolSubsystemSimplified* SimplifiedSubsystem = GetSimplifiedSubsystemSafe(WorldContext);
    if (SimplifiedSubsystem)
    {
        // 使用简化子系统（已实现永不失败机制）
        AActor* Actor = SimplifiedSubsystem->SpawnActorFromPool(ActorClass, SpawnTransform);

        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::SpawnActorFromPool (Simplified): %s, 结果: %s"),
            ActorClass ? *ActorClass->GetName() : TEXT("Invalid"),
            Actor ? *Actor->GetName() : TEXT("Failed"));

        return Actor;
    }

    // ✅ 回退到原始子系统
    UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::SpawnActorFromPool: 无法获取对象池子系统，尝试直接创建"));
        
        // ✅ 多级回退机制：即使子系统不可用也要尝试创建Actor
        UWorld* World = nullptr;

        // 首先尝试从WorldContext获取
        if (WorldContext)
        {
            World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
        }

        // 如果失败，尝试使用子系统的智能获取方法
        if (!World)
        {
            if (UObjectPoolSubsystem* TempSubsystem = UObjectPoolSubsystem::Get(WorldContext))
            {
                World = TempSubsystem->GetValidWorld();
            }
        }

        if (World)
        {
            // 第一级回退：尝试创建指定类型
            if (ActorClass)
            {
                FActorSpawnParameters SpawnParams;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                AActor* Actor = World->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);
                if (Actor)
                {
                    OBJECTPOOL_LOG(Verbose, TEXT("UObjectPoolLibrary: 回退创建成功: %s"), *Actor->GetName());
                    return Actor;
                }
            }

            // 第二级回退：创建默认Actor
            FActorSpawnParameters DefaultParams;
            DefaultParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            DefaultParams.bNoFail = true;
            AActor* DefaultActor = World->SpawnActor<AActor>(AActor::StaticClass(), SpawnTransform, DefaultParams);
            if (DefaultActor)
            {
                OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary: 回退到默认Actor: %s"), *DefaultActor->GetName());
                return DefaultActor;
            }
        }

        // ✅ 最后的保障：遵循永不失败原则
        OBJECTPOOL_LOG(Error, TEXT("UObjectPoolLibrary: 所有回退机制都失败，创建静态紧急Actor"));

        // 创建一个静态的紧急Actor，确保永不返回nullptr
        static AActor* LibraryEmergencyActor = nullptr;
        if (!LibraryEmergencyActor)
        {
            LibraryEmergencyActor = NewObject<AActor>();
            LibraryEmergencyActor->AddToRoot(); // 防止被垃圾回收
        }
        return LibraryEmergencyActor;
    }

    // ✅ 调用子系统方法（已实现永不失败机制）
    AActor* Actor = Subsystem->SpawnActorFromPool(ActorClass, SpawnTransform);
    
    OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::SpawnActorFromPool: %s, 结果: %s"), 
        ActorClass ? *ActorClass->GetName() : TEXT("Invalid"),
        Actor ? *Actor->GetName() : TEXT("Failed"));
    
    return Actor;
}

void UObjectPoolLibrary::ReturnActorToPool(const UObject* WorldContext, AActor* Actor)
{
    // ✅ 优先使用简化子系统
    UObjectPoolSubsystemSimplified* SimplifiedSubsystem = GetSimplifiedSubsystemSafe(WorldContext);
    if (SimplifiedSubsystem)
    {
        // 使用简化子系统（已有完整的错误处理）
        SimplifiedSubsystem->ReturnActorToPool(Actor);

        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::ReturnActorToPool (Simplified): %s"),
            Actor ? *Actor->GetName() : TEXT("Invalid"));
        return;
    }

    // ✅ 回退到原始子系统
    UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::ReturnActorToPool: 无法获取对象池子系统，直接销毁Actor"));

        // ✅ 即使子系统不可用也要处理Actor（避免内存泄漏）
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
        return;
    }

    // ✅ 调用子系统方法（已有完整的错误处理）
    Subsystem->ReturnActorToPool(Actor);

    OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::ReturnActorToPool: %s"),
        Actor ? *Actor->GetName() : TEXT("Invalid"));
}

AActor* UObjectPoolLibrary::QuickSpawnActor(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, const FVector& Location, const FRotator& Rotation)
{
    // ✅ 构建Transform并调用标准方法
    FTransform SpawnTransform(Rotation, Location, FVector::OneVector);
    return SpawnActorFromPool(WorldContext, ActorClass, SpawnTransform);
}

int32 UObjectPoolLibrary::BatchSpawnActors(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, const TArray<FTransform>& SpawnTransforms, TArray<AActor*>& OutActors)
{
    OutActors.Empty();
    
    if (SpawnTransforms.Num() == 0)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::BatchSpawnActors: 空的Transform数组"));
        return 0;
    }

    // ✅ 预分配输出数组
    OutActors.Reserve(SpawnTransforms.Num());
    
    int32 SuccessCount = 0;
    
    // ✅ 批量生成Actor
    for (const FTransform& Transform : SpawnTransforms)
    {
        AActor* Actor = SpawnActorFromPool(WorldContext, ActorClass, Transform);
        if (Actor)
        {
            OutActors.Add(Actor);
            ++SuccessCount;
        }
        else
        {
            // ✅ 即使单个失败也继续处理其他的
            OutActors.Add(nullptr);
            OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::BatchSpawnActors: 生成Actor失败"));
        }
    }
    
    OBJECTPOOL_LOG(Verbose, TEXT("UObjectPoolLibrary::BatchSpawnActors: 请求 %d 个，成功 %d 个"), 
        SpawnTransforms.Num(), SuccessCount);
    
    return SuccessCount;
}

bool UObjectPoolLibrary::IsActorClassRegistered(const UObject* WorldContext, TSubclassOf<AActor> ActorClass)
{
    // ✅ 优先使用简化子系统
    UObjectPoolSubsystemSimplified* SimplifiedSubsystem = GetSimplifiedSubsystemSafe(WorldContext);
    if (SimplifiedSubsystem)
    {
        // 检查是否有池存在（简化子系统中池的存在即表示已注册）
        TSharedPtr<FActorPoolSimplified> Pool = SimplifiedSubsystem->GetPool(ActorClass);
        bool bRegistered = Pool.IsValid();

        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::IsActorClassRegistered (Simplified): %s, 结果: %s"),
            ActorClass ? *ActorClass->GetName() : TEXT("Invalid"),
            bRegistered ? TEXT("已注册") : TEXT("未注册"));

        return bRegistered;
    }

    // ✅ 回退到原始子系统
    UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::IsActorClassRegistered: 无法获取对象池子系统"));
        return false;
    }

    bool bRegistered = Subsystem->IsActorClassRegistered(ActorClass);

    OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::IsActorClassRegistered: %s, 结果: %s"),
        ActorClass ? *ActorClass->GetName() : TEXT("Invalid"),
        bRegistered ? TEXT("已注册") : TEXT("未注册"));

    return bRegistered;
}

FObjectPoolStats UObjectPoolLibrary::GetPoolStats(const UObject* WorldContext, TSubclassOf<AActor> ActorClass)
{
    // ✅ 优先使用简化子系统
    UObjectPoolSubsystemSimplified* SimplifiedSubsystem = GetSimplifiedSubsystemSafe(WorldContext);
    if (SimplifiedSubsystem)
    {
        // 获取简化统计信息并转换为兼容格式
        TSharedPtr<FActorPoolSimplified> Pool = SimplifiedSubsystem->GetPool(ActorClass);
        if (Pool.IsValid())
        {
            FObjectPoolStatsSimplified SimplifiedStats = Pool->GetStats();

            // 转换为原始格式以保持API兼容性
            FObjectPoolStats CompatStats;
            CompatStats.ActorClassName = SimplifiedStats.ActorClassName;
            CompatStats.PoolSize = SimplifiedStats.PoolSize;
            CompatStats.CurrentActive = SimplifiedStats.CurrentActive;
            CompatStats.CurrentAvailable = SimplifiedStats.CurrentAvailable;
            CompatStats.TotalCreated = SimplifiedStats.TotalCreated;
            CompatStats.HitRate = SimplifiedStats.HitRate;
            // 注意：简化版本没有TotalDestroyed, CreationTime, LastAccessTime字段
            // 使用默认值或计算值
            CompatStats.CreationTime = FDateTime::Now(); // 使用当前时间作为默认值
            CompatStats.LastUsedTime = FDateTime::Now(); // 使用当前时间作为默认值

            OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::GetPoolStats (Simplified): %s"), *CompatStats.ToString());
            return CompatStats;
        }

        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::GetPoolStats (Simplified): 池不存在"));
        return FObjectPoolStats();
    }

    // ✅ 回退到原始子系统
    UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::GetPoolStats: 无法获取对象池子系统"));
        return FObjectPoolStats();
    }

    FObjectPoolStats Stats = Subsystem->GetPoolStats(ActorClass);

    OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::GetPoolStats: %s"), *Stats.ToString());

    return Stats;
}

void UObjectPoolLibrary::DisplayPoolStats(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, bool bShowOnScreen, bool bPrintToLog, float DisplayDuration, FLinearColor TextColor)
{
    // ✅ 构建统计信息字符串
    FString StatsText;

    if (ActorClass)
    {
        // 显示单个池的统计信息
        FObjectPoolStats Stats = GetPoolStats(WorldContext, ActorClass);
        if (Stats.PoolSize > 0 || Stats.TotalCreated > 0)
        {
            StatsText = FString::Printf(TEXT("=== 对象池统计信息 ===\n%s"), *Stats.ToString());
        }
        else
        {
            StatsText = FString::Printf(TEXT("=== 对象池统计信息 ===\n对象池[%s]: 未找到或未初始化"),
                ActorClass ? *ActorClass->GetName() : TEXT("Unknown"));
        }
    }
    else
    {
        // 显示所有池的统计信息
        StatsText = TEXT("=== 所有对象池统计信息 ===\n");

        // ✅ 优先使用简化子系统
        UObjectPoolSubsystemSimplified* SimplifiedSubsystem = GetSimplifiedSubsystemSafe(WorldContext);
        if (SimplifiedSubsystem)
        {
            TArray<FObjectPoolStatsSimplified> AllStats = SimplifiedSubsystem->GetAllPoolStats();
            if (AllStats.Num() > 0)
            {
                for (const FObjectPoolStatsSimplified& PoolStats : AllStats)
                {
                    StatsText += FString::Printf(TEXT("%s\n"), *PoolStats.ToString());
                }

                // 添加汇总信息
                int32 TotalPools = AllStats.Num();
                int32 TotalActive = 0;
                int32 TotalAvailable = 0;
                int32 TotalCreated = 0;
                float AverageHitRate = 0.0f;

                for (const FObjectPoolStatsSimplified& PoolStats : AllStats)
                {
                    TotalActive += PoolStats.CurrentActive;
                    TotalAvailable += PoolStats.CurrentAvailable;
                    TotalCreated += PoolStats.TotalCreated;
                    AverageHitRate += PoolStats.HitRate;
                }

                if (TotalPools > 0)
                {
                    AverageHitRate /= TotalPools;
                }

                StatsText += FString::Printf(TEXT("\n=== 汇总信息 ===\n"));
                StatsText += FString::Printf(TEXT("总池数: %d\n"), TotalPools);
                StatsText += FString::Printf(TEXT("总活跃数: %d\n"), TotalActive);
                StatsText += FString::Printf(TEXT("总可用数: %d\n"), TotalAvailable);
                StatsText += FString::Printf(TEXT("总创建数: %d\n"), TotalCreated);
                StatsText += FString::Printf(TEXT("平均命中率: %.1f%%"), AverageHitRate * 100.0f);
            }
            else
            {
                StatsText += TEXT("当前没有活跃的对象池");
            }
        }
        else
        {
            // 回退到原始子系统
            UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
            if (Subsystem)
            {
                TArray<FObjectPoolStats> AllStats = Subsystem->GetAllPoolStats();
                if (AllStats.Num() > 0)
                {
                    for (const FObjectPoolStats& PoolStats : AllStats)
                    {
                        StatsText += FString::Printf(TEXT("%s\n"), *PoolStats.ToString());
                    }
                }
                else
                {
                    StatsText += TEXT("当前没有活跃的对象池");
                }
            }
            else
            {
                StatsText += TEXT("无法获取对象池子系统");
            }
        }
    }

    // ✅ 显示到屏幕（使用固定的键值确保不被覆盖）
    if (bShowOnScreen && GEngine)
    {
        // 使用非常小的负数作为键值，确保显示在最顶部且不被覆盖
        const int32 PoolStatsKey = INT32_MIN + 500; // 为池统计信息预留的键值
        const FColor DisplayColor = TextColor.ToFColor(true);

        GEngine->AddOnScreenDebugMessage(PoolStatsKey, DisplayDuration, DisplayColor, StatsText);
    }

    // ✅ 输出到日志
    if (bPrintToLog)
    {
        OBJECTPOOL_LOG(Warning, TEXT("\n%s"), *StatsText);
    }
}

bool UObjectPoolLibrary::PrewarmPool(const UObject* WorldContext, TSubclassOf<AActor> ActorClass, int32 Count)
{
    if (Count <= 0)
    {
        OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::PrewarmPool: 无效的预热数量: %d"), Count);
        return false;
    }

    // ✅ 优先使用简化子系统
    UObjectPoolSubsystemSimplified* SimplifiedSubsystem = GetSimplifiedSubsystemSafe(WorldContext);
    if (SimplifiedSubsystem)
    {
        // 使用简化子系统的预热方法
        int32 PrewarmedCount = SimplifiedSubsystem->PrewarmPool(ActorClass, Count);
        bool bSuccess = PrewarmedCount > 0;

        OBJECTPOOL_LOG(Verbose, TEXT("UObjectPoolLibrary::PrewarmPool (Simplified): %s, 请求: %d, 实际: %d"),
            ActorClass ? *ActorClass->GetName() : TEXT("Invalid"), Count, PrewarmedCount);

        return bSuccess;
    }

    // ✅ 回退到原始子系统
    UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::PrewarmPool: 无法获取对象池子系统"));
        return false;
    }

    // ✅ 调用子系统预热方法
    Subsystem->PrewarmPool(ActorClass, Count);

    OBJECTPOOL_LOG(Verbose, TEXT("UObjectPoolLibrary::PrewarmPool: %s, 数量: %d"),
        ActorClass ? *ActorClass->GetName() : TEXT("Invalid"), Count);

    return true;
}

bool UObjectPoolLibrary::ClearPool(const UObject* WorldContext, TSubclassOf<AActor> ActorClass)
{
    UObjectPoolSubsystem* Subsystem = GetSubsystemSafe(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::ClearPool: 无法获取对象池子系统"));
        return false;
    }

    // ✅ 调用子系统清空方法
    Subsystem->ClearPool(ActorClass);

    OBJECTPOOL_LOG(Verbose, TEXT("UObjectPoolLibrary::ClearPool: %s"),
        ActorClass ? *ActorClass->GetName() : TEXT("Invalid"));

    return true;
}

UObjectPoolSubsystem* UObjectPoolLibrary::GetObjectPoolSubsystem(const UObject* WorldContext)
{
    return GetSubsystemSafe(WorldContext);
}

UObjectPoolSubsystemSimplified* UObjectPoolLibrary::GetObjectPoolSubsystemSimplified(const UObject* WorldContext)
{
    return GetSimplifiedSubsystemSafe(WorldContext);
}

// ✅ 迁移管理功能实现

bool UObjectPoolLibrary::IsUsingSimplifiedImplementation()
{
    OBJECTPOOL_MIXED_MODE_CODE(
        return FObjectPoolMigrationManager::IsUsingSimplifiedImplementation();
    )

    // 编译时确定的实现
#if OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 1
    return true;
#elif OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 0
    return false;
#else
    return FObjectPoolMigrationManager::IsUsingSimplifiedImplementation();
#endif
}

bool UObjectPoolLibrary::SwitchToSimplifiedImplementation()
{
    OBJECTPOOL_MIXED_MODE_CODE(
        FObjectPoolMigrationManager& Manager = FObjectPoolMigrationManager::Get();
        bool bSuccess = Manager.SwitchToSimplifiedImplementation();

        OBJECTPOOL_LOG(Log, TEXT("UObjectPoolLibrary::SwitchToSimplifiedImplementation: %s"),
            bSuccess ? TEXT("成功") : TEXT("失败"));

        return bSuccess;
    )

    // 编译时固定实现时，切换操作无效
    OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::SwitchToSimplifiedImplementation: 编译时固定实现，无法切换"));
    return false;
}

bool UObjectPoolLibrary::SwitchToOriginalImplementation()
{
    OBJECTPOOL_MIXED_MODE_CODE(
        FObjectPoolMigrationManager& Manager = FObjectPoolMigrationManager::Get();
        bool bSuccess = Manager.SwitchToOriginalImplementation();

        OBJECTPOOL_LOG(Log, TEXT("UObjectPoolLibrary::SwitchToOriginalImplementation: %s"),
            bSuccess ? TEXT("成功") : TEXT("失败"));

        return bSuccess;
    )

    // 编译时固定实现时，切换操作无效
    OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::SwitchToOriginalImplementation: 编译时固定实现，无法切换"));
    return false;
}

bool UObjectPoolLibrary::ToggleImplementation()
{
    OBJECTPOOL_MIXED_MODE_CODE(
        FObjectPoolMigrationManager& Manager = FObjectPoolMigrationManager::Get();
        bool bSuccess = Manager.ToggleImplementation();

        OBJECTPOOL_LOG(Log, TEXT("UObjectPoolLibrary::ToggleImplementation: %s"),
            bSuccess ? TEXT("成功") : TEXT("失败"));

        return bSuccess;
    )

    // 编译时固定实现时，切换操作无效
    OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::ToggleImplementation: 编译时固定实现，无法切换"));
    return false;
}

FString UObjectPoolLibrary::GetMigrationConfigurationSummary()
{
    OBJECTPOOL_MIXED_MODE_CODE(
        FObjectPoolMigrationManager& Manager = FObjectPoolMigrationManager::Get();
        return Manager.GetConfigurationSummary();
    )

    // 编译时固定实现的配置摘要
    FString ConfigSummary = OBJECTPOOL_GET_CONFIG_SUMMARY();
    return FString::Printf(TEXT(
        "=== 对象池配置摘要（编译时固定） ===\n"
        "实现类型: %s\n"
        "编译时配置: %s\n"
        "运行时切换: 不支持\n"
    ),
#if OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 1
        TEXT("简化实现"),
#elif OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 0
        TEXT("原始实现"),
#else
        TEXT("混合模式"),
#endif
        *ConfigSummary
    );
}

FString UObjectPoolLibrary::GenerateMigrationReport()
{
    OBJECTPOOL_MIXED_MODE_CODE(
        FObjectPoolMigrationManager& Manager = FObjectPoolMigrationManager::Get();
        return Manager.GenerateMigrationReport();
    )

    // 编译时固定实现的简单报告
    FString ConfigSummary = OBJECTPOOL_GET_CONFIG_SUMMARY();
    return FString::Printf(TEXT(
        "=== 对象池报告（编译时固定） ===\n"
        "当前实现: %s\n"
        "迁移状态: 不适用（编译时固定）\n"
        "配置: %s\n"
    ),
#if OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 1
        TEXT("简化实现"),
#elif OBJECTPOOL_USE_SIMPLIFIED_IMPLEMENTATION == 0
        TEXT("原始实现"),
#else
        TEXT("混合模式"),
#endif
        *ConfigSummary
    );
}

UObjectPoolSubsystem* UObjectPoolLibrary::GetSubsystemSafe(const UObject* WorldContext)
{
    if (!WorldContext)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::GetSubsystemSafe: WorldContext为空"));
        return nullptr;
    }

    // ✅ 使用子系统的静态方法获取实例
    UObjectPoolSubsystem* Subsystem = UObjectPoolSubsystem::Get(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::GetSubsystemSafe: 无法获取对象池子系统"));
    }

    return Subsystem;
}

UObjectPoolSubsystemSimplified* UObjectPoolLibrary::GetSimplifiedSubsystemSafe(const UObject* WorldContext)
{
    if (!WorldContext)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::GetSimplifiedSubsystemSafe: WorldContext为空"));
        return nullptr;
    }

    // ✅ 使用简化子系统的静态方法获取实例
    UObjectPoolSubsystemSimplified* Subsystem = UObjectPoolSubsystemSimplified::Get(WorldContext);
    if (!Subsystem)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::GetSimplifiedSubsystemSafe: 无法获取简化对象池子系统"));
    }

    return Subsystem;
}

bool UObjectPoolLibrary::CallLifecycleEvent(const UObject* WorldContext, AActor* Actor, EObjectPoolLifecycleEvent EventType, bool bAsync)
{
    if (!IsValid(Actor))
    {
        OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolLibrary::CallLifecycleEvent: Actor无效"));
        return false;
    }

    // ✅ 使用增强的生命周期事件系统
    bool bSuccess = IObjectPoolInterface::CallLifecycleEventEnhanced(Actor, EventType, bAsync, 1000);

    OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::CallLifecycleEvent: %s, 事件: %d, 结果: %s"),
        *Actor->GetName(), (int32)EventType, bSuccess ? TEXT("成功") : TEXT("失败"));

    return bSuccess;
}

int32 UObjectPoolLibrary::BatchCallLifecycleEvents(const UObject* WorldContext, const TArray<AActor*>& Actors, EObjectPoolLifecycleEvent EventType, bool bAsync)
{
    if (Actors.Num() == 0)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::BatchCallLifecycleEvents: 空的Actor数组"));
        return 0;
    }

    // ✅ 使用接口的批量调用方法
    int32 SuccessCount = IObjectPoolInterface::BatchCallLifecycleEvents(Actors, EventType, bAsync);

    OBJECTPOOL_LOG(Verbose, TEXT("UObjectPoolLibrary::BatchCallLifecycleEvents: 请求 %d 个，成功 %d 个"),
        Actors.Num(), SuccessCount);

    return SuccessCount;
}

bool UObjectPoolLibrary::HasLifecycleEventSupport(const UObject* WorldContext, AActor* Actor, EObjectPoolLifecycleEvent EventType)
{
    if (!IsValid(Actor))
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::HasLifecycleEventSupport: Actor无效"));
        return false;
    }

    // ✅ 使用接口的检查方法
    bool bSupported = IObjectPoolInterface::HasLifecycleEvent(Actor, EventType);

    OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::HasLifecycleEventSupport: %s, 事件: %d, 支持: %s"),
        *Actor->GetName(), (int32)EventType, bSupported ? TEXT("是") : TEXT("否"));

    return bSupported;
}

int32 UObjectPoolLibrary::BatchReturnActors(const UObject* WorldContext, const TArray<AActor*>& Actors)
{
    if (Actors.Num() == 0)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("UObjectPoolLibrary::BatchReturnActors: 空的Actor数组"));
        return 0;
    }

    int32 SuccessCount = 0;
    
    // ✅ 批量归还Actor
    for (AActor* Actor : Actors)
    {
        if (IsValid(Actor))
        {
            ReturnActorToPool(WorldContext, Actor);
            ++SuccessCount;
        }
    }
    
    OBJECTPOOL_LOG(Verbose, TEXT("UObjectPoolLibrary::BatchReturnActors: 请求 %d 个，成功 %d 个"), 
        Actors.Num(), SuccessCount);
    
    return SuccessCount;
}
