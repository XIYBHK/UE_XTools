// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 遵循IWYU原则的头文件包含
#include "ObjectPoolSubsystem.h"

// ✅ UE核心依赖
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"

// ✅ 对象池模块依赖
#include "ObjectPool.h"
#include "ActorPool.h"
#include "ObjectPoolInterface.h"
#include "ActorStateResetter.h"
#include "ObjectPoolConfigManager.h"
#include "ObjectPoolDebugManager.h"

void UObjectPoolSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    OBJECTPOOL_LOG(Log, TEXT("ObjectPool子系统初始化中..."));

    // ✅ 初始化状态
    bIsInitialized = false;

    // ✅ 预分配容器空间
    ActorPools.Reserve(16);

    // ✅ 初始化默认对象池
    InitializeDefaultPools();

    // ✅ 初始化状态重置管理器
    StateResetter = MakeShared<FActorStateResetter>();

    // ✅ 初始化配置管理器
    ConfigManager = MakeShared<FObjectPoolConfigManager>();
    if (ConfigManager.IsValid())
    {
        ConfigManager->Initialize();
        OBJECTPOOL_LOG(Log, TEXT("配置管理器初始化完成"));
    }

    // ✅ 初始化调试管理器
    DebugManager = MakeShared<FObjectPoolDebugManager>();
    if (DebugManager.IsValid())
    {
        DebugManager->Initialize();
        OBJECTPOOL_LOG(Log, TEXT("调试管理器初始化完成"));
    }

    bIsInitialized = true;

    OBJECTPOOL_LOG(Log, TEXT("ObjectPool子系统初始化完成"));
}

void UObjectPoolSubsystem::Deinitialize()
{
    OBJECTPOOL_LOG(Log, TEXT("ObjectPool子系统关闭中..."));

    // ✅ 清理性能监控定时器
    DisablePerformanceMonitoring();

    // ✅ 清理配置管理器
    if (ConfigManager.IsValid())
    {
        ConfigManager->Shutdown();
        ConfigManager.Reset();
        OBJECTPOOL_LOG(Log, TEXT("配置管理器已清理"));
    }

    // ✅ 清理调试管理器
    if (DebugManager.IsValid())
    {
        DebugManager->Shutdown();
        DebugManager.Reset();
        OBJECTPOOL_LOG(Log, TEXT("调试管理器已清理"));
    }

    // ✅ 清空所有对象池
    ClearAllPools();

    // ✅ 清理资源
    {
        FScopeLock Lock(&PoolsLock);
        ActorPools.Empty();
        DefaultPoolConfigs.Empty();
    }

    bIsInitialized = false;

    OBJECTPOOL_LOG(Log, TEXT("ObjectPool子系统关闭完成"));

    Super::Deinitialize();
}

UObjectPoolSubsystem* UObjectPoolSubsystem::Get(const UObject* WorldContext)
{
    if (!IsValid(WorldContext))
    {
        OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolSubsystem::Get: WorldContext无效"));
        return nullptr;
    }

    // ✅ 获取GameInstance
    UGameInstance* GameInstance = nullptr;
    if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull))
    {
        GameInstance = World->GetGameInstance();
    }

    if (!IsValid(GameInstance))
    {
        OBJECTPOOL_LOG(Warning, TEXT("UObjectPoolSubsystem::Get: 无法获取GameInstance"));
        return nullptr;
    }

    // ✅ 获取子系统实例
    return GameInstance->GetSubsystem<UObjectPoolSubsystem>();
}

UObjectPoolSubsystem* UObjectPoolSubsystem::GetGlobal()
{
    // ✅ 智能查找策略：遍历所有可用的World上下文
    if (GEngine && GEngine->GetWorldContexts().Num() > 0)
    {
        // 优先查找游戏世界的子系统
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.World() && IsValid(Context.World()))
            {
                UWorld* World = Context.World();

                // 优先选择游戏世界
                if (World->IsGameWorld())
                {
                    if (UGameInstance* GameInstance = World->GetGameInstance())
                    {
                        if (UObjectPoolSubsystem* Subsystem = GameInstance->GetSubsystem<UObjectPoolSubsystem>())
                        {
                            return Subsystem;
                        }
                    }
                }
            }
        }

        // 如果没有游戏世界，使用任何可用的世界
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.World() && IsValid(Context.World()))
            {
                UWorld* World = Context.World();
                if (UGameInstance* GameInstance = World->GetGameInstance())
                {
                    if (UObjectPoolSubsystem* Subsystem = GameInstance->GetSubsystem<UObjectPoolSubsystem>())
                    {
                        return Subsystem;
                    }
                }
            }
        }
    }

    OBJECTPOOL_LOG(VeryVerbose, TEXT("GetGlobal: 无法找到可用的对象池子系统"));
    return nullptr;
}

