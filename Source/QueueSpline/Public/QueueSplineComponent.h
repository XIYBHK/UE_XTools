#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QueueSplineTypes.h"
#include "UObject/WeakObjectPtr.h"
#include "QueueSplineComponent.generated.h"

class UQueueSplineMovementComponent;
class USplineComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FQueueSplineMemberTargetEvent, FQueueSplineMemberHandle, Handle, FQueueSplineMoveTarget, Target);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FQueueSplineMemberEvent, FQueueSplineMemberHandle, Handle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FQueueSplineSimpleEvent);

struct FQueueSplineMemberRuntime
{
	FQueueSplineMemberHandle Handle;
	TWeakObjectPtr<AActor> Actor;
	int32 SlotIndex = INDEX_NONE;
	int32 JoinOrder = 0;
	int32 RandomSeed = 0;
	double CurrentSplineDistance = 0.0;
	bool bHasSplineDistance = false;
	double CurrentRightOffset = 0.0;
	FQueueSplineSlot Slot;
	bool bReachedSlot = false;
	bool bPauseRequested = false;
	int32 PauseThroughJoinOrder = INDEX_NONE;
	EQueueSplineMemberPhase Phase = EQueueSplineMemberPhase::Queued;
};

/**
 * 排队样条线组件
 *
 * 移动模型：成员注册后持续沿样条向路径终点行进（每次追踪「当前距离 + 路径前视距离」的前视点），
 * 槽位仅用于预填充生成与状态查询，不作为停靠点；队列间距通过「成员及后方暂停」屏障维持。
 * 需要"到槽停靠、前队离开再递补"的固定队列时，请用暂停接口显式控制。
 *
 * 性能：成员较多时建议增大「更新间隔」，样条投影查询按成员数线性增长。
 */
