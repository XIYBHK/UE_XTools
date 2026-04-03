/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


//  遵循IWYU原则的头文件包含
#include "ObjectPool.h"
#include "ObjectPoolSubsystem.h"
#include "ActorPool.h"

//  UE核心依赖
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

//  定义日志类别
DEFINE_LOG_CATEGORY(LogObjectPool);

namespace
{
    /**
     * 在屏幕左上角显示对象池统计信息
     */
    void DisplayPoolStats()
    {
        if (!GEngine)
        {
            OBJECTPOOL_LOG(Warning, TEXT("GEngine不可用"));
            return;
        }

        // 获取当前游戏世界
        UWorld* World = nullptr;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
            {
                World = Context.World();
                break;
            }
        }

        if (!World)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[ObjectPool] 未找到游戏世界"));
            return;
        }

        // 获取对象池子系统
        UObjectPoolSubsystem* Subsystem = World->GetSubsystem<UObjectPoolSubsystem>();
        if (!Subsystem)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("[ObjectPool] 子系统未启用"));
            return;
        }

        // 显示配置
        const float DisplayTime = 8.0f;
        int32 Key = -1; // 使用负数key让消息自动堆叠

        // 获取子系统统计
        const FObjectPoolSubsystemStats& SysStats = Subsystem->GetStats();
        const int32 PoolCount = Subsystem->GetPoolCount();

        // 显示标题
        GEngine->AddOnScreenDebugMessage(Key--, DisplayTime, FColor::Cyan,
            TEXT("============ ObjectPool Stats ============"));

        // 显示子系统级别统计
        GEngine->AddOnScreenDebugMessage(Key--, DisplayTime, FColor::Yellow,
            FString::Printf(TEXT("Pools: %d | Spawns: %d | Returns: %d"),
                PoolCount, SysStats.TotalSpawnCalls, SysStats.TotalReturnCalls));

        // 计算命中率
        const float HitRate = SysStats.TotalSpawnCalls > 0
            ? (float)SysStats.TotalPoolHits / SysStats.TotalSpawnCalls * 100.0f
            : 0.0f;

        GEngine->AddOnScreenDebugMessage(Key--, DisplayTime, FColor::Yellow,
            FString::Printf(TEXT("PoolHits: %d | Fallbacks: %d | HitRate: %.1f%%"),
                SysStats.TotalPoolHits, SysStats.TotalFallbackSpawns, HitRate));

        // 获取并显示每个池的统计
        TArray<FObjectPoolStats> AllPoolStats = Subsystem->GetAllPoolStats();

        if (AllPoolStats.Num() == 0)
        {
            GEngine->AddOnScreenDebugMessage(Key--, DisplayTime, FColor::White,
                TEXT("  (No pools registered)"));
        }
        else
        {
            GEngine->AddOnScreenDebugMessage(Key--, DisplayTime, FColor::White,
                TEXT("------------------------------------------"));

            for (const FObjectPoolStats& Stats : AllPoolStats)
            {
                // 格式: ClassName: Active/Available/Total (HitRate%)
                const FColor StatColor = Stats.CurrentAvailable > 0 ? FColor::Green : FColor::Orange;

                GEngine->AddOnScreenDebugMessage(Key--, DisplayTime, StatColor,
                    FString::Printf(TEXT("  %s: %d/%d/%d (%.0f%%)"),
                        *Stats.ActorClassName,
                        Stats.CurrentActive,
                        Stats.CurrentAvailable,
                        Stats.TotalCreated,
                        Stats.HitRate * 100.0f));
            }
        }

        GEngine->AddOnScreenDebugMessage(Key--, DisplayTime, FColor::Cyan,
            TEXT("=========================================="));

        OBJECTPOOL_LOG(Log, TEXT("对象池统计: %d个池, %d次生成, %.1f%%命中率"),
            PoolCount, SysStats.TotalSpawnCalls, HitRate);
    }
}