UWorld* UObjectPoolSubsystem::GetValidWorldStatic(const UObject* WorldContext)
{
    // ✅ 如果提供了WorldContext，优先使用标准方法
    if (WorldContext)
    {
        if (UObjectPoolSubsystem* Subsystem = Get(WorldContext))
        {
            return Subsystem->GetValidWorld();
        }
    }

    // ✅ 否则使用全局查找
    if (UObjectPoolSubsystem* Subsystem = GetGlobal())
    {
        return Subsystem->GetValidWorld();
    }

    // ✅ 最后的回退：直接从引擎获取（保持兼容性）
    if (GEngine && GEngine->GetWorldContexts().Num() > 0)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.World() && IsValid(Context.World()))
            {
                return Context.World();
            }
        }
    }

    return nullptr;
}

void UObjectPoolSubsystem::RegisterActorClass(TSubclassOf<AActor> ActorClass, int32 InitialSize, int32 HardLimit)
{
    // ✅ 使用辅助函数验证Actor类
    if (!ValidateActorClass(ActorClass))
    {
        OBJECTPOOL_LOG(Warning, TEXT("RegisterActorClass: Actor类无效，忽略注册"));
        return;
    }

    UClass* Class = ActorClass.Get();

    // ✅ 参数范围检查和修正
    if (InitialSize < 0)
    {
        OBJECTPOOL_LOG(Warning, TEXT("RegisterActorClass: 初始大小不能为负数，设置为0"));
        InitialSize = 0;
    }

    if (HardLimit < 0)
    {
        OBJECTPOOL_LOG(Warning, TEXT("RegisterActorClass: 硬限制不能为负数，设置为0（无限制）"));
        HardLimit = 0;
    }

    // ✅ 即使子系统未初始化也允许注册（延迟初始化）
    if (!bIsInitialized)
    {
        OBJECTPOOL_LOG(Log, TEXT("RegisterActorClass: 子系统未初始化，将在初始化后处理"));
        // 可以考虑将配置保存到DefaultPoolConfigs中，等初始化后再处理
    }

    OBJECTPOOL_LOG(Log, TEXT("注册Actor类到对象池: %s, 初始大小: %d, 硬限制: %d"), 
        *Class->GetName(), InitialSize, HardLimit);

    // ✅ 获取或创建对象池
    TSharedPtr<FActorPool> Pool = GetOrCreatePool(Class);
    if (!Pool.IsValid())
    {
        OBJECTPOOL_LOG(Error, TEXT("创建对象池失败: %s"), *Class->GetName());
        return;
    }

    // ✅ 设置池参数
    Pool->SetHardLimit(HardLimit);

    // ✅ 预热池
    if (InitialSize > 0)
    {
        if (UWorld* World = GetWorld())
        {
            Pool->PrewarmPool(World, InitialSize);
        }
        else
        {
            OBJECTPOOL_LOG(Warning, TEXT("无法获取World，跳过预热: %s"), *Class->GetName());
        }
    }

    OBJECTPOOL_LOG(Log, TEXT("Actor类注册完成: %s"), *Class->GetName());
}

AActor* UObjectPoolSubsystem::SpawnActorFromPool(TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform)
{
    // OBJECTPOOL_STAT(ObjectPool_SpawnFromPool); // TODO: 实现性能统计

    // ✅ 永不失败原则：使用辅助函数进行安全检查
    UClass* Class = nullptr;

    // ✅ 验证Actor类
    if (ValidateActorClass(ActorClass))
    {
        Class = ActorClass.Get();
    }
    else
    {
        OBJECTPOOL_LOG(Warning, TEXT("SpawnActorFromPool: Actor类无效，使用默认Actor类"));
        Class = AActor::StaticClass();
    }

    // ✅ 获取有效的World - 永不失败原则
    UWorld* World = GetValidWorld();
    if (!World)
    {
        OBJECTPOOL_LOG(Error, TEXT("SpawnActorFromPool: 无法获取有效的World，尝试从引擎获取"));

        // ✅ 最后的回退：直接从引擎获取World
        if (GEngine && GEngine->GetWorldContexts().Num() > 0)
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.World() && IsValid(Context.World()))
                {
                    World = Context.World();
                    break;
                }
            }
        }

        // ✅ 如果仍然无法获取World，这是极端情况，但仍要遵循永不失败原则
        if (!World)
        {
            OBJECTPOOL_LOG(Error, TEXT("SpawnActorFromPool: 无法获取任何World，返回静态虚拟Actor"));
            // 返回一个静态的虚拟Actor，确保永不返回nullptr
            static AActor* EmergencyActor = NewObject<AActor>();
            return EmergencyActor;
        }
    }

    // ✅ 多级回退机制 - 确保永不失败
    AActor* Actor = nullptr;

    // 第一级：尝试从对象池获取
    Actor = TryGetFromPool(Class, SpawnTransform, World);
    if (Actor && ValidateSpawnedActor(Actor, Class))
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("从池获取Actor成功: %s"), *Actor->GetName());
        return Actor;
    }

    // 第二级：尝试直接创建指定类型
    Actor = TryCreateDirectly(Class, SpawnTransform, World);
    if (Actor && ValidateSpawnedActor(Actor, Class))
    {
        OBJECTPOOL_LOG(Verbose, TEXT("直接创建Actor成功: %s"), *Actor->GetName());
        return Actor;
    }

    // 第三级：回退到默认Actor类型
    Actor = FallbackToDefault(SpawnTransform, World);
    if (Actor)
    {
        OBJECTPOOL_LOG(Warning, TEXT("回退到默认Actor: %s"), *Actor->GetName());
        return Actor;
    }

    // ✅ 最后的保障：绝对永不失败原则
    OBJECTPOOL_LOG(Error, TEXT("所有回退机制都失败，创建紧急Actor"));

    // ✅ 创建一个最基本的Actor作为最后的保障
    FActorSpawnParameters EmergencyParams;
    EmergencyParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    EmergencyParams.bNoFail = true;

    Actor = World->SpawnActor<AActor>(AActor::StaticClass(), SpawnTransform, EmergencyParams);

    // ✅ 绝对保证永不返回nullptr - 这是设计文档的核心要求
    if (!Actor)
    {
        OBJECTPOOL_LOG(Error, TEXT("连基础Actor都无法创建，返回静态紧急Actor"));
        // 创建一个静态的紧急Actor，确保永不返回nullptr
        static AActor* StaticEmergencyActor = nullptr;
        if (!StaticEmergencyActor)
        {
            StaticEmergencyActor = NewObject<AActor>();
            StaticEmergencyActor->AddToRoot(); // 防止被垃圾回收
        }
        return StaticEmergencyActor;
    }

    return Actor;
}

