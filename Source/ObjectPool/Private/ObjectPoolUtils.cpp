// Fill out your copyright notice in the Description page of Project Settings.

#include "ObjectPoolUtils.h"
#include "ObjectPool.h"
#include "ObjectPoolInterface.h"

// ✅ UE核心依赖
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"

// ✅ 组件依赖
#include "GameFramework/ProjectileMovementComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/MovementComponent.h"
#include "Components/MeshComponent.h"

// ✅ 日志和统计
DEFINE_LOG_CATEGORY(LogObjectPoolUtils);

#if STATS
DEFINE_STAT(STAT_ResetActorForPooling);
DEFINE_STAT(STAT_ActivateActorFromPool);
DEFINE_STAT(STAT_ValidateConfig);
#endif

// ✅ Actor状态重置实现

bool FObjectPoolUtils::ResetActorForPooling(AActor* Actor)
{
    SCOPE_CYCLE_COUNTER(STAT_ResetActorForPooling);
    
    if (!IsValid(Actor))
    {
        OBJECTPOOL_UTILS_LOG(Warning, TEXT("ResetActorForPooling: Actor无效"));
        return false;
    }

    // ✅ 基本状态重置
    ResetBasicActorProperties(Actor, true);
    
    // ✅ 重置物理状态
    ResetActorPhysics(Actor);
    
    // ✅ 重置组件状态
    ResetActorComponents(Actor);
    
    // ✅ 调用生命周期接口
    SafeCallLifecycleInterface(Actor, TEXT("ReturnedToPool"));
    
    OBJECTPOOL_UTILS_LOG(VeryVerbose, TEXT("成功重置Actor到池化状态: %s"), *Actor->GetName());
    return true;
}

