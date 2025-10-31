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
    bIsMoving = false;
    OwnerCharacter = nullptr;
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
        return;
    }
    
    bIsMoving = true;
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

    if (bShouldBrake)
    {
        // 进入制动阶段，停止输入让角色自然减速
        OwnerCharacter->AddMovementInput(FVector::ZeroVector, 0.0f);

        UE_LOG(LogFormationSystem, VeryVerbose, TEXT("FormationMovementComponent: 制动中，距离=%.2f，制动距离=%.2f，当前速度=%.2f"),
               DistanceToTarget, BrakingDistance, CurrentSpeed);

        // 不执行旋转，让角色自然停止
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