void UObjectPoolSubsystem::ReturnActorToPool(AActor* Actor)
{
    // OBJECTPOOL_STAT(ObjectPool_ReturnToPool); // TODO: 实现性能统计

    // ✅ 安全检查：如果Actor无效，直接返回（不是错误）
    if (!IsValid(Actor))
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("ReturnActorToPool: Actor无效，忽略归还操作"));
        return;
    }

    UClass* ActorClass = Actor->GetClass();
    if (!ActorClass)
    {
        OBJECTPOOL_LOG(Warning, TEXT("ReturnActorToPool: 无法获取Actor类，安全销毁"));
        SafeDestroyActor(Actor);
        return;
    }

    // ✅ 即使子系统未初始化也要处理Actor（避免内存泄漏）
    if (!bIsInitialized)
    {
        OBJECTPOOL_LOG(Verbose, TEXT("ReturnActorToPool: 子系统未初始化，安全销毁Actor: %s"), *ActorClass->GetName());
        SafeDestroyActor(Actor);
        return;
    }

    // ✅ 查找对应的对象池
    TSharedPtr<FActorPool> Pool;
    {
        FScopeLock Lock(&PoolsLock);
        if (TSharedPtr<FActorPool>* FoundPool = ActorPools.Find(ActorClass))
        {
            Pool = *FoundPool;
        }
    }

    if (!Pool.IsValid())
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("Actor类未注册到对象池，安全销毁: %s"), *ActorClass->GetName());
        SafeDestroyActor(Actor);
        return;
    }

    // ✅ 归还到池
    if (Pool->ReturnActor(Actor))
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("Actor归还到池成功: %s"), *Actor->GetName());
    }
    else
    {
        OBJECTPOOL_LOG(Warning, TEXT("Actor归还到池失败，安全销毁: %s"), *Actor->GetName());
        SafeDestroyActor(Actor);
    }
}

void UObjectPoolSubsystem::PrewarmPool(TSubclassOf<AActor> ActorClass, int32 Count)
{
    if (!ActorClass || Count <= 0)
    {
        return;
    }

    UClass* Class = ActorClass.Get();
    UWorld* World = GetWorld();

    if (!Class || !World)
    {
        return;
    }

    // ✅ 获取或创建对象池
    TSharedPtr<FActorPool> Pool = GetOrCreatePool(Class);
    if (Pool.IsValid())
    {
        Pool->PrewarmPool(World, Count);
        OBJECTPOOL_LOG(Log, TEXT("预热对象池: %s, 数量: %d"), *Class->GetName(), Count);
    }
}

FObjectPoolStats UObjectPoolSubsystem::GetPoolStats(TSubclassOf<AActor> ActorClass)
{
    if (!ActorClass)
    {
        return FObjectPoolStats();
    }

    UClass* Class = ActorClass.Get();
    if (!Class)
    {
        return FObjectPoolStats();
    }

    // ✅ 查找对象池
    TSharedPtr<FActorPool> Pool;
    {
        FScopeLock Lock(&PoolsLock);
        if (TSharedPtr<FActorPool>* FoundPool = ActorPools.Find(Class))
        {
            Pool = *FoundPool;
        }
    }

    if (Pool.IsValid())
    {
        return Pool->GetStats();
    }

    return FObjectPoolStats();
}

