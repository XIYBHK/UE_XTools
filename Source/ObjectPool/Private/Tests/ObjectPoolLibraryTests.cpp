/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "ObjectPoolLibrary.h"
#include "ObjectPoolSettings.h"
#include "ObjectPoolSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    /**
     * 测试世界辅助：对象池子系统由运行时开发者设置 UObjectPoolSettings 控制（默认启用），
     * 且仅在 GameWorld 创建。这里临时翻转内存中的设置 CDO 开关（不写配置文件），
     * 创建 Game 类型测试世界，析构时销毁世界并恢复设置。
     */
    class FScopedObjectPoolTestWorld
    {
    public:
        explicit FScopedObjectPoolTestWorld(FName WorldName)
        {
            // 新默认值为 true；此处防御用户配置关闭子系统的场景，确保测试世界能创建子系统
            if (UObjectPoolSettings* Settings = GetMutableDefault<UObjectPoolSettings>())
            {
                bOriginalValue = Settings->bEnableObjectPoolSubsystem;
                bSettingFlipped = !bOriginalValue;
                if (bSettingFlipped)
                {
                    Settings->bEnableObjectPoolSubsystem = true;
                }
            }

            World = UWorld::CreateWorld(EWorldType::Game, false, WorldName);
        }

        ~FScopedObjectPoolTestWorld()
        {
            if (World)
            {
                World->DestroyWorld(false);
                World = nullptr;
            }

            if (bSettingFlipped)
            {
                if (UObjectPoolSettings* Settings = GetMutableDefault<UObjectPoolSettings>())
                {
                    Settings->bEnableObjectPoolSubsystem = bOriginalValue;
                }
            }
        }

        UWorld* Get() const { return World; }

    private:
        UWorld* World = nullptr;
        bool bOriginalValue = false;
        bool bSettingFlipped = false;
    };

    /** 构造硬限制已满的池：注册 AActor（初始 0、硬限制 1）并预热 1 个实例，不依赖 Tick 的延迟预热 */
    UObjectPoolSubsystem* BuildFullPool(UWorld* World)
    {
        UObjectPoolSubsystem* Subsystem = World ? World->GetSubsystem<UObjectPoolSubsystem>() : nullptr;
        if (!Subsystem)
        {
            return nullptr;
        }
        if (!Subsystem->RegisterActorClass(AActor::StaticClass(), 0, 1))
        {
            return nullptr;
        }
        if (Subsystem->PrewarmPool(AActor::StaticClass(), 1) != 1)
        {
            return nullptr;
        }
        return Subsystem;
    }
}

// a. SpawnActorFromPoolEx 对硬限制已满时的回退生成输出 FallbackSpawned；ReturnActorToPoolEx 语义不变
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolFallbackSpawnResultCodeTest,
    "XTools.ObjectPool.Library.FallbackSpawnResultCode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectPoolFallbackSpawnResultCodeTest::RunTest(const FString& Parameters)
{
    FScopedObjectPoolTestWorld TestWorld(TEXT("ObjectPoolTest_FallbackResultCode"));
    UObjectPoolSubsystem* Subsystem = BuildFullPool(TestWorld.Get());
    if (!TestTrue(TEXT("应能构造硬限制已满的对象池"), Subsystem != nullptr))
    {
        return false;
    }
    UWorld* World = TestWorld.Get();

    EPoolOpResult Result = EPoolOpResult::InvalidArgs;
    AActor* PooledActor = UObjectPoolLibrary::SpawnActorFromPoolEx(World, AActor::StaticClass(), FTransform::Identity, Result);
    TestTrue(TEXT("池内实例应获取成功"), PooledActor != nullptr);
    TestEqual(TEXT("池内实例结果码应为成功"), static_cast<int32>(Result), static_cast<int32>(EPoolOpResult::Success));
    TestTrue(TEXT("池内实例应受池管理"), UObjectPoolLibrary::IsActorPooled(World, PooledActor));

    AActor* FallbackActor = UObjectPoolLibrary::SpawnActorFromPoolEx(World, AActor::StaticClass(), FTransform::Identity, Result);
    TestTrue(TEXT("硬限制已满时仍应回退生成Actor"), FallbackActor != nullptr);
    TestEqual(TEXT("回退Actor结果码应为回退生成"), static_cast<int32>(Result), static_cast<int32>(EPoolOpResult::FallbackSpawned));
    TestFalse(TEXT("回退Actor不应受池管理"), UObjectPoolLibrary::IsActorPooled(World, FallbackActor));

    const bool bReturned = UObjectPoolLibrary::ReturnActorToPoolEx(World, FallbackActor, Result);
    TestFalse(TEXT("归还非池对象应返回false"), bReturned);
    TestEqual(TEXT("归还非池对象结果码应为非池对象"), static_cast<int32>(Result), static_cast<int32>(EPoolOpResult::NotPooled));
    TestTrue(TEXT("ReturnActorToPoolEx不应隐式销毁非池对象"), IsValid(FallbackActor));

    return true;
}

