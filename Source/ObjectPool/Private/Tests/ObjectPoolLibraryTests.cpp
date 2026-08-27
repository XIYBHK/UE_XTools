/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "ObjectPoolLibrary.h"
#include "ObjectPoolSettings.h"
#include "ObjectPoolSubsystem.h"
#include "ObjectPoolM14TestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    /**
     * 测试世界辅助：对象池子系统由运行时开发者设置 UObjectPoolSettings 控制（默认关闭），
     * 且仅在 GameWorld 创建。这里临时翻转内存中的设置 CDO 开关（不写配置文件），
     * 创建 Game 类型测试世界，析构时销毁世界并恢复设置。
     */
    class FScopedObjectPoolTestWorld
    {
    public:
        explicit FScopedObjectPoolTestWorld(FName WorldName)
        {
            // 默认值为 false；此处临时开启以确保测试世界能创建子系统
            // （同时防御用户配置关闭子系统的场景，兼容未来默认值变化）
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

    /** 临时覆写对象池开关并在析构时恢复原值（仅改内存 CDO，不写配置文件） */
    class FScopedObjectPoolSettingOverride
    {
    public:
        explicit FScopedObjectPoolSettingOverride(bool bNewValue)
        {
            Settings = GetMutableDefault<UObjectPoolSettings>();
            if (Settings)
            {
                bOriginalValue = Settings->bEnableObjectPoolSubsystem;
                Settings->bEnableObjectPoolSubsystem = bNewValue;
            }
        }

        ~FScopedObjectPoolSettingOverride()
        {
            if (Settings)
            {
                Settings->bEnableObjectPoolSubsystem = bOriginalValue;
            }
        }

        bool IsValid() const { return Settings != nullptr; }

    private:
        UObjectPoolSettings* Settings = nullptr;
        bool bOriginalValue = false;
    };

    /** 负向生成测试会按契约触发多层 Error 日志；仅该测试抑制日志事件，业务断言仍正常记录。 */
    class FObjectPoolExpectedSpawnFailureTestBase : public FAutomationTestBase
    {
    public:
        FObjectPoolExpectedSpawnFailureTestBase(const FString& InName, bool bInComplexTask)
            : FAutomationTestBase(InName, bInComplexTask)
        {
        }

        virtual bool SuppressLogs() override { return true; }
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

    TArray<AActor*> MixedActors = { PooledActor, FallbackActor };
    TestEqual(TEXT("批量归还应只统计实际归还成功的池对象"),
        UObjectPoolLibrary::BatchReturnActors(World, MixedActors), 1);
    TestTrue(TEXT("批量归还不应隐式销毁非池对象"), IsValid(FallbackActor));

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

// M-14 回归：请求类无法生成时，普通/Ex/获取或生成/批量入口均不得返回 AActor 基类空壳。
// 合法的池满回退由 FObjectPoolFallbackSpawnResultCodeTest 覆盖，不能因本测试而删除。
IMPLEMENT_SIMPLE_AUTOMATION_TEST_PRIVATE(FObjectPoolRejectsUnspawnableClassTest,
    FObjectPoolExpectedSpawnFailureTestBase,
    "XTools.ObjectPool.Library.RejectsUnspawnableClassWithoutBaseActorFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter,
    __FILE__, __LINE__)
namespace
{
    FObjectPoolRejectsUnspawnableClassTest FObjectPoolRejectsUnspawnableClassTestAutomationInstance(TEXT("FObjectPoolRejectsUnspawnableClassTest"));
}

bool FObjectPoolRejectsUnspawnableClassTest::RunTest(const FString& Parameters)
{
    FScopedObjectPoolTestWorld TestWorld(TEXT("ObjectPoolTest_M14Unspawnable"));
    UWorld* World = TestWorld.Get();
    if (!TestNotNull(TEXT("应能创建测试世界"), World))
    {
        return false;
    }

    const TSubclassOf<AActor> AbstractClass = AObjectPoolM14AbstractTestActor::StaticClass();
    const FTransform SpawnTransform = FTransform::Identity;

    UObjectPoolSubsystem* Subsystem = World->GetSubsystem<UObjectPoolSubsystem>();
    if (!TestNotNull(TEXT("应能创建对象池子系统"), Subsystem))
    {
        return false;
    }

    TestNull(TEXT("子系统普通入口对抽象类应返回nullptr"),
        Subsystem->SpawnActorFromPool(AbstractClass, SpawnTransform));
    TestNull(TEXT("子系统Deferred入口对抽象类应返回nullptr"),
        Subsystem->AcquireDeferredFromPool(AbstractClass));

    EPoolOpResult Result = EPoolOpResult::Success;
    TestNull(TEXT("库普通入口对抽象类应返回nullptr"),
        UObjectPoolLibrary::SpawnActorFromPool(World, AbstractClass, SpawnTransform));

    TestNull(TEXT("库Ex入口对抽象类应返回nullptr"),
        UObjectPoolLibrary::SpawnActorFromPoolEx(World, AbstractClass, SpawnTransform, Result));
    TestEqual(TEXT("Ex失败结果不得报告FallbackSpawned"), static_cast<int32>(Result),
        static_cast<int32>(EPoolOpResult::InvalidArgs));

    Result = EPoolOpResult::Success;
    TestNull(TEXT("AcquireOrSpawn对抽象类应返回nullptr"),
        UObjectPoolLibrary::AcquireOrSpawn(World, AbstractClass, SpawnTransform, Result));
    TestEqual(TEXT("AcquireOrSpawn失败结果不得报告FallbackSpawned"), static_cast<int32>(Result),
        static_cast<int32>(EPoolOpResult::InvalidArgs));

    TArray<FTransform> Transforms;
    Transforms.Add(SpawnTransform);
    Transforms.Add(FTransform(FRotator::ZeroRotator, FVector(100.0f, 0.0f, 0.0f)));
    Transforms.Add(FTransform(FRotator::ZeroRotator, FVector(200.0f, 0.0f, 0.0f)));

    TArray<AActor*> BatchActors;
    const int32 BatchSuccessCount = UObjectPoolLibrary::BatchSpawnActors(
        World, AbstractClass, Transforms, BatchActors);
    TestEqual(TEXT("普通批量抽象类成功数应为0"), BatchSuccessCount, 0);
    TestEqual(TEXT("普通批量输出应保留每个输入的nullptr占位"), BatchActors.Num(), Transforms.Num());
    for (AActor* Actor : BatchActors)
    {
        TestNull(TEXT("普通批量失败项不得返回基类或其他Actor"), Actor);
    }

    BatchActors.Reset();
    const int32 BatchExSuccessCount = UObjectPoolLibrary::BatchSpawnActorsEx(
        World, AbstractClass, Transforms, BatchActors, EBatchFailurePolicy::BestEffort, true);
    TestEqual(TEXT("Ex批量抽象类成功数应为0"), BatchExSuccessCount, 0);
    TestEqual(TEXT("Ex批量保持顺序时应保留nullptr占位"), BatchActors.Num(), Transforms.Num());
    for (AActor* Actor : BatchActors)
    {
        TestNull(TEXT("Ex批量失败项不得返回基类或其他Actor"), Actor);
    }

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

// d. 设置默认值：对象池子系统默认关闭（保守兼容——升级项目未经明确配置不应被静默启用）
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolSettingsDefaultDisabledTest,
    "XTools.ObjectPool.Settings.DefaultDisabled",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectPoolSettingsDefaultDisabledTest::RunTest(const FString& Parameters)
{
    // 断言目标：代码级默认值。CDO 会被 UPROPERTY(config) 在配置加载阶段灌入用户 ini 值，
    // 在显式开启对象池的项目中跑测试会误报；改用全新瞬态实例（不触发 LoadConfig）
    // 才能反映"未经明确配置时的出厂默认"。
    const UObjectPoolSettings* Settings = NewObject<UObjectPoolSettings>(GetTransientPackage());
    if (!TestNotNull(TEXT("对象池设置实例应可创建"), Settings))
    {
        return false;
    }

    TestFalse(TEXT("对象池子系统默认应关闭（升级项目未经明确配置不启用）"),
        Settings->bEnableObjectPoolSubsystem);
    return true;
}

// e. 子系统创建条件：开关关闭时 Game 世界不创建对象池子系统，开启时创建
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolSubsystemCreationRespectsSettingsTest,
    "XTools.ObjectPool.Settings.SubsystemCreatedOnlyWhenEnabled",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectPoolSubsystemCreationRespectsSettingsTest::RunTest(const FString& Parameters)
{
    // 关闭：Game 世界不应创建子系统
    {
        FScopedObjectPoolSettingOverride Override(false);
        if (!TestTrue(TEXT("设置对象应可修改"), Override.IsValid()))
        {
            return false;
        }

        UWorld* DisabledWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ObjectPoolTest_SettingsDisabled"));
        if (!TestNotNull(TEXT("开关关闭时应能创建测试世界"), DisabledWorld))
        {
            return false;
        }
        TestTrue(TEXT("开关关闭时不应创建对象池子系统"),
            DisabledWorld->GetSubsystem<UObjectPoolSubsystem>() == nullptr);
        DisabledWorld->DestroyWorld(false);
    }

    // 开启：Game 世界应创建子系统
    {
        FScopedObjectPoolSettingOverride Override(true);
        UWorld* EnabledWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ObjectPoolTest_SettingsEnabled"));
        if (!TestNotNull(TEXT("开关开启时应能创建测试世界"), EnabledWorld))
        {
            return false;
        }
        TestNotNull(TEXT("开关开启时应创建对象池子系统"),
            EnabledWorld->GetSubsystem<UObjectPoolSubsystem>());
        EnabledWorld->DestroyWorld(false);
    }

    // 离开作用域后开关应已恢复原值
    const UObjectPoolSettings* Settings = GetDefault<UObjectPoolSettings>();
    TestFalse(TEXT("测试结束后设置应恢复为默认关闭"), Settings->bEnableObjectPoolSubsystem);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