void UObjectPoolSubsystem::ClearPool(TSubclassOf<AActor> ActorClass)
{
    if (!ActorClass)
    {
        return;
    }

    UClass* Class = ActorClass.Get();
    if (!Class)
    {
        return;
    }

    // ✅ 查找并清空对象池
    TSharedPtr<FActorPool> Pool;
    {
        FScopeLock Lock(&PoolsLock);
        if (TSharedPtr<FActorPool>* FoundPool = ActorPools.Find(Class))
        {
            Pool = *FoundPool;
        }
    }

    if (Pool.IsValid())
    {
        Pool->ClearPool();
        OBJECTPOOL_LOG(Log, TEXT("清空对象池: %s"), *Class->GetName());
    }
}

void UObjectPoolSubsystem::ClearAllPools()
{
    OBJECTPOOL_LOG(Log, TEXT("清空所有对象池"));

    FScopeLock Lock(&PoolsLock);

    for (auto& PoolPair : ActorPools)
    {
        if (PoolPair.Value.IsValid())
        {
            PoolPair.Value->ClearPool();
        }
    }

    ActorPools.Empty();

    OBJECTPOOL_LOG(Log, TEXT("所有对象池已清空"));
}

TArray<FObjectPoolStats> UObjectPoolSubsystem::GetAllPoolStats()
{
    TArray<FObjectPoolStats> AllStats;

    FScopeLock Lock(&PoolsLock);

    for (const auto& PoolPair : ActorPools)
    {
        if (PoolPair.Value.IsValid())
        {
            AllStats.Add(PoolPair.Value->GetStats());
        }
    }

    return AllStats;
}

bool UObjectPoolSubsystem::IsActorClassRegistered(TSubclassOf<AActor> ActorClass)
{
    if (!ActorClass)
    {
        return false;
    }

    UClass* Class = ActorClass.Get();
    if (!Class)
    {
        return false;
    }

    FScopeLock Lock(&PoolsLock);
    return ActorPools.Contains(Class);
}

bool UObjectPoolSubsystem::ValidateAllPools()
{
    FScopeLock Lock(&PoolsLock);

    bool bAllValid = true;

    for (auto& PoolPair : ActorPools)
    {
        if (!PoolPair.Value.IsValid() || !PoolPair.Value->ValidatePool())
        {
            OBJECTPOOL_LOG(Warning, TEXT("对象池验证失败: %s"),
                PoolPair.Key ? *PoolPair.Key->GetName() : TEXT("Unknown"));
            bAllValid = false;
        }
    }

    return bAllValid;
}

TSharedPtr<FActorPool> UObjectPoolSubsystem::GetOrCreatePool(UClass* ActorClass)
{
    if (!ActorClass)
    {
        return nullptr;
    }

    FScopeLock Lock(&PoolsLock);

    // ✅ 查找现有池
    if (TSharedPtr<FActorPool>* ExistingPool = ActorPools.Find(ActorClass))
    {
        return *ExistingPool;
    }

    // ✅ 创建新池
    TSharedPtr<FActorPool> NewPool = MakeShared<FActorPool>(ActorClass, 0, 0);
    ActorPools.Add(ActorClass, NewPool);

    OBJECTPOOL_LOG(Log, TEXT("创建新对象池: %s"), *ActorClass->GetName());

    return NewPool;
}

void UObjectPoolSubsystem::CleanupInvalidPools()
{
    FScopeLock Lock(&PoolsLock);

    // ✅ 清理无效的池
    for (auto It = ActorPools.CreateIterator(); It; ++It)
    {
        if (!It.Value().IsValid() || !IsValid(It.Key()))
        {
            OBJECTPOOL_LOG(Log, TEXT("清理无效对象池: %s"),
                It.Key() ? *It.Key()->GetName() : TEXT("Unknown"));
            It.RemoveCurrent();
        }
    }
}

void UObjectPoolSubsystem::InitializeDefaultPools()
{
    // ✅ 这里可以添加默认的对象池配置
    // 例如：常用的Actor类型可以预先注册

    OBJECTPOOL_LOG(Verbose, TEXT("初始化默认对象池配置"));

    // TODO: 从配置文件或设置中读取默认池配置
    // 目前保持空实现，用户需要手动注册Actor类
}

UWorld* UObjectPoolSubsystem::GetValidWorld() const
{
    // ✅ 首先尝试从子系统获取World
    UWorld* World = GetWorld();
    if (IsValid(World))
    {
        return World;
    }

    // ✅ 如果失败，尝试从引擎获取
    if (GEngine && GEngine->GetWorldContexts().Num() > 0)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (IsValid(Context.World()))
            {
                return Context.World();
            }
        }
    }

    OBJECTPOOL_LOG(Warning, TEXT("无法获取有效的World实例"));
    return nullptr;
}