// b. AcquireOrSpawn 透传真实来源结果（含 FallbackSpawned）
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolAcquireOrSpawnFallbackTest,
    "XTools.ObjectPool.Library.AcquireOrSpawnPropagatesFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectPoolAcquireOrSpawnFallbackTest::RunTest(const FString& Parameters)
{
    FScopedObjectPoolTestWorld TestWorld(TEXT("ObjectPoolTest_AcquireOrSpawn"));
    UObjectPoolSubsystem* Subsystem = BuildFullPool(TestWorld.Get());
    if (!TestTrue(TEXT("应能构造硬限制已满的对象池"), Subsystem != nullptr))
    {
        return false;
    }
    UWorld* World = TestWorld.Get();

    EPoolOpResult Result = EPoolOpResult::InvalidArgs;
    AActor* PooledActor = UObjectPoolLibrary::AcquireOrSpawn(World, AActor::StaticClass(), FTransform::Identity, Result);
    TestTrue(TEXT("首次获取应从池中复用"), PooledActor != nullptr);
    TestEqual(TEXT("首次获取结果码应为成功"), static_cast<int32>(Result), static_cast<int32>(EPoolOpResult::Success));
    TestTrue(TEXT("首次获取的Actor应受池管理"), UObjectPoolLibrary::IsActorPooled(World, PooledActor));

    AActor* FallbackActor = UObjectPoolLibrary::AcquireOrSpawn(World, AActor::StaticClass(), FTransform::Identity, Result);
    TestTrue(TEXT("硬限制已满时应回退生成Actor"), FallbackActor != nullptr);
    TestEqual(TEXT("AcquireOrSpawn应透传回退生成结果码"), static_cast<int32>(Result), static_cast<int32>(EPoolOpResult::FallbackSpawned));
    TestFalse(TEXT("回退Actor不应受池管理"), UObjectPoolLibrary::IsActorPooled(World, FallbackActor));

    return true;
}

// c. ReleaseOrDespawn 对非池对象调用 Destroy，返回 true 且结果码保留 NotPooled；池对象行为不变
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolReleaseOrDespawnDestroysNonPooledTest,
    "XTools.ObjectPool.Library.ReleaseOrDespawnDestroysNonPooled",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectPoolReleaseOrDespawnDestroysNonPooledTest::RunTest(const FString& Parameters)
{
    FScopedObjectPoolTestWorld TestWorld(TEXT("ObjectPoolTest_ReleaseOrDespawn"));
    UObjectPoolSubsystem* Subsystem = BuildFullPool(TestWorld.Get());
    if (!TestTrue(TEXT("应能构造硬限制已满的对象池"), Subsystem != nullptr))
    {
        return false;
    }
    UWorld* World = TestWorld.Get();

    EPoolOpResult Result = EPoolOpResult::InvalidArgs;
    AActor* PooledActor = UObjectPoolLibrary::SpawnActorFromPoolEx(World, AActor::StaticClass(), FTransform::Identity, Result);
    AActor* FallbackActor = UObjectPoolLibrary::SpawnActorFromPoolEx(World, AActor::StaticClass(), FTransform::Identity, Result);
    if (!TestTrue(TEXT("应能取得池对象与回退对象"), PooledActor != nullptr && FallbackActor != nullptr))
    {
        return false;
    }

    bool bReleased = UObjectPoolLibrary::ReleaseOrDespawn(World, PooledActor, Result);
    TestTrue(TEXT("释放池对象应返回true"), bReleased);
    TestEqual(TEXT("释放池对象结果码应为成功"), static_cast<int32>(Result), static_cast<int32>(EPoolOpResult::Success));
    TestTrue(TEXT("池对象归还后不应被销毁"), IsValid(PooledActor));

    bReleased = UObjectPoolLibrary::ReleaseOrDespawn(World, FallbackActor, Result);
    TestTrue(TEXT("释放非池对象应返回true"), bReleased);
    TestEqual(TEXT("释放非池对象结果码应保留非池对象"), static_cast<int32>(Result), static_cast<int32>(EPoolOpResult::NotPooled));
    TestTrue(TEXT("非池对象应被销毁"), FallbackActor->IsPendingKillPending());

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
