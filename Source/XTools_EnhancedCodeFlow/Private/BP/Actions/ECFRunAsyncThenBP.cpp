// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#include "ECFRunAsyncThenBP.h"
#include "EnhancedCodeFlow.h"
#include "Async/Async.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UECFRunAsyncThenBP* UECFRunAsyncThenBP::ECFRunAsyncThen(const UObject* WorldContextObject, float InTimeOut, EECFAsyncPrio Priority, FECFActionSettings Settings, FECFHandleBP& Handle)
{
	UECFRunAsyncThenBP* Proxy = NewObject<UECFRunAsyncThenBP>();
	if (Proxy)
	{
		Proxy->Init(WorldContextObject, Settings);
		TWeakObjectPtr<UECFRunAsyncThenBP> WeakProxy(Proxy);
		Proxy->Proxy_Handle = FFlow::RunAsyncThen(WorldContextObject,
			[WeakProxy]()
			{
				::AsyncTask(ENamedThreads::GameThread, [WeakProxy]()
				{
					if (UECFRunAsyncThenBP* StrongProxy = WeakProxy.Get())
					{
						if (IsProxyValid(StrongProxy))
						{
							StrongProxy->AsyncTask.Broadcast(false, false);
						}
					}
				});
			},
			[WeakProxy](bool bTimedOut, bool bStopped)
			{
				if (UECFRunAsyncThenBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy))
					{
						StrongProxy->OnExecute.Broadcast(bTimedOut, bStopped);
						StrongProxy->ClearAsyncBPAction();
					}
				}
			},
		InTimeOut, Priority, Settings);
		Handle = FECFHandleBP(Proxy->Proxy_Handle);
	}

	return Proxy;
}

ECF_PRAGMA_ENABLE_OPTIMIZATION
