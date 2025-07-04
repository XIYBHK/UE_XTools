// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#include "ECFSubsystem.h"
#include "ECFActionBase.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

DEFINE_STAT(STAT_ECF_ActionsCount);
DEFINE_STAT(STAT_ECF_InstancesCount);

void UECFSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// 只有游戏世界的子系统可以 tick
	bCanTick = false;
	if (UWorld* ThisWorld = GetWorld())
	{
		switch (ThisWorld->WorldType)
		{
			case EWorldType::Game:
			case EWorldType::GamePreview:
			case EWorldType::GameRPC:
			case EWorldType::PIE:
				bCanTick = true;
		}
	}

	// 重置句柄ID计数器
	LastHandleId.Invalidate();
}

void UECFSubsystem::Deinitialize()
{
	Actions.Empty();
	PendingAddActions.Empty();
}

UECFSubsystem* UECFSubsystem::Get(const UObject* WorldContextObject)
{
	UWorld* ThisWorld = nullptr;
	if (GEngine)
	{
		ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	}

	ensureAlwaysMsgf(ThisWorld, TEXT("Can't obtain ThisWorld from WorldContextObject in ECF!"));
	if (ThisWorld)
	{
		UGameInstance* GameInstance = ThisWorld->GetGameInstance();

		ensureAlwaysMsgf(GameInstance, TEXT("Can't obtain GameInstance from WorldContextObject in ECF!"));
		if (GameInstance)
		{
			return GameInstance->GetSubsystem<UECFSubsystem>();
		}
	}

	return nullptr;
}	

void UECFSubsystem::Tick(float DeltaTime)
{
	// 当整个子系统暂停时不执行任何操作
	if (bIsECFPaused)
	{
		return;
	}

#if STATS
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Tick"), STAT_ECF_TickAll, STATGROUP_ECF);
#endif

#if ECF_INSIGHT_PROFILING
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ECF-Actions-Tick");
#endif

	// 首先移除所有过期的动作
	Actions.RemoveAll([](UECFActionBase* Action) { return IsActionValid(Action) == false; });

	// 可能存在待添加的动作也无效的情况
	PendingAddActions.RemoveAll([&](UECFActionBase* PendingAddAction) { return IsActionValid(PendingAddAction) == false; });

	// 添加所有待添加的动作
	Actions.Append(PendingAddActions);
	PendingAddActions.Empty();

#if STATS
	SET_DWORD_STAT(STAT_ECF_ActionsCount, Actions.Num());
	SET_DWORD_STAT(STAT_ECF_InstancesCount, 0);
#endif

	// Tick 所有活动的动作
	for (UECFActionBase* Action : Actions)
	{
		if (IsActionValid(Action))
		{
#if STATS
			if (Action->InstanceId.IsValid())
			{
				INC_DWORD_STAT(STAT_ECF_InstancesCount);
			}
#endif
			Action->DoTick(DeltaTime);
		}
	}
}

UECFActionBase* UECFSubsystem::FindAction(const FECFHandle& HandleId) const
{
	if (HandleId.IsValid())
	{
		if (UECFActionBase* const* ActionFound = Actions.FindByPredicate([&](UECFActionBase* Action) { return (IsActionValid(Action) && (Action->GetHandleId() == HandleId)); }))
		{
			return *ActionFound;
		}
		else if (UECFActionBase* const* PendingActionFound = PendingAddActions.FindByPredicate([&](UECFActionBase* PendingAddAction) { return (IsActionValid(PendingAddAction) && (PendingAddAction->GetHandleId() == HandleId)); }))
		{
			return *PendingActionFound;
		}
	}
	return nullptr;
}

void UECFSubsystem::PauseAction(const FECFHandle& HandleId)
{
	if (UECFActionBase* ActionFound = FindAction(HandleId))
	{
		ActionFound->bIsPaused = true;
	}
}

void UECFSubsystem::ResumeAction(const FECFHandle& HandleId)
{
	if (UECFActionBase* ActionFound = FindAction(HandleId))
	{
		ActionFound->bIsPaused = false;
	}
}

