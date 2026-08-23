// Copyright (c) 2026 Damian Nowakowski. All rights reserved.

#pragma once

#include "ECFCoroutine.h"
#include "ECFSubsystem.h"
#include "ECFTypes.h"
#include "Containers/Ticker.h"

class XTOOLS_ENHANCEDCODEFLOW_API FECFCoroutineAwaiter
{
public:

	// Returns the state of the corotuine after it's resumed.
	bool await_resume()
	{
		return CoroHandle.promise().bStopped;
	}

	// Required by the co-routine machinery, but we always want to suspend when co-routine is awaiting, so it just returns false.
	bool await_ready() { return false; }

protected:

	// Helper function for adding coroutine actions to the ECF subsystem.
	template<typename T, typename ... Ts>
	void AddCoroutineAction(const UObject* InOwner, FECFCoroutineHandle InCoroutineHandle, const FECFActionSettings& InSettings, Ts&& ... Args)
	{
		CoroHandle = InCoroutineHandle;
		if (UECFSubsystem* ECF = UECFSubsystem::Get(InOwner))
		{
			ECF->AddCoroutineAction<T>(InOwner, InCoroutineHandle, InSettings, Forward<Ts>(Args)...);
			return;
		}

		ensureMsgf(false, TEXT("ECF Coroutine - failed to obtain subsystem, suspended coroutine will be resumed immediately."));
		InCoroutineHandle.resume();
		if (InCoroutineHandle.done() && !InCoroutineHandle.promise().bFailureCleanupArmed)
		{
			// 不能在协程自身的 await_suspend 执行期间同步 destroy：
			// awaiter 临时对象存放在该协程帧内，resume 返回后立即销毁帧，
			// 仍在栈上执行的 awaiter/await_suspend 代码将构成 use-after-free。
			// 推迟到下一 tick，待本次恢复的调用栈完全展开后再回收；
			// bFailureCleanupArmed 防止嵌套 co_await 失败链对同一帧重复调度销毁。
			InCoroutineHandle.promise().bFailureCleanupArmed = true;
			const FECFCoroutineHandle DeferredDestroyHandle = InCoroutineHandle;
			FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda([DeferredDestroyHandle](float DeltaTime)
				{
					if (DeferredDestroyHandle.done())
					{
						// 先复位标记（协程帧可能被后续 await 复用），再安全销毁
						DeferredDestroyHandle.promise().bFailureCleanupArmed = false;
						DeferredDestroyHandle.destroy();
					}
					else
					{
						// 理论不可达（子系统缺失时不存在其他挂起点）；复位标记以允许将来重新调度
						DeferredDestroyHandle.promise().bFailureCleanupArmed = false;
					}
					return false;
				}),
				0.0f);
		}
	}

	// Storing the actual coroutine handle.
	FECFCoroutineHandle CoroHandle;

	// Storing owner to pass it to the ECF subsystem later.
	const UObject* Owner;

	// Storing settings to pass them to the ECF subsystem later.
	FECFActionSettings Settings;
};

struct FECFCoroutineAwaiter_ResultWithTimeout
{
	bool bStopped = false;
	bool bTimedOut = false;
	FECFCoroutineAwaiter_ResultWithTimeout(bool InStopped, bool InTimedOut) :
		bStopped(InStopped),
		bTimedOut(InTimedOut)
	{
	}
};

