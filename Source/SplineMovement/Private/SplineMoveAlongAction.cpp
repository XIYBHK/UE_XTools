#include "SplineMoveAlongAction.h"
#include "SplineMovementLog.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Pawn.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Engine/World.h"
#include "Math/UnrealMathUtility.h"

//=============================================================================
// 工厂函数
//=============================================================================

USplineMoveAlongAction* USplineMoveAlongAction::SplineMoveAlong(
	APawn* Pawn,
	USplineComponent* Spline,
	float LookaheadDistance,
	float RightOffsetRate,
	float InputWeight,
	bool bReverse,
	ESplineMoveMode MoveMode)
{
	if (!IsValid(Pawn))
	{
		UE_LOG(LogSplineMovement, Warning, TEXT("SplineMoveAlong: Pawn 无效，节点不会执行"));
		return nullptr;
	}
	if (!IsValid(Spline))
	{
		UE_LOG(LogSplineMovement, Warning, TEXT("SplineMoveAlong: Spline 无效，节点不会执行"));
		return nullptr;
	}

	USplineMoveAlongAction* Action = NewObject<USplineMoveAlongAction>(Pawn);
	Action->Pawn_Ptr       = Pawn;
	Action->Spline_Ptr     = Spline;
	Action->LookaheadDist  = FMath::Max(LookaheadDistance, KINDA_SMALL_NUMBER);
	Action->RightOffsetRate= RightOffsetRate;
	Action->InputWeight    = InputWeight;
	Action->bReverse       = bReverse;
	Action->MoveMode       = MoveMode;

	// 将异步节点注册到 GameInstance，防止中途被 GC
	Action->RegisterWithGameInstance(Pawn);

	return Action;
}

//=============================================================================
// UBlueprintAsyncActionBase 覆写
//=============================================================================

void USplineMoveAlongAction::Activate()
{
	// Pawn/Spline 二次检查（Activate 与工厂之间可能有一帧间隔）
	if (!IsValid(Pawn_Ptr.Get()) || !IsValid(Spline_Ptr.Get()))
	{
		UE_LOG(LogSplineMovement, Warning, TEXT("SplineMoveAlongAction::Activate: 对象已失效，提前终止"));
		FinishAction(true);
		return;
	}

	StartTick();
}

void USplineMoveAlongAction::BeginDestroy()
{
	StopTick();
	UnbindAIMoveCompleted();
	Super::BeginDestroy();
}

//=============================================================================
// 公开控制接口
//=============================================================================

void USplineMoveAlongAction::Interrupt()
{
	bInterruptRequested = true;
}

bool USplineMoveAlongAction::ShouldRepathAIMoveTo(const FVector& NewTarget, const FVector& PreviousTarget,
	bool bHasPreviousTarget, float RepathMinDistance)
{
	// 与 OnTicker 中目标推进判断统一采用水平面（Dist2D）口径，并以平方比较省去每帧开方：
	// 样条垂直起伏不再虚增重寻路判断距离，防抖阈值与实际沿路位移一致。
	return !bHasPreviousTarget ||
		FVector::DistSquared2D(NewTarget, PreviousTarget) > FMath::Square(RepathMinDistance);
}

//=============================================================================
// Ticker 管理
//=============================================================================

void USplineMoveAlongAction::StartTick()
{
	if (TickHandle.IsValid())
	{
		return;
	}

	TWeakObjectPtr<USplineMoveAlongAction> WeakThis(this);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([WeakThis](float DeltaTime) -> bool
		{
			if (USplineMoveAlongAction* StrongThis = WeakThis.Get())
			{
				return StrongThis->OnTicker(DeltaTime);
			}
			return false; // 对象已销毁，自动移除
		}),
		0.0f); // 每帧执行
}

void USplineMoveAlongAction::StopTick()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
}

//=============================================================================
// 核心 Tick 逻辑
//=============================================================================

