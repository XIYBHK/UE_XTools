#include "QueueSplineComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "QueueSplineMovementComponent.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQueueSplinePlanarArrivalTest,
	"XTools.QueueSpline.Component.PlanarArrival",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FQueueSplinePlanarArrivalTest::RunTest(const FString& Parameters)
{
	AActor* QueueOwner = NewObject<AActor>(GetTransientPackage());
	USplineComponent* Spline = NewObject<USplineComponent>(QueueOwner);
	UQueueSplineComponent* Queue = NewObject<UQueueSplineComponent>(QueueOwner);
	AActor* Member = NewObject<AActor>(GetTransientPackage());
	USceneComponent* MemberRoot = NewObject<USceneComponent>(Member);
	Member->SetRootComponent(MemberRoot);
	MemberRoot->SetWorldLocation(FVector(100.0, 0.0, 90.0));

	TArray<FVector> SplinePoints;
	SplinePoints.Add(FVector::ZeroVector);
	SplinePoints.Add(FVector(100.0, 0.0, 0.0));
	Spline->SetSplinePoints(SplinePoints, ESplineCoordinateSpace::Local);

	Queue->SplineComponent = Spline;
	Queue->Settings.SideOffset = 0.0;
	Queue->Settings.SideJitter = 0.0;
	Queue->Settings.DistanceJitter = 0.0;
	Queue->Settings.ArrivalTolerance = 30.0;

	FQueueSplineMemberHandle Handle;
	TestTrue(TEXT("应成功注册测试成员"), Queue->RegisterQueueMember(Member, Handle));
	Queue->UpdateQueueTargets(0.0f);

	FQueueSplineMoveTarget Target;
	TestTrue(TEXT("应能读取测试成员目标"), Queue->GetQueueMoveTarget(Handle, Target));
	TestTrue(TEXT("XY重合时应忽略胶囊中心高度差并判定到达"), Target.bReachedSlot);
	TestTrue(TEXT("XY重合时移动方向应为零"), Target.MoveDirection.IsNearlyZero());
	TestEqual(TEXT("到达距离应为水平距离"), Target.DistanceToTarget, 0.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQueueSplineLateJoinPauseTest,
	"XTools.QueueSpline.Component.LateJoinPause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FQueueSplineLateJoinPauseTest::RunTest(const FString& Parameters)
{
	AActor* QueueOwner = NewObject<AActor>(GetTransientPackage());
	USplineComponent* Spline = NewObject<USplineComponent>(QueueOwner);
	UQueueSplineComponent* Queue = NewObject<UQueueSplineComponent>(QueueOwner);

	TArray<FVector> SplinePoints;
	SplinePoints.Add(FVector::ZeroVector);
	SplinePoints.Add(FVector(1000.0, 0.0, 0.0));
	Spline->SetSplinePoints(SplinePoints, ESplineCoordinateSpace::Local);
	Queue->SplineComponent = Spline;
	Queue->Settings.Spacing = 100.0;

	AActor* ExistingMember = NewObject<AActor>(GetTransientPackage());
	USceneComponent* ExistingRoot = NewObject<USceneComponent>(ExistingMember);
	ExistingMember->SetRootComponent(ExistingRoot);
	ExistingRoot->SetWorldLocation(FVector(500.0, 0.0, 0.0));
	UQueueSplineMovementComponent* ExistingMovement = NewObject<UQueueSplineMovementComponent>(ExistingMember);
	ExistingMember->AddInstanceComponent(ExistingMovement);

	FQueueSplineMemberHandle ExistingHandle;
	TestTrue(TEXT("应成功注册已有成员"), Queue->RegisterQueueMember(ExistingMember, ExistingHandle));
	TestTrue(TEXT("已有成员应能形成暂停队尾"), Queue->SetMemberHandleAndFollowingPaused(ExistingHandle, true));

	AActor* LateMember = NewObject<AActor>(GetTransientPackage());
	USceneComponent* LateRoot = NewObject<USceneComponent>(LateMember);
	LateMember->SetRootComponent(LateRoot);
	LateRoot->SetWorldLocation(FVector(0.0, 0.0, 0.0));
	UQueueSplineMovementComponent* LateMovement = NewObject<UQueueSplineMovementComponent>(LateMember);
	LateMember->AddInstanceComponent(LateMovement);

	FQueueSplineMemberHandle LateHandle;
	TestTrue(TEXT("暂停后应允许新成员注册"), Queue->RegisterQueueMember(LateMember, LateHandle));
	TestFalse(TEXT("后加入成员距离队尾较远时应继续补位"), LateMovement->IsQueueMovementPaused());

	LateRoot->SetWorldLocation(FVector(400.0, 0.0, 0.0));
	Queue->UpdateQueueTargets(0.0f);
	TestTrue(TEXT("后加入成员补到队尾间距后应暂停"), LateMovement->IsQueueMovementPaused());

	TestTrue(TEXT("解除队尾暂停应成功"), Queue->SetMemberHandleAndFollowingPaused(ExistingHandle, false));
	TestFalse(TEXT("解除后后加入成员应继续移动"), LateMovement->IsQueueMovementPaused());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQueueSplineLateJoinBatchPauseTest,
	"XTools.QueueSpline.Component.LateJoinBatchPause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FQueueSplineLateJoinBatchPauseTest::RunTest(const FString& Parameters)
{
	AActor* QueueOwner = NewObject<AActor>(GetTransientPackage());
	USplineComponent* Spline = NewObject<USplineComponent>(QueueOwner);
	UQueueSplineComponent* Queue = NewObject<UQueueSplineComponent>(QueueOwner);

	TArray<FVector> SplinePoints;
	SplinePoints.Add(FVector::ZeroVector);
	SplinePoints.Add(FVector(1000.0, 0.0, 0.0));
	Spline->SetSplinePoints(SplinePoints, ESplineCoordinateSpace::Local);
	Queue->SplineComponent = Spline;
	Queue->Settings.Spacing = 100.0;

	AActor* ExistingMember = NewObject<AActor>(GetTransientPackage());
	USceneComponent* ExistingRoot = NewObject<USceneComponent>(ExistingMember);
	ExistingMember->SetRootComponent(ExistingRoot);
	ExistingRoot->SetWorldLocation(FVector(500.0, 0.0, 0.0));
	UQueueSplineMovementComponent* ExistingMovement = NewObject<UQueueSplineMovementComponent>(ExistingMember);
	ExistingMember->AddInstanceComponent(ExistingMovement);
	FQueueSplineMemberHandle ExistingHandle;
	TestTrue(TEXT("应成功注册已有成员"), Queue->RegisterQueueMember(ExistingMember, ExistingHandle));
	TestTrue(TEXT("已有成员应能形成暂停队尾"), Queue->SetMemberHandleAndFollowingPaused(ExistingHandle, true));

	AActor* FirstLateMember = NewObject<AActor>(GetTransientPackage());
	USceneComponent* FirstLateRoot = NewObject<USceneComponent>(FirstLateMember);
	FirstLateMember->SetRootComponent(FirstLateRoot);
	UQueueSplineMovementComponent* FirstLateMovement = NewObject<UQueueSplineMovementComponent>(FirstLateMember);
	FirstLateMember->AddInstanceComponent(FirstLateMovement);
	FQueueSplineMemberHandle FirstLateHandle;
	TestTrue(TEXT("应成功注册第一位后加入成员"), Queue->RegisterQueueMember(FirstLateMember, FirstLateHandle));

	AActor* SecondLateMember = NewObject<AActor>(GetTransientPackage());
	USceneComponent* SecondLateRoot = NewObject<USceneComponent>(SecondLateMember);
	SecondLateMember->SetRootComponent(SecondLateRoot);
	UQueueSplineMovementComponent* SecondLateMovement = NewObject<UQueueSplineMovementComponent>(SecondLateMember);
	SecondLateMember->AddInstanceComponent(SecondLateMovement);
	FQueueSplineMemberHandle SecondLateHandle;
	TestTrue(TEXT("应成功注册第二位后加入成员"), Queue->RegisterQueueMember(SecondLateMember, SecondLateHandle));

	TestFalse(TEXT("第一位后加入成员应继续补位"), FirstLateMovement->IsQueueMovementPaused());
	TestFalse(TEXT("同点生成的第二位成员不应被第一位阻塞"), SecondLateMovement->IsQueueMovementPaused());

	FirstLateRoot->SetWorldLocation(FVector(400.0, 0.0, 0.0));
	SecondLateRoot->SetWorldLocation(FVector(250.0, 0.0, 0.0));
	Queue->UpdateQueueTargets(0.0f);
	TestTrue(TEXT("第一位成员接近暂停队尾后应暂停"), FirstLateMovement->IsQueueMovementPaused());
	TestFalse(TEXT("第二位成员距离新队尾较远时应继续补位"), SecondLateMovement->IsQueueMovementPaused());

	SecondLateRoot->SetWorldLocation(FVector(300.0, 0.0, 0.0));
	Queue->UpdateQueueTargets(0.0f);
	TestTrue(TEXT("第二位成员补到间距后应暂停"), SecondLateMovement->IsQueueMovementPaused());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQueueSplineInitialSpawnTransformTest,
	"XTools.QueueSpline.Component.InitialSpawnTransforms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FQueueSplineInitialSpawnTransformTest::RunTest(const FString& Parameters)
{
	AActor* QueueOwner = NewObject<AActor>(GetTransientPackage());
	USplineComponent* Spline = NewObject<USplineComponent>(QueueOwner);
	UQueueSplineComponent* Queue = NewObject<UQueueSplineComponent>(QueueOwner);

	TArray<FVector> SplinePoints;
	SplinePoints.Add(FVector::ZeroVector);
	SplinePoints.Add(FVector(1000.0, 0.0, 0.0));
	Spline->SetSplinePoints(SplinePoints, ESplineCoordinateSpace::Local);

	Queue->SplineComponent = Spline;
	Queue->Settings.FillMode = EQueueSplineFillMode::PreFilledRatio;
	Queue->Settings.FillRatio = 0.5;
	Queue->Settings.Spacing = 100.0;
	Queue->Settings.SideOffset = 40.0;
	Queue->Settings.SideJitter = 0.0;
	Queue->Settings.DistanceJitter = 0.0;

	const TArray<FTransform> PrefilledTransforms = Queue->GenerateInitialSpawnTransforms(3);
	TestEqual(TEXT("预填充应返回请求数量的Transform"), PrefilledTransforms.Num(), 3);
	if (PrefilledTransforms.Num() == 3)
	{
		TestEqual(TEXT("队头位于预填充比例"), PrefilledTransforms[0].GetLocation().X, 500.0);
		TestEqual(TEXT("后续成员按间距向入口方向排列"), PrefilledTransforms[1].GetLocation().X, 400.0);
		TestEqual(TEXT("预填充第一个成员应用右侧横向偏移"), PrefilledTransforms[0].GetLocation().Y, 40.0);
		TestEqual(TEXT("预填充成员左右交错"), PrefilledTransforms[1].GetLocation().Y, -40.0);
	}

	Queue->Settings.FillMode = EQueueSplineFillMode::FromStart;
	Queue->Settings.EntryDistance = 25.0;
	const TArray<FTransform> EntryTransforms = Queue->GenerateInitialSpawnTransforms(3);
	TestEqual(TEXT("从入口模式只返回一个入口Transform"), EntryTransforms.Num(), 1);
	if (EntryTransforms.Num() == 1)
	{
		TestEqual(TEXT("入口Transform位于起始距离"), EntryTransforms[0].GetLocation().X, 25.0);
		TestEqual(TEXT("入口Transform应用横向偏移"), EntryTransforms[0].GetLocation().Y, 40.0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQueueSplineContinuousFollowTargetTest,
	"XTools.QueueSpline.Component.ContinuousFollowTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FQueueSplineContinuousFollowTargetTest::RunTest(const FString& Parameters)
{
	AActor* QueueOwner = NewObject<AActor>(GetTransientPackage());
	USplineComponent* Spline = NewObject<USplineComponent>(QueueOwner);
	UQueueSplineComponent* Queue = NewObject<UQueueSplineComponent>(QueueOwner);
	AActor* Member = NewObject<AActor>(GetTransientPackage());
	USceneComponent* MemberRoot = NewObject<USceneComponent>(Member);
	Member->SetRootComponent(MemberRoot);
	MemberRoot->SetWorldLocation(FVector(100.0, 0.0, 0.0));

	TArray<FVector> SplinePoints;
	SplinePoints.Add(FVector::ZeroVector);
	SplinePoints.Add(FVector(1000.0, 0.0, 0.0));
	Spline->SetSplinePoints(SplinePoints, ESplineCoordinateSpace::Local);

	Queue->SplineComponent = Spline;
	Queue->Settings.FillMode = EQueueSplineFillMode::FromStart;
	Queue->Settings.ExitLookAheadDistance = 150.0;
	Queue->Settings.ArrivalTolerance = 30.0;
	Queue->Settings.SideOffset = 0.0;
	Queue->Settings.SideJitter = 0.0;
	Queue->Settings.DistanceJitter = 0.0;

	FQueueSplineMemberHandle Handle;
	TestTrue(TEXT("应成功注册测试成员"), Queue->RegisterQueueMember(Member, Handle));
	Queue->UpdateQueueTargets(0.0f);

	FQueueSplineMoveTarget Target;
	TestTrue(TEXT("应能读取连续跟随目标"), Queue->GetQueueMoveTarget(Handle, Target));
	TestEqual(TEXT("目标应是前视距离而不是静态槽位"), Target.Slot.Distance, 250.0);
	TestFalse(TEXT("前视点不是样条终点，不应触发到达"), Target.bReachedSlot);

	MemberRoot->SetWorldLocation(FVector(1000.0, 0.0, 0.0));
	Queue->UpdateQueueTargets(0.0f);
	TestTrue(TEXT("到达真实样条终点后才触发到达"), Queue->GetQueueMoveTarget(Handle, Target) && Target.bReachedSlot);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQueueSplinePausePropagationTest,
	"XTools.QueueSpline.Component.PausePropagation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FQueueSplinePausePropagationTest::RunTest(const FString& Parameters)
{
	AActor* QueueOwner = NewObject<AActor>(GetTransientPackage());
	USplineComponent* Spline = NewObject<USplineComponent>(QueueOwner);
	UQueueSplineComponent* Queue = NewObject<UQueueSplineComponent>(QueueOwner);

	TArray<FVector> SplinePoints;
	SplinePoints.Add(FVector::ZeroVector);
	SplinePoints.Add(FVector(1000.0, 0.0, 0.0));
	Spline->SetSplinePoints(SplinePoints, ESplineCoordinateSpace::Local);
	Queue->SplineComponent = Spline;

	AActor* Members[3];
	UQueueSplineMovementComponent* Movements[3];
	FQueueSplineMemberHandle Handles[3];
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Members[Index] = NewObject<AActor>(GetTransientPackage());
		Movements[Index] = NewObject<UQueueSplineMovementComponent>(Members[Index]);
		Members[Index]->AddInstanceComponent(Movements[Index]);
		TestTrue(TEXT("应成功注册传播测试成员"), Queue->RegisterQueueMember(Members[Index], Handles[Index]));
	}

	TestTrue(TEXT("暂停中间成员应成功"), Queue->SetMemberHandleAndFollowingPaused(Handles[1], true));
	TestFalse(TEXT("前方成员不应暂停"), Movements[0]->IsQueueMovementPaused());
	TestTrue(TEXT("请求暂停的成员应暂停"), Movements[1]->IsQueueMovementPaused());
	TestTrue(TEXT("后方成员应跟随暂停"), Movements[2]->IsQueueMovementPaused());

	TestTrue(TEXT("后方成员可形成独立停靠状态"), Queue->SetMemberHandleAndFollowingPaused(Handles[2], true));
	TestTrue(TEXT("解除中间成员暂停应成功"), Queue->SetMemberHandleAndFollowingPaused(Handles[1], false));
	TestFalse(TEXT("解除后中间成员应恢复"), Movements[1]->IsQueueMovementPaused());
	TestTrue(TEXT("拥有独立停靠状态的后方成员仍应暂停"), Movements[2]->IsQueueMovementPaused());

	return true;
}

#endif