bool FObjectPoolUtils::ActivateActorFromPool(AActor* Actor, const FTransform& SpawnTransform)
{
    SCOPE_CYCLE_COUNTER(STAT_ActivateActorFromPool);
    
    if (!IsValid(Actor))
    {
        OBJECTPOOL_UTILS_LOG(Warning, TEXT("ActivateActorFromPool: Actor无效"));
        return false;
    }

    // ✅ 最安全策略：检查是否需要完成延迟构造
    bool bWasUninitialized = !Actor->IsActorInitialized();
    if (bWasUninitialized)
    {
        OBJECTPOOL_UTILS_LOG(VeryVerbose, TEXT("Actor未完成初始化，执行FinishSpawning: %s"), *Actor->GetName());
        
        // 完成延迟构造，这会调用BeginPlay等生命周期函数
        Actor->FinishSpawning(SpawnTransform);
        
        OBJECTPOOL_UTILS_LOG(VeryVerbose, TEXT("FinishSpawning完成: %s"), *Actor->GetName());
        
        // ✅ 首次初始化时调用OnPoolActorCreated事件
        if (IObjectPoolInterface::DoesActorImplementInterface(Actor))
        {
            if (IObjectPoolInterface* PoolInterface = Cast<IObjectPoolInterface>(Actor))
            {
                PoolInterface->OnPoolActorCreated_Implementation();
            }
            IObjectPoolInterface::Execute_OnPoolActorCreated(Actor);
            OBJECTPOOL_UTILS_LOG(VeryVerbose, TEXT("已调用生命周期事件OnPoolActorCreated: %s"), *Actor->GetName());
        }
    }
    else
    {
        // Actor已经初始化过，直接重用
        OBJECTPOOL_UTILS_LOG(VeryVerbose, TEXT("Actor已初始化，直接重用: %s"), *Actor->GetName());
    }

    // ✅ 应用新的Transform
    ApplyTransformToActor(Actor, SpawnTransform);

    // 复用路径的 Construction Script 重跑移至 FinalizeDeferred，确保顺序与原生更一致
    
    // ✅ 激活Actor
    ResetBasicActorProperties(Actor, false); // 显示Actor
    
    // ✅ 启用物理
    if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
    {
        RootPrimitive->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        RootPrimitive->SetSimulatePhysics(false); // 通常池化对象不需要物理模拟
    }
    
    // ✅ 启用Tick
    Actor->SetActorTickEnabled(true);

    // ✅ 重新启用ProjectileMovement组件
    TArray<UProjectileMovementComponent*> ProjectileComponents;
    Actor->GetComponents<UProjectileMovementComponent>(ProjectileComponents);

    for (UProjectileMovementComponent* ProjectileComp : ProjectileComponents)
    {
        if (IsValid(ProjectileComp))
        {
            ProjectileComp->SetActive(true);
            ProjectileComp->SetComponentTickEnabled(true);

            // ✅ 重新设置初始速度 - 使用新的SpawnTransform的方向
            FVector ForwardDirection = SpawnTransform.GetRotation().GetForwardVector();
            ProjectileComp->Velocity = ProjectileComp->InitialSpeed * ForwardDirection;

            // ✅ 确保组件状态正确
            ProjectileComp->UpdateComponentVelocity();

            // ✅ 重置内部状态
            ProjectileComp->bSimulationEnabled = true;

            OBJECTPOOL_UTILS_LOG(VeryVerbose, TEXT("激活ProjectileMovement: 速度=%s, 方向=%s, InitialSpeed=%f"),
                *ProjectileComp->Velocity.ToString(), *ForwardDirection.ToString(), ProjectileComp->InitialSpeed);
        }
    }

    // ✅ 调用生命周期接口（记录调试信息）
    OBJECTPOOL_UTILS_LOG(VeryVerbose, TEXT("即将触发Activated生命周期: %s"), *Actor->GetName());
    SafeCallLifecycleInterface(Actor, TEXT("Activated"));
    
    OBJECTPOOL_UTILS_LOG(VeryVerbose, TEXT("成功激活Actor从池: %s"), *Actor->GetName());
    // 调试：输出所有 ExposeOnSpawn 变量的当前值
    if (UE_LOG_ACTIVE(LogObjectPoolUtils, VeryVerbose))
    {
        UClass* Klass = Actor->GetClass();
        for (TFieldIterator<FProperty> It(Klass, EFieldIteratorFlags::IncludeSuper); It; ++It)
        {
            FProperty* Prop = *It;
            if (!Prop) { continue; }
            const bool bExpose = Prop->HasAnyPropertyFlags(CPF_BlueprintVisible) && Prop->HasAnyPropertyFlags(CPF_ExposeOnSpawn);
            const bool bEditable = !Prop->HasAnyPropertyFlags(CPF_DisableEditOnInstance);
            if (!bExpose || !bEditable)
            {
                continue;
            }
            FString TextValue;
            Prop->ExportTextItem_Direct(TextValue, Prop->ContainerPtrToValuePtr<void>(Actor), nullptr, Actor, PPF_None);
            OBJECTPOOL_UTILS_LOG(VeryVerbose, TEXT("ExposeOnSpawn属性: %s = %s"), *Prop->GetName(), *TextValue);
        }
    }
    OBJECTPOOL_UTILS_LOG(VeryVerbose, TEXT("ActivateActorFromPool 完成: %s"), *Actor->GetName());
    return true;
}

bool FObjectPoolUtils::BasicActorReset(AActor* Actor, const FTransform& NewTransform, bool bResetPhysics)
{
    if (!IsValid(Actor))
    {
        return false;
    }

    // ✅ 应用Transform
    ApplyTransformToActor(Actor, NewTransform);
    
    // ✅ 基本重置
    ResetBasicActorProperties(Actor, false);
    
    // ✅ 可选的物理重置
    if (bResetPhysics)
    {
        ResetActorPhysics(Actor);
    }
    
    return true;
}

// ✅ 配置管理实现

bool FObjectPoolUtils::ValidateConfig(const FObjectPoolConfig& Config, FString& OutErrorMessage)
{
    SCOPE_CYCLE_COUNTER(STAT_ValidateConfig);
    
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
    
    OutErrorMessage.Empty();
    return true;
}

