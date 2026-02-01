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
		// 修复：处理协程句柄生命周期管理
		// 当Owner在协程完成前被销毁时，需要安全地标记协程为已完成但不立即销毁句柄
		if (bHasCoroutineHandle)
		{
			// 先检查协程是否已完成
			if (CoroutineHandle.promise().bHasFinished == false)
			{
				// 如果协程仍在运行（可能有异步任务在后台），标记为已完成
				// 不立即销毁句柄，让异步任务自然结束
				CoroutineHandle.promise().bHasFinished = true;
				// 注意：不调用 destroy()，避免在异步任务可能仍在访问时销毁句柄
			}
			else
			{
				// 协程已完成，可以安全销毁句柄
				CoroutineHandle.destroy();
				bHasCoroutineHandle = false;
			}
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
