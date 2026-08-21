#include "QueueSplineComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "QueueSplineMovementComponent.h"
#include "QueueSplineTestObjects.h"
#include "UObject/Package.h"
#include "UObject/ScriptDelegates.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQueueSplineInitialSpawnAutoBindTest,
	"XTools.QueueSpline.Component.InitialSpawnAutoBind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FQueueSplineInitialSpawnAutoBindTest::RunTest(const FString& Parameters)
{
	AActor* QueueOwner = NewObject<AActor>(GetTransientPackage());
	USplineComponent* Spline = NewObject<USplineComponent>(QueueOwner);
	QueueOwner->AddInstanceComponent(Spline);
	// 故意不设置 Queue->SplineComponent，验证自动绑定所属 Actor 上的样条
	UQueueSplineComponent* Queue = NewObject<UQueueSplineComponent>(QueueOwner);

	TArray<FVector> SplinePoints;
	SplinePoints.Add(FVector::ZeroVector);
	SplinePoints.Add(FVector(1000.0, 0.0, 0.0));
	Spline->SetSplinePoints(SplinePoints, ESplineCoordinateSpace::Local);

	Queue->Settings.FillMode = EQueueSplineFillMode::PreFilledRatio;
	Queue->Settings.FillRatio = 0.5;
	Queue->Settings.Spacing = 100.0;
	Queue->Settings.SideOffset = 0.0;
	Queue->Settings.SideJitter = 0.0;
	Queue->Settings.DistanceJitter = 0.0;

	const TArray<FTransform> Transforms = Queue->GenerateInitialSpawnTransforms(3);
	TestEqual(TEXT("未手动设置样条时应自动绑定并返回请求数量"), Transforms.Num(), 3);
	if (Transforms.Num() == 3)
	{
		TestEqual(TEXT("自动绑定的队头位于预填充比例"), Transforms[0].GetLocation().X, 500.0);
	}

	AActor* EmptyOwner = NewObject<AActor>(GetTransientPackage());
	UQueueSplineComponent* EmptyQueue = NewObject<UQueueSplineComponent>(EmptyOwner);
	TestEqual(TEXT("无样条可用时应返回空数组并告警"), EmptyQueue->GenerateInitialSpawnTransforms(3).Num(), 0);

	return true;
}