bool UObjectPoolSubsystem::ValidateActorClass(TSubclassOf<AActor> ActorClass) const
{
    if (!ActorClass)
    {
        return false;
    }

    UClass* Class = ActorClass.Get();
    if (!Class)
    {
        return false;
    }

    // ✅ 检查是否是Actor的子类
    if (!Class->IsChildOf(AActor::StaticClass()))
    {
        OBJECTPOOL_LOG(Warning, TEXT("类 %s 不是Actor的子类"), *Class->GetName());
        return false;
    }

    return true;
}

void UObjectPoolSubsystem::SafeDestroyActor(AActor* Actor) const
{
    if (!IsValid(Actor))
    {
        return;
    }

    // ✅ 安全销毁Actor
    try
    {
        Actor->Destroy();
        OBJECTPOOL_LOG(VeryVerbose, TEXT("安全销毁Actor: %s"), *Actor->GetName());
    }
    catch (...)
    {
        OBJECTPOOL_LOG(Error, TEXT("销毁Actor时发生异常: %s"), *Actor->GetName());
    }
}

AActor* UObjectPoolSubsystem::TryGetFromPool(UClass* ActorClass, const FTransform& SpawnTransform, UWorld* World)
{
    // ✅ 检查前置条件
    if (!bIsInitialized || !ActorClass || !World)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("TryGetFromPool: 前置条件不满足"));
        return nullptr;
    }

    // ✅ 尝试获取或创建对象池
    TSharedPtr<FActorPool> Pool = GetOrCreatePool(ActorClass);
    if (!Pool.IsValid())
    {
        OBJECTPOOL_LOG(Verbose, TEXT("TryGetFromPool: 无法获取对象池: %s"), *ActorClass->GetName());
        return nullptr;
    }

    // ✅ 从池中获取Actor
    AActor* Actor = Pool->GetActor(World, SpawnTransform);
    if (Actor)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("TryGetFromPool: 成功从池获取: %s"), *Actor->GetName());
    }
    else
    {
        OBJECTPOOL_LOG(Verbose, TEXT("TryGetFromPool: 池为空或获取失败: %s"), *ActorClass->GetName());
    }

    return Actor;
}

AActor* UObjectPoolSubsystem::TryCreateDirectly(UClass* ActorClass, const FTransform& SpawnTransform, UWorld* World)
{
    if (!ActorClass || !World)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("TryCreateDirectly: 参数无效"));
        return nullptr;
    }

    // ✅ 尝试直接创建指定类型的Actor
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.bNoFail = false; // 允许失败，这样我们可以尝试其他回退策略

    AActor* Actor = World->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);

    if (Actor)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("TryCreateDirectly: 成功创建: %s"), *Actor->GetName());
    }
    else
    {
        OBJECTPOOL_LOG(Verbose, TEXT("TryCreateDirectly: 创建失败: %s"), *ActorClass->GetName());
    }

    return Actor;
}

AActor* UObjectPoolSubsystem::FallbackToDefault(const FTransform& SpawnTransform, UWorld* World)
{
    if (!World)
    {
        OBJECTPOOL_LOG(Warning, TEXT("FallbackToDefault: World无效，创建静态Actor"));
        // ✅ 即使World无效，也要遵循永不失败原则
        static AActor* StaticFallbackActor = nullptr;
        if (!StaticFallbackActor)
        {
            StaticFallbackActor = NewObject<AActor>();
            StaticFallbackActor->AddToRoot();
        }
        return StaticFallbackActor;
    }

    // ✅ 尝试创建默认Actor类型 - 使用bNoFail确保成功
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.bNoFail = true; // ✅ 确保不失败

    AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), SpawnTransform, SpawnParams);

    // ✅ 双重保障：即使bNoFail=true，也要检查结果
    if (Actor)
    {
        OBJECTPOOL_LOG(Verbose, TEXT("FallbackToDefault: 成功创建默认Actor: %s"), *Actor->GetName());
        return Actor;
    }
    else
    {
        OBJECTPOOL_LOG(Error, TEXT("FallbackToDefault: 即使bNoFail=true也失败，返回静态Actor"));
        // ✅ 最后的保障
        static AActor* EmergencyFallbackActor = nullptr;
        if (!EmergencyFallbackActor)
        {
            EmergencyFallbackActor = NewObject<AActor>();
            EmergencyFallbackActor->AddToRoot();
        }
        return EmergencyFallbackActor;
    }
}

