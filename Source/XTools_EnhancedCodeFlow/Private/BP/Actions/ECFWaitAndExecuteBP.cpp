// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#include "ECFWaitAndExecuteBP.h"
#include "EnhancedCodeFlow.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UECFWaitAndExecuteBP* UECFWaitAndExecuteBP::ECFWaitAndExecute(const UObject* WorldContextObject, float InTimeOut, FECFActionSettings Settings, FECFHandleBP& Handle)
{
	UECFWaitAndExecuteBP* Proxy = NewObject<UECFWaitAndExecuteBP>();
	if (Proxy)
	{
		Proxy->Init(WorldContextObject, Settings);
		Proxy->Proxy_HasFinished = false;
		// 使用弱指针捕获，防止回调延迟执行时 Proxy 已被 GC 导致悬空访问
		TWeakObjectPtr<UECFWaitAndExecuteBP> WeakProxy(Proxy);
		Proxy->Proxy_Handle = FFlow::WaitAndExecute(WorldContextObject,
			[WeakProxy](float DeltaTime)
			{
				if (UECFWaitAndExecuteBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy))
					{
						StrongProxy->OnWait.Broadcast(StrongProxy, DeltaTime, false, false);
						return StrongProxy->Proxy_HasFinished;
					}
				}
				return true;
			},
			[WeakProxy](bool bTimedOut, bool bStopped)
			{
				if (UECFWaitAndExecuteBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy))
					{
						StrongProxy->OnExecute.Broadcast(StrongProxy, 0.f, bTimedOut, bStopped);
						StrongProxy->ClearAsyncBPAction();
					}
				}
			},
		InTimeOut, Settings);
		Handle = FECFHandleBP(Proxy->Proxy_Handle);
	}

	return Proxy;
}

void UECFWaitAndExecuteBP::Predicate(bool bHasFinished)
{
	Proxy_HasFinished = bHasFinished;
}

ECF_PRAGMA_ENABLE_OPTIMIZATION
