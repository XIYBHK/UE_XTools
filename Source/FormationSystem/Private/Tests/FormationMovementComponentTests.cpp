/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "FormationMovementComponent.h"
#include "FormationMovementComponentTestTypes.h"

#include "Components/ActorComponent.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    /**
     * 测试世界辅助：创建 Game 类型测试世界，析构时销毁。
     * 真实 CharacterMovement 必须注册进 World 才能工作——
     * CMC 的 OnRegister 仅在 GameWorld 下调用 SetUpdatedComponent（由此设置 PawnOwner），
     * 而 APawn::AddMovementInput 正是把输入经 PawnOwner 写入 ControlInputVector 的。
     * 世界从不 Tick：不驱动物理模拟，Character 位置只由测试显式控制，完全确定。
     */
    class FScopedFormationTestWorld
    {
    public:
        explicit FScopedFormationTestWorld(FName WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, WorldName);
        }

        ~FScopedFormationTestWorld()
        {
            if (World)
            {
                World->DestroyWorld(false);
                World = nullptr;
            }
        }

        UWorld* Get() const { return World; }

    private:
        UWorld* World = nullptr;
    };

    /**
     * 搭建移动测试环境：生成真实 Character 并挂载测试组件（注册 + 注入 OwnerCharacter）。
     * 生产路径的 OwnerCharacter 在 BeginPlay 缓存，但测试世界不触发 BeginPlay，故经测试子类注入。
     */
    bool SetupMovementTestEnvironment(UWorld* World, ACharacter*& OutCharacter, UFormationMovementTestComponent*& OutMoveComp)
    {
        if (!World)
        {
            return false;
        }

        OutCharacter = World->SpawnActor<ACharacter>();
        if (!OutCharacter)
        {
            return false;
        }

        OutMoveComp = NewObject<UFormationMovementTestComponent>(OutCharacter);
        OutCharacter->AddInstanceComponent(OutMoveComp);
        OutMoveComp->RegisterComponent();
        OutMoveComp->SetOwnerCharacterForTest(OutCharacter);

        return OutMoveComp != nullptr && OutCharacter->GetCharacterMovement() != nullptr;
    }

    /** 手动驱动组件单帧（替代世界 Tick；UActorComponent::TickComponent 要求组件已注册） */
    void TickMoveComponentManually(UFormationMovementTestComponent* MoveComp, float DeltaTime)
    {
        // TickComponent 在派生类中为 protected、基类 UActorComponent 中为 public——
        // 引擎 FActorComponentTickFunction::ExecuteTick 同样经基类指针调用（虚函数分发不受访问控制影响）
        UActorComponent* TickableComponent = MoveComp;
        TickableComponent->TickComponent(DeltaTime, LEVELTICK_All, nullptr);
    }
}

// M-25 核心回归：制动带内速度归零后必须恢复移动输入。
// 旧逻辑 bShouldBrake 只看距离——距离 ∈ (接受半径, 1.5×接受半径] 且速度已归零时（卡墙或提前减速停下），
// 永远进入零输入分支：无法重新加速、永不进入接受半径、永不广播完成事件。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormationMovementBrakingBandZeroVelocityTest,
    "XTools.Formation.Movement.BrakingBandZeroVelocityResumesInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationMovementBrakingBandZeroVelocityTest::RunTest(const FString& Parameters)
{
    FScopedFormationTestWorld WorldScope(TEXT("FormationMovementTest_BrakeBandZeroVelocity"));
    ACharacter* TestCharacter = nullptr;
    UFormationMovementTestComponent* MoveComp = nullptr;
    if (!SetupMovementTestEnvironment(WorldScope.Get(), TestCharacter, MoveComp))
    {
        AddError(TEXT("测试环境初始化失败"));
        return false;
    }

    UCharacterMovementComponent* CMC = TestCharacter->GetCharacterMovement();
    TestNotNull(TEXT("Character 应持有移动组件"), CMC);
    if (!CMC)
    {
        return false;
    }

    // 目标距离 60 ∈ (50, 75]：接受半径外、制动带（1.5×半径）内；速度归零模拟死锁场景
    CMC->Velocity = FVector::ZeroVector;
    MoveComp->StartMoveToLocation(FVector(60.0f, 0.0f, 0.0f), 50.0f, 1.0f);
    TestTrue(TEXT("接受半径外开始移动应处于移动状态"), MoveComp->IsMoving());

    TickMoveComponentManually(MoveComp, 1.0f / 60.0f);

    // 修复后：速度 ≤ 1 不再进入制动分支，走减速分支恢复指向目标的非零输入
    const FVector PendingInput = CMC->ConsumeInputVector();
    TestTrue(TEXT("制动带内速度归零后应恢复沿目标方向的非零移动输入"),
        PendingInput.X > 0.1f && FMath::IsNearlyZero(PendingInput.Y) && FMath::IsNearlyZero(PendingInput.Z));
    TestTrue(TEXT("恢复输入后应仍处于移动状态"), MoveComp->IsMoving());

    return true;
}

