/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "FormationManagerComponent.h"
#include "FormationManagerComponentTestTypes.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include <limits>

namespace
{
    class FScopedFormationManagerTestWorld
    {
    public:
        explicit FScopedFormationManagerTestWorld(FName WorldName)
            : World(UWorld::CreateWorld(EWorldType::Game, false, WorldName))
        {
        }

        ~FScopedFormationManagerTestWorld()
        {
            if (World)
            {
                World->DestroyWorld(false);
            }
        }

        UWorld* Get() const { return World; }

    private:
        UWorld* World = nullptr;
    };
}

// 访问方式说明：无法通过"测试子类"暴露 protected CreateCostMatrix——
// UCLASS 不能声明在 .cpp 中（UHT 只处理头文件），无 UCLASS 的 C++ 子类无法经 NewObject 实例化（缺少 StaticClass），
// C++ protected 访问规则也不允许通过基类实例借用派生类访问权。
// 因此采用最小等价方案：头文件中 friend 声明本测试类（编译期授权，不新增生产 API）。
//
// 参数说明：CalculateAbsoluteDistanceCostMatrix 的行列数均取 FromPositions.Num() 并直接索引 ToPositions[j]，
// 假定 |From|==|To|（上游 CalculateOptimalAssignment 已强制等数）。故不使用 From=[A,B]/To=[C]（会越界读 To[1]），
// 改用同样保持"展平序列相同、切分点不同"这一结构碰撞本质的安全参数。

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFormationManagerComponentCacheTests,
    "XTools.Formation.Manager.CostMatrixCacheAvoidsStructuralCollision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationManagerComponentCacheTests::RunTest(const FString& Parameters)
{
    UFormationManagerComponent* Manager = NewObject<UFormationManagerComponent>();
    TestNotNull(TEXT("阵型管理器组件应可创建"), Manager);
    if (!Manager)
    {
        return false;
    }

    // 四个位置共线等距，距离全部为 100，矩阵值确定可读
    const FVector A = FVector::ZeroVector;
    const FVector B = FVector(100.0, 0.0, 0.0);
    const FVector C = FVector(200.0, 0.0, 0.0);
    const FVector D = FVector(300.0, 0.0, 0.0);

    // 第一次调用：From=[A]、To=[B,C,D]，展平序列 [A,B,C,D]
    // 矩阵行列均取 From.Num()=1 → 1x1，值 |A-B|=100
    const TArray<FVector> FirstFrom = { A };
    const TArray<FVector> FirstTo = { B, C, D };
    const TArray<TArray<float>> FirstMatrix = Manager->CreateCostMatrix(FirstFrom, FirstTo, false);

    TestEqual(TEXT("第一次调用应生成 1 行成本矩阵"), FirstMatrix.Num(), 1);
    if (FirstMatrix.Num() == 1)
    {
        TestEqual(TEXT("第一次调用每行应有 1 列"), FirstMatrix[0].Num(), 1);
        TestTrue(TEXT("第一次调用矩阵值应为 |A-B|=100"),
            FMath::IsNearlyEqual(FirstMatrix[0][0], 100.0f));
    }

    // 第二次调用：From=[A,B]、To=[C,D]，展平序列同为 [A,B,C,D]
    // 旧实现哈希只覆盖展平序列 → 与第一次同键（模式相同、TTL 内）→ 误命中第一次的 1x1 缓存
    // 新实现将两数组元素数量纳入哈希 → 键不同 → 重新计算 2x2
    const TArray<FVector> SecondFrom = { A, B };
    const TArray<FVector> SecondTo = { C, D };
    const TArray<TArray<float>> SecondMatrix = Manager->CreateCostMatrix(SecondFrom, SecondTo, false);

    TestEqual(TEXT("结构碰撞输入不应复用缓存，应生成 2 行成本矩阵"), SecondMatrix.Num(), 2);
    if (SecondMatrix.Num() == 2)
    {
        TestEqual(TEXT("第二次调用每行应有 2 列"), SecondMatrix[0].Num(), 2);
        TestEqual(TEXT("第二次调用第二行也应有 2 列"), SecondMatrix[1].Num(), 2);

        // 内容与直接计算一致：[i][j] = |From[i] - To[j]| = [[200, 300], [100, 200]]
        TestTrue(TEXT("矩阵[0][0]应为 |A-C|=200"), FMath::IsNearlyEqual(SecondMatrix[0][0], 200.0f));
        TestTrue(TEXT("矩阵[0][1]应为 |A-D|=300"), FMath::IsNearlyEqual(SecondMatrix[0][1], 300.0f));
        TestTrue(TEXT("矩阵[1][0]应为 |B-C|=100"), FMath::IsNearlyEqual(SecondMatrix[1][0], 100.0f));
        TestTrue(TEXT("矩阵[1][1]应为 |B-D|=200"), FMath::IsNearlyEqual(SecondMatrix[1][1], 200.0f));
    }

    // 第三次调用：与第二次完全相同 → 走正常缓存命中路径，结果仍应正确（防修复退化为禁用缓存）
    const TArray<TArray<float>> ThirdMatrix = Manager->CreateCostMatrix(SecondFrom, SecondTo, false);
    TestEqual(TEXT("相同输入重复调用应保持 2 行成本矩阵"), ThirdMatrix.Num(), 2);
    if (ThirdMatrix.Num() == 2)
    {
        TestEqual(TEXT("相同输入重复调用每行应有 2 列"), ThirdMatrix[0].Num(), 2);
        TestTrue(TEXT("相同输入重复调用矩阵内容应一致"),
            ThirdMatrix == SecondMatrix);
    }

    return true;
}