// ---------------------------------------------------------------------------
// 通知缓冲复用与重入隔离（分配优化回归测试）
//
// 订阅方式：使用 UQueueSplineTargetEventRecorder（见 QueueSplineTestObjects.h），
// 其 UFUNCTION 签名与 FQueueSplineMemberTargetEvent 完全一致（Handle + Target），
// 绑定后走标准反射调用路径，不依赖参数布局巧合。
// 广播次数以通知快照条数与 Recorder 记录序列等价验证（派发循环对每条有效通知恰好广播一次）。
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQueueSplineNotificationPerMemberTest,
	"XTools.QueueSpline.Component.NotificationPerMember",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FQueueSplineNotificationPerMemberTest::RunTest(const FString& Parameters)
{
	AActor* QueueOwner = NewObject<AActor>(GetTransientPackage());
	USplineComponent* Spline = NewObject<USplineComponent>(QueueOwner);
	UQueueSplineComponent* Queue = NewObject<UQueueSplineComponent>(QueueOwner);

	TArray<FVector> SplinePoints;
	SplinePoints.Add(FVector::ZeroVector);
	SplinePoints.Add(FVector(1000.0, 0.0, 0.0));
	Spline->SetSplinePoints(SplinePoints, ESplineCoordinateSpace::Local);
	Queue->SplineComponent = Spline;
	Queue->Settings.SideOffset = 0.0;
	Queue->Settings.SideJitter = 0.0;
	Queue->Settings.DistanceJitter = 0.0;
	Queue->bAutoPushToMovementComponent = false;

	AActor* Members[3];
	FQueueSplineMemberHandle Handles[3];
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Members[Index] = NewObject<AActor>(GetTransientPackage());
		USceneComponent* Root = NewObject<USceneComponent>(Members[Index]);
		Members[Index]->SetRootComponent(Root);
		TestTrue(TEXT("应成功注册通知测试成员"), Queue->RegisterQueueMember(Members[Index], Handles[Index]));
	}

	Queue->UpdateQueueTargets(0.0f);
	TestEqual(TEXT("第一次更新应为每个有效成员生成一条通知"), Queue->NotificationBuffer.Num(), 3);

	FQueueSplineMoveTarget Target;
	TestTrue(TEXT("应能读取第一位成员目标"), Queue->GetQueueMoveTarget(Handles[0], Target));
	TestEqual(TEXT("首次目标应位于前视距离"), Target.Slot.Distance, 150.0);

	// 移动成员后再次更新，验证通知内容每次重新计算而非陈旧复用
	Members[0]->GetRootComponent()->SetWorldLocation(FVector(100.0, 0.0, 0.0));
	Queue->UpdateQueueTargets(0.0f);
	TestEqual(TEXT("第二次更新仍应为每个有效成员生成一条通知"), Queue->NotificationBuffer.Num(), 3);
	TestTrue(TEXT("应能再次读取第一位成员目标"), Queue->GetQueueMoveTarget(Handles[0], Target));
	TestEqual(TEXT("移动后目标距离应随位置刷新"), Target.Slot.Distance, 250.0);
	TestEqual(TEXT("两次更新不应影响成员数量"), Queue->GetQueueMemberCount(), 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQueueSplineNotificationReentrancyTest,
	"XTools.QueueSpline.Component.NotificationReentrancy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FQueueSplineNotificationReentrancyTest::RunTest(const FString& Parameters)
{
	AActor* QueueOwner = NewObject<AActor>(GetTransientPackage());
	USplineComponent* Spline = NewObject<USplineComponent>(QueueOwner);
	UQueueSplineComponent* Queue = NewObject<UQueueSplineComponent>(QueueOwner);

	TArray<FVector> SplinePoints;
	SplinePoints.Add(FVector::ZeroVector);
	SplinePoints.Add(FVector(1000.0, 0.0, 0.0));
	Spline->SetSplinePoints(SplinePoints, ESplineCoordinateSpace::Local);
	Queue->SplineComponent = Spline;
	Queue->Settings.SideOffset = 0.0;
	Queue->Settings.SideJitter = 0.0;
	Queue->Settings.DistanceJitter = 0.0;
	Queue->bAutoPushToMovementComponent = false;

	AActor* Members[3];
	FQueueSplineMemberHandle Handles[3];
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Members[Index] = NewObject<AActor>(GetTransientPackage());
		USceneComponent* Root = NewObject<USceneComponent>(Members[Index]);
		Members[Index]->SetRootComponent(Root);
		TestTrue(TEXT("应成功注册递归测试成员"), Queue->RegisterQueueMember(Members[Index], Handles[Index]));
	}

	// 每次回调注销当前广播成员（成员数递减构成递归终止条件），外层回调再递归更新
	UQueueSplineTargetEventRecorder* Recorder = NewObject<UQueueSplineTargetEventRecorder>();
	Recorder->Queue = Queue;
	Recorder->bUnregisterCurrentOnEveryCallback = true;
	Recorder->bReenterOnOuterCallback = true;

	FScriptDelegate Delegate;
	Delegate.BindUFunction(Recorder, TEXT("HandleTargetUpdated"));
	Queue->OnMemberTargetUpdated.Add(Delegate);

	Queue->UpdateQueueTargets(0.0f);

	TestEqual(TEXT("递归广播链应完成全部成员注销"), Queue->GetQueueMemberCount(), 0);
	TestEqual(TEXT("递归调用不应覆盖外层通知快照"), Queue->NotificationBuffer.Num(), 3);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FQueueSplineMemberHandle& Expected = Handles[Index];
		const bool bHandleInSnapshot = Queue->NotificationBuffer.ContainsByPredicate(
			[&Expected](const UQueueSplineComponent::FQueueSplineTargetNotification& Notification)
			{
				return Notification.Handle == Expected;
			});
		TestTrue(TEXT("外层通知快照应保留全部注册句柄"), bHandleInSnapshot);
	}
	TestEqual(TEXT("递归链应按成员顺序各广播一次"), Recorder->ReceivedHandles.Num(), 3);
	if (Recorder->ReceivedHandles.Num() == 3)
	{
		TestTrue(TEXT("递归链广播顺序应保持成员顺序"), Recorder->ReceivedHandles[0] == Handles[0]
			&& Recorder->ReceivedHandles[1] == Handles[1]
			&& Recorder->ReceivedHandles[2] == Handles[2]);
	}

	// 递归结束后守卫应已恢复，后续更新走正常缓冲路径
	Queue->UpdateQueueTargets(0.0f);
	TestEqual(TEXT("守卫恢复后空队列更新不应崩溃"), Queue->GetQueueMemberCount(), 0);

	return true;
}

