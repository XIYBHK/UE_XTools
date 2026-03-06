// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "ECFActionBase.h"
#include "ECFCoroutine.h"
#include "ECFSubsystem.h"
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
		// 协程 frame 由 Action 持有并负责销毁；Owner 提前失效时直接终止这条协程链。
		if (bHasCoroutineHandle)
		{
			if (CoroutineHandle)
			{
				CoroutineHandle.promise().bHasFinished = true;
				CoroutineHandle.destroy();
			}
			bHasCoroutineHandle = false;
		}
		Super::BeginDestroy();
	}

private:

	// Setting up action. The same as in ActionBase, but it additionally sets the coroutine handle.
	void SetCoroutineAction(const UObject* InOwner, FECFCoroutineHandle InCoroutineHandle, const FECFHandle& InHandleId, const FECFActionSettings& InSettings)
	{
		UECFActionBase::SetAction(InOwner, InHandleId, {}, InSettings);
		CoroutineHandle = InCoroutineHandle;
		bHasCoroutineHandle = true;
	}
};

ECF_PRAGMA_ENABLE_OPTIMIZATION
