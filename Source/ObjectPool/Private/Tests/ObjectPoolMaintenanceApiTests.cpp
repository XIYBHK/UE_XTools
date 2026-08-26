/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "ObjectPoolManager.h"
#include "ObjectPool.h"
#include "ObjectPoolSettings.h"
#include "ObjectPoolSubsystem.h"
#include "ObjectPoolUtils.h"
#include "ActorPool.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace
{
    /**
     * 测试世界辅助：对象池子系统由运行时开发者设置 UObjectPoolSettings 控制（默认关闭），
     * 这里临时翻转内存中的设置 CDO 开关（不写配置文件），创建 Game 类型测试世界，
     * 析构时销毁世界并恢复设置。
     */
    class FScopedMaintenanceTestWorld
    {
    public:
        explicit FScopedMaintenanceTestWorld(FName WorldName)
        {
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

        ~FScopedMaintenanceTestWorld()
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

    /** 构造不含任何 Actor 的空池（不触碰 World，仅用于管理器层确定性验证） */
    TSharedPtr<FActorPool> MakeEmptyPool(int32 InHardLimit = 50)
    {
        return MakeShared<FActorPool>(AActor::StaticClass(), 1, InHardLimit);
    }
}

// M-17/M-18 回归：FObjectPoolManager 维护层公共 API 无内部调度入口，
// 但作为 OBJECTPOOL_API 导出接口保留，这里以确定性断言钉住其行为不腐烂。
// 覆盖：统计生命周期、空集维护、自动管理开关、管理报告、统计重置。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolManagerMaintenanceApiTest,
    "XTools.ObjectPool.Manager.MaintenanceApiDeterministic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectPoolManagerMaintenanceApiTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FActorPool> PoolA = MakeEmptyPool();
    TSharedPtr<FActorPool> PoolB = MakeEmptyPool();
    if (!TestTrue(TEXT("应能构造空池"), PoolA.IsValid() && PoolB.IsValid()))
    {
        return false;
    }

    FObjectPoolManager Manager;
    TestEqual(TEXT("初始总维护次数应为0"), Manager.GetManagementStats().TotalMaintenanceCount, 0);

    Manager.OnPoolCreated(AActor::StaticClass(), PoolA);
    Manager.OnPoolCreated(AActor::StaticClass(), PoolA);
    TestEqual(TEXT("创建两个池后管理计数应为2"), Manager.GetManagementStats().ManagedPoolCount, 2);

    Manager.OnPoolDestroying(AActor::StaticClass());
    TestEqual(TEXT("销毁一个池后管理计数应为1"), Manager.GetManagementStats().ManagedPoolCount, 1);

    // 空池集合上的维护是确定性的纯统计操作（不依赖 Timer/Ticker）
    const TMap<TObjectPtr<UClass>, TSharedPtr<FActorPool>> EmptyPools;
    Manager.PerformMaintenance(EmptyPools);
    const FObjectPoolManager::FManagementStats StatsAfterMaint = Manager.GetManagementStats();
    TestEqual(TEXT("执行一次维护后总维护次数应为1"), StatsAfterMaint.TotalMaintenanceCount, 1);
    TestTrue(TEXT("维护后应记录维护时间"), StatsAfterMaint.LastMaintenanceTime > 0.0);

    // 禁用自动管理后维护直接返回，不再累计次数
    Manager.SetAutoManagementEnabled(false);
    TestFalse(TEXT("自动管理应已禁用"), Manager.IsAutoManagementEnabled());
    Manager.PerformMaintenance(EmptyPools);
    TestEqual(TEXT("禁用后维护次数不应增加"), Manager.GetManagementStats().TotalMaintenanceCount, 1);

    // 报告接口可用且包含固定标题
    const FString Report = Manager.GenerateManagementReport(EmptyPools);
    TestTrue(TEXT("管理报告应包含固定标题"), Report.Contains(TEXT("池管理器报告")));

    Manager.ResetStats();
    const FObjectPoolManager::FManagementStats StatsAfterReset = Manager.GetManagementStats();
    TestEqual(TEXT("重置后管理计数应为0"), StatsAfterReset.ManagedPoolCount, 0);
    TestEqual(TEXT("重置后总维护次数应为0"), StatsAfterReset.TotalMaintenanceCount, 0);

    return true;
}

// M-17/M-18 回归：自动扩缩容与使用分析的确定性策略行为（Manual 不调整 / Adaptive 空池收敛到推荐值）
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolManagerAutoResizeDeterministicTest,
    "XTools.ObjectPool.Manager.AutoResizeDeterministic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectPoolManagerAutoResizeDeterministicTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FActorPool> Pool = MakeEmptyPool(50);
    if (!TestTrue(TEXT("应能构造硬限制为50的空池"), Pool.IsValid()))
    {
        return false;
    }

    // Manual 策略：直接返回 false，上限不变
    FObjectPoolManager ManualManager(FObjectPoolManager::EManagementStrategy::Manual);
    TestFalse(TEXT("Manual策略不应自动调整池大小"),
        ManualManager.AutoResizePool(AActor::StaticClass(), *Pool));
    TestEqual(TEXT("Manual策略下池上限应保持不变"), Pool->GetMaxSize(), 50);

    // Adaptive 策略 + 空池（PoolSize=0）：走"默认推荐大小10"分支，50 -> 10 为确定性结果
    FObjectPoolManager AdaptiveManager(FObjectPoolManager::EManagementStrategy::Adaptive);
    TestTrue(TEXT("Adaptive策略应对空池触发调整"),
        AdaptiveManager.AutoResizePool(AActor::StaticClass(), *Pool));
    TestEqual(TEXT("调整后池上限应为推荐值10"), Pool->GetMaxSize(), 10);

    // 已达到推荐值时幂等返回 false，不再调整
    TestFalse(TEXT("已达标时应幂等返回false"),
        AdaptiveManager.AutoResizePool(AActor::StaticClass(), *Pool));
    TestEqual(TEXT("幂等调用后池上限仍为10"), Pool->GetMaxSize(), 10);

    // 使用分析：空池固定产生"低使用率"+"低命中率"两条建议，趋势数据不足时不产生建议
    const TArray<FString> Suggestions = AdaptiveManager.AnalyzePoolUsage(AActor::StaticClass(), *Pool);
    TestEqual(TEXT("空池应产生两条固定建议（低使用率与低命中率）"), Suggestions.Num(), 2);

    return true;
}

