/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


//  遵循IWYU原则的头文件包含
#include "ObjectPoolPreallocator.h"

//  UE核心依赖
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "HAL/PlatformFilemanager.h"

//  对象池模块依赖
#include "ObjectPool.h"
#include "ActorPool.h"
#include "ObjectPoolUtils.h"
#include "ObjectPoolSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include <limits>
#endif

namespace
{
bool IsValidPreallocationConfig(const FObjectPoolConfig& Config)
{
    if (Config.PreallocationCount <= 0 ||
        !FMath::IsFinite(Config.PreallocationDelay) ||
        Config.PreallocationDelay < 0.0f)
    {
        return false;
    }

    switch (Config.PreallocationStrategy)
    {
    case EObjectPoolPreallocationStrategy::Immediate:
        return true;

    case EObjectPoolPreallocationStrategy::Progressive:
    case EObjectPoolPreallocationStrategy::Predictive:
    case EObjectPoolPreallocationStrategy::Adaptive:
        return Config.MaxAllocationsPerFrame > 0;

    default:
        return false;
    }
}

int32 CalculatePredictiveAllocationCount(
    int32 PredictedCount,
    int32 CurrentCount,
    int32 TargetCount,
    int32 MaxAllocationsPerFrame)
{
    const int32 PredictedShortfall = FMath::Max(0, PredictedCount - CurrentCount);
    const int32 RemainingTargetCount = FMath::Max(0, TargetCount - CurrentCount);
    return FMath::Min3(PredictedShortfall, RemainingTargetCount, FMath::Max(0, MaxAllocationsPerFrame));
}
}

FObjectPoolPreallocator::FObjectPoolPreallocator(FActorPool* InOwnerPool)
    : OwnerPool(InOwnerPool)
{
    check(OwnerPool);
    
    // 初始化统计信息
    Stats = FObjectPoolPreallocationStats();
    
    OBJECTPOOL_LOG(VeryVerbose, TEXT("ObjectPoolPreallocator创建"));
}

FObjectPoolPreallocator::~FObjectPoolPreallocator()
{
    StopPreallocation();
    OBJECTPOOL_LOG(VeryVerbose, TEXT("ObjectPoolPreallocator销毁"));
}

bool FObjectPoolPreallocator::StartPreallocation(UWorld* World, const FObjectPoolConfig& InConfig)
{
    if (!World || !OwnerPool)
    {
        OBJECTPOOL_LOG(Warning, TEXT("StartPreallocation: 无效的World或OwnerPool"));
        return false;
    }

    if (XTOOLS_ATOMIC_LOAD(bIsActive))
    {
        OBJECTPOOL_LOG(Warning, TEXT("StartPreallocation: 预分配已在进行中"));
        return false;
    }

    if (!IsValidPreallocationConfig(InConfig))
    {
        OBJECTPOOL_LOG(Warning, TEXT("StartPreallocation: 无效的预分配配置"));
        return false;
    }

    //  保存配置
    {
        FScopeLock Lock(&PreallocatorLock);
        Config = InConfig;
        Stats = FObjectPoolPreallocationStats();
        Stats.TargetCount = Config.PreallocationCount;
        Stats.PreallocationStartTime = FDateTime::Now();
        XTOOLS_ATOMIC_STORE(CurrentProgress, 0);
        AccumulatedTime = 0.0f;
        OwnerWorld = World;
    }

    //  检查内存预算
    int32 EstimatedActorSize = EstimateActorMemorySize(OwnerPool->GetActorClass());
    int64 EstimatedTotalMemory = (int64)EstimatedActorSize * Config.PreallocationCount;
    
    if (!CheckMemoryBudget(EstimatedTotalMemory))
    {
        OBJECTPOOL_LOG(Warning, TEXT("StartPreallocation: 超出内存预算限制"));
        return false;
    }

    XTOOLS_ATOMIC_STORE(bIsActive, true);

    if (TickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        TickHandle.Reset();
    }

    //  根据策略执行预分配
    switch (Config.PreallocationStrategy)
    {
    case EObjectPoolPreallocationStrategy::Immediate:
        ExecuteImmediatePreallocation(World, Config.PreallocationCount);
        break;

    case EObjectPoolPreallocationStrategy::Progressive:
    case EObjectPoolPreallocationStrategy::Predictive:
    case EObjectPoolPreallocationStrategy::Adaptive:
        // 这些策略在Tick中执行
        TickHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateRaw(this, &FObjectPoolPreallocator::HandleTicker));
        break;

    default:
        OBJECTPOOL_LOG(Warning, TEXT("StartPreallocation: 不支持的预分配策略"));
        XTOOLS_ATOMIC_STORE(bIsActive, false);
        return false;
    }

    OBJECTPOOL_LOG(Log, TEXT("StartPreallocation: 启动预分配，策略: %d, 目标数量: %d"),
        (int32)Config.PreallocationStrategy, Config.PreallocationCount);

    return true;
}

