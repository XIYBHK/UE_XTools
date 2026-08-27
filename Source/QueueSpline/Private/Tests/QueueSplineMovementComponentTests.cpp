#include "QueueSplineMovementComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQueueSplineMovementArrivalLatchTest,
	"XTools.QueueSpline.Movement.ArrivalLatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FQueueSplineMovementArrivalLatchTest::RunTest(const FString& Parameters)
{
	UQueueSplineMovementComponent* Component = NewObject<UQueueSplineMovementComponent>();
	Component->MovementMode = EQueueSplineMovementMode::AddMovementInput;
	Component->bAutoMove = true;

	FQueueSplineMoveTarget Target;
	Target.Handle.Id = FGuid::NewGuid();
	Target.Handle.Generation = 1;
	Target.Slot.SlotIndex = 2;
	Target.Slot.TargetLocation = FVector(100.0, 0.0, 0.0);
	Target.MoveDirection = FVector::ForwardVector;
	Component->SetQueueMoveTarget(Target);
	TestFalse(TEXT("新目标开始时未到达"), Component->GetQueueMoveTarget().bReachedSlot);

	Target.bReachedSlot = true;
	Component->SetQueueMoveTarget(Target);
	TestTrue(TEXT("进入容差后锁存到达状态"), Component->GetQueueMoveTarget().bReachedSlot);
	TestTrue(TEXT("到达后清除移动方向"), Component->GetQueueMoveTarget().MoveDirection.IsNearlyZero());

	Target.bReachedSlot = false;
	Target.MoveDirection = -FVector::ForwardVector;
	Component->SetQueueMoveTarget(Target);
	TestTrue(TEXT("同一目标越界后仍保持到达"), Component->GetQueueMoveTarget().bReachedSlot);
	TestTrue(TEXT("同一目标不会恢复反向输入"), Component->GetQueueMoveTarget().MoveDirection.IsNearlyZero());

	Target.Slot.TargetLocation.Z = 90.0;
	Component->SetQueueMoveTarget(Target);
	TestTrue(TEXT("同一地面槽位的高度变化不会解除锁存"), Component->GetQueueMoveTarget().bReachedSlot);

	Component->bAutoMove = false;
	Component->SetQueueMoveTarget(Target);
	TestFalse(TEXT("关闭自动移动后清除到达锁存"), Component->GetQueueMoveTarget().bReachedSlot);
	Component->bAutoMove = true;

	Target.Slot.SlotIndex = 1;
	Target.Slot.TargetLocation = FVector(200.0, 0.0, 0.0);
	Target.MoveDirection = FVector::ForwardVector;
	Component->SetQueueMoveTarget(Target);
	TestFalse(TEXT("槽位变化后解除到达锁存"), Component->GetQueueMoveTarget().bReachedSlot);
	TestTrue(TEXT("槽位变化后恢复移动方向"), Component->GetQueueMoveTarget().MoveDirection.Equals(FVector::ForwardVector));

	Target.bReachedSlot = true;
	Component->SetQueueMoveTarget(Target);
	Component->StopQueueMovement();
	Target.bReachedSlot = false;
	Target.MoveDirection = FVector::ForwardVector;
	Component->SetQueueMoveTarget(Target);
	TestFalse(TEXT("显式停止后不保留到达锁存"), Component->GetQueueMoveTarget().bReachedSlot);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQueueSplineMovementPauseTest,
	"XTools.QueueSpline.Movement.PauseResume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FQueueSplineMovementPauseTest::RunTest(const FString& Parameters)
{
	UQueueSplineMovementComponent* Component = NewObject<UQueueSplineMovementComponent>();
	TestFalse(TEXT("初始不应暂停"), Component->IsQueueMovementPaused());

	Component->SetQueueMovementPaused(true);
	TestTrue(TEXT("应能暂停单个成员移动"), Component->IsQueueMovementPaused());
	Component->SetQueueMovementPaused(false);
	TestFalse(TEXT("应能恢复单个成员移动"), Component->IsQueueMovementPaused());

	return true;
}

#endif
