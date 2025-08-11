// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 遵循IWYU原则的头文件包含
#include "ObjectPoolInterface.h"

// ✅ UE核心依赖
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Async/Async.h"

// ✅ 对象池模块依赖
#include "ObjectPool.h"

void IObjectPoolInterface::SafeCallLifecycleEvent(AActor* Actor, const FString& EventType)
{
    if (!IsValid(Actor))
    {
        OBJECTPOOL_LOG(Warning, TEXT("尝试调用生命周期事件但Actor无效: %s"), *EventType);
        return;
    }

    // ✅ 检查Actor是否实现了接口
    if (!DoesActorImplementInterface(Actor))
    {
        // 这不是错误，只是Actor没有实现接口
        OBJECTPOOL_LOG(VeryVerbose, TEXT("Actor %s 没有实现对象池接口，跳过生命周期事件: %s"), 
            *Actor->GetName(), *EventType);
        return;
    }

    OBJECTPOOL_LOG(Verbose, TEXT("调用Actor %s 的生命周期事件: %s"), *Actor->GetName(), *EventType);

    // ✅ 根据事件类型调用相应的函数
    if (EventType == TEXT("OnPoolActorCreated"))
    {
        ExecuteBlueprintEvent(Actor, FName("OnPoolActorCreated"));
        
        // 同时调用C++版本（如果有重写）
        if (IObjectPoolInterface* Interface = Cast<IObjectPoolInterface>(Actor))
        {
            Interface->OnPoolActorCreated_Implementation();
        }
    }
    else if (EventType == TEXT("OnPoolActorActivated"))
    {
        ExecuteBlueprintEvent(Actor, FName("OnPoolActorActivated"));
        
        // 同时调用C++版本（如果有重写）
        if (IObjectPoolInterface* Interface = Cast<IObjectPoolInterface>(Actor))
        {
            Interface->OnPoolActorActivated_Implementation();
        }
    }
    else if (EventType == TEXT("OnReturnToPool"))
    {
        ExecuteBlueprintEvent(Actor, FName("OnReturnToPool"));
        
        // 同时调用C++版本（如果有重写）
        if (IObjectPoolInterface* Interface = Cast<IObjectPoolInterface>(Actor))
        {
            Interface->OnReturnToPool_Implementation();
        }
    }
    else
    {
        OBJECTPOOL_LOG(Warning, TEXT("未知的生命周期事件类型: %s"), *EventType);
    }
}

void IObjectPoolInterface::ExecuteBlueprintEvent(AActor* Actor, const FName& FunctionName)
{
    if (!IsValid(Actor))
    {
        return;
    }

    // ✅ 查找蓝图函数
    UFunction* Function = Actor->GetClass()->FindFunctionByName(FunctionName);
    if (!Function)
    {
        // 这不是错误，Actor可能没有实现这个特定的事件
        OBJECTPOOL_LOG(VeryVerbose, TEXT("Actor %s 没有实现蓝图函数: %s"), 
            *Actor->GetName(), *FunctionName.ToString());
        return;
    }

    // ✅ 检查函数是否是蓝图事件（简化版本）
    if (!Function->HasAnyFunctionFlags(FUNC_BlueprintEvent))
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("函数 %s 不是蓝图事件"), *FunctionName.ToString());
        // 继续执行，不返回
    }

    // ✅ 安全调用蓝图函数（不使用 C++ 异常）
    Actor->ProcessEvent(Function, nullptr);
    OBJECTPOOL_LOG(VeryVerbose, TEXT("成功调用Actor %s 的蓝图事件: %s"), 
        *Actor->GetName(), *FunctionName.ToString());
}

// ✅ 接口的默认实现已在头文件中定义