void FObjectPoolPreallocator::StopPreallocation()
{
    if (!XTOOLS_ATOMIC_LOAD(bIsActive))
    {
        return;
    }

    XTOOLS_ATOMIC_STORE(bIsActive, false);
    OwnerWorld.Reset();

    if (TickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        TickHandle.Reset();
    }
    
    {
        FScopeLock Lock(&PreallocatorLock);
        Stats.PreallocationEndTime = FDateTime::Now();
        Stats.TotalPreallocationTimeMs = (Stats.PreallocationEndTime - Stats.PreallocationStartTime).GetTotalMilliseconds();
        if (Stats.PreallocationOperations > 0)
        {
            Stats.AveragePreallocationTimeMs = Stats.TotalPreallocationTimeMs / Stats.PreallocationOperations;
        }
    }

    OBJECTPOOL_LOG(Log, TEXT("StopPreallocation: 停止预分配，完成度: %.1f%%"), 
        Stats.GetCompletionPercentage());
}

void FObjectPoolPreallocator::Tick(float DeltaTime)
{
    if (!XTOOLS_ATOMIC_LOAD(bIsActive) || !OwnerPool)
    {
        return;
    }

    AccumulatedTime += DeltaTime;

    // Tick DeltaTime 使用秒，配置面板中的 PreallocationDelay 使用毫秒。
    const float PreallocationDelaySeconds = Config.PreallocationDelay / 1000.0f;
    if (AccumulatedTime < PreallocationDelaySeconds)
    {
        return;
    }

    UWorld* World = OwnerWorld.Get();

    if (!World)
    {
        OBJECTPOOL_LOG(Warning, TEXT("Tick: 无法获取有效的World，停止预分配"));
        StopPreallocation();
        return;
    }

    //  根据策略执行预分配
    switch (Config.PreallocationStrategy)
    {
    case EObjectPoolPreallocationStrategy::Progressive:
        ExecuteProgressivePreallocation(World, DeltaTime);
        break;
        
    case EObjectPoolPreallocationStrategy::Predictive:
        ExecutePredictivePreallocation(World);
        break;
        
    case EObjectPoolPreallocationStrategy::Adaptive:
        ExecuteAdaptivePreallocation(World, DeltaTime);
        break;
        
    default:
        // 其他策略不需要Tick
        break;
    }

    //  更新统计信息
    UpdateStats();

    //  检查是否完成
    if (!ShouldContinuePreallocation())
    {
        StopPreallocation();
    }
}

bool FObjectPoolPreallocator::HandleTicker(float DeltaTime)
{
    if (!XTOOLS_ATOMIC_LOAD(bIsActive))
    {
        TickHandle.Reset();
        return false;
    }

    Tick(DeltaTime);
    return XTOOLS_ATOMIC_LOAD(bIsActive);
}

FObjectPoolPreallocationStats FObjectPoolPreallocator::GetStats() const
{
    FScopeLock Lock(&PreallocatorLock);
    return Stats;
}

