#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QueueSplineTypes.h"
#include "QueueSplineMovementComponent.generated.h"

class UQueueSplineComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FQueueSplineMovementEvent, FQueueSplineMoveTarget, Target);

UCLASS(ClassGroup = (XTools), meta = (BlueprintSpawnableComponent))
class QUEUESPLINE_API UQueueSplineMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQueueSplineMovementComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "移动模式"))
	EQueueSplineMovementMode MovementMode = EQueueSplineMovementMode::ManualTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "自动移动"))
	bool bAutoMove = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "移动输入强度", ClampMin = "0.0"))
	float MovementInputScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "自动生成默认控制器"))
	bool bAutoSpawnDefaultController = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "直接插值速度", ClampMin = "0.0"))
	float DirectInterpSpeed = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "旋转插值速度", ClampMin = "0.0"))
	float RotationInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线",
		meta = (DisplayName = "直接插值碰撞扫描",
			ToolTip = "直接插值模式移动时进行碰撞扫描（仅检测根组件），防止排队成员互相穿模或穿墙；被其他成员挡住时会停在阻挡点。关闭可获得完全平滑的插值移动。"))
	bool bDirectInterpSweep = true;

	UPROPERTY(BlueprintAssignable, Category = "XTools|排队样条线|移动事件", meta = (DisplayName = "排队移动开始"))
	FQueueSplineMovementEvent OnQueueMoveStarted;

	UPROPERTY(BlueprintAssignable, Category = "XTools|排队样条线|移动事件", meta = (DisplayName = "排队移动到达"))
	FQueueSplineMovementEvent OnQueueMoveArrived;

	UPROPERTY(BlueprintAssignable, Category = "XTools|排队样条线|移动事件", meta = (DisplayName = "排队移动停止"))
	FQueueSplineMovementEvent OnQueueMoveStopped;

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线", meta = (DisplayName = "设置排队移动目标"))
	void SetQueueMoveTarget(UPARAM(DisplayName = "移动目标") const FQueueSplineMoveTarget& NewTarget);

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线", meta = (DisplayName = "停止排队移动", ToolTip = "清除当前移动目标并触发停止事件；自动推送开启时队列可能在下一帧重新下发目标。"))
	void StopQueueMovement();

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线", meta = (DisplayName = "暂停排队移动", ToolTip = "保留目标并暂停移动。已注册到队列时会同时暂停该成员之后的队伍；未注册时只暂停本组件。"))
	void SetQueueMovementPaused(UPARAM(DisplayName = "暂停") bool bPaused);

	UFUNCTION(BlueprintPure, Category = "XTools|排队样条线", meta = (DisplayName = "排队移动已暂停"))
	bool IsQueueMovementPaused() const;

	UFUNCTION(BlueprintPure, Category = "XTools|排队样条线", meta = (DisplayName = "获取排队移动目标"))
	FQueueSplineMoveTarget GetQueueMoveTarget() const;

	UFUNCTION(BlueprintPure, Category = "XTools|排队样条线", meta = (DisplayName = "是否有排队移动目标"))
	bool HasQueueMoveTarget() const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	friend class UQueueSplineComponent;

	UPROPERTY(Transient)
	FQueueSplineMoveTarget CurrentTarget;

	TWeakObjectPtr<UQueueSplineComponent> QueueOwner;

	bool bHasTarget = false;
	bool bQueueMovementPaused = false;
	bool bArrivalLatched = false;
	bool bHasAppliedMovementInput = false;
	bool bTriedAutoSpawnDefaultController = false;
	bool bWarnedInvalidMovementInputOwner = false;
	bool bWarnedMissingController = false;

	bool UsesArrivalLatch() const;
	void SetQueueOwner(UQueueSplineComponent* NewQueueOwner);
	void ApplyQueueMovementPaused(bool bPaused);
	static bool IsSameMoveGoal(const FQueueSplineMoveTarget& First, const FQueueSplineMoveTarget& Second);
	void StopAddMovementInputMotion();
	APawn* ResolveMovementInputPawn();
	void ApplyAddMovementInput();
	void ApplyDirectInterp(float DeltaTime);
};
