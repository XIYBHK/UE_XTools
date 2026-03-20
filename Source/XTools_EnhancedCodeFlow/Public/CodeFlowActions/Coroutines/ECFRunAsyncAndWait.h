// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "Coroutines/ECFCoroutineActionBase.h"
#include "Templates/Atomic.h"
#include "Async/Async.h"
#include "ECFTypes.h"
#include "Async/TaskGraphInterfaces.h"
#include "XToolsVersionCompat.h"  // UE版本兼容性
#include "ECFRunAsyncAndWait.generated.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFRunAsyncAndWait: public UECFCoroutineActionBase
{
	GENERATED_BODY()

	friend class UECFSubsystem;

protected:

	TUniqueFunction<void()> AsyncTaskFunc;
	float TimeOut = 0.f;
	bool bWithTimeOut = false;

	ENamedThreads::Type ThreadType = ENamedThreads::AnyBackgroundThreadNormalTask;
	TAtomic<bool> bIsAsyncTaskDone = false;

	bool Setup(TUniqueFunction<void()>&& InAsyncTaskFunc, float InTimeOut, EECFAsyncPrio InThreadPriority)
	{
		AsyncTaskFunc = MoveTemp(InAsyncTaskFunc);

		switch (InThreadPriority)
		{
			case EECFAsyncPrio::Normal:
				ThreadType = ENamedThreads::AnyBackgroundThreadNormalTask;
				break;
			case EECFAsyncPrio::HiPriority:
				ThreadType = ENamedThreads::AnyBackgroundHiPriTask;
				break;
		}

		if (AsyncTaskFunc)
		{
			if (InTimeOut > 0.f)
			{
				bWithTimeOut = true;
				TimeOut = InTimeOut;
				SetMaxActionTime(TimeOut);
			}
			else
			{
				bWithTimeOut = false;
			}

			XTOOLS_ATOMIC_STORE(bIsAsyncTaskDone, false);

			// 将任务函数按值 move 到 lambda 中，避免后台线程通过 TWeakObjectPtr 访问 UObject（非线程安全）
			TUniqueFunction<void()> TaskCopy = MoveTemp(AsyncTaskFunc);
			TAtomic<bool>* DoneFlag = &bIsAsyncTaskDone;
			AsyncTask(ThreadType, [Task = MoveTemp(TaskCopy), DoneFlag]()
			{
				Task();
				XTOOLS_ATOMIC_STORE(*DoneFlag, true);
			});

			return true;
		}
		else
		{
			ensureMsgf(false, TEXT("ECF Coroutine - Run Async Task and Wait failed to start. Are you sure the AsyncTask function is set properly?"));
			return false;
		}
	}

	void Tick(float DeltaTime) override
	{
#if STATS
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("RunAsyncAndWait - Tick"), STAT_ECFDETAILS_RUNASYNCANDWAIT, STATGROUP_ECFDETAILS);
#endif
		if (bWithTimeOut)
		{
			TimeOut -= DeltaTime;
			if (TimeOut <= 0.f)
			{
				Complete(false);
				MarkAsFinished();
				return;
			}
		}

		if (XTOOLS_ATOMIC_LOAD(bIsAsyncTaskDone))
		{
			Complete(false);
			MarkAsFinished();
		}
	}

	void Complete(bool bStopped) override
	{
		// 修复：异步任务可能在 Owner 销毁后完成，需安全跳过
		if (HasValidOwner())
		{
			if (bHasCoroutineHandle && !CoroutineHandle.promise().bHasFinished)
			{
				CoroutineHandle.resume();
			}
		}
	}
};

ECF_PRAGMA_ENABLE_OPTIMIZATION