void FObjectPoolModule::StartupModule()
{
    OBJECTPOOL_LOG(Log, TEXT("ObjectPool模块启动中..."));

    if (bIsInitialized)
    {
        OBJECTPOOL_LOG(Warning, TEXT("ObjectPool模块已初始化，跳过重复启动"));
        return;
    }
    
    //  初始化模块
    InitializeModule();
    
    //  注册控制台命令（仅在非Shipping版本，符合UE最佳实践）
#if !UE_BUILD_SHIPPING
    RegisterConsoleCommands();
#endif
    
    bIsInitialized = true;
    
    OBJECTPOOL_LOG(Log, TEXT("ObjectPool模块启动完成"));
}

void FObjectPoolModule::ShutdownModule()
{
    if (!bIsInitialized)
    {
        return;
    }

    OBJECTPOOL_LOG(Log, TEXT("ObjectPool模块关闭中..."));
    
    //  注销控制台命令（仅在非Shipping版本）
    #if !UE_BUILD_SHIPPING
        UnregisterConsoleCommands();
    #endif
    
    //  清理模块
    CleanupModule();
    
    bIsInitialized = false;
    
    OBJECTPOOL_LOG(Log, TEXT("ObjectPool模块关闭完成"));
}

void FObjectPoolModule::InitializeModule()
{
    //  模块初始化逻辑
    // 这里可以添加模块启动时需要的初始化代码
    
    OBJECTPOOL_LOG(Verbose, TEXT("ObjectPool模块初始化完成"));
}

void FObjectPoolModule::CleanupModule()
{
    //  模块清理逻辑
    // 这里可以添加模块关闭时需要的清理代码
    
    OBJECTPOOL_LOG(Verbose, TEXT("ObjectPool模块清理完成"));
}

