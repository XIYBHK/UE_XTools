/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


//  遵循IWYU原则的头文件包含
#include "ObjectPoolPreallocator.h"

//  UE核心依赖
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformFilemanager.h"

//  对象池模块依赖
#include "ObjectPool.h"
#include "ActorPool.h"
#include "ObjectPoolSubsystem.h"

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

    //  保存配置
    {
        FScopeLock Lock(&PreallocatorLock);
        Config = InConfig;
        Stats = FObjectPoolPreallocationStats();
        Stats.TargetCount = Config.PreallocationCount;
        Stats.PreallocationStartTime = FDateTime::Now();
        XTOOLS_ATOMIC_STORE(CurrentProgress, 0);
        AccumulatedTime = 0.0f;
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

    //  根据策略执行预分配
    switch (Config.Strategy)
    {
    case EObjectPoolPreallocationStrategy::Immediate:
        ExecuteImmediatePreallocation(World, Config.PreallocationCount);
        break;
        
    case EObjectPoolPreallocationStrategy::Progressive:
    case EObjectPoolPreallocationStrategy::Predictive:
    case EObjectPoolPreallocationStrategy::Adaptive:
        // 这些策略在Tick中执行
        break;
        
    default:
        OBJECTPOOL_LOG(Warning, TEXT("StartPreallocation: 不支持的预分配策略"));
        XTOOLS_ATOMIC_STORE(bIsActive, false);
        return false;
    }

    OBJECTPOOL_LOG(Log, TEXT("StartPreallocation: 启动预分配，策略: %d, 目标数量: %d"), 
        (int32)Config.Strategy, Config.PreallocationCount);

    return true;
}

void FObjectPoolPreallocator::StopPreallocation()
{
    if (!XTOOLS_ATOMIC_LOAD(bIsActive))
    {
        return;
    }

    XTOOLS_ATOMIC_STORE(bIsActive, false);
    
    {
        FScopeLock Lock(&PreallocatorLock);
        Stats.PreallocationEndTime = FDateTime::Now();
        Stats.TotalPreallocationTimeMs = (Stats.PreallocationEndTime - Stats.PreallocationStartTime).GetTotalMilliseconds();
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

    //  检查延迟时间
    if (AccumulatedTime < Config.PreallocationDelay)
    {
        return;
    }

    //  使用子系统的智能World获取方法
    UWorld* World = nullptr;
    if (UObjectPoolSubsystem* Subsystem = UObjectPoolSubsystem::Get(nullptr))
    {
        World = Subsystem->GetWorld();
    }

    if (!World)
    {
        OBJECTPOOL_LOG(Warning, TEXT("Tick: 无法获取有效的World"));
        return;
    }

    //  根据策略执行预分配
    switch (Config.Strategy)
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
        Stats.TotalPreallocationTimeMs = TotalTime;
        Stats.UpdateStats(SuccessCount, Count, OwnerPool->CalculateMemoryUsage());
    }

    OBJECTPOOL_LOG(Log, TEXT("ExecuteImmediatePreallocation: 完成，成功: %d/%d，耗时: %.2fms"), 
        SuccessCount, Count, TotalTime);

    // 立即预分配完成后停止
    XTOOLS_ATOMIC_STORE(bIsActive, false);
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
        int32 NeedToCreate = FMath::Min(PredictedCount - CurrentCount, Config.MaxAllocationsPerFrame);

        for (int32 i = 0; i < NeedToCreate; ++i)
        {
            if (CreateSingleActor(World))
            {
                XTOOLS_ATOMIC_INCREMENT(CurrentProgress);
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
        }

        OBJECTPOOL_LOG(VeryVerbose, TEXT("ExecuteAdaptivePreallocation: 使用率 %.1f%%，创建 %d 个"),
            UsageRate * 100.0f, NeedToCreate);
    }
}

bool FObjectPoolPreallocator::CreateSingleActor(UWorld* World)
{
    if (!World || !OwnerPool)
    {
        return false;
    }

    double StartTime = FPlatformTime::Seconds();

    //  通过对象池创建Actor
    FTransform DefaultTransform = FTransform::Identity;
            bool bSuccess = OwnerPool->CreateNewActor(World) != nullptr;

    if (bSuccess)
    {
        double EndTime = FPlatformTime::Seconds();
        double CreationTime = (EndTime - StartTime) * 1000.0; // 毫秒

        //  更新性能指标
        PerformanceMetrics.TotalCreationTimeMs += CreationTime;
        PerformanceMetrics.CreationCount++;
        PerformanceMetrics.AverageCreationTimeMs =
            PerformanceMetrics.TotalCreationTimeMs / PerformanceMetrics.CreationCount;
    }

    return bSuccess;
}

void FObjectPoolPreallocator::UpdateStats()
{
    FScopeLock Lock(&PreallocatorLock);

    int32 CurrentCount = XTOOLS_ATOMIC_LOAD(CurrentProgress);
    int64 MemoryUsage = OwnerPool ? OwnerPool->CalculateMemoryUsage() : 0;

    Stats.UpdateStats(CurrentCount, Config.PreallocationCount, MemoryUsage);
}

bool FObjectPoolPreallocator::ShouldContinuePreallocation() const
{
    int32 CurrentCount = XTOOLS_ATOMIC_LOAD(CurrentProgress);

    //  检查是否达到目标数量
    if (CurrentCount >= Config.PreallocationCount)
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