bool UECFSubsystem::IsActionPaused(const FECFHandle& HandleId, bool& bIsPaused) const
{
	if (UECFActionBase* ActionFound = FindAction(HandleId))
	{
		bIsPaused = ActionFound->bIsPaused;
		return true;
	}
	return false;
}

void UECFSubsystem::RemoveAction(FECFHandle& HandleId, bool bComplete)
{
	if (UECFActionBase* ActionFound = FindAction(HandleId))
	{
		FinishAction(ActionFound, bComplete);
		HandleId.Invalidate();
	}
}

void UECFSubsystem::RemoveActionsOfClass(TSubclassOf<UECFActionBase> ActionClass, bool bComplete, UObject* InOwner)
{
	// 查找分配给特定所有者（如果指定）的给定类的运行中动作并将其设置为已完成
	for (UECFActionBase* Action : Actions)
	{
		if (IsActionValid(Action))
		{
			if (Action->IsA(ActionClass))
			{
				if (InOwner == nullptr || InOwner == Action->Owner)
				{
					FinishAction(Action, bComplete);
				}
			}
		}
	}

	// 同时检查待添加的动作以防止启动它们
	for (UECFActionBase* PendingAction : PendingAddActions)
	{
		if (IsActionValid(PendingAction))
		{
			if (PendingAction->IsA(ActionClass))
			{
				if (InOwner == nullptr || InOwner == PendingAction->Owner)
				{
					FinishAction(PendingAction, bComplete);
				}
			}
		}
	}
}

void UECFSubsystem::RemoveInstancedAction(const FECFInstanceId& InstanceId, bool bComplete)
{
	// 停止所有具有给定实例ID的运行中和待添加的动作
	for (UECFActionBase* Action : Actions)
	{
		if (IsActionValid(Action))
		{
			if (Action->HasInstanceId(InstanceId))
			{
				FinishAction(Action, bComplete);
			}
		}
	}
	for (UECFActionBase* PendingAction : PendingAddActions)
	{
		if (IsActionValid(PendingAction))
		{
			if (PendingAction->HasInstanceId(InstanceId))
			{
				FinishAction(PendingAction, bComplete);
			}
		}
	}
}

void UECFSubsystem::RemoveAllActions(bool bComplete, UObject* InOwner)
{
	// 停止所有运行中和待添加的动作
	for (UECFActionBase* Action : Actions)
	{
		if (IsActionValid(Action))
		{
			if (InOwner == nullptr || InOwner == Action->Owner)
			{
				FinishAction(Action, bComplete);
			}
		}
	}
	for (UECFActionBase* PendingAction : PendingAddActions)
	{
		if (IsActionValid(PendingAction))
		{
			if (InOwner == nullptr || InOwner == PendingAction->Owner)
			{
				FinishAction(PendingAction, bComplete);
			}
		}
	}
}

bool UECFSubsystem::HasAction(const FECFHandle& HandleId) const
{
	if (UECFActionBase* ActionFound = FindAction(HandleId))
	{
		return true;
	}
	return false;
}

UECFActionBase* UECFSubsystem::GetInstancedAction(const FECFInstanceId& InstanceId) const
{
	if (InstanceId.IsValid())
	{
		if (UECFActionBase* const* ActionFound = Actions.FindByPredicate([&](UECFActionBase* Action) { return IsActionValid(Action) && Action->HasInstanceId(InstanceId); }))
		{
			return *ActionFound;
		}
		else if (UECFActionBase* const* PendingActionFound = PendingAddActions.FindByPredicate([&](UECFActionBase* PendingAction) { return IsActionValid(PendingAction) && PendingAction->HasInstanceId(InstanceId); }))
		{
			return *PendingActionFound;
		}
	}

	return nullptr;
}

void UECFSubsystem::FinishAction(UECFActionBase* Action, bool bComplete)
{
	if (IsActionValid(Action))
	{
		if (bComplete)
		{
			Action->Complete(true);
		}
		Action->MarkAsFinished();
	}
}

bool UECFSubsystem::IsActionValid(UECFActionBase* Action)
{
	return IsValid(Action) && (Action->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) == false) && Action->IsValid();
}

ECF_PRAGMA_ENABLE_OPTIMIZATION