void FObjectPoolPreallocator::ExecuteImmediatePreallocation(UWorld* World, int32 Count)
{
    OBJECTPOOL_LOG(Log, TEXT("ExecuteImmediatePreallocation: 开始立即预分配 %d 个Actor"), Count);

    double StartTime = FPlatformTime::Seconds();
    int32 SuccessCount = 0;

    for (int32 i = 0; i < Count; ++i)
    {
        if (CreateSingleActor(World))
        {
            ++SuccessCount;
            XTOOLS_ATOMIC_STORE(CurrentProgress, SuccessCount);
        }
        else
        {
            OBJECTPOOL_LOG(Warning, TEXT("ExecuteImmediatePreallocation: 创建Actor失败，索引: %d"), i);
            if (!OwnerPool->CanCreateMoreActors())
            {
                break;
            }
        }

        //  检查内存预算
        if (Config.bEnableMemoryBudget)
        {
            int64 CurrentMemory = OwnerPool->CalculateMemoryUsage();
            if (!CheckMemoryBudget(CurrentMemory))
            {
                OBJECTPOOL_LOG(Warning, TEXT("ExecuteImmediatePreallocation: 达到内存预算限制，停止预分配"));
                break;
            }
        }
    }

    double EndTime = FPlatformTime::Seconds();
    double TotalTime = (EndTime - StartTime) * 1000.0; // 转换为毫秒

    {
        FScopeLock Lock(&PreallocatorLock);
        Stats.PreallocatedCount = SuccessCount;
        Stats.PreallocationOperations = 1;
        Stats.TotalPreallocationTimeMs = TotalTime;
        Stats.AveragePreallocationTimeMs = TotalTime;
        Stats.MemoryUsageBytes = OwnerPool->CalculateMemoryUsage();
    }

    OBJECTPOOL_LOG(Log, TEXT("ExecuteImmediatePreallocation: 完成，成功: %d/%d，耗时: %.2fms"), 
        SuccessCount, Count, TotalTime);

    // 立即预分配也走统一收尾，确保结束时间与世界引用状态一致。
    StopPreallocation();
}

void FObjectPoolPreallocator::ExecuteProgressivePreallocation(UWorld* World, float DeltaTime)
{
    int32 CurrentCount = XTOOLS_ATOMIC_LOAD(CurrentProgress);
    if (CurrentCount >= Config.PreallocationCount)
    {
        return; // 已完成
    }

    //  每帧分配指定数量
    int32 AllocationsThisFrame = FMath::Min(Config.MaxAllocationsPerFrame, 
        Config.PreallocationCount - CurrentCount);

    for (int32 i = 0; i < AllocationsThisFrame; ++i)
    {
        if (CreateSingleActor(World))
        {
            XTOOLS_ATOMIC_INCREMENT(CurrentProgress);
        }
        else
        {
            OBJECTPOOL_LOG(Warning, TEXT("ExecuteProgressivePreallocation: 创建Actor失败"));
            break;
        }
    }

    OBJECTPOOL_LOG(VeryVerbose, TEXT("ExecuteProgressivePreallocation: 本帧分配 %d 个，总进度: %d/%d"),
        AllocationsThisFrame, XTOOLS_ATOMIC_LOAD(CurrentProgress), Config.PreallocationCount);
}

void FObjectPoolPreallocator::ExecutePredictivePreallocation(UWorld* World)
{
    //  基于使用模式预测需要的数量
    int32 PredictedCount = PredictRequiredCount();
    int32 CurrentCount = XTOOLS_ATOMIC_LOAD(CurrentProgress);

    if (PredictedCount > CurrentCount)
    {
        const int32 NeedToCreate = CalculatePredictiveAllocationCount(
            PredictedCount,
            CurrentCount,
            Config.PreallocationCount,
            Config.MaxAllocationsPerFrame);

        for (int32 i = 0; i < NeedToCreate; ++i)
        {
            if (CreateSingleActor(World))
            {
                XTOOLS_ATOMIC_INCREMENT(CurrentProgress);
            }
            else
            {
                break;
            }
        }

        OBJECTPOOL_LOG(Verbose, TEXT("ExecutePredictivePreallocation: 预测需要 %d 个，创建 %d 个"),
            PredictedCount, NeedToCreate);
    }
}