UCLASS(ClassGroup = (XTools), meta = (BlueprintSpawnableComponent))
class QUEUESPLINE_API UQueueSplineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQueueSplineComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "样条组件"))
	TObjectPtr<USplineComponent> SplineComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "队列配置"))
	FQueueSplineSettings Settings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "自动更新"))
	bool bAutoUpdate = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线",
		meta = (DisplayName = "更新间隔", ClampMin = "0.0",
			ToolTip = "两次排队目标更新之间的秒数，0 表示每帧更新。成员较多时增大该值可显著降低样条投影查询开销。"))
	float UpdateInterval = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "自动推送到移动组件"))
	bool bAutoPushToMovementComponent = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线|调试", meta = (DisplayName = "调试绘制"))
	bool bDrawDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线|调试", meta = (DisplayName = "调试持续时间"))
	float DebugDrawTime = 0.0f;

	UPROPERTY(BlueprintAssignable, Category = "XTools|排队样条线")
	FQueueSplineMemberTargetEvent OnMemberTargetUpdated;

	UPROPERTY(BlueprintAssignable, Category = "XTools|排队样条线")
	FQueueSplineMemberEvent OnMemberRegistered;

	UPROPERTY(BlueprintAssignable, Category = "XTools|排队样条线")
	FQueueSplineMemberEvent OnMemberUnregistered;

	UPROPERTY(BlueprintAssignable, Category = "XTools|排队样条线")
	FQueueSplineSimpleEvent OnQueueEmpty;

	UPROPERTY(BlueprintAssignable, Category = "XTools|排队样条线|服务")
	FQueueSplineMemberEvent OnMemberServiceStarted;

	UPROPERTY(BlueprintAssignable, Category = "XTools|排队样条线|服务")
	FQueueSplineMemberEvent OnMemberServiceCompleted;

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线", meta = (DisplayName = "注册排队成员"))
	bool RegisterQueueMember(
		UPARAM(DisplayName = "成员Actor") AActor* Member,
		UPARAM(DisplayName = "成员句柄") FQueueSplineMemberHandle& OutHandle);

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线", meta = (DisplayName = "移除排队成员"))
	bool UnregisterQueueMember(UPARAM(DisplayName = "成员句柄") FQueueSplineMemberHandle Handle);

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线", meta = (DisplayName = "按Actor移除排队成员"))
	bool UnregisterQueueActor(UPARAM(DisplayName = "成员Actor") AActor* Member);

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线", meta = (DisplayName = "清空排队成员"))
	void ClearQueueMembers();

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线", meta = (DisplayName = "重建排队槽位"))
	bool RebuildQueueSlots();

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线", meta = (DisplayName = "生成初始排队Transform", Keywords = "排队 样条 生成 初始位置 预填充 Spawn Transform", ToolTip = "预填充模式返回队头到队尾的初始Transform数组；从入口模式只返回一个入口Transform，适合每次生成一个角色。\n样条组件未设置时自动使用所属Actor上的第一个样条组件；返回空数组表示配置无效，具体原因见输出日志（Output Log）与屏幕警告。\n注意：在 Construction Script 中调用时样条可能尚未注册，返回的是局部坐标，请在 BeginPlay 之后使用。"))
	TArray<FTransform> GenerateInitialSpawnTransforms(UPARAM(DisplayName = "生成数量") int32 Count) const;

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线", meta = (DisplayName = "更新排队目标"))
	void UpdateQueueTargets(UPARAM(DisplayName = "DeltaTime") float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线", meta = (DisplayName = "设置排队暂停"))
	void SetQueuePaused(UPARAM(DisplayName = "暂停") bool bPaused);

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线", meta = (DisplayName = "设置成员及后方暂停", ToolTip = "暂停指定成员以及注册顺序位于其后的成员；解除时仍会保留其他成员形成的停靠状态。\n注意：暂停范围为调用时刻的快照，之后新注册的成员不受影响。"))
	bool SetMemberAndFollowingPaused(
		UPARAM(DisplayName = "成员Actor") AActor* Member,
		UPARAM(DisplayName = "暂停") bool bPaused);

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线", meta = (DisplayName = "按句柄设置成员及后方暂停", ToolTip = "暂停指定成员以及注册顺序位于其后的成员；解除时仍会保留其他成员形成的停靠状态。\n注意：暂停范围为调用时刻的快照，之后新注册的成员不受影响。"))
	bool SetMemberHandleAndFollowingPaused(
		UPARAM(DisplayName = "成员句柄") FQueueSplineMemberHandle Handle,
		UPARAM(DisplayName = "暂停") bool bPaused);

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线|服务", meta = (DisplayName = "通知成员到达服务点"))
	bool NotifyMemberReachedService(UPARAM(DisplayName = "成员Actor") AActor* Member);

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线|服务", meta = (DisplayName = "按句柄通知到达服务点"))
	bool NotifyMemberHandleReachedService(UPARAM(DisplayName = "成员句柄") FQueueSplineMemberHandle Handle);

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线|服务", meta = (DisplayName = "完成当前服务并离开"))
	bool CompleteCurrentServiceAndExit();

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线|服务", meta = (DisplayName = "设置服务锁定"))
	void SetServiceLocked(UPARAM(DisplayName = "锁定") bool bLocked);

	UFUNCTION(BlueprintPure, Category = "XTools|排队样条线", meta = (DisplayName = "排队已暂停"))
	bool IsQueuePaused() const;

	UFUNCTION(BlueprintPure, Category = "XTools|排队样条线|服务", meta = (DisplayName = "服务已锁定"))
	bool IsServiceLocked() const;

	UFUNCTION(BlueprintPure, Category = "XTools|排队样条线|服务", meta = (DisplayName = "获取当前服务成员"))
	bool GetCurrentServingMember(
		UPARAM(DisplayName = "成员句柄") FQueueSplineMemberHandle& OutHandle,
		UPARAM(DisplayName = "成员Actor") AActor*& OutActor) const;

	UFUNCTION(BlueprintPure, Category = "XTools|排队样条线", meta = (DisplayName = "获取排队成员数量"))
	int32 GetQueueMemberCount() const;

	UFUNCTION(BlueprintPure, Category = "XTools|排队样条线", meta = (DisplayName = "获取排队成员状态"))
	bool GetQueueMemberState(
		UPARAM(DisplayName = "成员句柄") FQueueSplineMemberHandle Handle,
		UPARAM(DisplayName = "成员状态") FQueueSplineMemberState& OutState) const;

	UFUNCTION(BlueprintPure, Category = "XTools|排队样条线", meta = (DisplayName = "获取排队成员目标"))
	bool GetQueueMoveTarget(
		UPARAM(DisplayName = "成员句柄") FQueueSplineMemberHandle Handle,
		UPARAM(DisplayName = "移动目标") FQueueSplineMoveTarget& OutTarget) const;

	UFUNCTION(BlueprintPure, Category = "XTools|排队样条线", meta = (DisplayName = "查找成员句柄"))
	bool FindQueueMemberHandle(
		UPARAM(DisplayName = "成员Actor") AActor* Member,
		UPARAM(DisplayName = "成员句柄") FQueueSplineMemberHandle& OutHandle) const;

	UFUNCTION(BlueprintPure, Category = "XTools|排队样条线", meta = (DisplayName = "排队配置有效"))
	bool IsQueueSplineValid(UPARAM(DisplayName = "错误信息") FString& OutMessage) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TArray<FQueueSplineMemberRuntime> Members;
	TMap<FGuid, int32> HandleToIndex;
	int32 NextGeneration = 1;
	int32 NextJoinOrder = 0;
	bool bQueuePaused = false;
	bool bServiceLocked = false;
	bool bIsEndingPlay = false;
	FQueueSplineMemberHandle CurrentServingHandle;

	/** 目标更新通知快照条目：先生成快照，再逐条广播；广播回调期间成员可增删，派发前按句柄复查有效性 */
	struct FQueueSplineTargetNotification
	{
		FQueueSplineMemberHandle Handle;
		TWeakObjectPtr<AActor> Actor;
		FQueueSplineMoveTarget Target;
	};

	/** 通知快照缓冲：非重入调用以 Reset 复用容量，避免每次更新重新分配 */
	TArray<FQueueSplineTargetNotification> NotificationBuffer;

	/** 通知缓冲使用期间（快照填充到派发结束）为真：此期间的递归 UpdateQueueTargets 改用独立局部数组，不覆盖外层快照 */
	bool bIsDispatchingNotifications = false;

	/** 测试访问授权：自动化测试读取通知缓冲状态验证复用与重入隔离（编译期授权，不新增运行时 API） */
	friend class FQueueSplineNotificationPerMemberTest;
	friend class FQueueSplineNotificationReentrancyTest;
	friend class FQueueSplineNotificationNestedReturnTest;
	friend class FQueueSplineNotificationSkipInvalidTest;

	void RefreshHandleMap();
	void CleanupInvalidMembers();
	bool IsHandleCurrent(FQueueSplineMemberHandle Handle, int32& OutIndex) const;
	double GetPathEndDistance() const;
	double GetActorSplineDistance(const AActor* Actor) const;
	double GetActorSplineRightOffset(const AActor* Actor, double SplineDistance) const;
	bool CalculateSlotForIndex(const USplineComponent* Spline, int32 SlotIndex, int32 MemberSeed, int32 QueueMemberCount, FQueueSplineSlot& OutSlot) const;
	FQueueSplineMoveTarget BuildMoveTarget(const FQueueSplineMemberRuntime& Member) const;
	FQueueSplineMoveTarget BuildMoveTargetAtDistance(FQueueSplineMemberHandle Handle, AActor* Actor, double Distance, double RightOffset, EQueueSplineMemberPhase Phase) const;
	void PushTargetToMovementComponent(AActor* Actor, const FQueueSplineMoveTarget& Target) const;
	void StopMovementComponent(AActor* Actor) const;
	void RefreshMovementPauseStates();
	void DrawDebugSlots() const;
};