bool UObjectPoolSubsystem::ValidateSpawnedActor(AActor* Actor, UClass* ExpectedClass) const
{
    if (!IsValid(Actor))
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("ValidateSpawnedActor: Actor无效"));
        return false;
    }

    if (!ExpectedClass)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("ValidateSpawnedActor: 期望类型无效，但Actor有效"));
        return true; // Actor有效就行
    }

    // ✅ 检查Actor是否是期望类型或其子类
    if (!Actor->IsA(ExpectedClass))
    {
        OBJECTPOOL_LOG(Verbose, TEXT("ValidateSpawnedActor: Actor类型不匹配，期望: %s, 实际: %s"),
            *ExpectedClass->GetName(), *Actor->GetClass()->GetName());
        return false;
    }

    // ✅ 检查Actor是否在正确的World中
    if (Actor->GetWorld() != GetWorld())
    {
        OBJECTPOOL_LOG(Verbose, TEXT("ValidateSpawnedActor: Actor不在正确的World中"));
        return false;
    }

    OBJECTPOOL_LOG(VeryVerbose, TEXT("ValidateSpawnedActor: Actor验证通过: %s"), *Actor->GetName());
    return true;
}

// ✅ Actor状态重置公共API实现

bool UObjectPoolSubsystem::ResetActorState(AActor* Actor, const FTransform& SpawnTransform, const FActorResetConfig& ResetConfig)
{
    if (!IsValid(Actor))
    {
        OBJECTPOOL_LOG(Warning, TEXT("ResetActorState: Actor无效"));
        return false;
    }

    if (!StateResetter.IsValid())
    {
        OBJECTPOOL_LOG(Warning, TEXT("ResetActorState: 状态重置管理器未初始化"));
        return false;
    }

    return StateResetter->ResetActorState(Actor, SpawnTransform, ResetConfig);
}

int32 UObjectPoolSubsystem::BatchResetActorStates(const TArray<AActor*>& Actors, const FActorResetConfig& ResetConfig)
{
    if (Actors.Num() == 0)
    {
        OBJECTPOOL_LOG(VeryVerbose, TEXT("BatchResetActorStates: 空的Actor数组"));
        return 0;
    }

    if (!StateResetter.IsValid())
    {
        OBJECTPOOL_LOG(Warning, TEXT("BatchResetActorStates: 状态重置管理器未初始化"));
        return 0;
    }

    return StateResetter->BatchResetActorStates(Actors, TArray<FTransform>(), ResetConfig);
}

FActorResetStats UObjectPoolSubsystem::GetActorResetStats() const
{
    if (!StateResetter.IsValid())
    {
        OBJECTPOOL_LOG(Warning, TEXT("GetActorResetStats: 状态重置管理器未初始化"));
        return FActorResetStats();
    }

    return StateResetter->GetResetStats();
}



// ✅ 性能统计和调试功能实现

void UObjectPoolSubsystem::LogPerformanceStats()
{
    OBJECTPOOL_LOG(Warning, TEXT("=== 对象池性能统计报告 ==="));

    if (ActorPools.Num() == 0)
    {
        OBJECTPOOL_LOG(Warning, TEXT("当前没有活跃的对象池"));
        return;
    }

    int32 TotalPools = 0;
    int32 TotalAvailableActors = 0;
    int32 TotalActiveActors = 0;
    int32 TotalCreatedActors = 0;
    float TotalHitRate = 0.0f;

    for (const auto& PoolPair : ActorPools)
    {
        if (FActorPool* Pool = PoolPair.Value.Get())
        {
            FActorPoolStats Stats = Pool->GetPoolStats();
            UClass* ActorClass = PoolPair.Key;

            OBJECTPOOL_LOG(Warning, TEXT("池 [%s]:"), ActorClass ? *ActorClass->GetName() : TEXT("Unknown"));
            OBJECTPOOL_LOG(Warning, TEXT("  - 可用Actor: %d"), Stats.CurrentAvailable);
            OBJECTPOOL_LOG(Warning, TEXT("  - 活跃Actor: %d"), Stats.CurrentActive);
            OBJECTPOOL_LOG(Warning, TEXT("  - 总创建数: %d"), Stats.TotalCreated);
            OBJECTPOOL_LOG(Warning, TEXT("  - 命中率: %.1f%%"), Stats.HitRate * 100.0f);
            OBJECTPOOL_LOG(Warning, TEXT("  - 最后使用: %s"), *Stats.LastUsedTime.ToString());

            ++TotalPools;
            TotalAvailableActors += Stats.CurrentAvailable;
            TotalActiveActors += Stats.CurrentActive;
            TotalCreatedActors += Stats.TotalCreated;
            TotalHitRate += Stats.HitRate;
        }
    }

    // ✅ 输出汇总统计
    OBJECTPOOL_LOG(Warning, TEXT("=== 汇总统计 ==="));
    OBJECTPOOL_LOG(Warning, TEXT("总池数: %d"), TotalPools);
    OBJECTPOOL_LOG(Warning, TEXT("总可用Actor: %d"), TotalAvailableActors);
    OBJECTPOOL_LOG(Warning, TEXT("总活跃Actor: %d"), TotalActiveActors);
    OBJECTPOOL_LOG(Warning, TEXT("总创建Actor: %d"), TotalCreatedActors);
    OBJECTPOOL_LOG(Warning, TEXT("平均命中率: %.1f%%"), TotalPools > 0 ? (TotalHitRate / TotalPools) * 100.0f : 0.0f);

    // ✅ 输出状态重置统计
    if (StateResetter.IsValid())
    {
        FActorResetStats ResetStats = StateResetter->GetResetStats();
        OBJECTPOOL_LOG(Warning, TEXT("=== 状态重置统计 ==="));
        OBJECTPOOL_LOG(Warning, TEXT("总重置次数: %d"), ResetStats.TotalResetCount);
        OBJECTPOOL_LOG(Warning, TEXT("重置成功率: %.1f%%"), ResetStats.ResetSuccessRate * 100.0f);
        OBJECTPOOL_LOG(Warning, TEXT("平均重置耗时: %.2fms"), ResetStats.AverageResetTimeMs);
    }
}