void FObjectPoolPreallocator::ExecuteAdaptivePreallocation(UWorld* World, float DeltaTime)
{
    //  自适应策略：结合当前使用情况和性能指标
    FObjectPoolStats PoolStats = OwnerPool->GetStats();

    // 计算使用率
    float UsageRate = 0.0f;
    int32 TotalActors = PoolStats.CurrentActive + PoolStats.CurrentAvailable;
    if (TotalActors > 0)
    {
        UsageRate = (float)PoolStats.CurrentActive / TotalActors;
    }

    // 根据使用率调整预分配速度
    int32 AllocationsThisFrame = Config.MaxAllocationsPerFrame;
    if (UsageRate > 0.8f)
    {
        // 高使用率，加快预分配
        AllocationsThisFrame = FMath::Min(AllocationsThisFrame * 2, 10);
    }
    else if (UsageRate < 0.3f)
    {
        // 低使用率，减慢预分配
        AllocationsThisFrame = FMath::Max(AllocationsThisFrame / 2, 1);
    }

    int32 CurrentCount = XTOOLS_ATOMIC_LOAD(CurrentProgress);
    if (CurrentCount < Config.PreallocationCount)
    {
        int32 NeedToCreate = FMath::Min(AllocationsThisFrame, Config.PreallocationCount - CurrentCount);

        for (int32 i = 0; i < NeedToCreate; ++i)
        {
            if (CreateSingleActor(World))
            {
                XTOOLS_ATOMIC_INCREMENT(CurrentProgress);
            }
            else
            {
                break;
            }
        }

        OBJECTPOOL_LOG(VeryVerbose, TEXT("ExecuteAdaptivePreallocation: 使用率 %.1f%%，创建 %d 个"),
            UsageRate * 100.0f, NeedToCreate);
    }
}

bool FObjectPoolPreallocator::CreateSingleActor(UWorld* World)
{
    if (!World || !OwnerPool)
    {
        RecordFailedPreallocation();
        return false;
    }

    if (!OwnerPool->CanCreateMoreActors())
    {
        RecordFailedPreallocation();
        return false;
    }

    double StartTime = FPlatformTime::Seconds();

    //  通过对象池创建Actor
    AActor* NewActor = OwnerPool->CreateNewActor(World);

    if (NewActor)
    {
        bool bRegistered = false;

        // 与普通 PrewarmPool 保持一致：在锁外停用延迟构造 Actor，避免原生组件提前注册后参与场景。
        NewActor->SetActorHiddenInGame(true);
        NewActor->SetActorTickEnabled(false);
        if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(NewActor->GetRootComponent()))
        {
            // 禁用前先保存原始碰撞/物理设置（与 PrewarmPool 同理，防止原始值在首次归还时被永久固化）
            FObjectPoolUtils::SaveOriginalCollisionSettings(RootPrimitive);
            RootPrimitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            RootPrimitive->SetSimulatePhysics(false);
        }
        // Spawn 在锁外执行，登记前再次检查容量，避免并发获取路径填满池。
        {
            FWriteScopeLock WriteLock(OwnerPool->PoolLock);
            if (OwnerPool->GetManagedActorCount_RequiresLock() < OwnerPool->MaxPoolSize)
            {
                OwnerPool->AvailableActors.Add(NewActor);
                OwnerPool->AllActorsSet.Add(NewActor);
                bRegistered = true;
            }
        }

        if (!bRegistered)
        {
            NewActor->Destroy();
            RecordFailedPreallocation();
            return false;
        }

        double EndTime = FPlatformTime::Seconds();
        double CreationTime = (EndTime - StartTime) * 1000.0; // 毫秒

        //  更新性能指标
        PerformanceMetrics.TotalCreationTimeMs += CreationTime;
        PerformanceMetrics.CreationCount++;
        PerformanceMetrics.AverageCreationTimeMs =
            PerformanceMetrics.TotalCreationTimeMs / PerformanceMetrics.CreationCount;
    }

    if (!NewActor)
    {
        RecordFailedPreallocation();
        return false;
    }

    return true;
}

void FObjectPoolPreallocator::RecordFailedPreallocation()
{
    FScopeLock Lock(&PreallocatorLock);
    ++Stats.FailedPreallocations;
}

void FObjectPoolPreallocator::UpdateStats()
{
    FScopeLock Lock(&PreallocatorLock);

    int32 CurrentCount = XTOOLS_ATOMIC_LOAD(CurrentProgress);
    int64 MemoryUsage = OwnerPool ? OwnerPool->CalculateMemoryUsage() : 0;

    Stats.PreallocatedCount = CurrentCount;
    Stats.MemoryUsageBytes = MemoryUsage;
    ++Stats.PreallocationOperations;
}

bool FObjectPoolPreallocator::ShouldContinuePreallocation() const
{
    int32 CurrentCount = XTOOLS_ATOMIC_LOAD(CurrentProgress);

    //  检查是否达到目标数量
    if (CurrentCount >= Config.PreallocationCount)
    {
        return false;
    }

    // 池已达到硬上限时不可能再取得进展，继续保留 Ticker 只会永久重试。
    if (OwnerPool && !OwnerPool->CanCreateMoreActors())
    {
        return false;
    }

    //  检查内存预算
    if (Config.bEnableMemoryBudget && OwnerPool)
    {
        int64 CurrentMemory = OwnerPool->CalculateMemoryUsage();
        if (!CheckMemoryBudget(CurrentMemory))
        {
            return false;
        }
    }

    return true;
}

