#include "SplineMoveAlongAction.h"
#include "SplineMovementLog.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Pawn.h"
#include "AIController.h"
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
	Super::BeginDestroy();
}

//=============================================================================
// 公开控制接口
//=============================================================================

void USplineMoveAlongAction::Interrupt()
{
	bInterruptRequested = true;
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

	// ---- 3. 计算下一个样条距离并检查终点 ----
	const float Direction = bReverse ? -1.f : 1.f;
	const float NextDistance = CurrentDistance + LookaheadDist * Direction;
	const bool bReachedEnd = bReverse
		? (CurrentDistance <= 0.f)
		: (NextDistance >= SplineLength);

	if (bReachedEnd)
	{
		FinishAction(false);
		return false;
	}

	// ---- 4. 计算刷新距离对应的目标点 ----
	//
	// 沿样条前瞻 LookaheadDist，加上右向偏移（还原蓝图宏的 ScaleAtDist.Y * 偏移率 逻辑）
	const float TargetDist = NextDistance;

	const FVector SplinePos  = Spline->GetLocationAtDistanceAlongSpline(TargetDist, ESplineCoordinateSpace::World);
	const FVector RightVec   = Spline->GetRightVectorAtDistanceAlongSpline(TargetDist, ESplineCoordinateSpace::World);
	const FVector ScaleAtDist= Spline->GetScaleAtDistanceAlongSpline(TargetDist);

	// 右向偏移量 = RightVector * Scale.Y * 偏移率（与蓝图宏 v4 完全一致）
	const FVector TargetPos = SplinePos + RightVec * ScaleAtDist.Y * RightOffsetRate;

	if (FVector::Dist2D(TargetPos, PawnPos) <= LookaheadDist)
	{
		CurrentDistance = NextDistance;
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
			// 重寻路防抖：MoveToLocation 会中止当前寻路请求，逐帧调用会导致路径反复重建。
			// 仅当新目标点相对上次下发目标的位移超过 RepathMinDistance（取前瞻距离的一半）
			// 时才重新发起 MoveToLocation，其余帧沿用 AI 当前移动目标。
			const float RepathMinDistance = LookaheadDist * 0.5f;
			if (!bHasLastMoveToTarget ||
				FVector::Dist(TargetPos, LastMoveToTarget) > RepathMinDistance)
			{
				AICtrl->MoveToLocation(TargetPos, /*AcceptanceRadius=*/LookaheadDist * 0.25f,
					/*bStopOnOverlap=*/false, /*bUsePathfinding=*/true,
					/*bProjectDestinationToNavigation=*/true);
				LastMoveToTarget = TargetPos;
				bHasLastMoveToTarget = true;
			}
		}
		else
		{
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
