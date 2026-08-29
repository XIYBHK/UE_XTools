#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Containers/Ticker.h"
#include "SplineMoveAlongAction.generated.h"

class AAIController;
class UPathFollowingComponent;
struct FPathFollowingResult;
struct FAIRequestID;

//---------------------------------------------------------------
// 委托声明
//---------------------------------------------------------------

/** 无参数信号委托 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSplineMoveSignal);

/** 每帧持续执行委托，携带当前目标位置和沿样条距离 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSplineMoveTick,
	FVector, TargetLocation, float, DistanceAlongSpline);

//---------------------------------------------------------------
// 移动模式枚举
//---------------------------------------------------------------

/**
 * 样条线移动模式
 */
UENUM(BlueprintType)
enum class ESplineMoveMode : uint8
{
	/** 使用 AddMovementInput 驱动角色移动（适用于玩家控制的 Character） */
	AddMovementInput UMETA(DisplayName = "角色移动输入（AddMovementInput）"),

	/** 使用 AI MoveTo 驱动寻路移动（适用于 AI 控制的 Pawn） */
	AIMoveTo UMETA(DisplayName = "AI 寻路移动（MoveToLocation）"),
};

//---------------------------------------------------------------
// 主类：沿样条线移动异步节点
//---------------------------------------------------------------

/**
 * 沿样条线移动异步节点
 *
 * 基于 UBlueprintAsyncActionBase 实现，支持两种移动模式：
 * 1. AddMovementInput - 适用于玩家控制的 Character
 * 2. AIMoveTo - 适用于 AI 控制的 Pawn
 *
 * 核心算法：
 * - 首帧找到 Pawn 在样条线上的最近点并保存当前距离
 * - 每轮按刷新距离推进下一个目标点，接近目标后更新当前距离
 * - 驱动 Pawn 向目标点移动
 * - 到达样条终点时触发完成
 */
UCLASS(meta = (HasDedicatedAsyncNode, HideThen))
class SPLINEMOVEMENT_API USplineMoveAlongAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

#if WITH_DEV_AUTOMATION_TESTS
	friend class FSplineMoveAlongActionRepathTest;
	friend class FSplineMoveAlongActionCompletionTest;
	friend class FSplineMoveAlongActionEndpointArrivalTest;
#endif