bool FObjectPoolPreallocator::CheckMemoryBudget(int64 EstimatedMemoryUsage) const
{
    if (!Config.bEnableMemoryBudget)
    {
        return true;
    }

    int64 BudgetBytes = (int64)Config.MaxMemoryBudgetMB * 1024 * 1024;
    return EstimatedMemoryUsage <= BudgetBytes;
}

int32 FObjectPoolPreallocator::EstimateActorMemorySize(UClass* ActorClass) const
{
    if (!ActorClass)
    {
        return 1024; // 默认估算值
    }

    //  简单的内存估算（可以根据需要优化）
    int32 BaseSize = sizeof(AActor);
    int32 ClassSize = ActorClass->GetStructureSize();

    // 估算组件和其他开销
    int32 EstimatedOverhead = 512;

    return BaseSize + ClassSize + EstimatedOverhead;
}

int32 FObjectPoolPreallocator::PredictRequiredCount() const
{
    FScopeLock Lock(&PreallocatorLock);

    if (UsageHistory.Num() < 3)
    {
        // 历史数据不足，返回配置的预分配数量
        return Config.PreallocationCount;
    }

    //  简单的预测算法：基于最近的使用模式
    int32 RecentSum = 0;
    int32 RecentCount = FMath::Min(10, UsageHistory.Num()); // 使用最近10次的数据

    for (int32 i = UsageHistory.Num() - RecentCount; i < UsageHistory.Num(); ++i)
    {
        RecentSum += UsageHistory[i];
    }

    float AverageUsage = (float)RecentSum / RecentCount;

    //  预测下次需要的数量（增加20%的缓冲）
    int32 PredictedCount = FMath::CeilToInt(AverageUsage * 1.2f);

    //  限制在合理范围内
    PredictedCount = FMath::Clamp(PredictedCount, 1, Config.PreallocationCount * 2);

    OBJECTPOOL_LOG(VeryVerbose, TEXT("PredictRequiredCount: 平均使用 %.1f，预测需要 %d"),
        AverageUsage, PredictedCount);

    return PredictedCount;
}

FObjectPoolPreallocator::FAdjustmentRecommendation FObjectPoolPreallocator::CheckAdjustmentNeeded(const FObjectPoolStats& CurrentUsage) const
{
    FAdjustmentRecommendation Recommendation;

    const int32 TotalActors = CurrentUsage.CurrentActive + CurrentUsage.CurrentAvailable;
    if (TotalActors <= 0)
    {
        return Recommendation;
    }

    const float UsageRatio = static_cast<float>(CurrentUsage.CurrentActive) / TotalActors;

    if (UsageRatio >= 0.85f)
    {
        Recommendation.bShouldAdjust = true;
        Recommendation.bShouldExpand = true;
        Recommendation.RecommendedSize = FMath::Max(CurrentUsage.PoolSize + Config.MaxAllocationsPerFrame, CurrentUsage.PoolSize + 1);
        Recommendation.Reason = TEXT("池使用率持续偏高，建议扩容预分配容量");
    }
    else if (UsageRatio <= 0.2f && UsageHistory.Num() >= 5)
    {
        Recommendation.bShouldAdjust = true;
        Recommendation.bShouldExpand = false;
        Recommendation.RecommendedSize = FMath::Max(1, CurrentUsage.CurrentActive + 1);
        Recommendation.Reason = TEXT("池长期低使用率，建议收缩预分配目标");
    }

    return Recommendation;
}

