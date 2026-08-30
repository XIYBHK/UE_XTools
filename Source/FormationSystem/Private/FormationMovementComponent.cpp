#include "FormationMovementComponent.h"
#include "FormationLog.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"

UFormationMovementComponent::UFormationMovementComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false; // 默认不启用Tick，只在移动时启用

    TargetLocation = FVector::ZeroVector;
    AcceptanceRadius = 50.0f;
    MoveSpeed = 1.0f;
    SlowDownDistanceMultiplier = 3.0f;
    MinSlowDownSpeed = 0.2f;
    RotationSpeed = 8.0f;
    bEnableSlowDown = true;
    bStopWhenStuck = false;
    StuckTimeSeconds = 2.0f;
    bIsMoving = false;
    OwnerCharacter = nullptr;
    BestDistanceToTarget = 0.0f;
    NoProgressElapsed = 0.0f;
    bBrakeReleased = false;
}

void UFormationMovementComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // 缓存Owner Character
    OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter)
    {
        UE_LOG(LogFormationSystem, Warning, TEXT("FormationMovementComponent: Owner不是Character类型"));
    }
}

void UFormationMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    if (bIsMoving)
    {
        UpdateMovement(DeltaTime);
    }
}

void UFormationMovementComponent::StartMoveToLocation(FVector InTargetLocation, float InAcceptanceRadius, float InMoveSpeed)
{
    const bool bTargetIsFinite =
        FMath::IsFinite(InTargetLocation.X) &&
        FMath::IsFinite(InTargetLocation.Y) &&
        FMath::IsFinite(InTargetLocation.Z);
    if (!bTargetIsFinite || !FMath::IsFinite(InAcceptanceRadius) || !FMath::IsFinite(InMoveSpeed))
    {
        UE_LOG(LogFormationSystem, Warning,
            TEXT("FormationMovementComponent: 移动请求包含非有限参数，保持当前移动状态"));
        return;
    }

    // 动态组件可能在 Owner BeginPlay 前注册，此时 BeginPlay 尚未缓存 Character；
    // Outer 已固定为 Owner，按需解析可避免注册后立即下令时丢失移动指令。
    if (!OwnerCharacter)
    {
        OwnerCharacter = Cast<ACharacter>(GetOwner());
    }

    if (!OwnerCharacter)
    {
        UE_LOG(LogFormationSystem, Warning, TEXT("FormationMovementComponent: 无效的Character"));
        return;
    }

    TargetLocation = InTargetLocation;
    AcceptanceRadius = FMath::Max(1.0f, InAcceptanceRadius);
    MoveSpeed = FMath::Clamp(InMoveSpeed, 0.1f, 2.0f);
    
    // 检查是否已经在目标位置
    if (HasReachedTarget())
    {
        UE_LOG(LogFormationSystem, Log, TEXT("FormationMovementComponent: 已在目标位置"));
        // 已到达目标的提前返回路径同样广播完成事件（与 UpdateMovement 正常到达路径
        // 保持一致的参数与时机语义），避免依赖完成事件的调用方永久挂起
        OnMovementCompleted.Broadcast(this);
        return;
    }
    
    bIsMoving = true;
    // 卡住检测状态复位：以起点距离为基准，移动中重定向（再次调用本函数）同样重新计程
    BestDistanceToTarget = GetDistanceToTarget();
    NoProgressElapsed = 0.0f;
    // 制动迟滞状态复位：新目标从一开始即按常规制动逻辑判定
    bBrakeReleased = false;
    SetComponentTickEnabled(true);
    
    // 简化日志输出 - 只在VeryVerbose级别输出详细信息
    UE_LOG(LogFormationSystem, VeryVerbose, TEXT("FormationMovementComponent: 开始移动到位置 %s"), *TargetLocation.ToString());
}

void UFormationMovementComponent::StopMovement()
{
    bIsMoving = false;
    SetComponentTickEnabled(false);

    // 确保清除任何残留的移动输入和状态
    if (OwnerCharacter)
    {
        // 清除移动输入
        OwnerCharacter->AddMovementInput(FVector::ZeroVector, 0.0f);

        // 如果Character有移动组件，完全停止移动
        if (UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement())
        {
            MovementComp->StopMovementImmediately();
            MovementComp->Velocity = FVector::ZeroVector;

            // 清除任何待处理的移动输入
            MovementComp->ConsumeInputVector();
        }
    }

    UE_LOG(LogFormationSystem, VeryVerbose, TEXT("FormationMovementComponent: 完全停止移动并清除所有移动状态"));
}

