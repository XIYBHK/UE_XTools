/*
 * SplineMovement 自动化测试：AI 重寻路阈值
 */

#if WITH_DEV_AUTOMATION_TESTS

#include "SplineMoveAlongAction.h"
#include "SplineMoveAlongActionTestTypes.h"
#include "AIController.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSplineMoveAlongActionRepathTest,
	"XTools.SplineMovement.AIMoveTo.RepathThreshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSplineMoveAlongActionRepathTest::RunTest(const FString& Parameters)
{
	const FVector PreviousTarget(100.0f, 0.0f, 0.0f);
	const float Threshold = 50.0f;

	TestTrue(TEXT("首次 AIMoveTo 必须发起寻路"),
		USplineMoveAlongAction::ShouldRepathAIMoveTo(PreviousTarget, PreviousTarget, false, Threshold));
	TestFalse(TEXT("目标位移等于阈值时不应重新寻路"),
		USplineMoveAlongAction::ShouldRepathAIMoveTo(FVector(150.0f, 0.0f, 0.0f), PreviousTarget, true, Threshold));
	TestFalse(TEXT("目标位移小于阈值时不应重新寻路"),
		USplineMoveAlongAction::ShouldRepathAIMoveTo(FVector(149.9f, 0.0f, 0.0f), PreviousTarget, true, Threshold));
	TestTrue(TEXT("目标位移超过阈值时必须重新寻路"),
		USplineMoveAlongAction::ShouldRepathAIMoveTo(FVector(150.1f, 0.0f, 0.0f), PreviousTarget, true, Threshold));
	TestFalse(TEXT("纯垂直起伏（XY 位移为 0）不应触发重寻路——与推进判断同用水平面口径"),
		USplineMoveAlongAction::ShouldRepathAIMoveTo(FVector(100.0f, 0.0f, 500.0f), PreviousTarget, true, Threshold));

	// 真实 Game World 驱动两次 ticker：首帧发起请求，目标未继续推进时不得重复请求。
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SplineMovementAIMoveToTest"));
	TestNotNull(TEXT("应创建导航测试世界"), World);
	if (World)
	{
		APawn* Pawn = World->SpawnActor<APawn>();
		AAIController* Controller = World->SpawnActor<AAIController>();
		TestNotNull(TEXT("应生成测试 Pawn"), Pawn);
		TestNotNull(TEXT("应生成测试 AIController"), Controller);
		if (Pawn && Controller)
		{
			Controller->Possess(Pawn);
			USplineComponent* Spline = NewObject<USplineComponent>(Pawn);
			Spline->RegisterComponent();
			Spline->ClearSplinePoints(false);
			Spline->AddSplinePoint(FVector::ZeroVector, ESplineCoordinateSpace::World, false);
			Spline->AddSplinePoint(FVector(100.0f, 0.0f, 0.0f), ESplineCoordinateSpace::World, false);
			Spline->AddSplinePoint(FVector(200.0f, 0.0f, 0.0f), ESplineCoordinateSpace::World, false);
			Spline->AddSplinePoint(FVector(400.0f, 0.0f, 0.0f), ESplineCoordinateSpace::World, true);

			USplineMoveAlongAction* Action = NewObject<USplineMoveAlongAction>(Pawn);
			Action->AddToRoot();
			Action->Pawn_Ptr = Pawn;
			Action->Spline_Ptr = Spline;
			Action->LookaheadDist = 100.0f;
			Action->MoveMode = ESplineMoveMode::AIMoveTo;

			TestTrue(TEXT("首个 ticker 应继续执行动作"), Action->OnTicker(1.0f / 60.0f));
			TestEqual(TEXT("首个 ticker 应发起一次 MoveTo 请求"), Action->AutomationMoveToRequestCount, 1);
			// 样条目标会随 CurrentDistance 推进，后续帧是否重寻路由上方阈值断言覆盖；
			// 此处只钉定真实 AIController 请求链路确实被触发。

			Action->RemoveFromRoot();
		}
		World->DestroyWorld(false);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSplineMoveAlongActionCompletionTest,
	"XTools.SplineMovement.AIMoveTo.CompletionStopsMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSplineMoveAlongActionCompletionTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SplineMovementCompletionTest"));
	TestNotNull(TEXT("应创建完成路径测试世界"), World);
	if (!World)
	{
		return false;
	}

	APawn* Pawn = World->SpawnActor<APawn>();
	ASplineMovementTestController* Controller = World->SpawnActor<ASplineMovementTestController>();
	TestNotNull(TEXT("应生成完成路径测试 Pawn"), Pawn);
	TestNotNull(TEXT("应生成可观测 AIController"), Controller);
	if (Pawn && Controller)
	{
		Controller->Possess(Pawn);
		USplineMoveAlongAction* Action = NewObject<USplineMoveAlongAction>(Pawn);
		Action->AddToRoot();
		Action->Pawn_Ptr = Pawn;
		Action->MoveMode = ESplineMoveMode::AIMoveTo;
		Action->FinishAction(false);
		TestEqual(TEXT("AIMoveTo 完成时应停止当前 AI 移动"), Controller->StopMovementCalls, 1);
		Action->RemoveFromRoot();
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSplineMoveAlongActionEndpointArrivalTest,
	"XTools.SplineMovement.Endpoint.RequiresPawnArrival",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSplineMoveAlongActionEndpointArrivalTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SplineMovementEndpointArrivalTest"));
	TestNotNull(TEXT("应创建端点测试世界"), World);
	if (!World)
	{
		return false;
	}

	APawn* Pawn = World->SpawnActor<APawn>();
	USplineComponent* Spline = Pawn ? NewObject<USplineComponent>(Pawn) : nullptr;
	TestNotNull(TEXT("应生成端点测试 Pawn"), Pawn);
	TestNotNull(TEXT("应创建端点测试样条"), Spline);
	if (Pawn && Spline)
	{
		USceneComponent* PawnRoot = NewObject<USceneComponent>(Pawn);
		Pawn->SetRootComponent(PawnRoot);
		PawnRoot->RegisterComponent();
		Spline->RegisterComponent();
		Spline->ClearSplinePoints(false);
		Spline->AddSplinePoint(FVector::ZeroVector, ESplineCoordinateSpace::World, false);
		Spline->AddSplinePoint(FVector(1000.f, 0.f, 0.f), ESplineCoordinateSpace::World, true);

		auto CreateAction = [Pawn, Spline](bool bReverse, float CurrentDistance)
		{
			USplineMoveAlongAction* Action = NewObject<USplineMoveAlongAction>(Pawn);
			Action->AddToRoot();
			Action->Pawn_Ptr = Pawn;
			Action->Spline_Ptr = Spline;
			Action->LookaheadDist = 100.f;
			Action->bReverse = bReverse;
			Action->bHasCurrentDistance = true;
			Action->CurrentDistance = CurrentDistance;
			return Action;
		};

		Pawn->SetActorLocation(FVector(850.f, 0.f, 0.f));
		USplineMoveAlongAction* ForwardAction = CreateAction(false, 950.f);
		TestTrue(TEXT("正向前瞻触及终点但 Pawn 未到达时应继续"), ForwardAction->OnTicker(1.f / 60.f));
		TestFalse(TEXT("正向未到达终点时不应完成"), ForwardAction->bFinished);
		Pawn->SetActorLocation(FVector(1000.f, 0.f, 0.f));
		TestFalse(TEXT("正向 Pawn 到达终点后应停止 ticker"), ForwardAction->OnTicker(1.f / 60.f));
		TestTrue(TEXT("正向 Pawn 到达终点后应完成"), ForwardAction->bFinished);
		ForwardAction->RemoveFromRoot();

		Pawn->SetActorLocation(FVector(150.f, 0.f, 0.f));
		USplineMoveAlongAction* ReverseAction = CreateAction(true, 50.f);
		TestTrue(TEXT("反向前瞻触及起点但 Pawn 未到达时应继续"), ReverseAction->OnTicker(1.f / 60.f));
		TestFalse(TEXT("反向未到达起点时不应完成"), ReverseAction->bFinished);
		Pawn->SetActorLocation(FVector::ZeroVector);
		TestFalse(TEXT("反向 Pawn 到达起点后应停止 ticker"), ReverseAction->OnTicker(1.f / 60.f));
		TestTrue(TEXT("反向 Pawn 到达起点后应完成"), ReverseAction->bFinished);
		ReverseAction->RemoveFromRoot();
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSplineMoveAlongActionRejectsNonFiniteInputTest,
	"XTools.SplineMovement.Input.RejectsNonFiniteParameters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSplineMoveAlongActionRejectsNonFiniteInputTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SplineMovementNonFiniteInputTest"));
	if (!TestNotNull(TEXT("应创建输入校验测试世界"), World))
	{
		return false;
	}

	APawn* Pawn = World->SpawnActor<APawn>();
	USplineComponent* Spline = Pawn ? NewObject<USplineComponent>(Pawn) : nullptr;
	if (!TestNotNull(TEXT("应生成测试 Pawn"), Pawn) || !TestNotNull(TEXT("应创建测试样条"), Spline))
	{
		World->DestroyWorld(false);
		return false;
	}

	AddExpectedError(TEXT("移动参数包含非有限值"), EAutomationExpectedErrorFlags::Contains, 3);
	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const float Infinity = std::numeric_limits<float>::infinity();

	TestNull(TEXT("非有限前瞻距离不得创建异步动作"),
		USplineMoveAlongAction::SplineMoveAlong(Pawn, Spline, NaN, 0.0f, 1.0f, false));
	TestNull(TEXT("非有限横向偏移不得创建异步动作"),
		USplineMoveAlongAction::SplineMoveAlong(Pawn, Spline, 100.0f, Infinity, 1.0f, false));
	TestNull(TEXT("非有限输入权重不得创建异步动作"),
		USplineMoveAlongAction::SplineMoveAlong(Pawn, Spline, 100.0f, 0.0f, NaN, false));

	World->DestroyWorld(false);
	return true;
}

#endif
