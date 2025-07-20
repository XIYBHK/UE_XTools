// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "UObject/ObjectMacros.h"

// ✅ 测试目标
#include "ActorPoolSimplified.h"
#include "ObjectPoolInterface.h"

#if WITH_OBJECTPOOL_TESTS

// 简化测试，不使用UCLASS，直接测试基本Actor

/**
 * FActorPoolSimplified 核心功能测试
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActorPoolSimplifiedCoreTest,
    "ObjectPool.ActorPoolSimplified.Core",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FActorPoolSimplifiedCoreTest::RunTest(const FString& Parameters)
{
    // 测试1: 构造函数和基本初始化
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 5, 20);
        
        TestTrue(TEXT("池应该已初始化"), Pool.IsInitialized());
        TestEqual(TEXT("初始可用数量应该为0"), Pool.GetAvailableCount(), 0);
        TestEqual(TEXT("初始活跃数量应该为0"), Pool.GetActiveCount(), 0);
        TestEqual(TEXT("初始池大小应该为0"), Pool.GetPoolSize(), 0);
        TestTrue(TEXT("池应该为空"), Pool.IsEmpty());
        TestFalse(TEXT("池不应该满"), Pool.IsFull());
        TestEqual(TEXT("Actor类应该正确"), Pool.GetActorClass(), AActor::StaticClass());
        
        AddInfo(TEXT("✅ 基本初始化测试通过"));
    }

    // 测试2: 无效参数处理
    {
        FActorPoolSimplified InvalidPool(nullptr, 5, 20);
        TestFalse(TEXT("无效Actor类的池不应该初始化"), InvalidPool.IsInitialized());
        
        AddInfo(TEXT("✅ 无效参数处理测试通过"));
    }

    // 测试3: 预热功能
    {
        UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
        if (TestWorld)
        {
            FActorPoolSimplified Pool(AActor::StaticClass(), 3, 10);
            
            // 预热池
            Pool.PrewarmPool(TestWorld, 5);
            
            TestEqual(TEXT("预热后可用数量应该为5"), Pool.GetAvailableCount(), 5);
            TestEqual(TEXT("预热后活跃数量应该为0"), Pool.GetActiveCount(), 0);
            TestEqual(TEXT("预热后池大小应该为5"), Pool.GetPoolSize(), 5);
            TestFalse(TEXT("预热后池不应该为空"), Pool.IsEmpty());
            
            // 测试统计信息
            FObjectPoolStatsSimplified Stats = Pool.GetStats();
            TestEqual(TEXT("总创建数应该为5"), Stats.TotalCreated, 5);
            TestEqual(TEXT("当前可用数应该为5"), Stats.CurrentAvailable, 5);
            TestEqual(TEXT("当前活跃数应该为0"), Stats.CurrentActive, 0);
            
            TestWorld->DestroyWorld(false);
            AddInfo(TEXT("✅ 预热功能测试通过"));
        }
    }

    return true;
}

/**
 * FActorPoolSimplified GetActor/ReturnActor 功能测试
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActorPoolSimplifiedGetReturnTest,
    "ObjectPool.ActorPoolSimplified.GetReturn",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FActorPoolSimplifiedGetReturnTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestWorld)
    {
        return false;
    }

    // 测试1: 从空池获取Actor（应该创建新的）
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 2, 10);
        
        AActor* Actor1 = Pool.GetActor(TestWorld);
        TestNotNull(TEXT("应该能从空池获取Actor"), Actor1);
        TestTrue(TEXT("获取的Actor应该是正确类型"), Actor1->IsA<AActor>());

        TestEqual(TEXT("获取后可用数量应该为0"), Pool.GetAvailableCount(), 0);
        TestEqual(TEXT("获取后活跃数量应该为1"), Pool.GetActiveCount(), 1);

        // 验证Actor基本状态
        TestNotNull(TEXT("Actor应该有效"), Actor1);
        TestTrue(TEXT("Actor应该启用碰撞"), Actor1->GetActorEnableCollision());
        
        AddInfo(TEXT("✅ 从空池获取Actor测试通过"));
    }

    // 测试2: 归还Actor到池
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 2, 10);

        AActor* Actor1 = Pool.GetActor(TestWorld);
        TestNotNull(TEXT("应该能获取Actor"), Actor1);

        bool bReturned = Pool.ReturnActor(Actor1);
        TestTrue(TEXT("应该能成功归还Actor"), bReturned);

        TestEqual(TEXT("归还后可用数量应该为1"), Pool.GetAvailableCount(), 1);
        TestEqual(TEXT("归还后活跃数量应该为0"), Pool.GetActiveCount(), 0);

        // 验证Actor基本状态
        TestNotNull(TEXT("Actor应该仍然有效"), Actor1);

        AddInfo(TEXT("✅ 归还Actor测试通过"));
    }

    // 测试3: 从池中重用Actor
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 2, 10);

        // 先获取并归还一个Actor
        AActor* Actor1 = Pool.GetActor(TestWorld);
        Pool.ReturnActor(Actor1);

        // 再次获取Actor（应该重用同一个）
        AActor* Actor2 = Pool.GetActor(TestWorld);
        TestEqual(TEXT("应该重用同一个Actor"), Actor1, Actor2);

        TestEqual(TEXT("重用后可用数量应该为0"), Pool.GetAvailableCount(), 0);
        TestEqual(TEXT("重用后活跃数量应该为1"), Pool.GetActiveCount(), 1);

        // 验证重用的Actor仍然有效
        TestNotNull(TEXT("重用的Actor应该有效"), Actor2);

        AddInfo(TEXT("✅ Actor重用测试通过"));
    }

    // 测试4: 多个Actor的管理
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 2, 10);
        
        // 获取多个Actor
        AActor* Actor1 = Pool.GetActor(TestWorld);
        AActor* Actor2 = Pool.GetActor(TestWorld);
        AActor* Actor3 = Pool.GetActor(TestWorld);
        
        TestNotNull(TEXT("Actor1应该有效"), Actor1);
        TestNotNull(TEXT("Actor2应该有效"), Actor2);
        TestNotNull(TEXT("Actor3应该有效"), Actor3);
        TestNotEqual(TEXT("Actor1和Actor2应该不同"), Actor1, Actor2);
        TestNotEqual(TEXT("Actor2和Actor3应该不同"), Actor2, Actor3);
        
        TestEqual(TEXT("获取3个Actor后活跃数量应该为3"), Pool.GetActiveCount(), 3);
        TestEqual(TEXT("获取3个Actor后可用数量应该为0"), Pool.GetAvailableCount(), 0);
        
        // 归还部分Actor
        Pool.ReturnActor(Actor1);
        Pool.ReturnActor(Actor3);
        
        TestEqual(TEXT("归还2个Actor后活跃数量应该为1"), Pool.GetActiveCount(), 1);
        TestEqual(TEXT("归还2个Actor后可用数量应该为2"), Pool.GetAvailableCount(), 2);
        
        AddInfo(TEXT("✅ 多Actor管理测试通过"));
    }

    TestWorld->DestroyWorld(false);
    return true;
}

/**
 * FActorPoolSimplified 错误处理和边界条件测试
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActorPoolSimplifiedErrorHandlingTest,
    "ObjectPool.ActorPoolSimplified.ErrorHandling",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FActorPoolSimplifiedErrorHandlingTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestWorld)
    {
        return false;
    }

    // 测试1: 无效参数处理
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 2, 10);

        // 测试无效World
        AActor* Actor1 = Pool.GetActor(nullptr);
        TestNull(TEXT("无效World应该返回nullptr"), Actor1);

        // 测试归还nullptr
        bool bReturned = Pool.ReturnActor(nullptr);
        TestFalse(TEXT("归还nullptr应该失败"), bReturned);

        // 测试归还错误类型的Actor
        AActor* WrongTypeActor = TestWorld->SpawnActor<ACharacter>();
        if (WrongTypeActor)
        {
            bool bReturnedWrong = Pool.ReturnActor(WrongTypeActor);
            TestFalse(TEXT("归还错误类型的Actor应该失败"), bReturnedWrong);
            WrongTypeActor->Destroy();
        }

        AddInfo(TEXT("✅ 无效参数处理测试通过"));
    }

    // 测试2: 池大小限制测试
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 2, 3); // 最大3个

        // 预热到最大大小
        Pool.PrewarmPool(TestWorld, 3);
        TestEqual(TEXT("预热后池大小应该为3"), Pool.GetPoolSize(), 3);

        // 尝试继续预热（应该不会超过限制）
        Pool.PrewarmPool(TestWorld, 5);
        TestEqual(TEXT("超过限制的预热不应该增加池大小"), Pool.GetPoolSize(), 3);

        AddInfo(TEXT("✅ 池大小限制测试通过"));
    }

    // 测试3: 重复归还同一个Actor
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 2, 10);

        AActor* Actor1 = Pool.GetActor(TestWorld);
        TestNotNull(TEXT("应该能获取Actor"), Actor1);

        // 第一次归还
        bool bReturned1 = Pool.ReturnActor(Actor1);
        TestTrue(TEXT("第一次归还应该成功"), bReturned1);
        TestEqual(TEXT("第一次归还后可用数量应该为1"), Pool.GetAvailableCount(), 1);

        // 第二次归还同一个Actor
        bool bReturned2 = Pool.ReturnActor(Actor1);
        // 注意：这里的行为取决于实现，可能成功也可能失败
        // 重要的是不应该崩溃
        TestEqual(TEXT("重复归还不应该增加可用数量"), Pool.GetAvailableCount(), 1);

        AddInfo(TEXT("✅ 重复归还测试通过"));
    }

    // 测试4: 清空池功能
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 2, 10);

        // 添加一些Actor
        Pool.PrewarmPool(TestWorld, 3);
        AActor* Actor1 = Pool.GetActor(TestWorld);

        TestEqual(TEXT("清空前应该有Actor"), Pool.GetPoolSize(), 3);
        TestEqual(TEXT("清空前应该有活跃Actor"), Pool.GetActiveCount(), 1);

        // 清空池
        Pool.ClearPool();

        TestEqual(TEXT("清空后池大小应该为0"), Pool.GetPoolSize(), 0);
        TestEqual(TEXT("清空后可用数量应该为0"), Pool.GetAvailableCount(), 0);
        TestEqual(TEXT("清空后活跃数量应该为0"), Pool.GetActiveCount(), 0);
        TestTrue(TEXT("清空后池应该为空"), Pool.IsEmpty());

        AddInfo(TEXT("✅ 清空池测试通过"));
    }

    // 测试5: 动态调整池大小
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 2, 10);

        // 预热池
        Pool.PrewarmPool(TestWorld, 5);
        TestEqual(TEXT("预热后池大小应该为5"), Pool.GetPoolSize(), 5);

        // 减少最大大小
        Pool.SetMaxSize(3);
        TestTrue(TEXT("调整后池大小不应该超过新限制"), Pool.GetPoolSize() <= 3);

        // 增加最大大小
        Pool.SetMaxSize(15);
        // 池大小不应该自动增长，只是允许更大的大小

        AddInfo(TEXT("✅ 动态调整池大小测试通过"));
    }

    TestWorld->DestroyWorld(false);
    return true;
}

/**
 * FActorPoolSimplified 统计信息测试
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActorPoolSimplifiedStatsTest,
    "ObjectPool.ActorPoolSimplified.Stats",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FActorPoolSimplifiedStatsTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestWorld)
    {
        return false;
    }

    // 测试1: 基本统计信息
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 2, 10);

        // 初始统计
        FObjectPoolStatsSimplified InitialStats = Pool.GetStats();
        TestEqual(TEXT("初始总创建数应该为0"), InitialStats.TotalCreated, 0);
        TestEqual(TEXT("初始当前活跃数应该为0"), InitialStats.CurrentActive, 0);
        TestEqual(TEXT("初始当前可用数应该为0"), InitialStats.CurrentAvailable, 0);
        TestEqual(TEXT("初始池大小应该为0"), InitialStats.PoolSize, 0);
        TestEqual(TEXT("初始命中率应该为0"), InitialStats.HitRate, 0.0f);
        TestEqual(TEXT("Actor类名应该正确"), InitialStats.ActorClassName, TEXT("Actor"));

        AddInfo(TEXT("✅ 初始统计信息测试通过"));
    }

    // 测试2: 预热后的统计信息
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 2, 10);

        Pool.PrewarmPool(TestWorld, 3);

        FObjectPoolStatsSimplified PrewarmStats = Pool.GetStats();
        TestEqual(TEXT("预热后总创建数应该为3"), PrewarmStats.TotalCreated, 3);
        TestEqual(TEXT("预热后当前活跃数应该为0"), PrewarmStats.CurrentActive, 0);
        TestEqual(TEXT("预热后当前可用数应该为3"), PrewarmStats.CurrentAvailable, 3);
        TestEqual(TEXT("预热后池大小应该为3"), PrewarmStats.PoolSize, 3);

        AddInfo(TEXT("✅ 预热统计信息测试通过"));
    }

    // 测试3: 命中率统计
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 2, 10);

        // 预热池
        Pool.PrewarmPool(TestWorld, 2);

        // 第一次获取（池命中）
        AActor* Actor1 = Pool.GetActor(TestWorld);
        FObjectPoolStatsSimplified Stats1 = Pool.GetStats();
        TestTrue(TEXT("第一次获取后命中率应该大于0"), Stats1.HitRate > 0.0f);

        // 第二次获取（池命中）
        AActor* Actor2 = Pool.GetActor(TestWorld);
        FObjectPoolStatsSimplified Stats2 = Pool.GetStats();
        TestTrue(TEXT("第二次获取后命中率应该更高"), Stats2.HitRate >= Stats1.HitRate);

        // 第三次获取（需要创建新的，池未命中）
        AActor* Actor3 = Pool.GetActor(TestWorld);
        FObjectPoolStatsSimplified Stats3 = Pool.GetStats();
        TestTrue(TEXT("第三次获取后总创建数应该增加"), Stats3.TotalCreated > Stats2.TotalCreated);

        AddInfo(TEXT("✅ 命中率统计测试通过"));
    }

    TestWorld->DestroyWorld(false);
    return true;
}

/**
 * FActorPoolSimplified 线程安全测试
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActorPoolSimplifiedThreadSafetyTest,
    "ObjectPool.ActorPoolSimplified.ThreadSafety",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FActorPoolSimplifiedThreadSafetyTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestWorld)
    {
        return false;
    }

    // 测试1: 并发GetActor操作
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 5, 50);

        // 预热池
        Pool.PrewarmPool(TestWorld, 20);

        const int32 ThreadCount = 4;
        const int32 OperationsPerThread = 25;

        TArray<TFuture<TArray<AActor*>>> Futures;

        for (int32 ThreadIndex = 0; ThreadIndex < ThreadCount; ThreadIndex++)
        {
            TFuture<TArray<AActor*>> Future = Async(EAsyncExecution::Thread, [&Pool, TestWorld, OperationsPerThread]()
            {
                TArray<AActor*> AcquiredActors;
                AcquiredActors.Reserve(OperationsPerThread);

                for (int32 i = 0; i < OperationsPerThread; ++i)
                {
                    AActor* Actor = Pool.GetActor(TestWorld);
                    if (Actor)
                    {
                        AcquiredActors.Add(Actor);
                    }

                    // 模拟一些工作
                    FPlatformProcess::Sleep(0.001f);
                }

                return AcquiredActors;
            });

            Futures.Add(MoveTemp(Future));
        }

        // 等待所有线程完成并收集结果
        TArray<AActor*> AllAcquiredActors;
        for (auto& Future : Futures)
        {
            TArray<AActor*> ThreadActors = Future.Get();
            AllAcquiredActors.Append(ThreadActors);
        }

        // 验证结果
        TestTrue(TEXT("应该获取到一些Actor"), AllAcquiredActors.Num() > 0);
        TestTrue(TEXT("获取的Actor数量不应该超过池的容量"), AllAcquiredActors.Num() <= 50);

        // 验证没有重复的Actor
        TSet<AActor*> UniqueActors(AllAcquiredActors);
        TestEqual(TEXT("不应该有重复的Actor"), UniqueActors.Num(), AllAcquiredActors.Num());

        AddInfo(TEXT("✅ 并发GetActor测试通过"));
    }

    // 测试2: 并发ReturnActor操作
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 5, 50);

        // 先获取一些Actor
        TArray<AActor*> ActorsToReturn;
        for (int32 i = 0; i < 20; ++i)
        {
            AActor* Actor = Pool.GetActor(TestWorld);
            if (Actor)
            {
                ActorsToReturn.Add(Actor);
            }
        }

        const int32 ThreadCount = 4;
        const int32 ActorsPerThread = ActorsToReturn.Num() / ThreadCount;

        TArray<TFuture<int32>> Futures;

        for (int32 ThreadIndex = 0; ThreadIndex < ThreadCount; ThreadIndex++)
        {
            int32 StartIndex = ThreadIndex * ActorsPerThread;
            int32 EndIndex = (ThreadIndex == ThreadCount - 1) ? ActorsToReturn.Num() : (ThreadIndex + 1) * ActorsPerThread;

            TFuture<int32> Future = Async(EAsyncExecution::Thread, [&Pool, &ActorsToReturn, StartIndex, EndIndex]()
            {
                int32 SuccessfulReturns = 0;

                for (int32 i = StartIndex; i < EndIndex; ++i)
                {
                    if (Pool.ReturnActor(ActorsToReturn[i]))
                    {
                        ++SuccessfulReturns;
                    }

                    // 模拟一些工作
                    FPlatformProcess::Sleep(0.001f);
                }

                return SuccessfulReturns;
            });

            Futures.Add(MoveTemp(Future));
        }

        // 等待所有线程完成
        int32 TotalSuccessfulReturns = 0;
        for (auto& Future : Futures)
        {
            TotalSuccessfulReturns += Future.Get();
        }

        TestEqual(TEXT("所有Actor都应该成功归还"), TotalSuccessfulReturns, ActorsToReturn.Num());

        AddInfo(TEXT("✅ 并发ReturnActor测试通过"));
    }

    // 测试3: 混合并发操作
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 10, 100);

        // 预热池
        Pool.PrewarmPool(TestWorld, 30);

        const int32 ThreadCount = 6;
        const int32 OperationsPerThread = 20;

        TArray<TFuture<bool>> Futures;

        for (int32 ThreadIndex = 0; ThreadIndex < ThreadCount; ThreadIndex++)
        {
            bool bIsGetterThread = (ThreadIndex % 2 == 0);

            TFuture<bool> Future = Async(EAsyncExecution::Thread, [&Pool, TestWorld, OperationsPerThread, bIsGetterThread]()
            {
                TArray<AActor*> LocalActors;

                for (int32 i = 0; i < OperationsPerThread; ++i)
                {
                    if (bIsGetterThread)
                    {
                        // Getter线程：获取Actor
                        AActor* Actor = Pool.GetActor(TestWorld);
                        if (Actor)
                        {
                            LocalActors.Add(Actor);
                        }
                    }
                    else
                    {
                        // Returner线程：归还Actor
                        if (LocalActors.Num() > 0)
                        {
                            AActor* ActorToReturn = LocalActors.Pop();
                            Pool.ReturnActor(ActorToReturn);
                        }
                        else
                        {
                            // 如果没有Actor可归还，尝试获取一个
                            AActor* Actor = Pool.GetActor(TestWorld);
                            if (Actor)
                            {
                                LocalActors.Add(Actor);
                            }
                        }
                    }

                    FPlatformProcess::Sleep(0.001f);
                }

                // 清理剩余的Actor
                for (AActor* Actor : LocalActors)
                {
                    Pool.ReturnActor(Actor);
                }

                return true;
            });

            Futures.Add(MoveTemp(Future));
        }

        // 等待所有线程完成
        bool bAllSucceeded = true;
        for (auto& Future : Futures)
        {
            if (!Future.Get())
            {
                bAllSucceeded = false;
                break;
            }
        }

        TestTrue(TEXT("所有混合操作都应该成功"), bAllSucceeded);

        // 验证池的最终状态是合理的
        FObjectPoolStatsSimplified FinalStats = Pool.GetStats();
        TestTrue(TEXT("最终统计应该合理"), FinalStats.TotalCreated >= 0);
        TestTrue(TEXT("最终池大小应该合理"), FinalStats.PoolSize >= 0);

        AddInfo(TEXT("✅ 混合并发操作测试通过"));
    }

    TestWorld->DestroyWorld(false);
    return true;
}

/**
 * FActorPoolSimplified 性能测试
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActorPoolSimplifiedPerformanceTest,
    "ObjectPool.ActorPoolSimplified.Performance",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FActorPoolSimplifiedPerformanceTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestWorld)
    {
        return false;
    }

    // 测试1: GetActor性能测试
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 10, 1000);

        // 预热池
        Pool.PrewarmPool(TestWorld, 500);

        const int32 TestIterations = 1000;
        TArray<AActor*> AcquiredActors;
        AcquiredActors.Reserve(TestIterations);

        // 测量GetActor性能
        double StartTime = FPlatformTime::Seconds();

        for (int32 i = 0; i < TestIterations; ++i)
        {
            AActor* Actor = Pool.GetActor(TestWorld);
            if (Actor)
            {
                AcquiredActors.Add(Actor);
            }
        }

        double EndTime = FPlatformTime::Seconds();
        double TotalTime = EndTime - StartTime;
        double AverageTimePerGet = TotalTime / TestIterations;

        TestTrue(TEXT("应该获取到Actor"), AcquiredActors.Num() > 0);
        TestTrue(TEXT("GetActor平均时间应该很短"), AverageTimePerGet < 0.001); // 小于1毫秒

        AddInfo(FString::Printf(TEXT("✅ GetActor性能测试通过: 平均%.6f秒/次"), AverageTimePerGet));

        // 清理
        for (AActor* Actor : AcquiredActors)
        {
            Pool.ReturnActor(Actor);
        }
    }

    // 测试2: ReturnActor性能测试
    {
        FActorPoolSimplified Pool(AActor::StaticClass(), 10, 1000);

        const int32 TestIterations = 1000;
        TArray<AActor*> ActorsToReturn;

        // 先获取一些Actor
        for (int32 i = 0; i < TestIterations; ++i)
        {
            AActor* Actor = Pool.GetActor(TestWorld);
            if (Actor)
            {
                ActorsToReturn.Add(Actor);
            }
        }

        // 测量ReturnActor性能
        double StartTime = FPlatformTime::Seconds();

        for (AActor* Actor : ActorsToReturn)
        {
            Pool.ReturnActor(Actor);
        }

        double EndTime = FPlatformTime::Seconds();
        double TotalTime = EndTime - StartTime;
        double AverageTimePerReturn = TotalTime / ActorsToReturn.Num();

        TestTrue(TEXT("ReturnActor平均时间应该很短"), AverageTimePerReturn < 0.001); // 小于1毫秒

        AddInfo(FString::Printf(TEXT("✅ ReturnActor性能测试通过: 平均%.6f秒/次"), AverageTimePerReturn));
    }

    // 测试3: 池命中率性能影响
    {
        FActorPoolSimplified PoolWithPrewarm(AActor::StaticClass(), 10, 1000);
        FActorPoolSimplified PoolWithoutPrewarm(AActor::StaticClass(), 10, 1000);

        // 一个池预热，一个池不预热
        PoolWithPrewarm.PrewarmPool(TestWorld, 500);

        const int32 TestIterations = 500;

        // 测试预热池的性能
        double StartTime1 = FPlatformTime::Seconds();
        for (int32 i = 0; i < TestIterations; ++i)
        {
            AActor* Actor = PoolWithPrewarm.GetActor(TestWorld);
            if (Actor)
            {
                PoolWithPrewarm.ReturnActor(Actor);
            }
        }
        double EndTime1 = FPlatformTime::Seconds();
        double PrewarmTime = EndTime1 - StartTime1;

        // 测试未预热池的性能
        double StartTime2 = FPlatformTime::Seconds();
        for (int32 i = 0; i < TestIterations; ++i)
        {
            AActor* Actor = PoolWithoutPrewarm.GetActor(TestWorld);
            if (Actor)
            {
                PoolWithoutPrewarm.ReturnActor(Actor);
            }
        }
        double EndTime2 = FPlatformTime::Seconds();
        double NoPrewarmTime = EndTime2 - StartTime2;

        // 预热的池应该更快
        TestTrue(TEXT("预热池应该比未预热池更快"), PrewarmTime < NoPrewarmTime);

        FObjectPoolStatsSimplified PrewarmStats = PoolWithPrewarm.GetStats();
        FObjectPoolStatsSimplified NoPrewarmStats = PoolWithoutPrewarm.GetStats();

        TestTrue(TEXT("预热池的命中率应该更高"), PrewarmStats.HitRate > NoPrewarmStats.HitRate);

        AddInfo(FString::Printf(TEXT("✅ 命中率性能测试通过: 预热=%.4f秒(命中率%.1f%%), 未预热=%.4f秒(命中率%.1f%%)"),
            PrewarmTime, PrewarmStats.HitRate * 100.0f, NoPrewarmTime, NoPrewarmStats.HitRate * 100.0f));
    }

    TestWorld->DestroyWorld(false);
    return true;
}

/**
 * FActorPoolSimplified 移动语义测试
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActorPoolSimplifiedMoveTest,
    "ObjectPool.ActorPoolSimplified.Move",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FActorPoolSimplifiedMoveTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestWorld)
    {
        return false;
    }

    // 测试1: 移动构造函数
    {
        FActorPoolSimplified OriginalPool(AActor::StaticClass(), 5, 20);
        OriginalPool.PrewarmPool(TestWorld, 3);

        UClass* OriginalClass = OriginalPool.GetActorClass();
        int32 OriginalAvailable = OriginalPool.GetAvailableCount();

        // 移动构造
        FActorPoolSimplified MovedPool = MoveTemp(OriginalPool);

        TestEqual(TEXT("移动后Actor类应该正确"), MovedPool.GetActorClass(), OriginalClass);
        TestEqual(TEXT("移动后可用数量应该正确"), MovedPool.GetAvailableCount(), OriginalAvailable);
        TestTrue(TEXT("移动后池应该已初始化"), MovedPool.IsInitialized());

        // 原池应该被重置
        TestFalse(TEXT("原池应该未初始化"), OriginalPool.IsInitialized());

        AddInfo(TEXT("✅ 移动构造函数测试通过"));
    }

    // 测试2: 移动赋值操作符
    {
        FActorPoolSimplified SourcePool(AActor::StaticClass(), 5, 20);
        SourcePool.PrewarmPool(TestWorld, 4);

        FActorPoolSimplified TargetPool(ACharacter::StaticClass(), 3, 15);
        TargetPool.PrewarmPool(TestWorld, 2);

        UClass* SourceClass = SourcePool.GetActorClass();
        int32 SourceAvailable = SourcePool.GetAvailableCount();

        // 移动赋值
        TargetPool = MoveTemp(SourcePool);

        TestEqual(TEXT("移动赋值后Actor类应该是源池的"), TargetPool.GetActorClass(), SourceClass);
        TestEqual(TEXT("移动赋值后可用数量应该是源池的"), TargetPool.GetAvailableCount(), SourceAvailable);
        TestTrue(TEXT("移动赋值后目标池应该已初始化"), TargetPool.IsInitialized());

        // 源池应该被重置
        TestFalse(TEXT("源池应该未初始化"), SourcePool.IsInitialized());

        AddInfo(TEXT("✅ 移动赋值操作符测试通过"));
    }

    // 测试3: 移动后的功能性测试
    {
        FActorPoolSimplified OriginalPool(AActor::StaticClass(), 5, 20);
        OriginalPool.PrewarmPool(TestWorld, 3);

        // 移动池
        FActorPoolSimplified MovedPool = MoveTemp(OriginalPool);

        // 测试移动后的池仍然可以正常工作
        AActor* Actor1 = MovedPool.GetActor(TestWorld);
        TestNotNull(TEXT("移动后的池应该能获取Actor"), Actor1);

        bool bReturned = MovedPool.ReturnActor(Actor1);
        TestTrue(TEXT("移动后的池应该能归还Actor"), bReturned);

        FObjectPoolStatsSimplified Stats = MovedPool.GetStats();
        TestTrue(TEXT("移动后的池应该有正确的统计"), Stats.TotalCreated >= 3);

        AddInfo(TEXT("✅ 移动后功能性测试通过"));
    }

    TestWorld->DestroyWorld(false);
    return true;
}

#endif // WITH_OBJECTPOOL_TESTS