bool USplineMoveAlongAction::OnTicker(float DeltaTime)
{
	// ---- 0. 中断检查 ----
	if (bInterruptRequested)
	{
		FinishAction(true);
		return false;
	}

	// ---- 1. 有效性检查 ----
	APawn* Pawn = Pawn_Ptr.Get();
	USplineComponent* Spline = Spline_Ptr.Get();

	if (!IsValid(Pawn) || !IsValid(Spline))
	{
		UE_LOG(LogSplineMovement, Warning, TEXT("SplineMoveAlongAction: Pawn 或 Spline 运行时失效，触发移动中断"));
		FinishAction(true);
		return false;
	}

	const float SplineLength = Spline->GetSplineLength();
	if (SplineLength <= SMALL_NUMBER)
	{
		UE_LOG(LogSplineMovement, Warning, TEXT("SplineMoveAlongAction: 样条线长度为 0，终止移动"));
		FinishAction(false);
		return false;
	}

	// ---- 2. 初始化并保持蓝图宏中的“当前长度” ----
	const FVector PawnPos = Pawn->GetActorLocation();
	if (!bHasCurrentDistance)
	{
		const float NearestKey = Spline->FindInputKeyClosestToWorldLocation(PawnPos);
		CurrentDistance = Spline->GetDistanceAlongSplineAtSplineInputKey(NearestKey);
		bHasCurrentDistance = true;
	}

	// ---- 3. 计算下一个样条目标距离 ----
	const float Direction = bReverse ? -1.f : 1.f;
	const float NextDistance = CurrentDistance + LookaheadDist * Direction;

	// ---- 4. 计算刷新距离对应的目标点 ----
	//
	// 沿样条前瞻 LookaheadDist，加上右向偏移（还原蓝图宏的 ScaleAtDist.Y * 偏移率 逻辑）
	const float TargetDist = FMath::Clamp(NextDistance, 0.f, SplineLength);

	const FVector SplinePos  = Spline->GetLocationAtDistanceAlongSpline(TargetDist, ESplineCoordinateSpace::World);
	const FVector RightVec   = Spline->GetRightVectorAtDistanceAlongSpline(TargetDist, ESplineCoordinateSpace::World);
	const FVector ScaleAtDist= Spline->GetScaleAtDistanceAlongSpline(TargetDist);

	// 右向偏移量 = RightVector * Scale.Y * 偏移率（与蓝图宏 v4 完全一致）
	const FVector TargetPos = SplinePos + RightVec * ScaleAtDist.Y * RightOffsetRate;
	const float DistanceToTarget = FVector::Dist2D(TargetPos, PawnPos);
	const float EndDistance = bReverse ? 0.f : SplineLength;
	const float ArrivalTolerance = FMath::Max(LookaheadDist * 0.25f, 1.f);
	if (FMath::IsNearlyEqual(TargetDist, EndDistance) && DistanceToTarget <= ArrivalTolerance)
	{
		FinishAction(false);
		return false;
	}

	if (DistanceToTarget <= LookaheadDist)
	{
		CurrentDistance = TargetDist;
	}

	// v4 的 Sequence 顺序：Then 先触发，随后移动，再触发持续执行。
	if (bFirstTick)
	{
		bFirstTick = false;
		Then.Broadcast();
	}

	// ---- 5. 驱动移动 ----
	switch (MoveMode)
	{
	case ESplineMoveMode::AddMovementInput:
	{
		// 计算 2D 方向（忽略 Z 轴，避免爬坡时输入方向偏转）
		FVector Dir = FVector(TargetPos.X - PawnPos.X, TargetPos.Y - PawnPos.Y, 0.f).GetSafeNormal();
		Pawn->AddMovementInput(Dir, InputWeight);
		break;
	}
	case ESplineMoveMode::AIMoveTo:
	{
		if (AAIController* AICtrl = Cast<AAIController>(Pawn->GetController()))
		{
			EnsureAIMoveCompletedBound(AICtrl);
			// 重寻路防抖：MoveToLocation 会中止当前寻路请求，逐帧调用会导致路径反复重建。
			// 仅当新目标点相对上次下发目标的位移超过 RepathMinDistance（取前瞻距离的一半）
			// 时才重新发起 MoveToLocation，其余帧沿用 AI 当前移动目标。
			// 请求失败或被中止时由 HandleAIMoveFinished 清空缓存，保证自愈重试能力。
			const float RepathMinDistance = LookaheadDist * 0.5f;
			if (ShouldRepathAIMoveTo(TargetPos, LastMoveToTarget, bHasLastMoveToTarget, RepathMinDistance))
			{
				#if WITH_DEV_AUTOMATION_TESTS
				++AutomationMoveToRequestCount;
				#endif
				const EPathFollowingRequestResult::Type MoveResult =
					AICtrl->MoveToLocation(TargetPos, /*AcceptanceRadius=*/LookaheadDist * 0.25f,
						/*bStopOnOverlap=*/false, /*bUsePathfinding=*/true,
						/*bProjectDestinationToNavigation=*/true);
				if (MoveResult == EPathFollowingRequestResult::Failed)
				{
					// 下发即失败（如导航数据暂不可用）：不缓存目标，下一帧直接重试
					UE_LOG(LogSplineMovement, Verbose,
						TEXT("SplineMoveAlongAction: MoveToLocation 下发失败，下一帧将重试"));
				}
				else
				{
					LastMoveToTarget = TargetPos;
					bHasLastMoveToTarget = true;
				}
			}
		}
		else if (!bHasLoggedMissingAIController)
		{
			bHasLoggedMissingAIController = true;
			UE_LOG(LogSplineMovement, Warning,
				TEXT("SplineMoveAlongAction: AIMoveTo 模式下未找到 AIController，请确保 Pawn 由 AI Controller 控制"));
		}
		break;
	}
	}

	// ---- 6. 广播持续执行事件 ----
	OnTick.Broadcast(TargetPos, TargetDist);

	return true; // 继续下一帧
}

