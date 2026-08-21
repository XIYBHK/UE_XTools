#include "QueueSplineComponent.h"

#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "QueueSplineLog.h"
#include "QueueSplineMovementComponent.h"
#include "QueueSplineSubsystem.h"
#include "XToolsErrorReporter.h"

namespace
{
	FVector GetPlanarDelta(const FVector& From, const FVector& To)
	{
		FVector Delta = To - From;
		Delta.Z = 0.0;
		return Delta;
	}

	bool IsSplineUsable(const USplineComponent* SplineComponent)
	{
		return IsValid(SplineComponent)
			&& SplineComponent->GetNumberOfSplinePoints() >= 2
			&& SplineComponent->GetSplineLength() > KINDA_SMALL_NUMBER;
	}

	double ClampDistance(double Distance, double SplineLength)
	{
		return SplineLength > static_cast<double>(KINDA_SMALL_NUMBER)
			? FMath::Clamp(Distance, 0.0, SplineLength)
			: 0.0;
	}

	double RandomRange(FRandomStream& RandomStream, double Radius)
	{
		if (Radius <= static_cast<double>(KINDA_SMALL_NUMBER))
		{
			return 0.0;
		}

		return static_cast<double>(RandomStream.FRandRange(
			static_cast<float>(-Radius),
			static_cast<float>(Radius)));
	}

	/** RAII 派发守卫：构造置位、析构恢复进入前状态；嵌套调用返回时不清零外层仍在派发的状态 */
	struct FScopedNotificationDispatch
	{
		bool& bDispatching;
		bool PreviousValue;

		explicit FScopedNotificationDispatch(bool& InDispatching)
			: bDispatching(InDispatching)
			, PreviousValue(InDispatching)
		{
			bDispatching = true;
		}

		~FScopedNotificationDispatch()
		{
			bDispatching = PreviousValue;
		}
	};
}

UQueueSplineComponent::UQueueSplineComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UQueueSplineComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UQueueSplineSubsystem* Subsystem = World->GetSubsystem<UQueueSplineSubsystem>())
		{
			Subsystem->RegisterQueue(this);
		}
	}
}

void UQueueSplineComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bIsEndingPlay = true;
	ClearQueueMembers();

	if (UWorld* World = GetWorld())
	{
		if (UQueueSplineSubsystem* Subsystem = World->GetSubsystem<UQueueSplineSubsystem>())
		{
			Subsystem->UnregisterQueue(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UQueueSplineComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 最佳实践：不需要每帧刷新时按更新间隔降频（样条投影查询开销随成员数线性增长）
	const float SafeUpdateInterval = FMath::Max(UpdateInterval, 0.0f);
	if (!FMath::IsNearlyEqual(PrimaryComponentTick.TickInterval, SafeUpdateInterval))
	{
		SetComponentTickInterval(SafeUpdateInterval);
	}

	// 失效成员清理不受暂停影响：成员销毁后始终应触发注销事件
	CleanupInvalidMembers();

	if (!bAutoUpdate || bQueuePaused)
	{
		return;
	}

	UpdateQueueTargets(DeltaTime);
}

bool UQueueSplineComponent::RegisterQueueMember(AActor* Member, FQueueSplineMemberHandle& OutHandle)
{
	OutHandle = FQueueSplineMemberHandle();
	if (bIsEndingPlay || !IsValid(Member))
	{
		return false;
	}

	FQueueSplineMemberHandle ExistingHandle;
	if (FindQueueMemberHandle(Member, ExistingHandle))
	{
		OutHandle = ExistingHandle;
		return true;
	}

	FString ValidationMessage;
	if (!IsQueueSplineValid(ValidationMessage))
	{
		return false;
	}

	FQueueSplineMemberRuntime Runtime;
	Runtime.Handle.Id = FGuid::NewGuid();
	Runtime.Handle.Generation = NextGeneration++;
	Runtime.Actor = Member;
	Runtime.JoinOrder = NextJoinOrder++;
	Runtime.RandomSeed = Settings.RandomSeed + Runtime.JoinOrder * 7919;
	Runtime.CurrentSplineDistance = GetActorSplineDistance(Member);
	Runtime.bHasSplineDistance = true;
	Runtime.CurrentRightOffset = GetActorSplineRightOffset(Member, Runtime.CurrentSplineDistance);
	Runtime.Phase = EQueueSplineMemberPhase::Queued;

	Members.Add(Runtime);
	RefreshHandleMap();
	RebuildQueueSlots();

	if (UQueueSplineMovementComponent* MovementComponent = Member->FindComponentByClass<UQueueSplineMovementComponent>())
	{
		MovementComponent->SetQueueOwner(this);
		if (bAutoPushToMovementComponent)
		{
			MovementComponent->AddTickPrerequisiteComponent(this);
		}
	}
	RefreshMovementPauseStates();

	OutHandle = Runtime.Handle;
	OnMemberRegistered.Broadcast(OutHandle);
	return true;
}

bool UQueueSplineComponent::UnregisterQueueMember(FQueueSplineMemberHandle Handle)
{
	int32 MemberIndex = INDEX_NONE;
	if (!IsHandleCurrent(Handle, MemberIndex))
	{
		return false;
	}

	AActor* MemberActor = Members[MemberIndex].Actor.Get();
	Members.RemoveAt(MemberIndex);
	if (CurrentServingHandle == Handle)
	{
		CurrentServingHandle = FQueueSplineMemberHandle();
		bServiceLocked = false;
	}
	RefreshHandleMap();
	RebuildQueueSlots();

	if (IsValid(MemberActor))
	{
		if (UQueueSplineMovementComponent* MovementComponent = MemberActor->FindComponentByClass<UQueueSplineMovementComponent>())
		{
			MovementComponent->RemoveTickPrerequisiteComponent(this);
			MovementComponent->SetQueueOwner(nullptr);
			MovementComponent->ApplyQueueMovementPaused(false);
		}
	}
	RefreshMovementPauseStates();
	if (bAutoPushToMovementComponent)
	{
		StopMovementComponent(MemberActor);
	}
	OnMemberUnregistered.Broadcast(Handle);
	if (Members.Num() == 0)
	{
		OnQueueEmpty.Broadcast();
	}

	return true;
}

bool UQueueSplineComponent::UnregisterQueueActor(AActor* Member)
{
	FQueueSplineMemberHandle Handle;
	return FindQueueMemberHandle(Member, Handle) && UnregisterQueueMember(Handle);
}

void UQueueSplineComponent::ClearQueueMembers()
{
	if (Members.Num() == 0)
	{
		return;
	}

	const TArray<FQueueSplineMemberRuntime> PreviousMembers = Members;
	Members.Reset();
	RefreshHandleMap();
	CurrentServingHandle = FQueueSplineMemberHandle();
	bServiceLocked = false;

	for (const FQueueSplineMemberRuntime& Member : PreviousMembers)
	{
		if (IsValid(Member.Actor.Get()))
		{
			if (UQueueSplineMovementComponent* MovementComponent = Member.Actor->FindComponentByClass<UQueueSplineMovementComponent>())
			{
				MovementComponent->RemoveTickPrerequisiteComponent(this);
				MovementComponent->SetQueueOwner(nullptr);
				MovementComponent->ApplyQueueMovementPaused(false);
			}
		}
		if (bAutoPushToMovementComponent)
		{
			StopMovementComponent(Member.Actor.Get());
		}
		OnMemberUnregistered.Broadcast(Member.Handle);
	}
	if (Members.Num() == 0)
	{
		OnQueueEmpty.Broadcast();
	}
}

bool UQueueSplineComponent::RebuildQueueSlots()
{
	FString ErrorMessage;
	if (!IsQueueSplineValid(ErrorMessage))
	{
		return false;
	}

	CleanupInvalidMembers();

	Members.Sort([](const FQueueSplineMemberRuntime& A, const FQueueSplineMemberRuntime& B)
	{
		return A.JoinOrder < B.JoinOrder;
	});

	int32 QueueMemberCount = 0;
	for (const FQueueSplineMemberRuntime& Member : Members)
	{
		if (Member.Phase != EQueueSplineMemberPhase::Exiting)
		{
			++QueueMemberCount;
		}
	}

	int32 SlotIndex = 0;
	for (FQueueSplineMemberRuntime& Member : Members)
	{
		if (Member.Phase == EQueueSplineMemberPhase::Exiting)
		{
			Member.SlotIndex = INDEX_NONE;
			continue;
		}

		FQueueSplineSlot Slot;
		if (!CalculateSlotForIndex(SplineComponent, SlotIndex, Member.RandomSeed, QueueMemberCount, Slot))
		{
			return false;
		}

		Member.SlotIndex = SlotIndex;
		Member.Slot = Slot;
		++SlotIndex;
	}

	RefreshHandleMap();
	return true;
}

TArray<FTransform> UQueueSplineComponent::GenerateInitialSpawnTransforms(int32 Count) const
{
	TArray<FTransform> Transforms;
	if (Count <= 0)
	{
		return Transforms;
	}

	// 样条未手动设置时，自动使用所属 Actor 上的第一个样条组件（蓝图常见用法）
	const USplineComponent* EffectiveSpline = SplineComponent;
	if (!IsValid(EffectiveSpline))
	{
		if (const AActor* Owner = GetOwner())
		{
			EffectiveSpline = Owner->FindComponentByClass<USplineComponent>();
		}
		if (IsValid(EffectiveSpline))
		{
			XTOOLS_LOG_INFO(LogQueueSpline, TEXT("生成初始排队Transform：样条组件未设置，已自动使用所属Actor上的第一个 USplineComponent。"));
		}
	}

	if (!IsValid(EffectiveSpline))
	{
		FXToolsErrorReporter::Warning(LogQueueSpline,
			TEXT("生成初始排队Transform失败：样条组件未设置，且所属Actor上没有可用的 USplineComponent。"),
			NAME_None, true, 5.0f);
		return Transforms;
	}
	if (EffectiveSpline->GetNumberOfSplinePoints() < 2)
	{
		FXToolsErrorReporter::Warning(LogQueueSpline,
			TEXT("生成初始排队Transform失败：样条点数量不足，至少需要 2 个点。"),
			NAME_None, true, 5.0f);
		return Transforms;
	}
	if (EffectiveSpline->GetSplineLength() <= KINDA_SMALL_NUMBER)
	{
		FXToolsErrorReporter::Warning(LogQueueSpline,
			TEXT("生成初始排队Transform失败：样条长度过短。"),
			NAME_None, true, 5.0f);
		return Transforms;
	}
	if (!EffectiveSpline->IsRegistered())
	{
		// 未注册组件的 ComponentToWorld 为单位变换，世界坐标查询会退化为局部坐标（看起来像原点附近的值）
		XTOOLS_LOG_WARNING(LogQueueSpline, TEXT("生成初始排队Transform：样条组件尚未注册（如在 Construction Script 中调用），返回的位置将是样条局部坐标而非世界坐标。"));
	}

	if (Settings.FillMode == EQueueSplineFillMode::FromStart)
	{
		const double SplineLength = static_cast<double>(EffectiveSpline->GetSplineLength());
		const double EntryOffset = ClampDistance(
			Settings.EntryDistance,
			SplineLength);
		const double EntryDistance = Settings.bHeadTowardSplineEnd
			? EntryOffset
			: SplineLength - EntryOffset;
		const FVector CenterLocation = EffectiveSpline->GetLocationAtDistanceAlongSpline(
			static_cast<float>(EntryDistance),
			ESplineCoordinateSpace::World);
		const FVector RightVector = EffectiveSpline->GetRightVectorAtDistanceAlongSpline(
			static_cast<float>(EntryDistance),
			ESplineCoordinateSpace::World);
		const FRotator Rotation = EffectiveSpline->GetRotationAtDistanceAlongSpline(
			static_cast<float>(EntryDistance),
			ESplineCoordinateSpace::World);

		FRandomStream RandomStream(Settings.RandomSeed + NextJoinOrder * 7919);
		const double SideJitter = RandomRange(RandomStream, FMath::Max(Settings.SideJitter, 0.0));
		const double SideSign = Settings.bAlternateSides
			? ((NextJoinOrder % 2) == 0 ? 1.0 : -1.0)
			: 0.0;
		const double RightOffset = SideSign * FMath::Max(Settings.SideOffset, 0.0) + SideJitter;
		Transforms.Add(FTransform(
			Rotation,
			CenterLocation + RightVector * static_cast<float>(RightOffset)));
		return Transforms;
	}

	Transforms.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FQueueSplineSlot Slot;
		if (CalculateSlotForIndex(EffectiveSpline, Index, Settings.RandomSeed + Index * 7919, Count, Slot))
		{
			Transforms.Add(FTransform(Slot.TargetRotation, Slot.TargetLocation));
		}
	}

	return Transforms;
}

void UQueueSplineComponent::UpdateQueueTargets(float DeltaTime)
{
	CleanupInvalidMembers();
	if (Members.Num() == 0)
	{
		return;
	}

	const double SafeDeltaTime = FMath::Max(static_cast<double>(DeltaTime), 0.0);
	const float InterpSpeed = static_cast<float>(FMath::Max(Settings.OffsetReturnSpeed, 0.0));

	// 非重入调用复用成员缓冲（Reset 保留容量，摊销零分配）；
	// 缓冲使用期间（快照填充到派发结束）的递归调用改用独立局部数组，不覆盖外层快照
	TArray<FQueueSplineTargetNotification> ReentrantNotifications;
	TArray<FQueueSplineTargetNotification>* NotificationsPtr = &NotificationBuffer;
	if (bIsDispatchingNotifications)
	{
		NotificationsPtr = &ReentrantNotifications;
		ReentrantNotifications.Reserve(Members.Num());
	}
	else
	{
		NotificationBuffer.Reset();
	}
	TArray<FQueueSplineTargetNotification>& Notifications = *NotificationsPtr;

	{
		// 守卫覆盖快照填充与派发全程：期间任何重入（含移动组件事件回调）都走局部数组
		FScopedNotificationDispatch DispatchGuard(bIsDispatchingNotifications);

		for (FQueueSplineMemberRuntime& Member : Members)
		{
			AActor* Actor = Member.Actor.Get();
			if (!IsValid(Actor))
			{
				continue;
			}

			Member.CurrentRightOffset = static_cast<double>(FMath::FInterpTo(
				static_cast<float>(Member.CurrentRightOffset),
				0.0f,
				static_cast<float>(SafeDeltaTime),
				InterpSpeed));

			const double ProjectedDistance = GetActorSplineDistance(Actor);
			if (!Member.bHasSplineDistance)
			{
				Member.CurrentSplineDistance = ProjectedDistance;
				Member.bHasSplineDistance = true;
			}
			else if (Settings.bHeadTowardSplineEnd)
			{
				Member.CurrentSplineDistance = FMath::Max(Member.CurrentSplineDistance, ProjectedDistance);
			}
			else
			{
				Member.CurrentSplineDistance = FMath::Min(Member.CurrentSplineDistance, ProjectedDistance);
			}

			FQueueSplineMoveTarget Target = BuildMoveTarget(Member);
			Member.bReachedSlot = Target.bReachedSlot;

			FQueueSplineTargetNotification& Notification = Notifications.AddDefaulted_GetRef();
			Notification.Handle = Member.Handle;
			Notification.Actor = Actor;
			Notification.Target = Target;
		}
		RefreshMovementPauseStates();

		for (const FQueueSplineTargetNotification& Notification : Notifications)
		{
			int32 MemberIndex = INDEX_NONE;
			if (!IsHandleCurrent(Notification.Handle, MemberIndex))
			{
				continue;
			}

			AActor* Actor = Notification.Actor.Get();
			if (!IsValid(Actor))
			{
				continue;
			}

			if (bAutoPushToMovementComponent)
			{
				PushTargetToMovementComponent(Actor, Notification.Target);
			}
			OnMemberTargetUpdated.Broadcast(Notification.Handle, Notification.Target);
		}
	}

	if (bDrawDebug)
	{
		DrawDebugSlots();
	}
}

void UQueueSplineComponent::SetQueuePaused(bool bPaused)
{
	bQueuePaused = bPaused;
	RefreshMovementPauseStates();
}

bool UQueueSplineComponent::SetMemberAndFollowingPaused(AActor* Member, bool bPaused)
{
	FQueueSplineMemberHandle Handle;
	return FindQueueMemberHandle(Member, Handle)
		&& SetMemberHandleAndFollowingPaused(Handle, bPaused);
}

bool UQueueSplineComponent::SetMemberHandleAndFollowingPaused(
	FQueueSplineMemberHandle Handle,
	bool bPaused)
{
	int32 MemberIndex = INDEX_NONE;
	if (!IsHandleCurrent(Handle, MemberIndex))
	{
		return false;
	}

	Members[MemberIndex].bPauseRequested = bPaused;
	Members[MemberIndex].PauseThroughJoinOrder = bPaused && Members.Num() > 0
		? Members.Last().JoinOrder
		: INDEX_NONE;
	RefreshMovementPauseStates();
	return true;
}

bool UQueueSplineComponent::IsQueuePaused() const
{
	return bQueuePaused;
}

bool UQueueSplineComponent::NotifyMemberReachedService(AActor* Member)
{
	FQueueSplineMemberHandle Handle;
	return FindQueueMemberHandle(Member, Handle) && NotifyMemberHandleReachedService(Handle);
}

bool UQueueSplineComponent::NotifyMemberHandleReachedService(FQueueSplineMemberHandle Handle)
{
	int32 MemberIndex = INDEX_NONE;
	if (!IsHandleCurrent(Handle, MemberIndex) || bServiceLocked)
	{
		return false;
	}

	FQueueSplineMemberRuntime& Member = Members[MemberIndex];
	if (Member.Phase == EQueueSplineMemberPhase::Exiting)
	{
		return false;
	}

	Member.Phase = EQueueSplineMemberPhase::Serving;
	bServiceLocked = true;
	CurrentServingHandle = Handle;
	OnMemberServiceStarted.Broadcast(Handle);
	return true;
}

bool UQueueSplineComponent::CompleteCurrentServiceAndExit()
{
	int32 MemberIndex = INDEX_NONE;
	if (!IsHandleCurrent(CurrentServingHandle, MemberIndex))
	{
		return false;
	}

	FQueueSplineMemberRuntime& Member = Members[MemberIndex];
	Member.Phase = EQueueSplineMemberPhase::Exiting;
	bServiceLocked = false;

	const FQueueSplineMemberHandle CompletedHandle = CurrentServingHandle;
	CurrentServingHandle = FQueueSplineMemberHandle();
	OnMemberServiceCompleted.Broadcast(CompletedHandle);
	RebuildQueueSlots();
	return true;
}

void UQueueSplineComponent::SetServiceLocked(bool bLocked)
{
	bServiceLocked = bLocked;
}

bool UQueueSplineComponent::IsServiceLocked() const
{
	return bServiceLocked;
}

bool UQueueSplineComponent::GetCurrentServingMember(FQueueSplineMemberHandle& OutHandle, AActor*& OutActor) const
{
	OutHandle = FQueueSplineMemberHandle();
	OutActor = nullptr;

	int32 MemberIndex = INDEX_NONE;
	if (!IsHandleCurrent(CurrentServingHandle, MemberIndex))
	{
		return false;
	}

	OutHandle = CurrentServingHandle;
	OutActor = Members[MemberIndex].Actor.Get();
	return IsValid(OutActor);
}

int32 UQueueSplineComponent::GetQueueMemberCount() const
{
	int32 Count = 0;
	for (const FQueueSplineMemberRuntime& Member : Members)
	{
		if (Member.Phase != EQueueSplineMemberPhase::Exiting)
		{
			++Count;
		}
	}
	return Count;
}

bool UQueueSplineComponent::GetQueueMemberState(FQueueSplineMemberHandle Handle, FQueueSplineMemberState& OutState) const
{
	OutState = FQueueSplineMemberState();

	int32 MemberIndex = INDEX_NONE;
	if (!IsHandleCurrent(Handle, MemberIndex))
	{
		return false;
	}

	const FQueueSplineMemberRuntime& Member = Members[MemberIndex];
	const FQueueSplineMoveTarget Target = BuildMoveTarget(Member);
	OutState.Handle = Member.Handle;
	OutState.Actor = Member.Actor.Get();
	OutState.SlotIndex = Member.SlotIndex;
	OutState.TargetDistance = Target.Slot.Distance;
	OutState.CurrentRightOffset = Member.CurrentRightOffset;
	OutState.TargetRightOffset = 0.0;
	OutState.bReachedSlot = Target.bReachedSlot;
	OutState.Phase = Member.Phase;
	return true;
}

bool UQueueSplineComponent::GetQueueMoveTarget(FQueueSplineMemberHandle Handle, FQueueSplineMoveTarget& OutTarget) const
{
	OutTarget = FQueueSplineMoveTarget();

	int32 MemberIndex = INDEX_NONE;
	if (!IsHandleCurrent(Handle, MemberIndex))
	{
		return false;
	}

	const FQueueSplineMemberRuntime& Member = Members[MemberIndex];
	OutTarget = BuildMoveTarget(Member);
	return true;
}

bool UQueueSplineComponent::FindQueueMemberHandle(AActor* Member, FQueueSplineMemberHandle& OutHandle) const
{
	OutHandle = FQueueSplineMemberHandle();
	if (!IsValid(Member))
	{
		return false;
	}

	for (const FQueueSplineMemberRuntime& Runtime : Members)
	{
		if (Runtime.Actor.Get() == Member)
		{
			OutHandle = Runtime.Handle;
			return true;
		}
	}

	return false;
}

bool UQueueSplineComponent::IsQueueSplineValid(FString& OutMessage) const
{
	OutMessage.Reset();

	if (!IsSplineUsable(SplineComponent))
	{
		OutMessage = TEXT("样条组件无效、样条点不足或样条长度过短。");
		return false;
	}

	if (Settings.Spacing <= 0.0)
	{
		OutMessage = TEXT("前后间距必须大于0。");
		return false;
	}

	OutMessage = TEXT("配置有效。");
	return true;
}

void UQueueSplineComponent::RefreshHandleMap()
{
	HandleToIndex.Reset();
	for (int32 Index = 0; Index < Members.Num(); ++Index)
	{
		if (Members[Index].Handle.Id.IsValid())
		{
			HandleToIndex.Add(Members[Index].Handle.Id, Index);
		}
	}
}

void UQueueSplineComponent::CleanupInvalidMembers()
{
	bool bRemovedAny = false;
	TArray<FQueueSplineMemberHandle> RemovedHandles;
	for (int32 Index = Members.Num() - 1; Index >= 0; --Index)
	{
		if (!Members[Index].Actor.IsValid())
		{
			if (CurrentServingHandle == Members[Index].Handle)
			{
				CurrentServingHandle = FQueueSplineMemberHandle();
				bServiceLocked = false;
			}

			RemovedHandles.Add(Members[Index].Handle);
			Members.RemoveAt(Index);
			bRemovedAny = true;
		}
	}

	if (bRemovedAny)
	{
		RefreshHandleMap();
		RefreshMovementPauseStates();
		for (const FQueueSplineMemberHandle& Handle : RemovedHandles)
		{
			OnMemberUnregistered.Broadcast(Handle);
		}
		if (Members.Num() == 0)
		{
			OnQueueEmpty.Broadcast();
		}
	}
}

bool UQueueSplineComponent::IsHandleCurrent(FQueueSplineMemberHandle Handle, int32& OutIndex) const
{
	OutIndex = INDEX_NONE;
	const int32* FoundIndex = HandleToIndex.Find(Handle.Id);
	if (!FoundIndex || !Members.IsValidIndex(*FoundIndex))
	{
		return false;
	}

	const FQueueSplineMemberRuntime& Member = Members[*FoundIndex];
	if (!(Member.Handle == Handle))
	{
		return false;
	}

	OutIndex = *FoundIndex;
	return true;
}

double UQueueSplineComponent::GetPathEndDistance() const
{
	if (!IsSplineUsable(SplineComponent))
	{
		return 0.0;
	}

	const double SplineLength = static_cast<double>(SplineComponent->GetSplineLength());
	return Settings.bHeadTowardSplineEnd ? SplineLength : 0.0;
}

double UQueueSplineComponent::GetActorSplineDistance(const AActor* Actor) const
{
	if (!IsValid(Actor) || !IsSplineUsable(SplineComponent))
	{
		return 0.0;
	}

	const double SplineLength = static_cast<double>(SplineComponent->GetSplineLength());
	const float InputKey = SplineComponent->FindInputKeyClosestToWorldLocation(Actor->GetActorLocation());
	return ClampDistance(
		static_cast<double>(SplineComponent->GetDistanceAlongSplineAtSplineInputKey(InputKey)),
		SplineLength);
}

double UQueueSplineComponent::GetActorSplineRightOffset(const AActor* Actor, double SplineDistance) const
{
	if (!IsValid(Actor) || !IsSplineUsable(SplineComponent))
	{
		return 0.0;
	}

	const double ClampedDistance = ClampDistance(
		SplineDistance,
		static_cast<double>(SplineComponent->GetSplineLength()));
	const FVector CenterLocation = SplineComponent->GetLocationAtDistanceAlongSpline(
		static_cast<float>(ClampedDistance),
		ESplineCoordinateSpace::World);
	const FVector RightVector = SplineComponent->GetRightVectorAtDistanceAlongSpline(
		static_cast<float>(ClampedDistance),
		ESplineCoordinateSpace::World);
	return static_cast<double>(FVector::DotProduct(Actor->GetActorLocation() - CenterLocation, RightVector));
}

bool UQueueSplineComponent::CalculateSlotForIndex(
	const USplineComponent* Spline,
	int32 SlotIndex,
	int32 MemberSeed,
	int32 QueueMemberCount,
	FQueueSplineSlot& OutSlot) const
{
	OutSlot = FQueueSplineSlot();
	if (!IsSplineUsable(Spline) || SlotIndex < 0)
	{
		return false;
	}

	const double SplineLength = static_cast<double>(Spline->GetSplineLength());
	const double SafeSpacing = FMath::Max(Settings.Spacing, 1.0);
	const double QueueLength = SafeSpacing * static_cast<double>(FMath::Max(QueueMemberCount - 1, 0));
	const double SafeEntryDistance = ClampDistance(Settings.EntryDistance, SplineLength);
	const double DirectionSign = Settings.bHeadTowardSplineEnd ? -1.0 : 1.0;

	double HeadDistance = 0.0;
	if (Settings.FillMode == EQueueSplineFillMode::FromStart)
	{
		HeadDistance = Settings.bHeadTowardSplineEnd
			? SafeEntryDistance + QueueLength
			: SplineLength - SafeEntryDistance - QueueLength;
	}
	else
	{
		const double FillDistance = SplineLength * FMath::Clamp(Settings.FillRatio, 0.0, 1.0);
		HeadDistance = Settings.bHeadTowardSplineEnd ? FillDistance : SplineLength - FillDistance;
	}

	FRandomStream RandomStream(MemberSeed);
	const double DistanceJitter = RandomRange(RandomStream, FMath::Max(Settings.DistanceJitter, 0.0));
	const double SideJitter = RandomRange(RandomStream, FMath::Max(Settings.SideJitter, 0.0));

	double Distance = HeadDistance + DirectionSign * (SafeSpacing * static_cast<double>(SlotIndex) + DistanceJitter);
	if (Settings.bClampToSpline)
	{
		Distance = ClampDistance(Distance, SplineLength);
	}

	const double SideSign = Settings.bAlternateSides
		? ((SlotIndex % 2) == 0 ? 1.0 : -1.0)
		: 0.0;

	const double RightOffset = SideSign * FMath::Max(Settings.SideOffset, 0.0) + SideJitter;
	const FVector CenterLocation = Spline->GetLocationAtDistanceAlongSpline(
		static_cast<float>(Distance),
		ESplineCoordinateSpace::World);
	const FVector RightVector = Spline->GetRightVectorAtDistanceAlongSpline(
		static_cast<float>(Distance),
		ESplineCoordinateSpace::World);

	OutSlot.SlotIndex = SlotIndex;
	OutSlot.Distance = Distance;
	OutSlot.RightOffset = RightOffset;
	OutSlot.CenterLocation = CenterLocation;
	OutSlot.TargetLocation = CenterLocation + RightVector * static_cast<float>(RightOffset);
	OutSlot.TargetRotation = Spline->GetRotationAtDistanceAlongSpline(
		static_cast<float>(Distance),
		ESplineCoordinateSpace::World);
	return true;
}

FQueueSplineMoveTarget UQueueSplineComponent::BuildMoveTarget(const FQueueSplineMemberRuntime& Member) const
{
	AActor* Actor = Member.Actor.Get();
	if (!IsValid(Actor) || !IsSplineUsable(SplineComponent))
	{
		FQueueSplineMoveTarget Target;
		Target.Handle = Member.Handle;
		Target.Phase = Member.Phase;
		return Target;
	}

	const double SplineLength = static_cast<double>(SplineComponent->GetSplineLength());
	const double EndDistance = GetPathEndDistance();
	const double CurrentDistance = ClampDistance(
		Member.bHasSplineDistance ? Member.CurrentSplineDistance : GetActorSplineDistance(Actor),
		SplineLength);
	const double LookAhead = FMath::Max(Settings.ExitLookAheadDistance, 1.0);
	const double TargetDistance = Settings.bHeadTowardSplineEnd
		? FMath::Min(CurrentDistance + LookAhead, EndDistance)
		: FMath::Max(CurrentDistance - LookAhead, EndDistance);
	const bool bAtPathEnd = FMath::IsNearlyEqual(TargetDistance, EndDistance, 0.1);
	const double TargetRightOffset = bAtPathEnd ? 0.0 : Member.CurrentRightOffset;

	FQueueSplineMoveTarget Target = BuildMoveTargetAtDistance(
		Member.Handle,
		Actor,
		TargetDistance,
		TargetRightOffset,
		Member.Phase);
	// 透传真实槽位索引：移动组件据此判断槽位变化并重置到达锁存，同时与「获取排队成员状态」的槽位索引保持一致
	Target.Slot.SlotIndex = Member.SlotIndex;
	const FVector EndLocation = SplineComponent->GetLocationAtDistanceAlongSpline(
		static_cast<float>(EndDistance),
		ESplineCoordinateSpace::World);
	Target.bReachedSlot = GetPlanarDelta(Actor->GetActorLocation(), EndLocation).SizeSquared()
		<= FMath::Square(static_cast<float>(FMath::Max(Settings.ArrivalTolerance, 0.0)));
	if (Target.bReachedSlot)
	{
		Target.MoveDirection = FVector::ZeroVector;
	}
	return Target;
}

FQueueSplineMoveTarget UQueueSplineComponent::BuildMoveTargetAtDistance(
	FQueueSplineMemberHandle Handle,
	AActor* Actor,
	double Distance,
	double RightOffset,
	EQueueSplineMemberPhase Phase) const
{
	FQueueSplineMoveTarget Target;
	Target.Handle = Handle;
	Target.Phase = Phase;

	if (!IsValid(Actor) || !IsSplineUsable(SplineComponent))
	{
		return Target;
	}

	const double SplineLength = static_cast<double>(SplineComponent->GetSplineLength());
	const double ClampedDistance = ClampDistance(Distance, SplineLength);
	Target.Slot.SlotIndex = INDEX_NONE;
	Target.Slot.Distance = ClampedDistance;
	Target.Slot.CenterLocation = SplineComponent->GetLocationAtDistanceAlongSpline(
		static_cast<float>(ClampedDistance),
		ESplineCoordinateSpace::World);
	const FVector RightVector = SplineComponent->GetRightVectorAtDistanceAlongSpline(
		static_cast<float>(ClampedDistance),
		ESplineCoordinateSpace::World);
	Target.Slot.RightOffset = RightOffset;
	Target.Slot.TargetLocation = Target.Slot.CenterLocation + RightVector * static_cast<float>(RightOffset);
	Target.Slot.TargetRotation = SplineComponent->GetRotationAtDistanceAlongSpline(
		static_cast<float>(ClampedDistance),
		ESplineCoordinateSpace::World);

	const FVector ToTarget = GetPlanarDelta(Actor->GetActorLocation(), Target.Slot.TargetLocation);
	Target.DistanceToTarget = static_cast<double>(ToTarget.Size());
	Target.MoveDirection = ToTarget.GetSafeNormal();
	return Target;
}

void UQueueSplineComponent::PushTargetToMovementComponent(AActor* Actor, const FQueueSplineMoveTarget& Target) const
{
	if (!IsValid(Actor))
	{
		return;
	}

	UQueueSplineMovementComponent* MovementComponent = Actor->FindComponentByClass<UQueueSplineMovementComponent>();
	if (MovementComponent)
	{
		MovementComponent->SetQueueMoveTarget(Target);
	}
}

void UQueueSplineComponent::StopMovementComponent(AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return;
	}

	UQueueSplineMovementComponent* MovementComponent = Actor->FindComponentByClass<UQueueSplineMovementComponent>();
	if (MovementComponent)
	{
		MovementComponent->StopQueueMovement();
	}
}