void FObjectPoolUtils::ApplyDefaultConfig(FObjectPoolConfig& Config)
{
    if (!Config.ActorClass)
    {
        OBJECTPOOL_UTILS_LOG(Warning, TEXT("ApplyDefaultConfig: Actor类为空，无法应用默认配置"));
        return;
    }
    
    // ✅ 应用默认值
    if (Config.InitialSize <= 0)
    {
        GetDefaultConfigForActorClass(Config.ActorClass, Config.InitialSize, Config.HardLimit);
    }
    
    if (Config.HardLimit <= 0)
    {
        Config.HardLimit = FMath::Max(Config.InitialSize * 5, DEFAULT_HARD_LIMIT);
    }
    
    // ✅ 确保配置合理
    Config.InitialSize = FMath::Clamp(Config.InitialSize, 1, 1000);
    Config.HardLimit = FMath::Max(Config.HardLimit, Config.InitialSize);
}

FObjectPoolConfig FObjectPoolUtils::CreateDefaultConfig(TSubclassOf<AActor> ActorClass, const FString& PoolType)
{
    FObjectPoolConfig Config;
    Config.ActorClass = ActorClass;
    
    // ✅ 根据池类型设置默认值
    if (PoolType.Contains(TEXT("子弹")) || PoolType.Contains(TEXT("Bullet")))
    {
        Config.InitialSize = 50;
        Config.HardLimit = 200;
    }
    else if (PoolType.Contains(TEXT("敌人")) || PoolType.Contains(TEXT("Enemy")))
    {
        Config.InitialSize = 20;
        Config.HardLimit = 100;
    }
    else if (PoolType.Contains(TEXT("特效")) || PoolType.Contains(TEXT("Effect")))
    {
        Config.InitialSize = 15;
        Config.HardLimit = 50;
    }
    else
    {
        // 通用默认值
        GetDefaultConfigForActorClass(ActorClass, Config.InitialSize, Config.HardLimit);
    }
    
    Config.bEnablePrewarm = true;
    return Config;
}

// ✅ 调试和监控实现

FObjectPoolDebugInfo FObjectPoolUtils::GetDebugInfo(const FObjectPoolStats& Stats, const FString& PoolName)
{
    FObjectPoolDebugInfo DebugInfo;
    DebugInfo.PoolName = PoolName;
    // DebugInfo.Stats = Stats; // Stats字段不存在，移除这行
    
    // ✅ 健康检查
    DebugInfo.bIsHealthy = IsPoolHealthy(Stats);
    
    // ✅ 添加性能建议
    if (!DebugInfo.bIsHealthy)
    {
        TArray<FString> Suggestions = GetPerformanceSuggestions(Stats);
        for (const FString& Suggestion : Suggestions)
        {
            DebugInfo.AddWarning(Suggestion);
        }
    }
    
    return DebugInfo;
}

void FObjectPoolUtils::LogPoolStats(const FObjectPoolStats& Stats, const FString& PoolName, ELogVerbosity::Type Verbosity)
{
    FString StatsString = FormatStatsString(Stats, true);
    
    switch (Verbosity)
    {
    case ELogVerbosity::Error:
        OBJECTPOOL_UTILS_LOG(Error, TEXT("[%s] %s"), *PoolName, *StatsString);
        break;
    case ELogVerbosity::Warning:
        OBJECTPOOL_UTILS_LOG(Warning, TEXT("[%s] %s"), *PoolName, *StatsString);
        break;
    case ELogVerbosity::Log:
        OBJECTPOOL_UTILS_LOG(Log, TEXT("[%s] %s"), *PoolName, *StatsString);
        break;
    case ELogVerbosity::Verbose:
        OBJECTPOOL_UTILS_LOG(Verbose, TEXT("[%s] %s"), *PoolName, *StatsString);
        break;
    default:
        OBJECTPOOL_UTILS_LOG(VeryVerbose, TEXT("[%s] %s"), *PoolName, *StatsString);
        break;
    }
}