// 停止事件语义与幂等性（无 World：StopFormationTransition 的停止路径不触碰 GetWorld()，
// 手动置位 TransitionState.bIsTransitioning 即可覆盖；完整 Start→Tick→完成链路依赖
// GetWorld()->GetTimeSeconds()，需最小场景测试，不在本测试范围内）。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFormationManagerComponentStopTransitionTests,
    "XTools.Formation.Manager.StopTransitionBroadcastsStoppedOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationManagerComponentStopTransitionTests::RunTest(const FString& Parameters)
{
    UFormationManagerComponent* Manager = NewObject<UFormationManagerComponent>();
    TestNotNull(TEXT("阵型管理器组件应可创建"), Manager);
    if (!Manager)
    {
        return false;
    }

    UFormationTestEventReceiver* Receiver = NewObject<UFormationTestEventReceiver>();
    TestNotNull(TEXT("事件接收器应可创建"), Receiver);
    if (!Receiver)
    {
        return false;
    }

    Manager->OnFormationTransitionCompleted.AddDynamic(Receiver, &UFormationTestEventReceiver::OnCompleted);
    Manager->OnFormationTransitionStopped.AddDynamic(Receiver, &UFormationTestEventReceiver::OnStopped);

    // 1) 默认状态：未变换、Tick 禁用（bStartWithTickEnabled=false 生效）
    TestFalse(TEXT("默认不应处于变换中"), Manager->IsTransitioning());
    TestFalse(TEXT("默认 Tick 应禁用"), Manager->IsComponentTickEnabled());

    // 2) 未变换时调用 Stop：空操作，不广播任何事件
    Manager->StopFormationTransition(false);
    TestEqual(TEXT("未变换时 Stop 不应广播停止事件"), Receiver->StoppedCount, 0);
    TestEqual(TEXT("未变换时 Stop 不应广播完成事件"), Receiver->CompletedCount, 0);
    TestFalse(TEXT("未变换时 Stop 后仍不应处于变换中"), Manager->IsTransitioning());

    // 3) 变换中调用 Stop：广播停止事件一次，不触发完成事件，状态复位且 Tick 关闭
    Manager->TransitionState.bIsTransitioning = true;
    Manager->TransitionState.OverallProgress = 0.5f;
    Manager->SetComponentTickEnabled(true);

    Manager->StopFormationTransition(false);
    TestEqual(TEXT("变换中 Stop 应广播停止事件一次"), Receiver->StoppedCount, 1);
    TestEqual(TEXT("Stop 不应伪装成完成（完成事件不触发）"), Receiver->CompletedCount, 0);
    TestFalse(TEXT("Stop 后不应处于变换中"), Manager->IsTransitioning());
    TestFalse(TEXT("Stop 后 Tick 应禁用"), Manager->IsComponentTickEnabled());
    TestTrue(TEXT("Stop 后总进度应归零"), FMath::IsNearlyZero(Manager->TransitionState.OverallProgress));
    TestEqual(TEXT("Stop 后单位变换数据应清空"), Manager->TransitionState.UnitTransitions.Num(), 0);

    // 4) 重复 Stop：入口早退拦截，不重复广播
    Manager->StopFormationTransition(false);
    TestEqual(TEXT("重复 Stop 不应重复广播停止事件"), Receiver->StoppedCount, 1);

    // 5) 再次进入变换并以 bSnapToTarget=true 停止（单位列表为空，快照循环空转）：
    //    停止事件再次广播，验证"每次真实停止恰好广播一次"
    Manager->TransitionState.bIsTransitioning = true;
    Manager->StopFormationTransition(true);
    TestEqual(TEXT("再次真实停止应再广播一次停止事件"), Receiver->StoppedCount, 2);
    TestEqual(TEXT("完成事件全程不应触发"), Receiver->CompletedCount, 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFormationManagerComponentActorTransformSafetyTests,
    "XTools.Formation.Manager.RejectsRootlessActorsAndPreservesScale",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationManagerComponentActorTransformSafetyTests::RunTest(const FString& Parameters)
{
    FScopedFormationManagerTestWorld WorldScope(TEXT("FormationManagerTest_ActorTransformSafety"));
    UWorld* World = WorldScope.Get();
    if (!TestNotNull(TEXT("应创建阵型管理器测试世界"), World))
    {
        return false;
    }

    AActor* ManagerActor = World->SpawnActor<AActor>();
    UFormationManagerComponent* Manager = ManagerActor
        ? NewObject<UFormationManagerComponent>(ManagerActor)
        : nullptr;
    if (!TestNotNull(TEXT("应创建阵型管理器 Actor"), ManagerActor)
        || !TestNotNull(TEXT("应创建阵型管理器组件"), Manager))
    {
        return false;
    }
    ManagerActor->AddInstanceComponent(Manager);
    Manager->RegisterComponent();

    FFormationData FromFormation;
    FromFormation.Positions = {FVector::ZeroVector};
    FFormationData ToFormation;
    ToFormation.Positions = {FVector(100.0f, 0.0f, 0.0f)};
    FFormationTransitionConfig Config;

    AActor* RootlessActor = World->SpawnActor<AActor>();
    TestNotNull(TEXT("应创建无根组件测试 Actor"), RootlessActor);
    TestNull(TEXT("普通 AActor 默认应无根组件"), RootlessActor ? RootlessActor->GetRootComponent() : nullptr);
    AddExpectedError(TEXT("StartFormationTransition: 跳过无效或无根组件的单位"),
        EAutomationExpectedErrorFlags::Contains, 1);
    AddExpectedError(TEXT("StartFormationTransition: 没有可移动的有效单位"),
        EAutomationExpectedErrorFlags::Contains, 1);
    TestFalse(TEXT("仅包含无根组件 Actor 时不应启动伪过渡"),
        Manager->StartFormationTransition({RootlessActor}, FromFormation, ToFormation, Config));
    TestFalse(TEXT("拒绝无根组件 Actor 后不应处于变换中"), Manager->IsTransitioning());

    FFormationData NonFiniteFromFormation = FromFormation;
    NonFiniteFromFormation.Positions[0].X = std::numeric_limits<float>::quiet_NaN();
    AddExpectedError(TEXT("StartFormationTransition: 起始阵型包含非有限位置"),
        EAutomationExpectedErrorFlags::Contains, 1);
    TestFalse(TEXT("含非有限位置的阵型不应启动过渡"),
        Manager->StartFormationTransition({RootlessActor}, NonFiniteFromFormation, ToFormation, Config));
    TestFalse(TEXT("拒绝非有限阵型后不应处于变换中"), Manager->IsTransitioning());

    AActor* MovableActor = World->SpawnActor<AActor>();
    USceneComponent* RootComponent = MovableActor
        ? NewObject<USceneComponent>(MovableActor, TEXT("FormationTestRoot"))
        : nullptr;
    if (!TestNotNull(TEXT("应创建可移动测试 Actor"), MovableActor)
        || !TestNotNull(TEXT("应创建测试根组件"), RootComponent))
    {
        return false;
    }
    MovableActor->SetRootComponent(RootComponent);
    MovableActor->AddInstanceComponent(RootComponent);
    RootComponent->RegisterComponent();

    MovableActor->SetActorScale3D(FVector::OneVector);
    TestTrue(TEXT("有根组件 Actor 应成功启动过渡"),
        Manager->StartFormationTransition({MovableActor}, FromFormation, ToFormation, Config));

    const FVector ExternalScale(2.0f, 3.0f, 4.0f);
    MovableActor->SetActorScale3D(ExternalScale);
    UActorComponent* TickableManager = Manager;
    TickableManager->TickComponent(0.016f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("阵型位置过渡不应每帧覆盖外部缩放"),
        MovableActor->GetActorScale3D().Equals(ExternalScale));

    Manager->StopFormationTransition(false);
    return true;
}

#endif