void UObjectPoolSubsystem::LogMemoryUsage()
{
    OBJECTPOOL_LOG(Warning, TEXT("=== 对象池内存使用报告 ==="));

    if (ActorPools.Num() == 0)
    {
        OBJECTPOOL_LOG(Warning, TEXT("当前没有活跃的对象池"));
        return;
    }

    int64 TotalMemoryUsage = 0;

    for (const auto& PoolPair : ActorPools)
    {
        if (FActorPool* Pool = PoolPair.Value.Get())
        {
            int64 PoolMemory = Pool->CalculateMemoryUsage();
            UClass* ActorClass = PoolPair.Key;

            OBJECTPOOL_LOG(Warning, TEXT("池 [%s]: %.2f KB"),
                ActorClass ? *ActorClass->GetName() : TEXT("Unknown"),
                PoolMemory / 1024.0f);

            TotalMemoryUsage += PoolMemory;
        }
    }

    OBJECTPOOL_LOG(Warning, TEXT("=== 内存使用汇总 ==="));
    OBJECTPOOL_LOG(Warning, TEXT("总内存使用: %.2f KB (%.2f MB)"),
        TotalMemoryUsage / 1024.0f,
        TotalMemoryUsage / (1024.0f * 1024.0f));

    // ✅ 输出系统内存信息
    FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    OBJECTPOOL_LOG(Warning, TEXT("系统可用内存: %.2f MB"), MemStats.AvailablePhysical / (1024.0f * 1024.0f));
    OBJECTPOOL_LOG(Warning, TEXT("系统已用内存: %.2f MB"), MemStats.UsedPhysical / (1024.0f * 1024.0f));
}

void UObjectPoolSubsystem::EnablePerformanceMonitoring(float IntervalSeconds)
{
    if (IntervalSeconds <= 0.0f)
    {
        OBJECTPOOL_LOG(Warning, TEXT("EnablePerformanceMonitoring: 无效的间隔时间 %.2f，使用默认值60秒"), IntervalSeconds);
        IntervalSeconds = 60.0f;
    }

    // ✅ 清除现有定时器
    if (PerformanceMonitoringTimer.IsValid())
    {
        if (UWorld* World = GetValidWorld())
        {
            World->GetTimerManager().ClearTimer(PerformanceMonitoringTimer);
        }
    }

    // ✅ 设置新的定时器
    if (UWorld* World = GetValidWorld())
    {
        World->GetTimerManager().SetTimer(
            PerformanceMonitoringTimer,
            this,
            &UObjectPoolSubsystem::LogPerformanceStats,
            IntervalSeconds,
            true  // 循环执行
        );

        OBJECTPOOL_LOG(Warning, TEXT("性能监控已启用，间隔: %.1f秒"), IntervalSeconds);
    }
    else
    {
        OBJECTPOOL_LOG(Error, TEXT("EnablePerformanceMonitoring: 无法获取有效的World"));
    }
}

void UObjectPoolSubsystem::DisablePerformanceMonitoring()
{
    if (PerformanceMonitoringTimer.IsValid())
    {
        if (UWorld* World = GetValidWorld())
        {
            World->GetTimerManager().ClearTimer(PerformanceMonitoringTimer);
            PerformanceMonitoringTimer.Invalidate();
            OBJECTPOOL_LOG(Warning, TEXT("性能监控已禁用"));
        }
    }
    else
    {
        OBJECTPOOL_LOG(Warning, TEXT("性能监控未启用"));
    }
}

// ✅ 配置管理功能实现 - 利用已有的配置系统

