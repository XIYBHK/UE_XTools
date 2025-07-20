// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 遵循IWYU原则的头文件包含
#include "ActorStateResetter.h"

// ✅ UE核心依赖
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ActorComponent.h"

// ✅ 对象池模块依赖
#include "ObjectPool.h"

FActorStateResetter::FActorStateResetter()
{
    OBJECTPOOL_LOG(VeryVerbose, TEXT("ActorStateResetter创建"));
}

FActorStateResetter::~FActorStateResetter()
{
    OBJECTPOOL_LOG(VeryVerbose, TEXT("ActorStateResetter销毁"));
}

bool FActorStateResetter::ResetActorState(AActor* Actor, const FTransform& SpawnTransform, const FActorResetConfig& ResetConfig)
{
    if (!IsValid(Actor))
    {
        OBJECTPOOL_LOG(Warning, TEXT("ResetActorState: Actor无效"));
        return false;
    }

    double StartTime = FPlatformTime::Seconds();
    bool bSuccess = true;

    OBJECTPOOL_LOG(VeryVerbose, TEXT("开始重置Actor状态: %s"), *Actor->GetName());

    try
    {
        // ✅ 1. 重置基本属性
        if (ResetConfig.bResetTransform)
        {
            ResetBasicProperties(Actor, true, SpawnTransform);
        }
        else
        {
            ResetBasicProperties(Actor, false);
        }

        // ✅ 2. 重置物理状态
        if (ResetConfig.bResetPhysics)
        {
            ResetPhysicsState(Actor);
        }

        // ✅ 3. 重置组件状态
        ResetComponentStates(Actor, ResetConfig);

        // ✅ 4. 清理定时器和事件
        if (ResetConfig.bClearTimers)
        {
            ClearTimersAndEvents(Actor);
        }

        // ✅ 5. 重置AI状态
        if (ResetConfig.bResetAI)
        {
            ResetAIState(Actor);
        }

        // ✅ 6. 重置动画状态
        if (ResetConfig.bResetAnimation)
        {
            ResetAnimationState(Actor);
        }

        // ✅ 7. 重置音频状态
        if (ResetConfig.bResetAudio)
        {
            ResetAudioState(Actor);
        }

        // ✅ 8. 重置粒子系统
        if (ResetConfig.bResetParticles)
        {
            ResetParticleState(Actor);
        }

        // ✅ 9. 重置网络状态
        if (ResetConfig.bResetNetwork)
        {
            ResetNetworkState(Actor);
        }

        OBJECTPOOL_LOG(VeryVerbose, TEXT("完成重置Actor状态: %s"), *Actor->GetName());
    }
    catch (...)
    {
        OBJECTPOOL_LOG(Error, TEXT("重置Actor状态时发生异常: %s"), *Actor->GetName());
        bSuccess = false;
    }

    // ✅ 更新统计信息
    double EndTime = FPlatformTime::Seconds();
    float ResetTimeMs = (EndTime - StartTime) * 1000.0f;
    UpdateResetStats(bSuccess, ResetTimeMs);

    return bSuccess;
}

int32 FActorStateResetter::BatchResetActorStates(const TArray<AActor*>& Actors, const TArray<FTransform>& Transforms, const FActorResetConfig& ResetConfig)
{
    if (Actors.Num() == 0)
    {
        return 0;
    }

    int32 SuccessCount = 0;
    bool bUseTransforms = Transforms.Num() == Actors.Num();

    for (int32 i = 0; i < Actors.Num(); ++i)
    {
        AActor* Actor = Actors[i];
        if (!IsValid(Actor))
        {
            continue;
        }

        FTransform UseTransform = bUseTransforms ? Transforms[i] : Actor->GetActorTransform();
        
        if (ResetActorState(Actor, UseTransform, ResetConfig))
        {
            ++SuccessCount;
        }
    }

    OBJECTPOOL_LOG(Verbose, TEXT("BatchResetActorStates: 请求 %d 个，成功 %d 个"), Actors.Num(), SuccessCount);
    return SuccessCount;
}

bool FActorStateResetter::ResetActorForPooling(AActor* Actor)
{
    if (!IsValid(Actor))
    {
        return false;
    }

    // ✅ 池化专用配置：重置所有状态但不改变Transform
    FActorResetConfig PoolingConfig;
    PoolingConfig.bResetTransform = false; // 保持当前位置
    PoolingConfig.bResetPhysics = true;
    PoolingConfig.bResetAI = true;
    PoolingConfig.bResetAnimation = true;
    PoolingConfig.bClearTimers = true;
    PoolingConfig.bResetAudio = true;
    PoolingConfig.bResetParticles = true;
    PoolingConfig.bResetNetwork = false;

    return ResetActorState(Actor, Actor->GetActorTransform(), PoolingConfig);
}