bool FObjectPoolUtils::IsPoolHealthy(const FObjectPoolStats& Stats)
{
    // ✅ 命中率检查
    if (Stats.HitRate < MIN_HEALTHY_HIT_RATE && Stats.TotalCreated > 10)
    {
        return false;
    }
    
    // ✅ 未使用对象比例检查
    if (Stats.TotalCreated > 0)
    {
        float UnusedRatio = static_cast<float>(Stats.CurrentAvailable) / static_cast<float>(Stats.TotalCreated);
        if (UnusedRatio > MAX_HEALTHY_UNUSED_RATIO && Stats.TotalCreated > 20)
        {
            return false;
        }
    }
    
    return true;
}

TArray<FString> FObjectPoolUtils::GetPerformanceSuggestions(const FObjectPoolStats& Stats)
{
    TArray<FString> Suggestions;
    
    if (Stats.HitRate < 0.5f && Stats.TotalCreated > 10)
    {
        Suggestions.Add(TEXT("建议增加初始池大小以提高命中率"));
    }
    
    if (Stats.TotalCreated > 0)
    {
        float UnusedRatio = static_cast<float>(Stats.CurrentAvailable) / static_cast<float>(Stats.TotalCreated);
        if (UnusedRatio > 0.7f && Stats.TotalCreated > 20)
        {
            Suggestions.Add(TEXT("池中有过多未使用对象，考虑减少初始大小"));
        }
    }
    
    if (Stats.PoolSize > 100)
    {
        Suggestions.Add(TEXT("池大小较大，建议分析使用模式"));
    }
    
    if (Stats.TotalCreated == Stats.CurrentActive && Stats.CurrentAvailable == 0)
    {
        Suggestions.Add(TEXT("池可能过小，考虑增加最大限制"));
    }
    
    return Suggestions;
}

// ✅ 性能分析实现

int64 FObjectPoolUtils::EstimateMemoryUsage(TSubclassOf<AActor> ActorClass, int32 PoolSize)
{
    if (!ActorClass || PoolSize <= 0)
    {
        return 0;
    }
    
    int64 SingleActorMemory = CalculateActorMemoryFootprint(ActorClass);
    return SingleActorMemory * PoolSize;
}

FString FObjectPoolUtils::AnalyzeUsagePattern(const FObjectPoolStats& Stats)
{
    if (Stats.TotalCreated == 0)
    {
        return TEXT("无使用数据");
    }
    
    float HitRate = Stats.HitRate;
    float ActiveRatio = Stats.TotalCreated > 0 ? static_cast<float>(Stats.CurrentActive) / static_cast<float>(Stats.TotalCreated) : 0.0f;
    
    if (HitRate > 0.8f)
    {
        return TEXT("高效使用模式");
    }
    else if (HitRate > 0.5f)
    {
        return TEXT("中等使用模式");
    }
    else if (ActiveRatio > 0.8f)
    {
        return TEXT("高负载模式");
    }
    else
    {
        return TEXT("低效使用模式");
    }
}

TArray<FString> FObjectPoolUtils::GetOptimizationSuggestions(const FObjectPoolConfig& Config, const FObjectPoolStats& Stats)
{
    TArray<FString> Suggestions;
    
    // ✅ 基于统计数据的建议
    TArray<FString> PerfSuggestions = GetPerformanceSuggestions(Stats);
    Suggestions.Append(PerfSuggestions);
    
    // ✅ 基于配置的建议
    if (Config.InitialSize > Stats.TotalCreated * 2 && Stats.TotalCreated > 10)
    {
        Suggestions.Add(TEXT("初始大小可能过大，考虑减少"));
    }
    
    if (Config.HardLimit > 0 && Stats.TotalCreated >= Config.HardLimit * 0.9f)
    {
        Suggestions.Add(TEXT("接近硬限制，考虑增加限制或优化使用"));
    }
    
    return Suggestions;
}

// ✅ 实用工具实现

void FObjectPoolUtils::SafeCallLifecycleInterface(AActor* Actor, const FString& EventType)
{
    if (!IsValid(Actor) || !Actor->Implements<UObjectPoolInterface>())
    {
        return;
    }

    if (EventType == TEXT("Created"))
    {
        IObjectPoolInterface::Execute_OnPoolActorCreated(Actor);
    }
    else if (EventType == TEXT("Activated"))
    {
        IObjectPoolInterface::Execute_OnPoolActorActivated(Actor);
    }
    else if (EventType == TEXT("ReturnedToPool"))
    {
        IObjectPoolInterface::Execute_OnReturnToPool(Actor);
    }

    OBJECTPOOL_UTILS_LOG(VeryVerbose, TEXT("成功调用生命周期接口: %s - %s"), *Actor->GetName(), *EventType);
}

