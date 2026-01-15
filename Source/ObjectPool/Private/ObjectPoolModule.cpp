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
    
    // 清空指定对象池
    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("objectpool.clear"),
        TEXT("清空指定类型的对象池。用法: objectpool.clear <ClassName>"),
        FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
        {
            if (Args.Num() > 0)
            {
                OBJECTPOOL_LOG(Warning, TEXT("清空对象池功能尚未实现: %s"), *Args[0]);
                // TODO: 实现对象池清空功能
            }
            else
            {
                OBJECTPOOL_LOG(Warning, TEXT("请指定要清空的Actor类名"));
            }
        }),
        ECVF_Default
    ));
    
    // 验证对象池完整性
    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("objectpool.validate"),
        TEXT("验证所有对象池的完整性和状态"),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            OBJECTPOOL_LOG(Warning, TEXT("对象池验证功能尚未实现"));
            // TODO: 实现对象池验证功能
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