bool IObjectPoolInterface::CallLifecycleEventEnhanced(AActor* Actor, EObjectPoolLifecycleEvent EventType, bool bAsync, int32 TimeoutMs)
{
    if (!IsValid(Actor))
    {
        OBJECTPOOL_LOG(Warning, TEXT("CallLifecycleEventEnhanced: Actor无效"));
        return false;
    }

    // ✅ 检查Actor是否实现了接口
    if (!DoesActorImplementInterface(Actor))
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("CallLifecycleEventEnhanced: Actor %s 没有实现对象池接口"), *Actor->GetName());
        return false;
    }

    // ✅ 记录开始时间（用于性能统计）
    double StartTime = FPlatformTime::Seconds();
    bool bSuccess = false;

    // ✅ 根据事件类型调用相应的方法
    FString EventName;
    switch (EventType)
    {
    case EObjectPoolLifecycleEvent::Created:
        EventName = TEXT("OnPoolActorCreated");
        break;
    case EObjectPoolLifecycleEvent::Activated:
        EventName = TEXT("OnPoolActorActivated");
        break;
    case EObjectPoolLifecycleEvent::ReturnedToPool:
        EventName = TEXT("OnReturnToPool");
        break;
    default:
        OBJECTPOOL_LOG(Warning, TEXT("CallLifecycleEventEnhanced: 未知的事件类型"));
        return false;
    }

    if (bAsync)
    {
        // ✅ 异步调用（在游戏线程的下一帧执行）
        AsyncTask(ENamedThreads::GameThread, [Actor, EventName]()
        {
            if (IsValid(Actor))
            {
                ExecuteBlueprintEvent(Actor, FName(*EventName));

                // 同时调用C++版本
                if (IObjectPoolInterface* Interface = Cast<IObjectPoolInterface>(Actor))
                {
                    if (EventName == TEXT("OnPoolActorCreated"))
                    {
                        Interface->OnPoolActorCreated_Implementation();
                    }
                    else if (EventName == TEXT("OnPoolActorActivated"))
                    {
                        Interface->OnPoolActorActivated_Implementation();
                    }
                    else if (EventName == TEXT("OnReturnToPool"))
                    {
                        Interface->OnReturnToPool_Implementation();
                    }
                }
            }
        });
        bSuccess = true; // 异步调用认为是成功的
    }
    else
    {
        // ✅ 同步调用（不使用 C++ 异常）
        ExecuteBlueprintEvent(Actor, FName(*EventName));

        // 同时调用C++版本
        if (IObjectPoolInterface* Interface = Cast<IObjectPoolInterface>(Actor))
        {
            if (EventName == TEXT("OnPoolActorCreated"))
            {
                Interface->OnPoolActorCreated_Implementation();
            }
            else if (EventName == TEXT("OnPoolActorActivated"))
            {
                Interface->OnPoolActorActivated_Implementation();
            }
            else if (EventName == TEXT("OnReturnToPool"))
            {
                Interface->OnReturnToPool_Implementation();
            }
        }
        bSuccess = true;
    }

    // ✅ 记录性能统计
    double EndTime = FPlatformTime::Seconds();
    float ExecutionTimeUs = (EndTime - StartTime) * 1000000.0f;

    OBJECTPOOL_LOG(VeryVerbose, TEXT("CallLifecycleEventEnhanced: %s, 事件: %s, 耗时: %.2f微秒"),
        *Actor->GetName(), *EventName, ExecutionTimeUs);

    return bSuccess;
}

int32 IObjectPoolInterface::BatchCallLifecycleEvents(const TArray<AActor*>& Actors, EObjectPoolLifecycleEvent EventType, bool bAsync)
{
    if (Actors.Num() == 0)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("BatchCallLifecycleEvents: 空的Actor数组"));
        return 0;
    }

    int32 SuccessCount = 0;

    // ✅ 批量调用生命周期事件
    for (AActor* Actor : Actors)
    {
        if (CallLifecycleEventEnhanced(Actor, EventType, bAsync))
        {
            ++SuccessCount;
        }
    }

    OBJECTPOOL_LOG(Verbose, TEXT("BatchCallLifecycleEvents: 请求 %d 个，成功 %d 个"),
        Actors.Num(), SuccessCount);

    return SuccessCount;
}

bool IObjectPoolInterface::HasLifecycleEvent(AActor* Actor, EObjectPoolLifecycleEvent EventType)
{
    if (!IsValid(Actor))
    {
        return false;
    }

    // ✅ 检查是否实现了接口
    if (!DoesActorImplementInterface(Actor))
    {
        return false;
    }

    // ✅ 检查是否有对应的蓝图事件
    FString EventName;
    switch (EventType)
    {
    case EObjectPoolLifecycleEvent::Created:
        EventName = TEXT("OnPoolActorCreated");
        break;
    case EObjectPoolLifecycleEvent::Activated:
        EventName = TEXT("OnPoolActorActivated");
        break;
    case EObjectPoolLifecycleEvent::ReturnedToPool:
        EventName = TEXT("OnReturnToPool");
        break;
    default:
        return false;
    }

    // ✅ 检查蓝图中是否有该事件
    UClass* ActorClass = Actor->GetClass();
    if (ActorClass)
    {
        UFunction* Function = ActorClass->FindFunctionByName(FName(*EventName));
        if (Function && Function->HasAnyFunctionFlags(FUNC_BlueprintEvent))
        {
            return true;
        }
    }

    // ✅ 检查C++接口实现
    if (IObjectPoolInterface* Interface = Cast<IObjectPoolInterface>(Actor))
    {
        return true; // 如果实现了接口，就认为有事件
    }

    return false;
}

FObjectPoolLifecycleStats IObjectPoolInterface::GetLifecycleStats(AActor* Actor)
{
    FObjectPoolLifecycleStats Stats;

    if (!IsValid(Actor))
    {
        return Stats;
    }

    // ✅ 这里可以从Actor的组件或属性中获取统计信息
    // 目前返回默认统计，后续可以扩展

    OBJECTPOOL_LOG(VeryVerbose, TEXT("GetLifecycleStats: %s"), *Actor->GetName());

    return Stats;
}
