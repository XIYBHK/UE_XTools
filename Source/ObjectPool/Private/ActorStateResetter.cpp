// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 遵循IWYU原则的头文件包含
#include "ActorStateResetter.h"

// ✅ UE核心依赖
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ActorComponent.h"

// ✅ 组件依赖
#include "GameFramework/ProjectileMovementComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/MovementComponent.h"
#include "Components/MeshComponent.h"

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

    // ✅ 基础组件重置 - 处理常见组件类型
    ResetCommonComponentTypes(Component, ResetConfig);
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

void FActorStateResetter::ResetCommonComponentTypes(UActorComponent* Component, const FActorResetConfig& ResetConfig)
{
    if (!IsValid(Component))
    {
        return;
    }

    // ✅ ProjectileMovement组件重置
    if (UProjectileMovementComponent* ProjectileComp = Cast<UProjectileMovementComponent>(Component))
    {
        ResetProjectileMovementComponent(ProjectileComp, ResetConfig);
        return;
    }

    // ✅ 粒子系统组件重置
    if (UParticleSystemComponent* ParticleComp = Cast<UParticleSystemComponent>(Component))
    {
        if (ResetConfig.bResetParticles)
        {
            ParticleComp->DeactivateSystem();
            ParticleComp->ResetParticles();
        }
        return;
    }

    // ✅ 音频组件重置
    if (UAudioComponent* AudioComp = Cast<UAudioComponent>(Component))
    {
        if (ResetConfig.bResetAudio)
        {
            AudioComp->Stop();
            AudioComp->SetVolumeMultiplier(1.0f);
            AudioComp->SetPitchMultiplier(1.0f);
        }
        return;
    }

    // ✅ 移动组件重置
    if (UMovementComponent* MovementComp = Cast<UMovementComponent>(Component))
    {
        if (ResetConfig.bResetPhysics)
        {
            MovementComp->StopMovementImmediately();
            MovementComp->Velocity = FVector::ZeroVector;
        }
        return;
    }

    // ✅ 网格组件重置
    if (UMeshComponent* MeshComp = Cast<UMeshComponent>(Component))
    {
        MeshComp->SetVisibility(true);
        // 重置材质参数等可以在这里添加
        return;
    }
}

void FActorStateResetter::ResetProjectileMovementComponent(UProjectileMovementComponent* ProjectileComp, const FActorResetConfig& ResetConfig)
{
    if (!IsValid(ProjectileComp))
    {
        return;
    }

    OBJECTPOOL_LOG(VeryVerbose, TEXT("重置ProjectileMovement组件"));

    // ✅ 停止当前移动
    ProjectileComp->StopMovementImmediately();

    // ✅ 重置速度
    ProjectileComp->Velocity = FVector::ZeroVector;

    // ✅ 重置位置（如果需要）
    if (ResetConfig.bResetTransform)
    {
        // 位置重置由Actor的Transform处理，这里不需要额外操作
    }

    // ✅ 重置物理状态
    if (ResetConfig.bResetPhysics)
    {
        // 重置重力相关
        ProjectileComp->ProjectileGravityScale = ProjectileComp->GetClass()->GetDefaultObject<UProjectileMovementComponent>()->ProjectileGravityScale;

        // 重置弹跳相关
        ProjectileComp->bShouldBounce = ProjectileComp->GetClass()->GetDefaultObject<UProjectileMovementComponent>()->bShouldBounce;
        ProjectileComp->Bounciness = ProjectileComp->GetClass()->GetDefaultObject<UProjectileMovementComponent>()->Bounciness;

        // 重置其他物理属性
        ProjectileComp->Friction = ProjectileComp->GetClass()->GetDefaultObject<UProjectileMovementComponent>()->Friction;
    }

    // ✅ 重置初始速度（保持原始设计的初始速度）
    ProjectileComp->InitialSpeed = ProjectileComp->GetClass()->GetDefaultObject<UProjectileMovementComponent>()->InitialSpeed;
    ProjectileComp->MaxSpeed = ProjectileComp->GetClass()->GetDefaultObject<UProjectileMovementComponent>()->MaxSpeed;

    // ✅ 重置生命周期（在UE5.3中可能没有这个属性，跳过）
    // ProjectileComp->ProjectileLifeSpan = ProjectileComp->GetClass()->GetDefaultObject<UProjectileMovementComponent>()->ProjectileLifeSpan;

    // ✅ 重新激活组件（如果之前被停用）
    ProjectileComp->SetActive(true);
    ProjectileComp->SetComponentTickEnabled(true);

    OBJECTPOOL_LOG(VeryVerbose, TEXT("ProjectileMovement组件重置完成"));
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