float UFormationMovementComponent::GetDistanceToTarget() const
{
    if (!OwnerCharacter)
    {
        return -1.0f;
    }

    // 只计算水平面距离，忽略Z轴高度差异
    FVector CurrentLocation = OwnerCharacter->GetActorLocation();
    FVector CurrentLocation2D = FVector(CurrentLocation.X, CurrentLocation.Y, 0.0f);
    FVector TargetLocation2D = FVector(TargetLocation.X, TargetLocation.Y, 0.0f);

    return FVector::Dist(CurrentLocation2D, TargetLocation2D);
}

void UFormationMovementComponent::UpdateMovement(float DeltaTime)
{
    if (!OwnerCharacter)
    {
        StopMovement();
        return;
    }

    UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
    if (!MovementComp)
    {
        StopMovement();
        return;
    }

    FVector CurrentLocation = OwnerCharacter->GetActorLocation();
    float DistanceToTarget = GetDistanceToTarget();

    // 获取当前速度（只考虑水平面）
    FVector CurrentVelocity = MovementComp->Velocity;
    CurrentVelocity.Z = 0.0f;
    float CurrentSpeed = CurrentVelocity.Size();

    // 计算制动距离（基于当前速度和制动减速度）
    float BrakingDeceleration = MovementComp->GetMaxBrakingDeceleration();
    float BrakingDistance = 0.0f;
    if (BrakingDeceleration > 0.0f && CurrentSpeed > 0.0f)
    {
        // 制动距离 = v²/(2*a) + 安全余量
        BrakingDistance = (CurrentSpeed * CurrentSpeed) / (2.0f * BrakingDeceleration) + AcceptanceRadius * 0.5f;
    }

    // 检查是否应该开始制动
    bool bShouldBrake = DistanceToTarget <= FMath::Max(BrakingDistance, AcceptanceRadius * 1.5f);

    // 检查是否已经到达目标
    if (DistanceToTarget <= AcceptanceRadius)
    {
        // 立即停止移动输入并清除所有移动状态
        OwnerCharacter->AddMovementInput(FVector::ZeroVector, 0.0f);
        MovementComp->StopMovementImmediately();
        MovementComp->Velocity = FVector::ZeroVector;

        StopMovement();
        OnMovementCompleted.Broadcast(this);

        UE_LOG(LogFormationSystem, VeryVerbose, TEXT("FormationMovementComponent: 到达目标位置，距离=%.2f"), DistanceToTarget);
        return;
    }

    // 可选卡住检测：连续无有效距离进展达到 StuckTimeSeconds 时，按"停止移动"语义终止
    // （清理速度/输入、禁用Tick），但不广播完成事件——完成事件仅表示进入接受半径。
    // 放在制动提前返回之前，确保任何分支下无进展都会被累计。
    if (bStopWhenStuck && CheckStuck(DistanceToTarget, DeltaTime))
    {
        UE_LOG(LogFormationSystem, Log, TEXT("FormationMovementComponent: 连续 %.2f 秒无有效距离进展（距目标 %.2f cm），判定卡住并停止"),
            FMath::Clamp(StuckTimeSeconds, 0.5f, 30.0f), DistanceToTarget);
        StopMovement();
        return;
    }

    // 计算移动方向（只在水平面，忽略Z轴差异）
    FVector DirectionToTarget = TargetLocation - CurrentLocation;
    DirectionToTarget.Z = 0.0f;
    FVector Direction = DirectionToTarget.GetSafeNormal();

    // 如果方向无效，停止移动
    if (Direction.IsNearlyZero())
    {
        UE_LOG(LogFormationSystem, Warning, TEXT("FormationMovementComponent: 无效的移动方向，停止移动"));
        StopMovement();
        return;
    }

    // 计算速度倍数
    float SpeedMultiplier = 1.0f;

    // 制动迟滞：制动带内停止输入自然减速；速度跌破 BrakeReleaseSpeed 时释放制动恢复
    // 输入（保持原"速度归零可恢复"语义）；恢复后须重新加速到更高的 BrakeReEnterSpeed
    // 才会再次进入制动。单一阈值在速度逐帧穿越其两侧时会形成"走-停"振荡蠕行，
    // 双阈值迟滞把振荡域推开，使减速表现为连续的减速-滑行。
    constexpr float BrakeReleaseSpeed = 1.0f;   // cm/s：释放制动的速度下限
    constexpr float BrakeReEnterSpeed = 20.0f;  // cm/s：释放后重新进入制动的加速下限

    if (bShouldBrake && !bBrakeReleased)
    {
        if (CurrentSpeed <= BrakeReleaseSpeed)
        {
            bBrakeReleased = true;
            UE_LOG(LogFormationSystem, VeryVerbose,
                TEXT("FormationMovementComponent: 速度=%.2fcm/s 跌至 %.1f 以下，释放制动恢复输入"), CurrentSpeed, BrakeReleaseSpeed);
        }
        else
        {
            // 进入制动阶段，停止输入让角色自然减速
            OwnerCharacter->AddMovementInput(FVector::ZeroVector, 0.0f);

            UE_LOG(LogFormationSystem, VeryVerbose, TEXT("FormationMovementComponent: 制动中，距离=%.2f，制动距离=%.2f，当前速度=%.2f"),
                   DistanceToTarget, BrakingDistance, CurrentSpeed);

            // 不执行旋转，让角色自然停止
            return;
        }
    }

    if (bShouldBrake && bBrakeReleased && CurrentSpeed >= BrakeReEnterSpeed)
    {
        bBrakeReleased = false;

        OwnerCharacter->AddMovementInput(FVector::ZeroVector, 0.0f);
        UE_LOG(LogFormationSystem, VeryVerbose,
            TEXT("FormationMovementComponent: 恢复后速度回升至 %.2fcm/s（>= %.1f），重新进入制动"), CurrentSpeed, BrakeReEnterSpeed);
        return;
    }
    else if (bEnableSlowDown)
    {
        // 正常减速阶段
        float SlowDownDistance = AcceptanceRadius * SlowDownDistanceMultiplier;

        if (DistanceToTarget <= SlowDownDistance)
        {
            // 使用类似"限制映射"的方式：将距离映射到速度倍数
            float ClampedDistance = FMath::Clamp(DistanceToTarget, AcceptanceRadius, SlowDownDistance);
            float Alpha = (ClampedDistance - AcceptanceRadius) / (SlowDownDistance - AcceptanceRadius);
            SpeedMultiplier = FMath::Lerp(MinSlowDownSpeed, 1.0f, Alpha);

            UE_LOG(LogFormationSystem, VeryVerbose, TEXT("FormationMovementComponent: 减速中，距离=%.2f，Alpha=%.2f，速度倍数=%.2f"),
                   DistanceToTarget, Alpha, SpeedMultiplier);
        }
    }

    // 使用AddMovementInput添加移动输入，应用减速
    float FinalMoveSpeed = MoveSpeed * SpeedMultiplier;
    OwnerCharacter->AddMovementInput(Direction, FinalMoveSpeed);

    // 让Character面向移动方向（只在水平面旋转）
    if (SpeedMultiplier > 0.01f)
    {
        FRotator TargetRotation = FRotator(0.0f, Direction.Rotation().Yaw, 0.0f);
        FRotator CurrentRotation = OwnerCharacter->GetActorRotation();

        float YawDifference = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, TargetRotation.Yaw));
        if (YawDifference > 1.0f)
        {
            FRotator NewRotation = FMath::RInterpTo(
                FRotator(CurrentRotation.Pitch, CurrentRotation.Yaw, CurrentRotation.Roll),
                FRotator(CurrentRotation.Pitch, TargetRotation.Yaw, CurrentRotation.Roll),
                DeltaTime,
                RotationSpeed
            );

            OwnerCharacter->SetActorRotation(NewRotation);
        }
    }
}

bool UFormationMovementComponent::HasReachedTarget() const
{
    if (!OwnerCharacter)
    {
        return false;
    }

    float DistanceToTarget = GetDistanceToTarget();
    return DistanceToTarget <= AcceptanceRadius;
}

bool UFormationMovementComponent::CheckStuck(float DistanceToTarget, float DeltaTime)
{
    if (DistanceToTarget < BestDistanceToTarget - StuckProgressEpsilon)
    {
        // 有有效进展：刷新历史最优距离并清零计时
        BestDistanceToTarget = DistanceToTarget;
        NoProgressElapsed = 0.0f;
        return false;
    }

    NoProgressElapsed += DeltaTime;
    // StuckTimeSeconds 是 EditAnywhere/BlueprintReadWrite，蓝图运行时写入不受 meta Clamp 约束，判定处统一钳制
    return NoProgressElapsed >= FMath::Clamp(StuckTimeSeconds, 0.5f, 30.0f);
}
