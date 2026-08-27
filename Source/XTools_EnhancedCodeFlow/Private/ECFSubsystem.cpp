// Copyright (c) 2026 Damian Nowakowski. All rights reserved.

#include "ECFSubsystem.h"
#include "ECFActionBase.h"
#include "ECFSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Interfaces/IPluginManager.h"
#include "UObject/UObjectGlobals.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

DEFINE_STAT(STAT_ECF_ActionsCount);
DEFINE_STAT(STAT_ECF_InstancesCount);
DEFINE_STAT(STAT_ECF_DelayActionsCount);
DEFINE_STAT(STAT_ECF_AsyncActionsCount);
DEFINE_STAT(STAT_ECF_CoroutineActionsCount);
DEFINE_STAT(STAT_ECF_AverageActionDuration);

void UECFSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

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
	// 在子系统销毁前显式停止所有动作，确保回调和状态收敛
	RemoveAllActions(false, nullptr);
	Actions.Empty();
	PendingAddActions.Empty();

	Super::Deinitialize();
}

bool UECFSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// 如果项目中已启用 Marketplace 版本的 EnhancedCodeFlow 插件，则不创建 XTools 集成版子系统，避免重复调度和状态冲突
	static bool bHasLoggedExternalPluginWarning = false;
	if (const TSharedPtr<IPlugin> ExternalECFPlugin = IPluginManager::Get().FindPlugin(TEXT("EnhancedCodeFlow")))
	{
		if (ExternalECFPlugin->IsEnabled())
		{
			if (!bHasLoggedExternalPluginWarning)
			{
				bHasLoggedExternalPluginWarning = true;
				UE_LOG(LogTemp, Warning, TEXT("XTools_EnhancedCodeFlow: Detected external EnhancedCodeFlow plugin enabled, XTools subsystem will not be created."));
			}
			return false;
		}
	}

	//  检查插件设置，如果未启用则不创建子系统
	//  迁移说明：不再反射读取 Editor-only 模块的 X_AssetEditorSettings（打包后该类不存在导致开关被忽略、
	//  编辑器与打包成品行为反转），改为读取本模块运行时开发者设置，保证编辑器与打包成品行为一致
	const UECFSettings* Settings = GetDefault<UECFSettings>();
	const bool bEnabled = Settings ? Settings->bEnableEnhancedCodeFlowSubsystem : true;
	if (!bEnabled)
	{
		return false;
	}

	return Super::ShouldCreateSubsystem(Outer);
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
		else
		{
#if ECF_LOGS
			UE_LOG(LogECF, Error, TEXT("Can't obtain GameInstance from WorldContextObject in ECF!"));
#endif
		}
	}
	else
	{
#if ECF_LOGS
		UE_LOG(LogECF, Error, TEXT("Can't obtain ThisWorld from WorldContextObject in ECF!"));
#endif
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
	TRACE_CPUPROFILER_EVENT_SCOPE("ECF - Subsystem Tick");
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

TArray<FECFHandle> UECFSubsystem::GetActionsHandlesByClass(TSubclassOf<UECFActionBase> Class) const
{
	TArray<FECFHandle> Result;
	if (Class == nullptr)
	{
		return Result;
	}
	// Search in active actions
	for (UECFActionBase* Action : Actions)
	{
		if (IsActionValid(Action) && Action->IsA(Class))
		{
			Result.Add(Action->GetHandleId());
		}
	}
	// Search in pending actions
	for (UECFActionBase* PendingAction : PendingAddActions)
	{
		if (IsActionValid(PendingAction) && PendingAction->IsA(Class))
		{
			Result.Add(PendingAction->GetHandleId());
		}
	}
	return Result;
}

TArray<FECFHandle> UECFSubsystem::GetActionsHandlesByLabel(const FString& Label) const
{
	TArray<FECFHandle> Result;
	if (Label.IsEmpty())
	{
		return Result;
	}
	// Search in active actions
	for (UECFActionBase* Action : Actions)
	{
		if (IsActionValid(Action) && (Action->Settings.Label == Label))
		{
			Result.Add(Action->GetHandleId());
		}
	}
	// Search in pending actions
	for (UECFActionBase* PendingAction : PendingAddActions)
	{
		if (IsActionValid(PendingAction) && (PendingAction->Settings.Label == Label))
		{
			Result.Add(PendingAction->GetHandleId());
		}
	}
	return Result;
}

TArray<UECFActionBase*> UECFSubsystem::GetAllActions() const
{
	TArray<UECFActionBase*> Result;
	Result.Reserve(GetActionsCount());
	Result.Append(Actions);
	Result.Append(PendingAddActions);
	return Result;
}

int32 UECFSubsystem::GetActionsCount() const
{
	return Actions.Num() + PendingAddActions.Num();
}

void UECFSubsystem::PauseAction(const FECFHandle& HandleId)
{
	if (UECFActionBase* ActionFound = FindAction(HandleId))
	{
#if (ECF_LOGS && ECF_LOGS_VERBOSE)
		UE_LOG(LogECF, Verbose, TEXT("Paused Action of class: %s, Label: %s"), *ActionFound->GetName(), *ActionFound->GetLabel());
#endif
		ActionFound->bIsPaused = true;
	}
	else
	{
#if ECF_LOGS
		UE_LOG(LogECF, Error, TEXT("Can't find Action of id %s to pause"), *HandleId.ToString());
#endif
	}
}

void UECFSubsystem::ResumeAction(const FECFHandle& HandleId)
{
	if (UECFActionBase* ActionFound = FindAction(HandleId))
	{
#if (ECF_LOGS && ECF_LOGS_VERBOSE)
		UE_LOG(LogECF, Verbose, TEXT("Resume Action of class: %s, Label: %s"), *ActionFound->GetName(), *ActionFound->GetLabel());
#endif
		ActionFound->bIsPaused = false;
	}
	else
	{
#if ECF_LOGS
		UE_LOG(LogECF, Error, TEXT("Can't find Action of id %s to resume"), *HandleId.ToString());
#endif
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

bool UECFSubsystem::ResetAction(const FECFHandle& HandleId, bool bCallUpdate)
{
	if (UECFActionBase* ActionFound = FindAction(HandleId))
	{
		if (IsActionValid(ActionFound))
		{
#if (ECF_LOGS && ECF_LOGS_VERBOSE)
			UE_LOG(LogECF, Verbose, TEXT("Reset Action of class: %s, Label: %s"), *ActionFound->GetName(), *ActionFound->GetLabel());
#endif
			return ActionFound->Reset(bCallUpdate);
		}
	}

#if ECF_LOGS
	UE_LOG(LogECF, Error, TEXT("Can't find Action of id %s to reset"), *HandleId.ToString());
#endif

	return false;
}

void UECFSubsystem::RemoveAction(FECFHandle& HandleId, bool bComplete)
{
	if (UECFActionBase* ActionFound = FindAction(HandleId))
	{
#if (ECF_LOGS && ECF_LOGS_VERBOSE)
		UE_LOG(LogECF, Verbose, TEXT("Remove Action of class: %s, Label: %s"), *ActionFound->GetName(), *ActionFound->GetLabel());
#endif
		FinishAction(ActionFound, bComplete);
		HandleId.Invalidate();
	}
	else
	{
#if ECF_LOGS
		UE_LOG(LogECF, Error, TEXT("Can't find Action of id %s to remove"), *HandleId.ToString());
#endif
	}
}

void UECFSubsystem::RemoveActionsOfClass(TSubclassOf<UECFActionBase> ActionClass, bool bComplete, UObject* InOwner)
{
	if (ActionClass == nullptr)
	{
#if (ECF_LOGS && ECF_LOGS_VERBOSE)
		UE_LOG(LogECF, Warning, TEXT("Trying to remove Actions of empty class!"));
#endif
		return;
	}

#if (ECF_LOGS && ECF_LOGS_VERBOSE)
	UE_LOG(LogECF, Verbose, TEXT("Removing Actions of class: %s"), *ActionClass->GetName());
#endif

	// Find running actions of given class assigned to a specific owner (if specified) and set it as finished.
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

void UECFSubsystem::RemoveActionsOfLabel(const FString& Label, bool bComplete, UObject* InOwner)
{
	if (Label.IsEmpty())
	{
#if (ECF_LOGS && ECF_LOGS_VERBOSE)
		UE_LOG(LogECF, Warning, TEXT("Trying to remove Actions of Label, but Label is empty!"));
#endif
		return;
	}

#if (ECF_LOGS && ECF_LOGS_VERBOSE)
	UE_LOG(LogECF, Verbose, TEXT("Removing Actions of Label: %s"), *Label);
#endif

	// Find running actions of given class assigned to a specific owner (if specified) and set it as finished.
	for (UECFActionBase* Action : Actions)
	{
		if (IsActionValid(Action))
		{
			if (Action->GetLabel() == Label)
			{
				if (InOwner == nullptr || InOwner == Action->Owner)
				{
					FinishAction(Action, bComplete);
				}
			}
		}
	}

	// Also check pending actions to prevent from launching it.
	for (UECFActionBase* PendingAction : PendingAddActions)
	{
		if (IsActionValid(PendingAction))
		{
			if (PendingAction->GetLabel() == Label)
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
#if (ECF_LOGS && ECF_LOGS_VERBOSE)
	UE_LOG(LogECF, Verbose, TEXT("Removing Instanced Action of InstanceId: %s"), *InstanceId.ToString());
#endif

	// Stop all running and pending actions with the given InstanceId.
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
#if (ECF_LOGS && ECF_LOGS_VERBOSE)
	UE_LOG(LogECF, Verbose, TEXT("Removing All Actions"));
#endif

	// Stop all running and pending actions.
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

float UECFSubsystem::GetActionTime(const FECFHandle& HandleId)
{
	if (UECFActionBase* ActionFound = FindAction(HandleId))
	{
		return ActionFound->GetActionTime();
	}
	else
	{
#if ECF_LOGS
		UE_LOG(LogECF, Error, TEXT("GetActionTime can't be called, because of action that can't be found. Id: %s"), *HandleId.ToString());
#endif
		return -1.f;
	}
}

bool UECFSubsystem::SetActionTime(const FECFHandle& HandleId, float NewTime, bool bCallUpdate)
{
	if (UECFActionBase* ActionFound = FindAction(HandleId))
	{
		return ActionFound->SetActionTime(NewTime, bCallUpdate);
	}
	else
	{
#if ECF_LOGS
		UE_LOG(LogECF, Error, TEXT("SetActionTime can't be called, because of action that can't be found. Id: %s"), *HandleId.ToString());
#endif
		return false;
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

UECFActionBase* UECFSubsystem::GetInstancedAction(const FECFInstanceId& InstanceId, bool bPrintErrorIfFailed/* = true*/) const
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

	if (bPrintErrorIfFailed)
	{
#if ECF_LOGS
		UE_LOG(LogECF, Error, TEXT("Failed to Get Instanced Action from InstanceId: %s"), *InstanceId.ToString());
#endif
	}

	return nullptr;
}

void UECFSubsystem::FinishAction(UECFActionBase* Action, bool bComplete)
{
	if (IsActionValid(Action))
	{
		Action->MarkAsFinished();
		if (bComplete)
		{
			Action->Complete(true);
		}
	}
}

bool UECFSubsystem::IsActionValid(UECFActionBase* Action)
{
	return IsValid(Action) && (Action->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) == false) && Action->IsValid();
}

ECF_PRAGMA_ENABLE_OPTIMIZATION