bool UObjectPoolSubsystem::ApplyConfigTemplate(const FString& TemplateName)
{
    if (!ConfigManager.IsValid())
    {
        OBJECTPOOL_LOG(Warning, TEXT("ApplyConfigTemplate: 配置管理器未初始化"));
        return false;
    }

    if (TemplateName.IsEmpty())
    {
        OBJECTPOOL_LOG(Warning, TEXT("ApplyConfigTemplate: 模板名称为空"));
        return false;
    }

    bool bSuccess = ConfigManager->ApplyPresetTemplate(TemplateName, this);
    if (bSuccess)
    {
        OBJECTPOOL_LOG(Log, TEXT("成功应用配置模板: %s"), *TemplateName);
    }
    else
    {
        OBJECTPOOL_LOG(Warning, TEXT("应用配置模板失败: %s"), *TemplateName);
    }

    return bSuccess;
}

TArray<FString> UObjectPoolSubsystem::GetAvailableConfigTemplates() const
{
    if (!ConfigManager.IsValid())
    {
        OBJECTPOOL_LOG(Warning, TEXT("GetAvailableConfigTemplates: 配置管理器未初始化"));
        return TArray<FString>();
    }

    TArray<FString> Templates = ConfigManager->GetAvailableTemplateNames();
    OBJECTPOOL_LOG(VeryVerbose, TEXT("获取到 %d 个可用配置模板"), Templates.Num());

    return Templates;
}

void UObjectPoolSubsystem::ResetToDefaultConfig()
{
    if (!ConfigManager.IsValid())
    {
        OBJECTPOOL_LOG(Warning, TEXT("ResetToDefaultConfig: 配置管理器未初始化"));
        return;
    }

    ConfigManager->ResetToDefaults(this);
    OBJECTPOOL_LOG(Log, TEXT("已重置为默认配置"));
}

// ✅ 调试工具功能实现 - 基于已有的统计系统

void UObjectPoolSubsystem::SetDebugMode(int32 DebugMode)
{
    if (!DebugManager.IsValid())
    {
        OBJECTPOOL_LOG(Warning, TEXT("SetDebugMode: 调试管理器未初始化"));
        return;
    }

    EObjectPoolDebugMode Mode = static_cast<EObjectPoolDebugMode>(DebugMode);
    DebugManager->SetDebugMode(Mode);
    OBJECTPOOL_LOG(Log, TEXT("调试模式已设置为: %d"), DebugMode);
}

FString UObjectPoolSubsystem::GetDebugSummary()
{
    if (!DebugManager.IsValid())
    {
        OBJECTPOOL_LOG(Warning, TEXT("GetDebugSummary: 调试管理器未初始化"));
        return TEXT("调试管理器未初始化");
    }

    // ✅ 利用调试管理器的功能，基于已有统计数据
    return DebugManager->GetDebugSummary(this);
}

TArray<FString> UObjectPoolSubsystem::DetectPerformanceHotspots()
{
    TArray<FString> HotspotDescriptions;

    if (!DebugManager.IsValid())
    {
        OBJECTPOOL_LOG(Warning, TEXT("DetectPerformanceHotspots: 调试管理器未初始化"));
        HotspotDescriptions.Add(TEXT("调试管理器未初始化"));
        return HotspotDescriptions;
    }

    // ✅ 利用已有的统计API进行简化的热点检测
    TArray<FObjectPoolStats> AllStats = GetAllPoolStats();

    for (const FObjectPoolStats& PoolStats : AllStats)
    {
        // 检测低命中率
        if (PoolStats.HitRate < 0.5f)
        {
            FString Description = FString::Printf(TEXT("[低命中率] %s - 命中率仅为 %.1f%% (建议: 增加初始池大小)"),
                *PoolStats.ActorClassName, PoolStats.HitRate * 100.0f);
            HotspotDescriptions.Add(Description);
        }

        // 检测大池
        if (PoolStats.PoolSize > 100)
        {
            FString Description = FString::Printf(TEXT("[大池] %s - 池大小为 %d (建议: 启用自动收缩)"),
                *PoolStats.ActorClassName, PoolStats.PoolSize);
            HotspotDescriptions.Add(Description);
        }

        // 检测空闲池
        if (PoolStats.CurrentActive == 0 && PoolStats.CurrentAvailable > 0)
        {
            FString Description = FString::Printf(TEXT("[空闲池] %s - 有 %d 个未使用的Actor (建议: 启用自动收缩)"),
                *PoolStats.ActorClassName, PoolStats.CurrentAvailable);
            HotspotDescriptions.Add(Description);
        }
    }

    // 检测重置性能问题
    FActorResetStats ResetStats = GetActorResetStats();
    if (ResetStats.AverageResetTimeMs > 10.0f)
    {
        FString Description = FString::Printf(TEXT("[慢重置] 全局 - 平均重置耗时 %.2fms (建议: 优化重置配置)"),
            ResetStats.AverageResetTimeMs);
        HotspotDescriptions.Add(Description);
    }

    OBJECTPOOL_LOG(Log, TEXT("检测到 %d 个性能热点"), HotspotDescriptions.Num());
    return HotspotDescriptions;
}