/*^^^ Wait Seconds Coroutine Awaiter ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

class XTOOLS_ENHANCEDCODEFLOW_API FECFCoroutineAwaiter_WaitSeconds : public FECFCoroutineAwaiter
{
public:

	// C-tor
	FECFCoroutineAwaiter_WaitSeconds(const UObject* InOwner, const FECFActionSettings& InSettings, float InTime);

	// Called when the suspension begins
	void await_suspend(FECFCoroutineHandle InCoroHandle);

private:

	// Storing values in order to use them when await_suspend is called
	float Time = 0.f;
};

/*^^^ Wait Ticks Coroutine Awaiter ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

class XTOOLS_ENHANCEDCODEFLOW_API FECFCoroutineAwaiter_WaitTicks : public FECFCoroutineAwaiter
{
public:

	// C-tor
	FECFCoroutineAwaiter_WaitTicks(const UObject* InOwner, const FECFActionSettings& InSettings, int32 InTicks);

	// Called when the suspension begins
	void await_suspend(FECFCoroutineHandle InCoroHandle);

private:

	// Storing values in order to use them when await_suspend is called
	int32 Ticks = 0;
};

/*^^^ Wait Until Coroutine Awaiter ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

enum class EECFWaitUntilPredicateType : uint8
{
	HasFinished,
	HasFinished_Deltatime
};

class XTOOLS_ENHANCEDCODEFLOW_API FECFCoroutineAwaiter_WaitUntil : public FECFCoroutineAwaiter
{
public:

	// C-tor
	FECFCoroutineAwaiter_WaitUntil(const UObject* InOwner, const FECFActionSettings& InSettings, TUniqueFunction<bool()>&& InPredicate, float InTimeOut);
	FECFCoroutineAwaiter_WaitUntil(const UObject* InOwner, const FECFActionSettings& InSettings, TUniqueFunction<bool(float)>&& InPredicate, float InTimeOut);

	// Called when the suspension begins
	void await_suspend(FECFCoroutineHandle InCoroHandle);

	// Returns the state of the corotuine after it's resumed.
	FECFCoroutineAwaiter_ResultWithTimeout await_resume()
	{
		return FECFCoroutineAwaiter_ResultWithTimeout(
			CoroHandle.promise().bStopped,
			CoroHandle.promise().bTimedOut);
	}

private:

	// Storing values in order to use them when await_suspend is called
	EECFWaitUntilPredicateType PredicateType = EECFWaitUntilPredicateType::HasFinished;
	TUniqueFunction<bool()> PredicateHasFinished;
	TUniqueFunction<bool(float)> PredicateHasFinishedDeltaTime;
	float TimeOut = 0.f;
};

/*^^^ Wait For Flag Coroutine Awaiter ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

class XTOOLS_ENHANCEDCODEFLOW_API FECFCoroutineAwaiter_WaitForFlag : public FECFCoroutineAwaiter
{
public:

	// C-tor
	FECFCoroutineAwaiter_WaitForFlag(const UObject* InOwner, const FECFActionSettings& InSettings, bool* bInFlag, float InTimeOut);

	// Called when the suspension begins
	void await_suspend(FECFCoroutineHandle InCoroHandle);

	// Returns the state of the corotuine after it's resumed.
	FECFCoroutineAwaiter_ResultWithTimeout await_resume()
	{
		return FECFCoroutineAwaiter_ResultWithTimeout(
			CoroHandle.promise().bStopped,
			CoroHandle.promise().bTimedOut);
	}

private:

	// Storing values in order to use them when await_suspend is called
	bool* bFlag = nullptr;
	float TimeOut = 0.f;
};

/*^^^ Run Async And Wait Coroutine Awaiter ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

class XTOOLS_ENHANCEDCODEFLOW_API FECFCoroutineAwaiter_RunAsyncAndWait : public FECFCoroutineAwaiter
{
public:

	// C-tor
	FECFCoroutineAwaiter_RunAsyncAndWait(const UObject* InOwner, const FECFActionSettings& InSettings, TUniqueFunction<void()>&& InAsyncTaskFunc, float InTimeOut, EECFAsyncPrio InThreadPriority);

	// Called when the suspension begins
	void await_suspend(FECFCoroutineHandle InCoroHandle);

	// Returns the state of the corotuine after it's resumed.
	FECFCoroutineAwaiter_ResultWithTimeout await_resume()
	{
		return FECFCoroutineAwaiter_ResultWithTimeout(
			CoroHandle.promise().bStopped,
			CoroHandle.promise().bTimedOut);
	}

private:

	// Storing values in order to use them when await_suspend is called
	TUniqueFunction<void()> AsyncTaskFunction;
	float TimeOut = 0.f;
	EECFAsyncPrio ThreadPriority = EECFAsyncPrio::Normal;
};

/*^^^ Wait Load Objects Coroutine Awaiter ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

class XTOOLS_ENHANCEDCODEFLOW_API FECFCoroutineAwaiter_WaitLoadObjects : public FECFCoroutineAwaiter
{
public:

	// C-tor
	FECFCoroutineAwaiter_WaitLoadObjects(const UObject* InOwner, const FECFActionSettings& InSettings, const TArray<FSoftObjectPath>& InObjectsToLoad);
	FECFCoroutineAwaiter_WaitLoadObjects(const UObject* InOwner, const FECFActionSettings& InSettings, const TArray<FPrimaryAssetId>& InPrimaryAssetsToLoad);

	// Called when the suspension begins
	void await_suspend(FECFCoroutineHandle InCoroHandle);

private:

	// Storing values in order to use them when await_suspend is called
	TArray<FSoftObjectPath> ObjectsToLoad;
	TArray<FPrimaryAssetId> PrimaryAssetsToLoad;

	// Tracks which constructor overload was used, so the empty-array fallback
	// routes to the correct Setup overload for accurate error messages.
	bool bUsedPrimaryAssets = false;
};

/*^^^ Wait And Loop Coroutine Awaiter ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

class XTOOLS_ENHANCEDCODEFLOW_API FECFCoroutineAwaiter_LoopAndWait : public FECFCoroutineAwaiter
{
public:

	// C-tor
	FECFCoroutineAwaiter_LoopAndWait(const UObject* InOwner, const FECFActionSettings& InSettings, TUniqueFunction<bool()>&& InPredicate, TUniqueFunction<void(float)>&& InTickFunc, float InTimeOut);

	// Called when the suspension begins
	void await_suspend(FECFCoroutineHandle InCoroHandle);

	// Returns the state of the corotuine after it's resumed.
	FECFCoroutineAwaiter_ResultWithTimeout await_resume()
	{
		return FECFCoroutineAwaiter_ResultWithTimeout(
			CoroHandle.promise().bStopped,
			CoroHandle.promise().bTimedOut);
	}

private:

	// Storing values in order to use them when await_suspend is called
	TUniqueFunction<bool()> Predicate;
	TUniqueFunction<void(float)> TickFunc;
	float TimeOut = 0.f;
};