bool FObjectPoolUtils::IsActorSuitableForPooling(TSubclassOf<AActor> ActorClass)
{
    if (!ActorClass)
    {
        return false;
    }

    // ✅ 检查是否是合适的Actor类型
    if (ActorClass->IsChildOf<APawn>())
    {
        // Pawn通常需要特殊处理，但可以池化
        return true;
    }

    if (ActorClass->IsChildOf<ACharacter>())
    {
        // Character可以池化，但需要注意AI和动画状态
        return true;
    }

    // ✅ 大部分Actor都适合池化
    return true;
}

FString FObjectPoolUtils::FormatStatsString(const FObjectPoolStats& Stats, bool bDetailed)
{
    if (bDetailed)
    {
        return FString::Printf(TEXT("总创建=%d, 活跃=%d, 可用=%d, 池大小=%d, 命中率=%.1f%%, 类型=%s"),
            Stats.TotalCreated, Stats.CurrentActive, Stats.CurrentAvailable,
            Stats.PoolSize, Stats.HitRate * 100.0f, *Stats.ActorClassName);
    }
    else
    {
        return FString::Printf(TEXT("活跃=%d, 可用=%d, 命中率=%.1f%%"),
            Stats.CurrentActive, Stats.CurrentAvailable, Stats.HitRate * 100.0f);
    }
}

FString FObjectPoolUtils::GeneratePoolId(TSubclassOf<AActor> ActorClass)
{
    if (!ActorClass)
    {
        return TEXT("InvalidPool");
    }

    return FString::Printf(TEXT("Pool_%s_%u"), *ActorClass->GetName(), GetTypeHash(ActorClass));
}

// ✅ 内部辅助方法实现

void FObjectPoolUtils::ResetBasicActorProperties(AActor* Actor, bool bHideActor)
{
    if (!IsValid(Actor))
    {
        return;
    }

    // ✅ 设置可见性
    Actor->SetActorHiddenInGame(bHideActor);

    // ✅ 设置Tick状态
    Actor->SetActorTickEnabled(!bHideActor);

    // ✅ 设置所有PrimitiveComponent的碰撞（包括子组件）
    TArray<UPrimitiveComponent*> PrimitiveComponents;
    Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

    for (UPrimitiveComponent* PrimComp : PrimitiveComponents)
    {
        if (IsValid(PrimComp))
        {
            if (bHideActor)
            {
                // 归还到池时：保存原始碰撞设置并禁用碰撞
                SaveOriginalCollisionSettings(PrimComp);
                PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
            else
            {
                // 从池激活时：恢复原始碰撞设置
                RestoreOriginalCollisionSettings(PrimComp);
            }
        }
    }

    // ✅ 归还到池时，移动到池外位置
    if (bHideActor)
    {
        // 移动到远离游戏世界的位置
        FVector PoolLocation = FVector(0.0f, 0.0f, -100000.0f);
        Actor->SetActorLocation(PoolLocation, false, nullptr, ETeleportType::ResetPhysics);
    }
}

void FObjectPoolUtils::ResetActorPhysics(AActor* Actor)
{
    if (!IsValid(Actor))
    {
        return;
    }

    // ✅ 重置根组件物理
    if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
    {
        RootPrimitive->SetSimulatePhysics(false);
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
            Component->SetSimulatePhysics(false);
            Component->SetPhysicsLinearVelocity(FVector::ZeroVector);
            Component->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        }
    }
}

