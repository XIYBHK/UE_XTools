// Copyright (c) 2026 Damian Nowakowski. All rights reserved.

#include "ECFLoadObjectsAsyncBP.h"
#include "EnhancedCodeFlow.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UECFLoadObjectsAsyncBP* UECFLoadObjectsAsyncBP::ECFLoadObjectsAsync(const UObject* WorldContextObject, const TArray<FSoftObjectPath>& ObjectsToLoad, FECFActionSettings Settings, FECFHandleBP& Handle)
{
	UECFLoadObjectsAsyncBP* Proxy = NewObject<UECFLoadObjectsAsyncBP>();
	if (Proxy)
	{
		Proxy->Init(WorldContextObject, Settings);
		// 使用弱指针捕获，防止回调延迟执行时 Proxy 已被 GC 导致悬空访问
		TWeakObjectPtr<UECFLoadObjectsAsyncBP> WeakProxy(Proxy);
		Proxy->Proxy_Handle = FFlow::LoadObjectsAsync(WorldContextObject, ObjectsToLoad, [WeakProxy](bool bStopped)
		{
			if (UECFLoadObjectsAsyncBP* StrongProxy = WeakProxy.Get())
			{
				if (IsProxyValid(StrongProxy))
				{
					StrongProxy->OnComplete.Broadcast(bStopped);
					StrongProxy->ClearAsyncBPAction();
				}
			}
		}, Settings);
		Handle = FECFHandleBP(Proxy->Proxy_Handle);
	}

	return Proxy;
}

ECF_PRAGMA_ENABLE_OPTIMIZATION