bool FActorStateResetter::ActivateActorFromPool(AActor* Actor, const FTransform& SpawnTransform)
{
    if (!IsValid(Actor))
    {
        return false;
    }

    // ✅ 激活专用配置：重置到新状态
    FActorResetConfig ActivationConfig;
    ActivationConfig.bResetTransform = true;
    ActivationConfig.bResetPhysics = true;
    ActivationConfig.bResetAI = true;
    ActivationConfig.bResetAnimation = true;
    ActivationConfig.bClearTimers = true;
    ActivationConfig.bResetAudio = false; // 激活时不重置音频
    ActivationConfig.bResetParticles = false; // 激活时不重置粒子
    ActivationConfig.bResetNetwork = true;

    return ResetActorState(Actor, SpawnTransform, ActivationConfig);
}

void FActorStateResetter::ResetBasicProperties(AActor* Actor, bool bResetTransform, const FTransform& NewTransform)
{
    if (!IsValid(Actor))
    {
        return;
    }

    // ✅ 重置Transform
    if (bResetTransform)
    {
        Actor->SetActorTransform(NewTransform);
    }

    // ✅ 重置基本状态
    Actor->SetActorHiddenInGame(false);
    Actor->SetActorEnableCollision(true);
    Actor->SetActorTickEnabled(true);

    OBJECTPOOL_LOG(VeryVerbose, TEXT("重置基本属性: %s"), *Actor->GetName());
}

void FActorStateResetter::ResetPhysicsState(AActor* Actor)
{
    if (!IsValid(Actor))
    {
        return;
    }

    // ✅ 重置根组件的物理状态
    if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
    {
        RootPrimitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
        RootPrimitive->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    }

    // ✅ 重置所有物理组件
    TArray<UPrimitiveComponent*> PrimitiveComponents;
    Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

    for (UPrimitiveComponent* Component : PrimitiveComponents)
    {
        if (IsValid(Component))
        {
            Component->SetPhysicsLinearVelocity(FVector::ZeroVector);
            Component->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        }
    }

    OBJECTPOOL_LOG(VeryVerbose, TEXT("重置物理状态: %s"), *Actor->GetName());
}

void FActorStateResetter::ResetComponentStates(AActor* Actor, const FActorResetConfig& ResetConfig)
{
    if (!IsValid(Actor))
    {
        return;
    }

    // ✅ 获取所有组件并重置
    TArray<UActorComponent*> Components;
    Actor->GetComponents<UActorComponent>(Components);

    for (UActorComponent* Component : Components)
    {
        if (IsValid(Component))
        {
            ResetSingleComponent(Component, ResetConfig);
        }
    }

    OBJECTPOOL_LOG(VeryVerbose, TEXT("重置组件状态: %s"), *Actor->GetName());
}

void FActorStateResetter::ResetSingleComponent(UActorComponent* Component, const FActorResetConfig& ResetConfig)
{
    if (!IsValid(Component))
    {
        return;
    }

    // ✅ 检查自定义重置器
    UClass* ComponentClass = Component->GetClass();
    if (CustomComponentResetters.Contains(ComponentClass))
    {
        CustomComponentResetters[ComponentClass](Component);
        return;
    }

    // ✅ 基础组件重置（简化版本）
    // 这里可以根据需要扩展更多组件类型的重置逻辑
}

FActorResetStats FActorStateResetter::GetResetStats() const
{
    FScopeLock Lock(&ResetterLock);
    return ResetStatsData;
}

void FActorStateResetter::UpdateResetStats(bool bSuccess, float ResetTimeMs)
{
    FScopeLock Lock(&ResetterLock);
    ResetStatsData.UpdateStats(bSuccess, ResetTimeMs);
}

// ✅ 简化的实现，后续可以扩展
void FActorStateResetter::ClearTimersAndEvents(AActor* Actor) {}
void FActorStateResetter::ResetAIState(AActor* Actor) {}
void FActorStateResetter::ResetAnimationState(AActor* Actor) {}
void FActorStateResetter::ResetAudioState(AActor* Actor) {}
void FActorStateResetter::ResetParticleState(AActor* Actor) {}
void FActorStateResetter::ResetNetworkState(AActor* Actor) {}
void FActorStateResetter::SetDefaultResetConfig(const FActorResetConfig& Config) {}
void FActorStateResetter::ResetStats() {}
void FActorStateResetter::RegisterCustomComponentResetter(UClass* ComponentClass, TFunction<void(UActorComponent*)> ResetFunction) {}
void FActorStateResetter::UnregisterCustomComponentResetter(UClass* ComponentClass) {}
bool FActorStateResetter::ShouldResetActor(AActor* Actor) const { return true; }
bool FActorStateResetter::SafeExecuteReset(TFunction<void()> ResetFunction, const FString& Context) { return true; }