public:
	//---------------------------------------------------------------
	// 输出引脚
	//---------------------------------------------------------------

	/** 首帧触发一次 */
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "Then"))
	FOnSplineMoveSignal Then;

	/** 每帧持续触发，携带当前目标位置和沿样条距离 */
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "持续执行"))
	FOnSplineMoveTick OnTick;

	/** 到达样条线终点时触发 */
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "移动成功"))
	FOnSplineMoveSignal OnSuccess;

	/** 被 Interrupt() 中断或 Pawn/Spline 失效时触发 */
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "移动中断"))
	FOnSplineMoveSignal OnInterrupted;

	//---------------------------------------------------------------
	// 节点工厂函数
	//---------------------------------------------------------------

	/**
	 * 沿样条线移动
	 *
	 * @param Pawn              要移动的 Pawn（需要有 MovementComponent 或 AIController）
	 * @param Spline            目标样条线组件
	 * @param LookaheadDistance 刷新距离（单位 cm），控制样条上的下一个目标点间距
	 * @param RightOffsetRate   沿样条右向的偏移系数（乘以样条 Scale.Y）
	 * @param InputWeight       AddMovementInput 的 ScaleValue（仅 AddMovementInput 模式）
	 * @param bReverse          是否沿样条反向移动
	 * @param MoveMode          AddMovementInput 或 AIMoveTo
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|样条线|移动",
		meta = (
			BlueprintInternalUseOnly = "true",
			DisplayName = "沿样条线移动",
			Keywords = "样条,spline,移动,路径,follow",
			ToolTip = "驱动 Pawn 沿样条线路径移动，到达终点时触发移动成功。\n\n参数:\nPawn - 被移动的角色\nSpline - 路径样条组件\nRefresh Distance - 刷新距离(cm)，控制下一个样条目标点\nRight Offset Rate - 横向偏移，乘以样条 Scale.Y\nInput Weight - 移动输入强度（仅 AddMovementInput 模式）\nReverse - 沿样条反向行进\nMove Mode - 角色输入 或 AI 寻路"
		))
	static USplineMoveAlongAction* SplineMoveAlong(
		UPARAM(DisplayName = "角色") APawn* Pawn,
		UPARAM(DisplayName = "样条线") class USplineComponent* Spline,
		UPARAM(DisplayName = "刷新距离") float LookaheadDistance = 100.f,
		UPARAM(DisplayName = "向右偏移率") float RightOffsetRate = 0.f,
		UPARAM(DisplayName = "输入权重") float InputWeight = 1.f,
		UPARAM(DisplayName = "反向") bool bReverse = false,
		UPARAM(DisplayName = "移动模式") ESplineMoveMode MoveMode = ESplineMoveMode::AddMovementInput);

	/**
	 * 中断正在执行的沿样条线移动，触发"移动中断"引脚
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|样条线|移动",
		meta = (DisplayName = "中断样条线移动"))
	void Interrupt();

	//---------------------------------------------------------------
	// UBlueprintAsyncActionBase 覆写
	//---------------------------------------------------------------

	virtual void Activate() override;
	virtual void BeginDestroy() override;

private:
	//---------------------------------------------------------------
	// 内部方法
	//---------------------------------------------------------------

	/** 启动 Ticker 心跳 */
	void StartTick();

	/** 停止 Ticker 心跳 */
	void StopTick();

	/** Ticker 回调，返回 false 时自动移除 */
	bool OnTicker(float DeltaTime);

	/** 结束动作，广播完成或中断事件 */
	void FinishAction(bool bInterrupted);

	/** 判断新目标是否已移动到需要重新发起寻路请求的距离。 */
	static bool ShouldRepathAIMoveTo(const FVector& NewTarget, const FVector& PreviousTarget,
		bool bHasPreviousTarget, float RepathMinDistance);

	/** AI 寻路失败自愈：请求被中止或失败时清除防抖缓存，下一帧重新发起寻路 */
	void HandleAIMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);

	/** 确保 PathFollowingComponent 的完成事件已绑定（组件变更时先解绑旧绑定） */
	void EnsureAIMoveCompletedBound(AAIController* AICtrl);

	/** 解绑移动完成事件，在动作结束时调用 */
	void UnbindAIMoveCompleted();

	//---------------------------------------------------------------
	// 输入参数（从工厂函数传入）
	//---------------------------------------------------------------

	UPROPERTY(Transient)
	TObjectPtr<APawn> Pawn_Ptr;

	UPROPERTY(Transient)
	TObjectPtr<class USplineComponent> Spline_Ptr;

	float LookaheadDist = 100.f;
	float RightOffsetRate = 0.f;
	float InputWeight = 1.f;
	bool bReverse = false;
	ESplineMoveMode MoveMode = ESplineMoveMode::AddMovementInput;

	//---------------------------------------------------------------
	// 运行时状态
	//---------------------------------------------------------------

	bool bInterruptRequested = false;
	bool bFirstTick = true;
	bool bFinished = false;
	bool bHasCurrentDistance = false;
	float CurrentDistance = 0.f;

	// AIMoveTo 模式防抖状态：记录上次 MoveToLocation 下发的目标位置，
	// 用于判断目标位移是否超过重寻路阈值
	FVector LastMoveToTarget = FVector::ZeroVector;
	bool bHasLastMoveToTarget = false;

	// 已绑定的寻路跟随组件（失败自愈事件的目标，兼作重复绑定判定）
	TWeakObjectPtr<UPathFollowingComponent> BoundPathFollowing;

	// 缺失 AIController 的告警只打印一次，避免逐帧刷屏
	bool bHasLoggedMissingAIController = false;

#if WITH_DEV_AUTOMATION_TESTS
	int32 AutomationMoveToRequestCount = 0;
#endif

	FTSTicker::FDelegateHandle TickHandle;
};