void FObjectPoolModule::RegisterConsoleCommands()
{
    //  注册调试用的控制台命令
    if (ConsoleCommands.Num() > 0)
    {
        UnregisterConsoleCommands();
    }
    
    // 显示对象池统计信息
    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("objectpool.stats"),
        TEXT("显示所有对象池的统计信息（屏幕左上角）"),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            DisplayPoolStats();
        }),
        ECVF_Default
    ));
    
    // 清空指定对象池（无参数时清空所有）
    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("objectpool.clear"),
        TEXT("清空对象池。用法: objectpool.clear [ClassName]（不指定则清空所有）"),
        FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
        {
            if (!GEngine) return;

            UWorld* World = nullptr;
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
                {
                    World = Context.World();
                    break;
                }
            }

            if (!World)
            {
                OBJECTPOOL_LOG(Warning, TEXT("objectpool.clear: 未找到游戏世界"));
                return;
            }

            UObjectPoolSubsystem* Subsystem = World->GetSubsystem<UObjectPoolSubsystem>();
            if (!Subsystem)
            {
                OBJECTPOOL_LOG(Warning, TEXT("objectpool.clear: 对象池子系统未启用"));
                return;
            }

            if (Args.Num() > 0)
            {
                // 按类名查找并清空指定池
                UClass* FoundClass = FindObject<UClass>(nullptr, *Args[0]);
                if (!FoundClass)
                {
                    FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *Args[0]));
                }
                if (FoundClass && Subsystem->ClearPoolByClass(FoundClass))
                {
                    OBJECTPOOL_LOG(Log, TEXT("objectpool.clear: 已清空 %s 的对象池"), *Args[0]);
                }
                else
                {
                    OBJECTPOOL_LOG(Warning, TEXT("objectpool.clear: 未找到类 '%s' 的对象池"), *Args[0]);
                }
            }
            else
            {
                // 通过统计信息获取所有池的类名，逐个清空（ClearAllPools 非公开接口）
                TArray<FObjectPoolStats> AllStats = Subsystem->GetAllPoolStats();
                int32 ClearedCount = 0;
                for (const FObjectPoolStats& Stats : AllStats)
                {
                    UClass* PoolClass = FindObject<UClass>(nullptr, *Stats.ActorClassName);
                    if (PoolClass && Subsystem->ClearPoolByClass(PoolClass))
                    {
                        ++ClearedCount;
                    }
                }
                OBJECTPOOL_LOG(Log, TEXT("objectpool.clear: 已清空 %d/%d 个对象池"), ClearedCount, AllStats.Num());
            }
        }),
        ECVF_Default
    ));

    // 验证对象池完整性
    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("objectpool.validate"),
        TEXT("验证所有对象池的完整性，报告无效Actor"),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            if (!GEngine) return;

            UWorld* World = nullptr;
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
                {
                    World = Context.World();
                    break;
                }
            }

            if (!World)
            {
                OBJECTPOOL_LOG(Warning, TEXT("objectpool.validate: 未找到游戏世界"));
                return;
            }

            UObjectPoolSubsystem* Subsystem = World->GetSubsystem<UObjectPoolSubsystem>();
            if (!Subsystem)
            {
                OBJECTPOOL_LOG(Warning, TEXT("objectpool.validate: 对象池子系统未启用"));
                return;
            }

            // 获取所有池统计并输出验证结果
            // 注意：TotalCreated 是累计创建数（包含已销毁的），不等于当前存活数
            // 有效不变式：Active+Available <= TotalCreated，且各计数非负，Available <= PoolSize
            TArray<FObjectPoolStats> AllStats = Subsystem->GetAllPoolStats();
            int32 TotalIssues = 0;

            for (const FObjectPoolStats& Stats : AllStats)
            {
                TArray<FString> Issues;
                const int32 CurrentAlive = Stats.CurrentActive + Stats.CurrentAvailable;

                if (Stats.CurrentActive < 0 || Stats.CurrentAvailable < 0)
                {
                    Issues.Add(TEXT("计数为负数"));
                }
                if (CurrentAlive > Stats.TotalCreated)
                {
                    Issues.Add(FString::Printf(TEXT("存活数(%d)>累计创建数(%d)"), CurrentAlive, Stats.TotalCreated));
                }
                if (Stats.PoolSize > 0 && Stats.CurrentAvailable > Stats.PoolSize)
                {
                    Issues.Add(FString::Printf(TEXT("可用数(%d)>池上限(%d)"), Stats.CurrentAvailable, Stats.PoolSize));
                }

                if (Issues.Num() > 0)
                {
                    OBJECTPOOL_LOG(Warning, TEXT("  [!] %s: %s (Active=%d, Available=%d, Created=%d, PoolSize=%d)"),
                        *Stats.ActorClassName, *FString::Join(Issues, TEXT("; ")),
                        Stats.CurrentActive, Stats.CurrentAvailable, Stats.TotalCreated, Stats.PoolSize);
                    ++TotalIssues;
                }
                else
                {
                    OBJECTPOOL_LOG(Log, TEXT("  [OK] %s: Active=%d, Available=%d, Created=%d, HitRate=%.0f%%"),
                        *Stats.ActorClassName, Stats.CurrentActive, Stats.CurrentAvailable,
                        Stats.TotalCreated, Stats.HitRate * 100.0f);
                }
            }

            if (AllStats.Num() == 0)
            {
                OBJECTPOOL_LOG(Log, TEXT("objectpool.validate: 当前没有注册的对象池"));
            }
            else if (TotalIssues == 0)
            {
                OBJECTPOOL_LOG(Log, TEXT("objectpool.validate: 所有 %d 个对象池验证通过"), AllStats.Num());
            }
            else
            {
                OBJECTPOOL_LOG(Warning, TEXT("objectpool.validate: %d 个池中发现 %d 个异常"), AllStats.Num(), TotalIssues);
            }
        }),
        ECVF_Default
    ));
    
    OBJECTPOOL_LOG(Verbose, TEXT("控制台命令注册完成，共注册 %d 个命令"), ConsoleCommands.Num());
}

void FObjectPoolModule::UnregisterConsoleCommands()
{
    //  注销所有控制台命令
    for (IConsoleCommand* Command : ConsoleCommands)
    {
        if (Command)
        {
            IConsoleManager::Get().UnregisterConsoleObject(Command);
        }
    }
    
    ConsoleCommands.Empty();
    
    OBJECTPOOL_LOG(Verbose, TEXT("控制台命令注销完成"));
}

//  实现模块接口
IMPLEMENT_MODULE(FObjectPoolModule, ObjectPool)
