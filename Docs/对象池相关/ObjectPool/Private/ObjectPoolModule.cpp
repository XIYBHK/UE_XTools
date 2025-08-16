// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 遵循IWYU原则的头文件包含
#include "ObjectPool.h"

// ✅ UE核心依赖
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

// ✅ 定义日志类别
DEFINE_LOG_CATEGORY(LogObjectPool);

void FObjectPoolModule::StartupModule()
{
    OBJECTPOOL_LOG(Log, TEXT("ObjectPool模块启动中..."));
    
    // ✅ 初始化模块
    InitializeModule();
    
    // ✅ 注册控制台命令
    RegisterConsoleCommands();
    
    bIsInitialized = true;
    
    OBJECTPOOL_LOG(Log, TEXT("ObjectPool模块启动完成"));
}

void FObjectPoolModule::ShutdownModule()
{
    OBJECTPOOL_LOG(Log, TEXT("ObjectPool模块关闭中..."));
    
    // ✅ 注销控制台命令
    UnregisterConsoleCommands();
    
    // ✅ 清理模块
    CleanupModule();
    
    bIsInitialized = false;
    
    OBJECTPOOL_LOG(Log, TEXT("ObjectPool模块关闭完成"));
}

void FObjectPoolModule::InitializeModule()
{
    // ✅ 模块初始化逻辑
    // 这里可以添加模块启动时需要的初始化代码
    
    OBJECTPOOL_LOG(Verbose, TEXT("ObjectPool模块初始化完成"));
}

void FObjectPoolModule::CleanupModule()
{
    // ✅ 模块清理逻辑
    // 这里可以添加模块关闭时需要的清理代码
    
    OBJECTPOOL_LOG(Verbose, TEXT("ObjectPool模块清理完成"));
}

void FObjectPoolModule::RegisterConsoleCommands()
{
    // ✅ 注册调试用的控制台命令
    
    // 显示对象池统计信息
    ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("objectpool.stats"),
        TEXT("显示所有对象池的统计信息"),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            OBJECTPOOL_LOG(Warning, TEXT("对象池统计功能尚未实现"));
            // TODO: 实现统计信息显示
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
    // ✅ 注销所有控制台命令
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

// ✅ 实现模块接口
IMPLEMENT_MODULE(FObjectPoolModule, ObjectPool)
