#include "QueueSplineMovementComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "QueueSplineComponent.h"
#include "QueueSplineLog.h"
#include "XToolsErrorReporter.h"

namespace
{
	constexpr float MoveGoalLocationTolerance = 1.0f;
}

UQueueSplineMovementComponent::UQueueSplineMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UQueueSplineMovementComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UQueueSplineMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!UsesArrivalLatch())
	{
		bArrivalLatched = false;
	}

	if (bQueueMovementPaused || !bAutoMove || !bHasTarget)
	{
		return;
	}

	if (MovementMode == EQueueSplineMovementMode::AddMovementInput)
	{
		if (!bArrivalLatched)
		{
			ApplyAddMovementInput();
		}
	}
	else if (MovementMode == EQueueSplineMovementMode::DirectInterp)
	{
		ApplyDirectInterp(DeltaTime);
	}
}

void UQueueSplineMovementComponent::SetQueueMoveTarget(const FQueueSplineMoveTarget& NewTarget)
{
	if (!NewTarget.Handle.IsValid())
	{
		StopQueueMovement();
		return;
	}

	const bool bHadTarget = bHasTarget;
	const FQueueSplineMoveTarget PreviousTarget = CurrentTarget;
	const bool bChangedMember = bHadTarget && !(PreviousTarget.Handle == NewTarget.Handle);
	const bool bChangedGoal = bHadTarget && !IsSameMoveGoal(PreviousTarget, NewTarget);
	const bool bUseArrivalLatch = UsesArrivalLatch();

	if (bChangedMember)
	{
		OnQueueMoveStopped.Broadcast(PreviousTarget);
	}

	if (!bUseArrivalLatch || !bHadTarget || bChangedGoal)
	{
		bArrivalLatched = false;
	}

	CurrentTarget = NewTarget;
	bHasTarget = true;
	if (bUseArrivalLatch)
	{
		if (bArrivalLatched)
		{
			CurrentTarget.bReachedSlot = true;
			CurrentTarget.MoveDirection = FVector::ZeroVector;
		}
		else if (CurrentTarget.bReachedSlot)
		{
			bArrivalLatched = true;
			CurrentTarget.MoveDirection = FVector::ZeroVector;
			StopAddMovementInputMotion();
		}
	}

	if (!bHadTarget || bChangedMember)
	{
		if (CurrentTarget.bReachedSlot)
		{
			OnQueueMoveArrived.Broadcast(CurrentTarget);
		}
		else
		{
			OnQueueMoveStarted.Broadcast(CurrentTarget);
		}
		return;
	}

	if (!PreviousTarget.bReachedSlot && CurrentTarget.bReachedSlot)
	{
		OnQueueMoveArrived.Broadcast(CurrentTarget);
	}
	else if (PreviousTarget.bReachedSlot && !CurrentTarget.bReachedSlot)
	{
		OnQueueMoveStarted.Broadcast(CurrentTarget);
	}
}

void UQueueSplineMovementComponent::StopQueueMovement()
{
	bArrivalLatched = false;

	if (!bHasTarget)
	{
		return;
	}

	const FQueueSplineMoveTarget StoppedTarget = CurrentTarget;
	bHasTarget = false;
	CurrentTarget = FQueueSplineMoveTarget();
	if (bHasAppliedMovementInput)
	{
		StopAddMovementInputMotion();
	}
	OnQueueMoveStopped.Broadcast(StoppedTarget);
}

void UQueueSplineMovementComponent::SetQueueMovementPaused(bool bPaused)
{
	if (QueueOwner.IsValid()
		&& QueueOwner->SetMemberAndFollowingPaused(GetOwner(), bPaused))
	{
		return;
	}

	ApplyQueueMovementPaused(bPaused);
}

void UQueueSplineMovementComponent::SetQueueOwner(UQueueSplineComponent* NewQueueOwner)
{
	QueueOwner = NewQueueOwner;
}

