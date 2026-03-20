// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "Coroutines/ECFCoroutineActionBase.h"
#include "ECFWaitUntil.generated.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFWaitUntil : public UECFCoroutineActionBase
{
	GENERATED_BODY()

	friend class UECFSubsystem;

protected:

	TUniqueFunction<bool(float)> Predicate;
	float TimeOut = 0.f;
	bool bWithTimeOut = false;

	bool Setup(TUniqueFunction<bool(float)>&& InPredicate, float InTimeOut)
	{
		Predicate = MoveTemp(InPredicate);

		if (Predicate)
		{
			// 首帧条件已满足时直接返回 false，由 await_suspend 统一 resume（避免双重 resume 未定义行为）
			if (Predicate(0.f))
			{
				return false;
			}
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
			return true;
		}
		else
		{
			ensureMsgf(false, TEXT("ECF Coroutine - Wait Until failed to start. Are you sure the Predicate is set properly?"));
			return false;
		}
	}

	void Tick(float DeltaTime) override
	{
#if STATS
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("WaitUntil - Tick"), STAT_ECFDETAILS_WAITUNTIL, STATGROUP_ECFDETAILS);
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

		if (Predicate(DeltaTime))
		{
			Complete(false);
			MarkAsFinished();
		}
	}

	void Complete(bool bStopped) override
	{
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