// M-18 回归：FObjectPoolUtils 中保留的公共旧工具函数行为钉定（仓库内零调用但属于导出 API）
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolUtilsLegacyApiBehaviorTest,
    "XTools.ObjectPool.Utils.LegacyUtilityApiPinned",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectPoolUtilsLegacyApiBehaviorTest::RunTest(const FString& Parameters)
{
    // GeneratePoolId：空类与确定性格式
    TestEqual(TEXT("空类应返回InvalidPool"),
        FObjectPoolUtils::GeneratePoolId(nullptr), FString(TEXT("InvalidPool")));
    const FString PoolId = FObjectPoolUtils::GeneratePoolId(AActor::StaticClass());
    TestTrue(TEXT("池ID应以Pool_Actor_开头"), PoolId.StartsWith(TEXT("Pool_Actor_")));
    TestEqual(TEXT("同类重复生成应稳定"),
        FObjectPoolUtils::GeneratePoolId(AActor::StaticClass()), PoolId);

    // ApplyDefaultConfig：完整配置保持不变
    FObjectPoolConfig FullConfig;
    FullConfig.ActorClass = AActor::StaticClass();
    FullConfig.InitialSize = 7;
    FullConfig.HardLimit = 30;
    FObjectPoolUtils::ApplyDefaultConfig(FullConfig);
    TestEqual(TEXT("完整配置的初始大小不应被修改"), FullConfig.InitialSize, 7);
    TestEqual(TEXT("完整配置的硬限制不应被修改"), FullConfig.HardLimit, 30);

    // ApplyDefaultConfig：缺省字段填充确定性默认值（普通Actor：初始10、硬限制100）
    FObjectPoolConfig SparseConfig;
    SparseConfig.ActorClass = AActor::StaticClass();
    SparseConfig.InitialSize = 0;
    SparseConfig.HardLimit = 0;
    FObjectPoolUtils::ApplyDefaultConfig(SparseConfig);
    TestEqual(TEXT("缺省初始大小应填充为10"), SparseConfig.InitialSize, 10);
    TestEqual(TEXT("缺省硬限制应填充为100"), SparseConfig.HardLimit, 100);

    // ApplyDefaultConfig：空Actor类时安全返回且不改动入参（预期触发一条警告日志）
    AddExpectedMessage(TEXT("ApplyDefaultConfig: Actor类为空，无法应用默认配置"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
    FObjectPoolConfig NullClassConfig;
    NullClassConfig.InitialSize = 3;
    NullClassConfig.HardLimit = 9;
    FObjectPoolUtils::ApplyDefaultConfig(NullClassConfig);
    TestEqual(TEXT("空类配置不应被修改（初始大小）"), NullClassConfig.InitialSize, 3);
    TestEqual(TEXT("空类配置不应被修改（硬限制）"), NullClassConfig.HardLimit, 9);

    // CreateDefaultConfig：类型化默认值
    const FObjectPoolConfig BulletConfig = FObjectPoolUtils::CreateDefaultConfig(AActor::StaticClass(), TEXT("子弹"));
    TestEqual(TEXT("子弹池初始大小应为50"), BulletConfig.InitialSize, 50);
    TestEqual(TEXT("子弹池硬限制应为200"), BulletConfig.HardLimit, 200);
    TestTrue(TEXT("类型化配置应启用预热"), BulletConfig.bEnablePrewarm);

    // ValidateConfig：合法通过、硬限制小于初始大小失败、缺少Actor类失败
    FString ErrorMessage;
    FObjectPoolConfig ValidConfig;
    ValidConfig.ActorClass = AActor::StaticClass();
    ValidConfig.InitialSize = 10;
    ValidConfig.HardLimit = 100;
    TestTrue(TEXT("合法配置应通过校验"), FObjectPoolUtils::ValidateConfig(ValidConfig, ErrorMessage));
    TestTrue(TEXT("校验通过时错误信息应为空"), ErrorMessage.IsEmpty());

    ValidConfig.HardLimit = 5;
    TestFalse(TEXT("硬限制小于初始大小应校验失败"), FObjectPoolUtils::ValidateConfig(ValidConfig, ErrorMessage));
    TestFalse(TEXT("校验失败时应给出错误信息"), ErrorMessage.IsEmpty());

    FObjectPoolConfig NoClassConfig;
    NoClassConfig.InitialSize = 1;
    TestFalse(TEXT("缺少Actor类应校验失败"), FObjectPoolUtils::ValidateConfig(NoClassConfig, ErrorMessage));

    return true;
}

// 清理回归：子系统核心路径（注册→获取→归还→复用→统计→清空）在移除死维护层后行为不变
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolSubsystemRegisterSpawnReturnRoundtripTest,
    "XTools.ObjectPool.Subsystem.RegisterSpawnReturnRoundtrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectPoolSubsystemRegisterSpawnReturnRoundtripTest::RunTest(const FString& Parameters)
{
    FScopedMaintenanceTestWorld TestWorld(TEXT("ObjectPoolTest_MaintRoundtrip"));
    UWorld* World = TestWorld.Get();
    if (!TestNotNull(TEXT("应能创建测试世界"), World))
    {
        return false;
    }

    UObjectPoolSubsystem* Subsystem = World->GetSubsystem<UObjectPoolSubsystem>();
    if (!TestNotNull(TEXT("开关开启后应能创建对象池子系统"), Subsystem))
    {
        return false;
    }

    // 初始大小传 0：跳过延迟预热队列，保证池从确定性的空状态开始
    TestTrue(TEXT("注册Actor类应成功"),
        Subsystem->RegisterActorClass(AActor::StaticClass(), 0, 20));
    TestEqual(TEXT("注册后池数量应为1"), Subsystem->GetPoolCount(), 1);
    TestTrue(TEXT("已注册类查询应为真"), Subsystem->IsActorClassRegistered(AActor::StaticClass()));

    AActor* First = Subsystem->SpawnActorFromPool(AActor::StaticClass(), FTransform::Identity);
    if (!TestNotNull(TEXT("首次获取应成功"), First))
    {
        return false;
    }
    TestTrue(TEXT("获取的Actor应受池管理"), Subsystem->IsActorPooled(First));

    TestTrue(TEXT("归还应成功"), Subsystem->ReturnActorToPool(First));

    AActor* Second = Subsystem->SpawnActorFromPool(AActor::StaticClass(), FTransform::Identity);
    if (!TestNotNull(TEXT("归还后再次获取应成功"), Second))
    {
        return false;
    }
    TestTrue(TEXT("第二次获取应命中并复用同一实例"), Second == First);
    TestTrue(TEXT("复用实例应受池管理"), Subsystem->IsActorPooled(Second));

    const TArray<FObjectPoolStats> AllStats = Subsystem->GetAllPoolStats();
    TestEqual(TEXT("统计应恰好包含一个池"), AllStats.Num(), 1);
    if (AllStats.Num() == 1)
    {
        TestEqual(TEXT("累计创建数应为1（仅首取创建）"), AllStats[0].TotalCreated, 1);
        TestEqual(TEXT("当前活跃数应为1"), AllStats[0].CurrentActive, 1);
        TestEqual(TEXT("当前可用数应为0"), AllStats[0].CurrentAvailable, 0);
        TestEqual(TEXT("累计获取请求数应为2"), AllStats[0].TotalAcquired, 2);
        TestEqual(TEXT("累计归还数应为1"), AllStats[0].TotalReleased, 1);
        TestEqual(TEXT("两次获取中一次命中，命中率应为0.5"), AllStats[0].HitRate, 0.5f);
    }

    // 按类清空路径回归：清空后池数量归零
    TestTrue(TEXT("按类清空应成功"), Subsystem->ClearPoolByClass(AActor::StaticClass()));
    TestEqual(TEXT("清空后池数量应为0"), Subsystem->GetPoolCount(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolConsoleClassResolutionTest,
    "XTools.ObjectPool.Console.UniqueRegisteredClassResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectPoolConsoleClassResolutionTest::RunTest(const FString& Parameters)
{
    UPackage* PackageA = CreatePackage(TEXT("/Temp/ObjectPoolConsoleResolution/A"));
    UPackage* PackageB = CreatePackage(TEXT("/Temp/ObjectPoolConsoleResolution/B"));
    UClass* ClassA = NewObject<UClass>(PackageA, TEXT("BP_SharedPoolActor_C"), RF_Transient);
    UClass* ClassB = NewObject<UClass>(PackageB, TEXT("BP_SharedPoolActor_C"), RF_Transient);
    UBlueprintGeneratedClass* BlueprintClass = NewObject<UBlueprintGeneratedClass>(
        PackageA, TEXT("BP_RealGeneratedActor_C"), RF_Transient);
    if (!TestNotNull(TEXT("应创建第一个同名测试类"), ClassA)
        || !TestNotNull(TEXT("应创建第二个同名测试类"), ClassB)
        || !TestNotNull(TEXT("应创建蓝图生成类测试对象"), BlueprintClass))
    {
        return false;
    }

    bool bAmbiguous = false;
    TArray<FString> CandidatePaths;
    const TArray<UClass*> Candidates = {AActor::StaticClass(), ClassA, ClassB, ClassA};

    UClass* Resolved = FObjectPoolModule::ResolveUniquePoolClassIdentifier(
        TEXT("Actor"), Candidates, bAmbiguous, CandidatePaths);
    TestTrue(TEXT("唯一原生短类名应解析到已注册候选"), Resolved == AActor::StaticClass());
    TestFalse(TEXT("唯一短名不应标记歧义"), bAmbiguous);

    Resolved = FObjectPoolModule::ResolveUniquePoolClassIdentifier(
        ClassA->GetPathName(), Candidates, bAmbiguous, CandidatePaths);
    TestTrue(TEXT("完整对象路径应精确解析同名类"), Resolved == ClassA);
    TestFalse(TEXT("完整路径不应标记歧义"), bAmbiguous);

    const FString ExportTextPath = FString::Printf(TEXT("Class'%s'"), *ClassB->GetPathName());
    Resolved = FObjectPoolModule::ResolveUniquePoolClassIdentifier(
        ExportTextPath, Candidates, bAmbiguous, CandidatePaths);
    TestTrue(TEXT("导出文本路径应精确解析同名类"), Resolved == ClassB);

    Resolved = FObjectPoolModule::ResolveUniquePoolClassIdentifier(
        TEXT("BP_SharedPoolActor_C"), Candidates, bAmbiguous, CandidatePaths);
    TestNull(TEXT("歧义短名不得静默选择任一类"), Resolved);
    TestTrue(TEXT("同名短类名应标记歧义"), bAmbiguous);
    TestEqual(TEXT("歧义诊断应列出两个去重后的完整路径"), CandidatePaths.Num(), 2);

    Resolved = FObjectPoolModule::ResolveUniquePoolClassIdentifier(
        TEXT("MissingPoolClass"), Candidates, bAmbiguous, CandidatePaths);
    TestNull(TEXT("不存在的类名应返回空"), Resolved);
    TestFalse(TEXT("未命中不应误报歧义"), bAmbiguous);
    TestEqual(TEXT("未命中不应产生候选路径"), CandidatePaths.Num(), 0);

    const TArray<UClass*> BlueprintCandidates = {BlueprintClass};
    Resolved = FObjectPoolModule::ResolveUniquePoolClassIdentifier(
        TEXT("BP_RealGeneratedActor"), BlueprintCandidates, bAmbiguous, CandidatePaths);
    TestTrue(TEXT("蓝图生成类短名省略_C时仍应解析"), Resolved == BlueprintClass);
    TestFalse(TEXT("蓝图生成类唯一短名不应标记歧义"), bAmbiguous);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
