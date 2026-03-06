/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


//  遵循IWYU原则的头文件包含
#include "ObjectPoolInterface.h"

//  UE核心依赖
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Async/Async.h"

//  对象池模块依赖
#include "ObjectPool.h"

void IObjectPoolInterface::SafeCallLifecycleEvent(AActor* Actor, const FString& EventType)
{
    if (!IsValid(Actor))
    {
        OBJECTPOOL_LOG(Warning, TEXT("尝试调用生命周期事件但Actor无效: %s"), *EventType);
        return;
    }

    //  检查Actor是否实现了接口
    if (!DoesActorImplementInterface(Actor))
    {
        // 这不是错误，只是Actor没有实现接口
        OBJECTPOOL_LOG(VeryVerbose, TEXT("Actor %s 没有实现对象池接口，跳过生命周期事件: %s"), 
            *Actor->GetName(), *EventType);
        return;
    }

    OBJECTPOOL_LOG(Verbose, TEXT("调用Actor %s 的生命周期事件: %s"), *Actor->GetName(), *EventType);

    if (EventType == TEXT("OnPoolActorCreated"))
    {
        IObjectPoolInterface::Execute_OnPoolActorCreated(Actor);
    }
    else if (EventType == TEXT("OnPoolActorActivated"))
    {
        IObjectPoolInterface::Execute_OnPoolActorActivated(Actor);
    }
    else if (EventType == TEXT("OnReturnToPool"))
    {
        IObjectPoolInterface::Execute_OnReturnToPool(Actor);
    }
    else
    {
        OBJECTPOOL_LOG(Warning, TEXT("未知的生命周期事件类型: %s"), *EventType);
    }
}

//  接口的默认实现已在头文件中定义

bool IObjectPoolInterface::CallLifecycleEventEnhanced(AActor* Actor, EObjectPoolLifecycleEvent EventType, bool bAsync, int32 TimeoutMs)
{
    if (!IsValid(Actor))
    {
        OBJECTPOOL_LOG(Warning, TEXT("CallLifecycleEventEnhanced: Actor无效"));
        return false;
    }

    //  检查Actor是否实现了接口
    if (!DoesActorImplementInterface(Actor))
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("CallLifecycleEventEnhanced: Actor %s 没有实现对象池接口"), *Actor->GetName());
        return false;
    }

    //  记录开始时间（用于性能统计）
    double StartTime = FPlatformTime::Seconds();
    bool bSuccess = false;

    auto ExecuteEvent = [Actor](EObjectPoolLifecycleEvent InEventType)
    {
        switch (InEventType)
        {
        case EObjectPoolLifecycleEvent::Created:
            IObjectPoolInterface::Execute_OnPoolActorCreated(Actor);
            return TEXT("OnPoolActorCreated");
        case EObjectPoolLifecycleEvent::Activated:
            IObjectPoolInterface::Execute_OnPoolActorActivated(Actor);
            return TEXT("OnPoolActorActivated");
        case EObjectPoolLifecycleEvent::ReturnedToPool:
            IObjectPoolInterface::Execute_OnReturnToPool(Actor);
            return TEXT("OnReturnToPool");
        default:
            return TEXT("Unknown");
        }
    };

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
        //  异步调用（在游戏线程的下一帧执行）
        AsyncTask(ENamedThreads::GameThread, [Actor, EventType]()
        {
            if (IsValid(Actor))
            {
                switch (EventType)
                {
                case EObjectPoolLifecycleEvent::Created:
                    IObjectPoolInterface::Execute_OnPoolActorCreated(Actor);
                    break;
                case EObjectPoolLifecycleEvent::Activated:
                    IObjectPoolInterface::Execute_OnPoolActorActivated(Actor);
                    break;
                case EObjectPoolLifecycleEvent::ReturnedToPool:
                    IObjectPoolInterface::Execute_OnReturnToPool(Actor);
                    break;
                default:
                    break;
                }
            }
        });
        bSuccess = true; // 异步调用认为是成功的
    }
    else
    {
        ExecuteEvent(EventType);
        bSuccess = true;
    }

    //  记录性能统计
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

    //  批量调用生命周期事件
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

    //  检查是否实现了接口
    if (!DoesActorImplementInterface(Actor))
    {
        return false;
    }

    //  检查是否有对应的蓝图事件
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

    //  检查蓝图中是否有该事件
    UClass* ActorClass = Actor->GetClass();
    if (ActorClass)
    {
        UFunction* Function = ActorClass->FindFunctionByName(FName(*EventName));
        if (Function && Function->HasAnyFunctionFlags(FUNC_BlueprintEvent))
        {
            return true;
        }
    }

    //  检查C++接口实现
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

    //  这里可以从Actor的组件或属性中获取统计信息
    // 目前返回默认统计，后续可以扩展

    OBJECTPOOL_LOG(VeryVerbose, TEXT("GetLifecycleStats: %s"), *Actor->GetName());

    return Stats;
}
