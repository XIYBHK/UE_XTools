/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "ObjectPoolInterface.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectPoolLifecycleTestTypes.h"
#include "ActorPool.h"
#include "Async/TaskGraphInterfaces.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"

namespace
{
    /**
     * 已初始化的最小 Game 世界：AActor::ProcessEvent 仅在 Actor 初始化完成后派发事件。
     * 生命周期事件不依赖对象池子系统，但测试世界必须遵守该引擎前置条件。
     */
    class FScopedLifecycleTestWorld
    {
    public:
        explicit FScopedLifecycleTestWorld(FName WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, WorldName);
            if (World)
            {
                FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
                WorldContext.SetCurrentWorld(World);

                FURL URL;
                World->InitializeActorsForPlay(URL);
            }
        }

        ~FScopedLifecycleTestWorld()
        {
            if (World)
            {
                GEngine->DestroyWorldContext(World);
                World->DestroyWorld(false);
                World = nullptr;
            }
        }

        UWorld* Get() const { return World; }

    private:
        UWorld* World = nullptr;
    };

    /**
     * 冲刷游戏线程任务队列，使 AsyncTask(ENamedThreads::GameThread) 的挂起回调确定性执行。
     * 自动化测试运行在游戏线程，ProcessThreadUntilIdle 由本线程把队列跑到空（引擎 Core 测试同款模式），
     * 不依赖 Tick 推进或任何时序窗口。
     */
    void FlushGameThreadAsyncTasks()
    {
        FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolActiveRemovalScaleTest,
    "XTools.ObjectPool.ActorPool.ActiveRemovalScale",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectPoolActiveRemovalScaleTest::RunTest(const FString& Parameters)
{
    FScopedLifecycleTestWorld TestWorld(TEXT("ObjectPoolActiveRemovalScale"));
    UWorld* World = TestWorld.Get();
    if (!TestNotNull(TEXT("Test world is valid"), World)) { return false; }
    for (int32 Count : {100, 1000, 10000})
    {
        FActorPool Pool(AActor::StaticClass(), 1, Count);
        TArray<AActor*> Actors;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            AActor* Actor = Pool.GetActor(World);
            if (!Actor) { AddError(TEXT("Failed to acquire test Actor")); return false; }
            Actors.Add(Actor);
        }
        FRandomStream Random(72341);
        for (int32 Index = Actors.Num() - 1; Index > 0; --Index) { Actors.Swap(Index, Random.RandRange(0, Index)); }
        const double Start = FPlatformTime::Seconds();
        int32 Returned = 0;
        for (AActor* Actor : Actors) { Returned += Pool.ReturnActor(Actor) ? 1 : 0; }
        AddInfo(FString::Printf(TEXT("ActiveRemovalScale: %d shuffled returns, %.3f ms"), Count, (FPlatformTime::Seconds() - Start) * 1000.0));
        TestEqual(TEXT("Every active actor is returned exactly once"), Returned, Count);
        TestEqual(TEXT("Active count after batch return"), Pool.GetActiveCount(), 0);
        TestEqual(TEXT("Available count after batch return"), Pool.GetAvailableCount(), Count);

        // Exercise stale weak keys and swapped indices, then reuse surviving actors.
        Actors.Reset();
        for (int32 Index = 0; Index < 64; ++Index) { Actors.Add(Pool.GetActor(World)); }
        for (int32 Index = 0; Index < Actors.Num(); Index += 7) { Actors[Index]->Destroy(); }
        Pool.CleanupInvalidActors();
        for (AActor* Actor : Actors)
        {
            if (IsValid(Actor)) { TestTrue(TEXT("Survivor remains returnable after cleanup swaps"), Pool.ReturnActor(Actor)); }
        }
        TestEqual(TEXT("Cleanup and reacquisition leave no active entries"), Pool.GetActiveCount(), 0);
        Pool.ClearPool();
        AActor* Deferred = Pool.AcquireDeferred(World);
        TestTrue(TEXT("Deferred finalization after clearing succeeds"), Pool.FinalizeDeferred(Deferred, FTransform::Identity));
        TestTrue(TEXT("Finalized actor can be returned"), Pool.ReturnActor(Deferred));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolActiveRemovalReentrancyTest,
    "XTools.ObjectPool.ActorPool.ActiveRemovalReentrancy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectPoolActiveRemovalReentrancyTest::RunTest(const FString& Parameters)
{
    FScopedLifecycleTestWorld TestWorld(TEXT("ObjectPoolActiveRemovalReentrancy"));
    UWorld* World = TestWorld.Get();
    if (!TestNotNull(TEXT("Test world is valid"), World)) { return false; }
    FActorPool Pool(AObjectPoolLifecycleTestActor::StaticClass(), 1, 512);
    TArray<AObjectPoolLifecycleTestActor*> Actors;
    for (int32 Index = 0; Index < 300; ++Index)
    {
        AObjectPoolLifecycleTestActor* Actor = Cast<AObjectPoolLifecycleTestActor>(Pool.GetActor(World));
        if (!Actor) { AddError(TEXT("Failed to acquire test Actor")); return false; }
        Actors.Add(Actor);
    }
    AddExpectedError(TEXT("Actor不在活跃列表中，拒绝归还"), EAutomationExpectedErrorFlags::Contains, 1);
    bool bNestedReturn = true;
    Actors[100]->OnReturnOnce = [&]() { bNestedReturn = Pool.ReturnActor(Actors[100]); };
    TestTrue(TEXT("Outer return succeeds"), Pool.ReturnActor(Actors[100]));
    TestFalse(TEXT("Returning actor cannot be returned twice during callback"), bNestedReturn);
    TestEqual(TEXT("Return callback fires once"), Actors[100]->ReturnedCount, 1);

    AddExpectedError(TEXT("ReturnActor: 回调后复核失败"), EAutomationExpectedErrorFlags::Contains, 2);
    Actors[200]->OnReturnOnce = [&]() { Actors[200]->Destroy(); };
    TestFalse(TEXT("Destroyed actor is not committed as available"), Pool.ReturnActor(Actors[200]));
    TestEqual(TEXT("Destroyed return no longer counts as active"), Pool.GetActiveCount(), 298);

    Actors[150]->OnReturnOnce = [&]() { Pool.ClearPool(); };
    TestFalse(TEXT("ClearPool during return prevents stale commit"), Pool.ReturnActor(Actors[150]));
    TestEqual(TEXT("Clear removes all active indices"), Pool.GetActiveCount(), 0);
    TestEqual(TEXT("Clear removes all available actors"), Pool.GetAvailableCount(), 0);
    for (int32 Index = 0; Index < 300; ++Index)
    {
        if (!Pool.GetActor(World)) { AddError(TEXT("Pool failed to refill after ClearPool")); return false; }
    }
    AActor* Deferred = Pool.AcquireDeferred(World);
    TestTrue(TEXT("Deferred path joins an already indexed active array"), Pool.FinalizeDeferred(Deferred, FTransform::Identity));
    TestTrue(TEXT("Deferred actor has a valid active index"), Pool.ReturnActor(Deferred));
    return true;
}

// M-12 回归：CallLifecycleEventEnhanced 对原生 C++ 接口实现者的同步/异步派发。
// UFunction::Invoke 会在调用接口 thunk 前通过 GetInterfaceAddress 调整多继承指针，
// 因此 Execute_* 必须精确触达 _Implementation；计数断言同时防止派发链静默退化。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolLifecycleSyncAsyncValidActorTest,
    "XTools.ObjectPool.Lifecycle.SyncAndAsyncCallSafeForValidActor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectPoolLifecycleSyncAsyncValidActorTest::RunTest(const FString& Parameters)
{
    FScopedLifecycleTestWorld TestWorld(TEXT("ObjectPoolTest_LifecycleValid"));
    UWorld* World = TestWorld.Get();
    if (!TestNotNull(TEXT("应能创建测试世界"), World))
    {
        return false;
    }

    AObjectPoolLifecycleTestActor* Actor = World->SpawnActor<AObjectPoolLifecycleTestActor>();
    if (!TestNotNull(TEXT("应能生成接口测试Actor"), Actor))
    {
        return false;
    }

    // 对照组：经 GetNativeInterfaceAddress 的原生直调通道可正常触达 _Implementation，
    // 证明测试 Actor 的接口覆盖与计数接线正确。
    void* NativeAddr = Actor->GetNativeInterfaceAddress(UObjectPoolInterface::StaticClass());
    if (TestNotNull(TEXT("应能解析原生接口地址"), NativeAddr))
    {
        static_cast<IObjectPoolInterface*>(NativeAddr)->OnPoolActorActivated_Implementation();
        TestEqual(TEXT("对照组: 原生直接派发应+1"), Actor->ActivatedCount, 1);
    }

    // 调用门：未实现接口的 Actor 同步/异步调用都必须返回 false，且不得入队任何任务。
    AActor* PlainActor = World->SpawnActor<AActor>();
    if (TestNotNull(TEXT("应能生成普通Actor"), PlainActor))
    {
        TestFalse(TEXT("未实现接口时同步调用应返回false"),
            IObjectPoolInterface::CallLifecycleEventEnhanced(PlainActor, EObjectPoolLifecycleEvent::Activated, false));
        TestFalse(TEXT("未实现接口时异步调用应返回false"),
            IObjectPoolInterface::CallLifecycleEventEnhanced(PlainActor, EObjectPoolLifecycleEvent::Activated, true));
        PlainActor->Destroy();
    }

    // 同步路径：实现接口的 Actor 调用应被接受。
    TestTrue(TEXT("同步调用应成功"),
        IObjectPoolInterface::CallLifecycleEventEnhanced(Actor, EObjectPoolLifecycleEvent::Activated, false));
    TestEqual(TEXT("同步调用应精确派发一次原生实现"), Actor->ActivatedCount, 2);

    // 异步路径：调用仅入队。测试体阻塞游戏线程，回调在冲刷前不可能执行——
    // 冲刷前计数不变，冲刷后应精确增加一次。
    const int32 CountBeforeAsync = Actor->ActivatedCount;
    TestTrue(TEXT("异步调用应成功入队"),
        IObjectPoolInterface::CallLifecycleEventEnhanced(Actor, EObjectPoolLifecycleEvent::Activated, true));
    TestEqual(TEXT("异步回调在任务冲刷前不应执行"), Actor->ActivatedCount, CountBeforeAsync);

    FlushGameThreadAsyncTasks();
    TestEqual(TEXT("任务冲刷后异步回调应精确派发一次原生实现"),
        Actor->ActivatedCount, CountBeforeAsync + 1);
    TestEqual(TEXT("不应触发其他生命周期事件"), Actor->CreatedCount + Actor->ReturnedCount, 0);

    return true;
}

// M-12 回归：异步入队后、回调执行前销毁 Actor——弱指针捕获的回调解析后必须安全跳过，
// 不得悬空访问。Destroy 立即将 Actor 标记为垃圾，TWeakObjectPtr::Get 随之返回 nullptr，
// 无需等待 GC，因此该路径完全确定；冲刷后不崩溃且未派发即证明弱指针防护生效。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolLifecycleAsyncDestroyedActorTest,
    "XTools.ObjectPool.Lifecycle.AsyncCallSkippedForDestroyedActor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectPoolLifecycleAsyncDestroyedActorTest::RunTest(const FString& Parameters)
{
    FScopedLifecycleTestWorld TestWorld(TEXT("ObjectPoolTest_LifecycleDestroyed"));
    UWorld* World = TestWorld.Get();
    if (!TestNotNull(TEXT("应能创建测试世界"), World))
    {
        return false;
    }

    AObjectPoolLifecycleTestActor* Actor = World->SpawnActor<AObjectPoolLifecycleTestActor>();
    if (!TestNotNull(TEXT("应能生成接口测试Actor"), Actor))
    {
        return false;
    }

    TestTrue(TEXT("销毁前异步调用应成功入队"),
        IObjectPoolInterface::CallLifecycleEventEnhanced(Actor, EObjectPoolLifecycleEvent::Activated, true));

    // 关键时序：任务已入队但尚未执行时销毁 Actor（MarkAsGarbage，IsValid 即刻为 false）。
    // 对象内存在本测试内不会被 GC 回收，下方读取计数器是安全的。
    Actor->Destroy();

    FlushGameThreadAsyncTasks();
    TestEqual(TEXT("Actor已销毁时异步回调不应派发"), Actor->ActivatedCount, 0);
    TestEqual(TEXT("不应触发其他生命周期事件"), Actor->CreatedCount + Actor->ReturnedCount, 0);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