void FObjectPoolPreallocator::RecordUsagePattern(int32 UsedCount)
{
    FScopeLock Lock(&PreallocatorLock);
    UsageHistory.Add(FMath::Max(0, UsedCount));

    if (UsageHistory.Num() > MaxHistorySize)
    {
        UsageHistory.RemoveAt(0, UsageHistory.Num() - MaxHistorySize);
    }
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectPoolPredictiveAllocationCountTest,
    "XTools.ObjectPool.Preallocator.PredictiveTargetBound",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectPoolPredictiveAllocationCountTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("预测需求不得突破剩余目标"), CalculatePredictiveAllocationCount(20, 7, 10, 8), 3);
    TestEqual(TEXT("达到目标后不得继续分配"), CalculatePredictiveAllocationCount(20, 10, 10, 8), 0);
    TestEqual(TEXT("目标充足时仍应遵守单帧上限"), CalculatePredictiveAllocationCount(20, 2, 15, 4), 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectPoolPreallocationConfigValidationTest,
    "XTools.ObjectPool.Preallocator.ConfigurationValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectPoolPreallocationConfigValidationTest::RunTest(const FString& Parameters)
{
    FObjectPoolConfig Config;
    Config.PreallocationCount = 4;
    Config.PreallocationStrategy = EObjectPoolPreallocationStrategy::Progressive;
    Config.MaxAllocationsPerFrame = 0;
    TestFalse(TEXT("分帧策略应拒绝零单帧分配上限"), IsValidPreallocationConfig(Config));

    Config.PreallocationStrategy = EObjectPoolPreallocationStrategy::Immediate;
    TestTrue(TEXT("立即策略不依赖单帧分配上限"), IsValidPreallocationConfig(Config));

    Config.PreallocationCount = 0;
    TestFalse(TEXT("所有策略都应拒绝零目标数量"), IsValidPreallocationConfig(Config));

    Config.PreallocationCount = 4;
    Config.PreallocationDelay = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("所有策略都应拒绝非有限延迟"), IsValidPreallocationConfig(Config));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectPoolPreallocationCapacityStopTest,
    "XTools.ObjectPool.Preallocator.CapacityStop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectPoolPreallocationCapacityStopTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ObjectPoolTest_PreallocationCapacity"));
    if (!TestNotNull(TEXT("应能创建预分配测试世界"), World))
    {
        return false;
    }
    if (!TestNotNull(TEXT("测试运行时应存在引擎实例"), GEngine))
    {
        World->DestroyWorld(false);
        return false;
    }
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);

    {
        FActorPool Pool(AActor::StaticClass(), 1, 1);
        Pool.PrewarmPool(World, 1);
        TestEqual(TEXT("预热后应达到池硬上限"), Pool.GetPoolSize(), 1);

        FObjectPoolConfig Config;
        Config.PreallocationCount = 2;
        Config.MaxAllocationsPerFrame = 1;
        Config.PreallocationStrategy = EObjectPoolPreallocationStrategy::Immediate;

        FObjectPoolPreallocator ImmediatePreallocator(&Pool);
        AddExpectedError(TEXT("ExecuteImmediatePreallocation: 创建Actor失败"), EAutomationExpectedErrorFlags::Contains, 1);
        TestTrue(TEXT("立即预分配请求应被接受"), ImmediatePreallocator.StartPreallocation(World, Config));
        TestFalse(TEXT("立即策略返回前应完成统一收尾"), ImmediatePreallocator.IsPreallocating());
        const FObjectPoolPreallocationStats ImmediateStats = ImmediatePreallocator.GetStats();
        TestEqual(TEXT("立即策略应记录一次容量失败"), ImmediateStats.FailedPreallocations, 1);
        TestTrue(TEXT("立即策略应记录结束时间"), ImmediateStats.PreallocationEndTime.GetTicks() > 0);

        Config.PreallocationStrategy = EObjectPoolPreallocationStrategy::Progressive;
        FObjectPoolPreallocator ProgressivePreallocator(&Pool);
        AddExpectedError(TEXT("ExecuteProgressivePreallocation: 创建Actor失败"), EAutomationExpectedErrorFlags::Contains, 1);
        TestTrue(TEXT("渐进预分配请求应被接受"), ProgressivePreallocator.StartPreallocation(World, Config));
        ProgressivePreallocator.Tick(0.0f);
        TestFalse(TEXT("池满后渐进策略应停止重试"), ProgressivePreallocator.IsPreallocating());
        const FObjectPoolPreallocationStats ProgressiveStats = ProgressivePreallocator.GetStats();
        TestEqual(TEXT("渐进策略应记录一次容量失败"), ProgressiveStats.FailedPreallocations, 1);
        TestTrue(TEXT("渐进策略应记录结束时间"), ProgressiveStats.PreallocationEndTime.GetTicks() > 0);
    }

    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}
#endif