// 反向保护：修复不得削弱正常制动——高速进入制动带时仍应零输入自然减速。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormationMovementBrakingBandHighVelocityTest,
    "XTools.Formation.Movement.BrakingBandHighVelocityStillBrakes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationMovementBrakingBandHighVelocityTest::RunTest(const FString& Parameters)
{
    FScopedFormationTestWorld WorldScope(TEXT("FormationMovementTest_BrakeBandHighVelocity"));
    ACharacter* TestCharacter = nullptr;
    UFormationMovementTestComponent* MoveComp = nullptr;
    if (!SetupMovementTestEnvironment(WorldScope.Get(), TestCharacter, MoveComp))
    {
        AddError(TEXT("测试环境初始化失败"));
        return false;
    }

    UCharacterMovementComponent* CMC = TestCharacter->GetCharacterMovement();
    TestNotNull(TEXT("Character 应持有移动组件"), CMC);
    if (!CMC)
    {
        return false;
    }

    // 速度 600 >> 1：即使距离 60 在制动带内，也应保持零输入自然减速
    CMC->Velocity = FVector(600.0f, 0.0f, 0.0f);
    MoveComp->StartMoveToLocation(FVector(60.0f, 0.0f, 0.0f), 50.0f, 1.0f);

    TickMoveComponentManually(MoveComp, 1.0f / 60.0f);

    const FVector PendingInput = CMC->ConsumeInputVector();
    TestTrue(TEXT("高速进入制动带应保持零输入自然减速"), PendingInput.IsNearlyZero(0.01f));
    TestTrue(TEXT("制动中应仍处于移动状态"), MoveComp->IsMoving());

    return true;
}

// 既有行为保护：开始时已在接受半径内 → 立即广播完成事件且不进入移动状态（不得改变）。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormationMovementStartInsideRadiusTest,
    "XTools.Formation.Movement.StartInsideRadiusCompletesImmediately",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationMovementStartInsideRadiusTest::RunTest(const FString& Parameters)
{
    FScopedFormationTestWorld WorldScope(TEXT("FormationMovementTest_StartInsideRadius"));
    ACharacter* TestCharacter = nullptr;
    UFormationMovementTestComponent* MoveComp = nullptr;
    if (!SetupMovementTestEnvironment(WorldScope.Get(), TestCharacter, MoveComp))
    {
        AddError(TEXT("测试环境初始化失败"));
        return false;
    }

    UFormationMovementTestEventReceiver* Receiver = NewObject<UFormationMovementTestEventReceiver>();
    MoveComp->OnMovementCompleted.AddDynamic(Receiver, &UFormationMovementTestEventReceiver::OnMovementCompleted);

    // 起点到目标距离 30 ≤ 接受半径 50
    MoveComp->StartMoveToLocation(FVector(30.0f, 0.0f, 0.0f), 50.0f, 1.0f);

    TestEqual(TEXT("起点已在接受半径内应立即广播完成事件一次"), Receiver->CompletedCount, 1);
    TestTrue(TEXT("完成事件应携带来源组件"), Receiver->LastComponent == MoveComp);
    TestFalse(TEXT("立即完成不应进入移动状态"), MoveComp->IsMoving());

    return true;
}

// XY 平面距离契约：Z 轴高度差不参与到达判定与距离计算。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormationMovementDistanceIgnoresZTest,
    "XTools.Formation.Movement.DistanceIgnoresZAxis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationMovementDistanceIgnoresZTest::RunTest(const FString& Parameters)
{
    FScopedFormationTestWorld WorldScope(TEXT("FormationMovementTest_DistanceIgnoresZ"));
    ACharacter* TestCharacter = nullptr;
    UFormationMovementTestComponent* MoveComp = nullptr;
    if (!SetupMovementTestEnvironment(WorldScope.Get(), TestCharacter, MoveComp))
    {
        AddError(TEXT("测试环境初始化失败"));
        return false;
    }

    UFormationMovementTestEventReceiver* Receiver = NewObject<UFormationMovementTestEventReceiver>();
    MoveComp->OnMovementCompleted.AddDynamic(Receiver, &UFormationMovementTestEventReceiver::OnMovementCompleted);

    // XY 距离 50，三维距离 ≈ 502.5：只有忽略 Z 才会立即完成
    TestTrue(TEXT("抬升角色位置应成功"), TestCharacter->SetActorLocation(FVector(30.0f, 40.0f, 500.0f)));
    MoveComp->StartMoveToLocation(FVector::ZeroVector, 60.0f, 1.0f);

    TestEqual(TEXT("XY 距离进入接受半径应立即广播完成事件（Z 差不参与判定）"), Receiver->CompletedCount, 1);
    TestTrue(TEXT("距离计算应只按 XY 平面（应为 50）"),
        FMath::IsNearlyEqual(MoveComp->GetDistanceToTarget(), 50.0f));

    return true;
}