void FObjectPoolUtils::ResetActorComponents(AActor* Actor)
{
    if (!IsValid(Actor))
    {
        return;
    }

    // ✅ 重置ProjectileMovement组件
    TArray<UProjectileMovementComponent*> ProjectileComponents;
    Actor->GetComponents<UProjectileMovementComponent>(ProjectileComponents);

    for (UProjectileMovementComponent* ProjectileComp : ProjectileComponents)
    {
        if (IsValid(ProjectileComp))
        {
            ResetProjectileMovementComponent(ProjectileComp);
        }
    }

    // ✅ 重置粒子系统组件
    TArray<UParticleSystemComponent*> ParticleComponents;
    Actor->GetComponents<UParticleSystemComponent>(ParticleComponents);

    for (UParticleSystemComponent* ParticleComp : ParticleComponents)
    {
        if (IsValid(ParticleComp))
        {
            ParticleComp->DeactivateSystem();
            ParticleComp->ResetParticles();
        }
    }

    // ✅ 重置音频组件
    TArray<UAudioComponent*> AudioComponents;
    Actor->GetComponents<UAudioComponent>(AudioComponents);

    for (UAudioComponent* AudioComp : AudioComponents)
    {
        if (IsValid(AudioComp))
        {
            AudioComp->Stop();
            AudioComp->SetVolumeMultiplier(1.0f);
            AudioComp->SetPitchMultiplier(1.0f);
        }
    }

    // ✅ 重置移动组件
    TArray<UMovementComponent*> MovementComponents;
    Actor->GetComponents<UMovementComponent>(MovementComponents);

    for (UMovementComponent* MovementComp : MovementComponents)
    {
        if (IsValid(MovementComp))
        {
            MovementComp->StopMovementImmediately();
            MovementComp->Velocity = FVector::ZeroVector;
        }
    }

    // ✅ 重置网格组件
    TArray<UMeshComponent*> MeshComponents;
    Actor->GetComponents<UMeshComponent>(MeshComponents);

    for (UMeshComponent* MeshComp : MeshComponents)
    {
        if (IsValid(MeshComp))
        {
            // 重置材质参数等
            MeshComp->SetVisibility(true);
        }
    }
}

void FObjectPoolUtils::ApplyTransformToActor(AActor* Actor, const FTransform& NewTransform)
{
    if (!IsValid(Actor))
    {
        return;
    }

    Actor->SetActorTransform(NewTransform, false, nullptr, ETeleportType::ResetPhysics);
}

void FObjectPoolUtils::ResetProjectileMovementComponent(UProjectileMovementComponent* ProjectileComp)
{
    if (!IsValid(ProjectileComp))
    {
        return;
    }

    OBJECTPOOL_UTILS_LOG(VeryVerbose, TEXT("重置ProjectileMovement组件"));

    // ✅ 停止当前移动
    ProjectileComp->StopMovementImmediately();

    // ✅ 使用UE官方推荐的方式：从CDO重新初始化所有属性
    // 这会自动重置所有UPROPERTY标记的属性到默认值
    ProjectileComp->ReinitializeProperties();

    // ✅ 重置运行时状态
    ProjectileComp->Velocity = FVector::ZeroVector;

    // ✅ 停用组件（归还到池时应该停用）
    ProjectileComp->SetActive(false);
    ProjectileComp->SetComponentTickEnabled(false);

    OBJECTPOOL_UTILS_LOG(VeryVerbose, TEXT("ProjectileMovement组件重置完成"));
}

void FObjectPoolUtils::ResetComponentFromCDO(UActorComponent* Component)
{
    if (!IsValid(Component))
    {
        return;
    }

    OBJECTPOOL_UTILS_LOG(VeryVerbose, TEXT("使用CDO重置组件: %s"), *Component->GetName());

    // ✅ 使用UE官方推荐的方式：从CDO重新初始化所有属性
    // 这会自动重置所有UPROPERTY标记的属性到默认值
    Component->ReinitializeProperties();

    OBJECTPOOL_UTILS_LOG(VeryVerbose, TEXT("组件重置完成: %s"), *Component->GetName());
}