// 专项回归：嵌套 UpdateQueueTargets 返回后，外层派发继续执行有效回调并再次重入。
// 若派发守卫在嵌套返回时错误清零派发标志（而非恢复进入前状态），
// 外层后续回调的重入会复用并覆盖外层正在遍历的通知缓冲，快照条数被内层数据替换。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQueueSplineNotificationNestedReturnTest,
	"XTools.QueueSpline.Component.NotificationNestedReturnReentrancy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FQueueSplineNotificationNestedReturnTest::RunTest(const FString& Parameters)
{
	AActor* QueueOwner = NewObject<AActor>(GetTransientPackage());
	USplineComponent* Spline = NewObject<USplineComponent>(QueueOwner);
	UQueueSplineComponent* Queue = NewObject<UQueueSplineComponent>(QueueOwner);

	TArray<FVector> SplinePoints;
	SplinePoints.Add(FVector::ZeroVector);
	SplinePoints.Add(FVector(1000.0, 0.0, 0.0));
	Spline->SetSplinePoints(SplinePoints, ESplineCoordinateSpace::Local);
	Queue->SplineComponent = Spline;
	Queue->Settings.SideOffset = 0.0;
	Queue->Settings.SideJitter = 0.0;
	Queue->Settings.DistanceJitter = 0.0;
	Queue->bAutoPushToMovementComponent = false;

	AActor* Members[4];
	FQueueSplineMemberHandle Handles[4];
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Members[Index] = NewObject<AActor>(GetTransientPackage());
		USceneComponent* Root = NewObject<USceneComponent>(Members[Index]);
		Members[Index]->SetRootComponent(Root);
		TestTrue(TEXT("应成功注册嵌套返回测试成员"), Queue->RegisterQueueMember(Members[Index], Handles[Index]));
	}

	// 外层回调重入；首个外层回调注销成员 D，使内层快照（3 条）与外层快照（4 条）长度不同：
	// 守卫若在嵌套返回时清零标志，外层后续重入会 Reset 成员缓冲并以 3 条内层数据覆盖外层 4 条快照
	UQueueSplineTargetEventRecorder* Recorder = NewObject<UQueueSplineTargetEventRecorder>();
	Recorder->Queue = Queue;
	Recorder->bReenterOnOuterCallback = true;
	Recorder->HandleToRemoveOnFirstOuter = Handles[3];

	FScriptDelegate Delegate;
	Delegate.BindUFunction(Recorder, TEXT("HandleTargetUpdated"));
	Queue->OnMemberTargetUpdated.Add(Delegate);

	Queue->UpdateQueueTargets(0.0f);

	TestEqual(TEXT("首个外层回调应注销注入成员 D"), Queue->GetQueueMemberCount(), 3);
	// 决定性断言：外层快照必须保持 4 条。守卫 bug 版本会被嵌套返回后的外层重入覆盖成内层的 3 条
	TestEqual(TEXT("嵌套返回后的外层重入不得覆盖外层通知快照"), Queue->NotificationBuffer.Num(), 4);

	// 精确广播序列：外层按 [A,B,C] 各触发一次，每次重入内层完整广播当轮 3 名成员；D 已注销不广播
	TestEqual(TEXT("广播序列条数应符合嵌套返回推演"), Recorder->ReceivedHandles.Num(), 12);
	if (Recorder->ReceivedHandles.Num() == 12)
	{
		const FQueueSplineMemberHandle ExpectedSequence[] = {
			Handles[0],
			Handles[0], Handles[1], Handles[2],
			Handles[1],
			Handles[0], Handles[1], Handles[2],
			Handles[2],
			Handles[0], Handles[1], Handles[2],
		};
		for (int32 Index = 0; Index < 12; ++Index)
		{
			TestTrue(TEXT("嵌套返回场景广播序列应与推演一致"),
				Recorder->ReceivedHandles[Index] == ExpectedSequence[Index]);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQueueSplineNotificationSkipInvalidTest,
	"XTools.QueueSpline.Component.NotificationSkipsUnregistered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FQueueSplineNotificationSkipInvalidTest::RunTest(const FString& Parameters)
{
	AActor* QueueOwner = NewObject<AActor>(GetTransientPackage());
	USplineComponent* Spline = NewObject<USplineComponent>(QueueOwner);
	UQueueSplineComponent* Queue = NewObject<UQueueSplineComponent>(QueueOwner);

	TArray<FVector> SplinePoints;
	SplinePoints.Add(FVector::ZeroVector);
	SplinePoints.Add(FVector(1000.0, 0.0, 0.0));
	Spline->SetSplinePoints(SplinePoints, ESplineCoordinateSpace::Local);
	Queue->SplineComponent = Spline;
	Queue->Settings.SideOffset = 0.0;
	Queue->Settings.SideJitter = 0.0;
	Queue->Settings.DistanceJitter = 0.0;
	Queue->bAutoPushToMovementComponent = false;

	AActor* Members[3];
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Members[Index] = NewObject<AActor>(GetTransientPackage());
		USceneComponent* Root = NewObject<USceneComponent>(Members[Index]);
		Members[Index]->SetRootComponent(Root);
		FQueueSplineMemberHandle Handle;
		TestTrue(TEXT("应成功注册跳过测试成员"), Queue->RegisterQueueMember(Members[Index], Handle));
	}

	// 广播回调中清空全部成员：后续快照通知的句柄复查应失败并被安全跳过，不崩溃
	UQueueSplineTargetEventRecorder* Recorder = NewObject<UQueueSplineTargetEventRecorder>();
	Recorder->Queue = Queue;
	Recorder->bClearMembersOnEveryCallback = true;

	FScriptDelegate Delegate;
	Delegate.BindUFunction(Recorder, TEXT("HandleTargetUpdated"));
	Queue->OnMemberTargetUpdated.Add(Delegate);

	Queue->UpdateQueueTargets(0.0f);

	TestEqual(TEXT("派发中注销应清空全部成员"), Queue->GetQueueMemberCount(), 0);
	TestEqual(TEXT("派发中注销不应破坏已生成的通知快照"), Queue->NotificationBuffer.Num(), 3);
	TestEqual(TEXT("清空后其余成员的广播应被句柄复查跳过"), Recorder->ReceivedHandles.Num(), 1);

	// 清空后的再次更新应安全返回（成员为空直接退出）
	Queue->UpdateQueueTargets(0.0f);
	TestEqual(TEXT("空队列再次更新应安全返回"), Queue->GetQueueMemberCount(), 0);

	return true;
}

#endif
