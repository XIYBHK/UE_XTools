// Copyright (c) 2026 Damian Nowakowski. All rights reserved.

#pragma once

#include "ECFActionBase.h"
#include "Templates/Atomic.h"
#include "Async/Async.h"
#include "ECFTypes.h"
#include "Async/TaskGraphInterfaces.h"
#include "XToolsVersionCompat.h"  // UE版本兼容性
#include "ECFRunAsyncThen.generated.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFRunAsyncThen : public UECFActionBase
{
	GENERATED_BODY()

	friend class UECFSubsystem;

protected:

	TUniqueFunction<void()> AsyncTaskFunc;
	TUniqueFunction<void(bool, bool)> Func;
	TUniqueFunction<void(bool)> Func_NoStopped;
	TUniqueFunction<void()> Func_NoTimeOut_NoStopped;

	float TimeOut = 0.f;
	float OriginTimeOut = 0.f;
	bool bWithTimeOut = false;
	bool bTimedOut = false;

	ENamedThreads::Type ThreadType = ENamedThreads::AnyBackgroundThreadNormalTask;
	TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> AsyncTaskDone;

	bool Setup(TUniqueFunction<void()>&& InAsyncTaskFunc, TUniqueFunction<void(bool, bool)>&& InFunc, float InTimeOut, EECFAsyncPrio ThreadPriority)
	{
		AsyncTaskFunc = MoveTemp(InAsyncTaskFunc);
		Func = MoveTemp(InFunc);

		switch (ThreadPriority)
		{
			case EECFAsyncPrio::Normal:
				ThreadType = ENamedThreads::AnyBackgroundThreadNormalTask;
				break;
			case EECFAsyncPrio::HiPriority:
				ThreadType = ENamedThreads::AnyBackgroundHiPriTask;
				break;
		}

		if (AsyncTaskFunc && Func)
		{
			if (InTimeOut > 0.f)	
			{
				bWithTimeOut = true;
				bTimedOut = false;
				TimeOut = InTimeOut;
				OriginTimeOut = InTimeOut;
				SetMaxActionTime(TimeOut);
			}
			else
			{
				bWithTimeOut = false;
				bTimedOut = false;
			}

			AsyncTaskDone = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);

			// 将任务函数按值 move 到 lambda 中，避免后台线程通过 TWeakObjectPtr 访问 UObject（非线程安全）
			TUniqueFunction<void()> TaskCopy = MoveTemp(AsyncTaskFunc);
			const TSharedRef<TAtomic<bool>, ESPMode::ThreadSafe> DoneFlag = AsyncTaskDone.ToSharedRef();
			AsyncTask(ThreadType, [Task = MoveTemp(TaskCopy), DoneFlag]()
			{
				Task();
				XTOOLS_ATOMIC_STORE(DoneFlag.Get(), true);
			});

			return true;
		}
		else
		{
#if ECF_LOGS
			UE_LOG(LogECF, Error, TEXT("ECF - [%s] Run Async Task and Run failed to start. Are you sure the AsyncTask and Function are set properly?"), *Settings.Label);
#endif
			return false;
		}
	}

	bool Setup(TUniqueFunction<void()>&& InAsyncTaskFunc, TUniqueFunction<void(bool)>&& InFunc, float InTimeOut, EECFAsyncPrio ThreadPriority)
	{
		Func_NoStopped = MoveTemp(InFunc);
		if (Func_NoStopped)
		{
			return Setup(MoveTemp(InAsyncTaskFunc), [this](bool bTimeOut, bool bStopped)
			{
				Func_NoStopped(bTimeOut);
			}, InTimeOut, ThreadPriority);
		}
		else
		{
#if ECF_LOGS
			UE_LOG(LogECF, Error, TEXT("ECF - [%s] Run Async Task and Run failed to start. Are you sure the Function is set properly?"), *Settings.Label);
#endif
			return false;
		}
	}

	bool Setup(TUniqueFunction<void()>&& InAsyncTaskFunc, TUniqueFunction<void()>&& InFunc, float InTimeOut, EECFAsyncPrio ThreadPriority)
	{
		Func_NoTimeOut_NoStopped = MoveTemp(InFunc);
		if (Func_NoTimeOut_NoStopped)
		{
			return Setup(MoveTemp(InAsyncTaskFunc), [this](bool bTimeOut, bool bStopped)
			{
				Func_NoTimeOut_NoStopped();
			}, InTimeOut, ThreadPriority);
		}
		else
		{
#if ECF_LOGS
			UE_LOG(LogECF, Error, TEXT("ECF - [%s] Run Async Task and Run failed to start. Are you sure the Function is set properly?"), *Settings.Label);
#endif
			return false;
		}
	}

	bool Reset(bool bCallUpdate) override
	{
		if (bWithTimeOut)
		{
			TimeOut = OriginTimeOut;
		}
		return true;
	}

	void Tick(float DeltaTime) override 
	{
#if STATS
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("RunAsyncThen - Tick"), STAT_ECFDETAILS_RUNASYNCTHEN, STATGROUP_ECFDETAILS);
#endif

#if ECF_INSIGHT_PROFILING
		TRACE_CPUPROFILER_EVENT_SCOPE("ECF - RunAsyncThen Tick");
#endif

		if (bWithTimeOut)
		{
			TimeOut -= DeltaTime;
			if (TimeOut <= 0.f)
			{
				bTimedOut = true;
				MarkAsFinished();
				Complete(false);
				return;
			}
		}

		if (AsyncTaskDone.IsValid() && XTOOLS_ATOMIC_LOAD(*AsyncTaskDone))
		{
			MarkAsFinished();
			Complete(false);
		}
	}

	void Complete(bool bStopped) override
	{
		// 【防御性编程】：在异步任务完成后，确保 Owner 仍然有效
		// 注：异步任务可能在 Owner 销毁后完成，导致 Func 中的捕获变量访问悬空指针
		if (HasValidOwner() && Func)
		{
			Func(bTimedOut, bStopped);
		}
		// 注：Owner 已销毁时静默跳过回调，避免崩溃
		// 在 Development 构建中可通过 ensureMsgf 检测此情况
	}
};

ECF_PRAGMA_ENABLE_OPTIMIZATION