// StopMovement 清理语义：速度归零、待处理输入清空、Tick 禁用、退出移动状态。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormationMovementStopClearsStateTest,
    "XTools.Formation.Movement.StopMovementClearsState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationMovementStopClearsStateTest::RunTest(const FString& Parameters)
{
    FScopedFormationTestWorld WorldScope(TEXT("FormationMovementTest_StopClearsState"));
    ACharacter* TestCharacter = nullptr;
    UFormationMovementTestComponent* MoveComp = nullptr;
    if (!SetupMovementTestEnvironment(WorldScope.Get(), TestCharacter, MoveComp))
    {
        AddError(TEXT("测试环境初始化失败"));
        return false;
    }

    UCharacterMovementComponent* CMC = TestCharacter->GetCharacterMovement();
    TestNotNull(TEXT("Character 应持有移动组件"), CMC);
    if (!CMC)
    {
        return false;
    }

    // 远距离目标：正常移动并产生输入
    CMC->Velocity = FVector(100.0f, 0.0f, 0.0f);
    MoveComp->StartMoveToLocation(FVector(500.0f, 0.0f, 0.0f), 50.0f, 1.0f);
    TickMoveComponentManually(MoveComp, 1.0f / 60.0f);
    TestFalse(TEXT("远距离移动应产生移动输入"), CMC->ConsumeInputVector().IsNearlyZero(0.01f));

    // 再积累一份输入用于验证 Stop 的清理
    TickMoveComponentManually(MoveComp, 1.0f / 60.0f);
    TestTrue(TEXT("停止前应处于移动状态"), MoveComp->IsMoving());

    MoveComp->StopMovement();

    TestFalse(TEXT("停止后不应处于移动状态"), MoveComp->IsMoving());
    TestTrue(TEXT("停止后速度应归零"), CMC->Velocity.IsNearlyZero(0.01f));
    TestTrue(TEXT("停止后待处理移动输入应清空"), CMC->ConsumeInputVector().IsNearlyZero(0.01f));
    TestFalse(TEXT("停止后组件Tick应禁用"), MoveComp->IsComponentTickEnabled());

    return true;
}

// Tick 驱动的到达路径：进入接受半径时广播完成事件一次、停止并禁用Tick；到达后的多余Tick不重复广播。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormationMovementReachesRadiusDuringTickTest,
    "XTools.Formation.Movement.ReachingRadiusDuringTickBroadcastsOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationMovementReachesRadiusDuringTickTest::RunTest(const FString& Parameters)
{
    FScopedFormationTestWorld WorldScope(TEXT("FormationMovementTest_ReachesRadiusDuringTick"));
    ACharacter* TestCharacter = nullptr;
    UFormationMovementTestComponent* MoveComp = nullptr;
    if (!SetupMovementTestEnvironment(WorldScope.Get(), TestCharacter, MoveComp))
    {
        AddError(TEXT("测试环境初始化失败"));
        return false;
    }

    UCharacterMovementComponent* CMC = TestCharacter->GetCharacterMovement();
    TestNotNull(TEXT("Character 应持有移动组件"), CMC);
    if (!CMC)
    {
        return false;
    }

    UFormationMovementTestEventReceiver* Receiver = NewObject<UFormationMovementTestEventReceiver>();
    MoveComp->OnMovementCompleted.AddDynamic(Receiver, &UFormationMovementTestEventReceiver::OnMovementCompleted);

    CMC->Velocity = FVector::ZeroVector;
    MoveComp->StartMoveToLocation(FVector(60.0f, 0.0f, 0.0f), 50.0f, 1.0f);
    TickMoveComponentManually(MoveComp, 1.0f / 60.0f);

    // 手动把角色移入接受半径（世界不 Tick，位置由测试显式控制）
    TestTrue(TEXT("移动角色进入接受半径应成功"), TestCharacter->SetActorLocation(FVector(40.0f, 0.0f, 0.0f)));
    TickMoveComponentManually(MoveComp, 1.0f / 60.0f);

    TestEqual(TEXT("进入接受半径应广播完成事件一次"), Receiver->CompletedCount, 1);
    TestTrue(TEXT("完成事件应携带来源组件"), Receiver->LastComponent == MoveComp);
    TestFalse(TEXT("到达后应退出移动状态"), MoveComp->IsMoving());
    TestFalse(TEXT("到达后组件Tick应禁用"), MoveComp->IsComponentTickEnabled());
    TestTrue(TEXT("到达后速度应归零"), CMC->Velocity.IsNearlyZero(0.01f));

    // 到达后的多余Tick不应重复广播（bIsMoving=false 时TickComponent空转）
    TickMoveComponentManually(MoveComp, 1.0f / 60.0f);
    TestEqual(TEXT("到达后重复Tick不应重复广播完成事件"), Receiver->CompletedCount, 1);

    return true;
}

#endif
