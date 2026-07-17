// Copyright (c) 2026 Damian Nowakowski. All rights reserved.

#pragma once

#include "ECFActionBase.h"
#include "ECFCoroutine.h"
#include "ECFLogs.h"
#include "ECFCoroutineActionBase.generated.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFCoroutineActionBase : public UECFActionBase
{
	GENERATED_BODY()

	friend class UECFSubsystem;

protected:

	// Coroutine handle used to control the coroutine inside the Action.
	FECFCoroutineHandle CoroutineHandle;

	// Flag indicating if the coroutine handle has been set.
	bool bHasCoroutineHandle = false;

	void BeginDestroy() override
	{
		// A later await reassigns the same frame to a new action. Only its current action may destroy it.
		if (bHasCoroutineHandle && CoroutineHandle &&
			CoroutineHandle.promise().ActionHandle == HandleId)
		{
#if (ECF_LOGS && ECF_LOGS_VERBOSE)
			UE_LOG(LogECF, Log, TEXT("Destroying coroutine frame for Handle: %s"), *HandleId.ToString());
#endif
			CoroutineHandle.promise().bHasFinished = true;
			CoroutineHandle.destroy();
		}
		bHasCoroutineHandle = false;
		Super::BeginDestroy();
	}

	void ResumeCoroutine(bool bStopped, bool bTimedOut = false)
	{
		if (!bHasCoroutineHandle || !CoroutineHandle || !HasValidOwner())
		{
			return;
		}

		FECFCoroutinePromise& Promise = CoroutineHandle.promise();
		if (Promise.bHasFinished || Promise.ActionHandle != HandleId)
		{
			return;
		}

		Promise.bStopped = bStopped;
		Promise.bTimedOut = bTimedOut;
		CoroutineHandle.resume();
	}

private:

	// Setting up action. The same as in ActionBase, but it additionally sets the coroutine handle.
	void SetCoroutineAction(const UObject* InOwner, FECFCoroutineHandle InCoroutineHandle, const FECFHandle& InHandleId, const FECFActionSettings& InSettings)
	{
		UECFActionBase::SetAction(InOwner, InHandleId, {}, InSettings);
		CoroutineHandle = InCoroutineHandle;
		CoroutineHandle.promise().AssignHandle(InHandleId);
		bHasCoroutineHandle = true;

#if (ECF_LOGS && ECF_LOGS_VERBOSE)
		UE_LOG(LogECF, Log, TEXT("Sets Coroutine Action for Handle: %s"), *InHandleId.ToString());
#endif
	}
};

ECF_PRAGMA_ENABLE_OPTIMIZATION