//=============================================================================
// AI 寻路失败自愈
//=============================================================================

void USplineMoveAlongAction::HandleAIMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	// 失败或被外部中止（Blocked/Aborted 等）：清空防抖缓存，
	// 下一帧应重寻路判断直接放行，重新发起寻路请求。
	// 注：因自身重寻路而取消旧请求产生的 Skipped 结果发生在 MoveToLocation 调用内部、
	// 新目标缓存赋值之前，清除后立即被覆盖，不影响防抖语义。
	if (!bFinished && !Result.IsSuccess())
	{
		LastMoveToTarget = FVector::ZeroVector;
		bHasLastMoveToTarget = false;
	}
}

void USplineMoveAlongAction::EnsureAIMoveCompletedBound(AAIController* AICtrl)
{
	UPathFollowingComponent* PathFollowing = AICtrl ? AICtrl->GetPathFollowingComponent() : nullptr;
	if (!PathFollowing || BoundPathFollowing.Get() == PathFollowing)
	{
		return;
	}
	// 控制器可能被外部替换：先解绑旧组件再绑新组件
	UnbindAIMoveCompleted();
	BoundPathFollowing = PathFollowing;
	PathFollowing->OnRequestFinished.AddUObject(this, &USplineMoveAlongAction::HandleAIMoveFinished);
}

void USplineMoveAlongAction::UnbindAIMoveCompleted()
{
	if (UPathFollowingComponent* PathFollowing = BoundPathFollowing.Get())
	{
		PathFollowing->OnRequestFinished.RemoveAll(this);
	}
	BoundPathFollowing.Reset();
}

//=============================================================================
// 结束动作
//=============================================================================

void USplineMoveAlongAction::FinishAction(bool bInterrupted)
{
	if (bFinished)
	{
		return; // 防止重复触发
	}
	bFinished = true;

	StopTick();
	UnbindAIMoveCompleted();

	// AIMoveTo 模式：停止 AI 当前移动，避免残留目标
	if (MoveMode == ESplineMoveMode::AIMoveTo)
	{
		if (APawn* Pawn = Pawn_Ptr.Get())
		{
			if (AAIController* AICtrl = Cast<AAIController>(Pawn->GetController()))
			{
				AICtrl->StopMovement();
			}
		}
	}

	if (bInterrupted)
	{
		OnInterrupted.Broadcast();
	}
	else
	{
		OnSuccess.Broadcast();
	}

	SetReadyToDestroy();
}