void UQueueSplineMovementComponent::ApplyQueueMovementPaused(bool bPaused)
{
	if (bQueueMovementPaused == bPaused)
	{
		return;
	}

	bQueueMovementPaused = bPaused;
	if (bQueueMovementPaused && bHasAppliedMovementInput)
	{
		StopAddMovementInputMotion();
	}
}

bool UQueueSplineMovementComponent::IsQueueMovementPaused() const
{
	return bQueueMovementPaused;
}

FQueueSplineMoveTarget UQueueSplineMovementComponent::GetQueueMoveTarget() const
{
	return CurrentTarget;
}

bool UQueueSplineMovementComponent::HasQueueMoveTarget() const
{
	return bHasTarget;
}

bool UQueueSplineMovementComponent::UsesArrivalLatch() const
{
	return bAutoMove && MovementMode == EQueueSplineMovementMode::AddMovementInput;
}

bool UQueueSplineMovementComponent::IsSameMoveGoal(
	const FQueueSplineMoveTarget& First,
	const FQueueSplineMoveTarget& Second)
{
	return First.Handle == Second.Handle
		&& First.Phase == Second.Phase
		&& First.Slot.SlotIndex == Second.Slot.SlotIndex
		&& FVector::DistSquared2D(First.Slot.TargetLocation, Second.Slot.TargetLocation)
			<= FMath::Square(MoveGoalLocationTolerance);
}

void UQueueSplineMovementComponent::StopAddMovementInputMotion()
{
	bHasAppliedMovementInput = false;

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	OwnerPawn->ConsumeMovementInputVector();
	if (UPawnMovementComponent* MovementComponent = OwnerPawn->GetMovementComponent())
	{
		MovementComponent->StopMovementImmediately();
	}
}

APawn* UQueueSplineMovementComponent::ResolveMovementInputPawn()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		if (!bWarnedInvalidMovementInputOwner)
		{
			bWarnedInvalidMovementInputOwner = true;
			XTOOLS_LOG_WARNING(LogQueueSpline, TEXT("AddMovementInput 模式要求组件所属Actor是 Pawn 或 Character。"));
		}
		return nullptr;
	}

	if (!OwnerPawn->GetController() && bAutoSpawnDefaultController && !bTriedAutoSpawnDefaultController)
	{
		bTriedAutoSpawnDefaultController = true;
		OwnerPawn->SpawnDefaultController();
	}

	if (!OwnerPawn->GetController())
	{
		if (!bWarnedMissingController)
		{
			bWarnedMissingController = true;
			XTOOLS_LOG_WARNING(LogQueueSpline, TEXT("AddMovementInput 模式下 Pawn 没有 Controller。Spawn 出来的 Character 请将 AutoPossessAI 设为 PlacedInWorldOrSpawned，或手动 Possess，或开启自动生成默认控制器。"));
		}
		return nullptr;
	}

	return OwnerPawn;
}

void UQueueSplineMovementComponent::ApplyAddMovementInput()
{
	APawn* OwnerPawn = ResolveMovementInputPawn();
	if (!OwnerPawn)
	{
		return;
	}

	OwnerPawn->AddMovementInput(CurrentTarget.MoveDirection, MovementInputScale);
	bHasAppliedMovementInput = true;
}

void UQueueSplineMovementComponent::ApplyDirectInterp(float DeltaTime)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || DeltaTime <= 0.0f)
	{
		return;
	}

	const FVector NewLocation = FMath::VInterpTo(
		OwnerActor->GetActorLocation(),
		CurrentTarget.Slot.TargetLocation,
		DeltaTime,
		DirectInterpSpeed);
	const FRotator NewRotation = FMath::RInterpTo(
		OwnerActor->GetActorRotation(),
		CurrentTarget.Slot.TargetRotation,
		DeltaTime,
		RotationInterpSpeed);

	OwnerActor->SetActorLocationAndRotation(NewLocation, NewRotation);
}