void UQueueSplineComponent::RefreshMovementPauseStates()
{
	int32 FirstPauseJoinOrder = MAX_int32;
	int32 ImmediatePauseThroughJoinOrder = INDEX_NONE;
	for (const FQueueSplineMemberRuntime& Member : Members)
	{
		if (Member.bPauseRequested)
		{
			FirstPauseJoinOrder = FMath::Min(FirstPauseJoinOrder, Member.JoinOrder);
			ImmediatePauseThroughJoinOrder = FMath::Max(
				ImmediatePauseThroughJoinOrder,
				Member.PauseThroughJoinOrder);
		}
	}

	const bool bHasPauseBarrier = FirstPauseJoinOrder != MAX_int32;
	const FQueueSplineMemberRuntime* PreviousMember = nullptr;
	bool bPreviousMemberPaused = false;
	for (const FQueueSplineMemberRuntime& Member : Members)
	{
		AActor* Actor = Member.Actor.Get();
		if (!IsValid(Actor))
		{
			continue;
		}

		if (UQueueSplineMovementComponent* MovementComponent = Actor->FindComponentByClass<UQueueSplineMovementComponent>())
		{
			const bool bInImmediatePauseRange = bHasPauseBarrier
				&& Member.JoinOrder >= FirstPauseJoinOrder
				&& Member.JoinOrder <= ImmediatePauseThroughJoinOrder;

			bool bBlockedByFollowingDistance = false;
			if (bHasPauseBarrier
				&& Member.JoinOrder > ImmediatePauseThroughJoinOrder
				&& bPreviousMemberPaused
				&& PreviousMember)
			{
				const double PreviousDistance = GetActorSplineDistance(PreviousMember->Actor.Get());
				const double CurrentDistance = GetActorSplineDistance(Actor);
				const double DistanceBehindPrevious = Settings.bHeadTowardSplineEnd
					? PreviousDistance - CurrentDistance
					: CurrentDistance - PreviousDistance;
				const double StopDistance = FMath::Max(Settings.Spacing, 1.0)
					+ FMath::Max(Settings.ArrivalTolerance, 0.0);
				bBlockedByFollowingDistance = DistanceBehindPrevious <= StopDistance;
			}

			const bool bShouldPause = bQueuePaused
				|| bInImmediatePauseRange
				|| bBlockedByFollowingDistance;
			MovementComponent->ApplyQueueMovementPaused(bShouldPause);
			bPreviousMemberPaused = bShouldPause;
		}
		else
		{
			bPreviousMemberPaused = false;
		}

		PreviousMember = &Member;
	}
}

void UQueueSplineComponent::DrawDebugSlots() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const FQueueSplineMemberRuntime& Member : Members)
	{
		// 黄色：真实槽位（预填充/状态查询用）；青色：当前前视移动目标
		if (Member.SlotIndex != INDEX_NONE)
		{
			DrawDebugSphere(World, Member.Slot.TargetLocation, 10.0f, 8, FColor::Yellow, false, DebugDrawTime);
		}

		const FQueueSplineMoveTarget Target = BuildMoveTarget(Member);
		DrawDebugSphere(World, Target.Slot.TargetLocation, 18.0f, 12, FColor::Cyan, false, DebugDrawTime);
		DrawDebugLine(World, Target.Slot.CenterLocation, Target.Slot.TargetLocation, FColor::Green, false, DebugDrawTime, 0, 2.0f);
	}
}