void FObjectPoolUtils::SaveOriginalCollisionSettings(UPrimitiveComponent* PrimComp)
{
    if (!IsValid(PrimComp))
    {
        return;
    }

    // 使用组件的标签来保存原始碰撞设置
    FString OriginalCollisionTag = FString::Printf(TEXT("OriginalCollision_%d"), (int32)PrimComp->GetCollisionEnabled());

    // 清除之前的标签
    PrimComp->ComponentTags.RemoveAll([](const FName& Tag) {
        return Tag.ToString().StartsWith(TEXT("OriginalCollision_"));
    });

    // 添加新的标签保存原始设置
    PrimComp->ComponentTags.Add(FName(*OriginalCollisionTag));

    OBJECTPOOL_UTILS_LOG(VeryVerbose, TEXT("保存组件碰撞设置: %s -> %s"),
        *PrimComp->GetName(), *OriginalCollisionTag);
}

void FObjectPoolUtils::RestoreOriginalCollisionSettings(UPrimitiveComponent* PrimComp)
{
    if (!IsValid(PrimComp))
    {
        return;
    }

    // 查找保存的碰撞设置标签
    for (const FName& Tag : PrimComp->ComponentTags)
    {
        FString TagString = Tag.ToString();
        if (TagString.StartsWith(TEXT("OriginalCollision_")))
        {
            // 提取原始碰撞设置
            FString CollisionValueStr = TagString.RightChop(18); // 移除 "OriginalCollision_" 前缀
            int32 CollisionValue = FCString::Atoi(*CollisionValueStr);
            ECollisionEnabled::Type OriginalCollision = (ECollisionEnabled::Type)CollisionValue;

            // 恢复原始碰撞设置
            PrimComp->SetCollisionEnabled(OriginalCollision);

            OBJECTPOOL_UTILS_LOG(VeryVerbose, TEXT("恢复组件碰撞设置: %s -> %d"),
                *PrimComp->GetName(), (int32)OriginalCollision);
            break;
        }
    }
}

void FObjectPoolUtils::GetDefaultConfigForActorClass(TSubclassOf<AActor> ActorClass, int32& OutInitialSize, int32& OutHardLimit)
{
    OutInitialSize = DEFAULT_POOL_SIZE;
    OutHardLimit = DEFAULT_HARD_LIMIT;

    if (!ActorClass)
    {
        return;
    }

    // ✅ 根据Actor类型调整默认值
    if (ActorClass->IsChildOf<ACharacter>())
    {
        OutInitialSize = 5;  // Character通常较重
        OutHardLimit = 20;
    }
    else if (ActorClass->IsChildOf<APawn>())
    {
        OutInitialSize = 8;
        OutHardLimit = 30;
    }
    else
    {
        OutInitialSize = DEFAULT_POOL_SIZE;
        OutHardLimit = DEFAULT_HARD_LIMIT;
    }
}

int64 FObjectPoolUtils::CalculateActorMemoryFootprint(TSubclassOf<AActor> ActorClass)
{
    if (!ActorClass)
    {
        return BASE_ACTOR_MEMORY;
    }

    // ✅ 基础内存 + 组件估算
    int64 EstimatedMemory = BASE_ACTOR_MEMORY;

    // ✅ 根据Actor类型调整估算
    if (ActorClass->IsChildOf<ACharacter>())
    {
        EstimatedMemory += COMPONENT_MEMORY_ESTIMATE * 10; // Character有更多组件
    }
    else if (ActorClass->IsChildOf<APawn>())
    {
        EstimatedMemory += COMPONENT_MEMORY_ESTIMATE * 5;
    }
    else
    {
        EstimatedMemory += COMPONENT_MEMORY_ESTIMATE * 2;
    }

    return EstimatedMemory;
}

// ✅ 性能计时器实现

FObjectPoolUtilsTimer::FObjectPoolUtilsTimer(const FString& OperationName)
    : Operation(OperationName)
    , StartTime(FPlatformTime::Seconds())
{
}

FObjectPoolUtilsTimer::~FObjectPoolUtilsTimer()
{
    double EndTime = FPlatformTime::Seconds();
    double ElapsedMs = (EndTime - StartTime) * 1000.0;

    if (ElapsedMs > 1.0) // 只记录超过1ms的操作
    {
        OBJECTPOOL_UTILS_LOG(Verbose, TEXT("操作 '%s' 耗时: %.2f ms"), *Operation, ElapsedMs);
    }
}


