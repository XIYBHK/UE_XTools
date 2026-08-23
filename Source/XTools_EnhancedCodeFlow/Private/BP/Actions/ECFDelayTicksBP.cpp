// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#include "ECFDelayTicksBP.h"
#include "EnhancedCodeFlow.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UECFDelayTicksBP* UECFDelayTicksBP::ECFDelayTicks(const UObject* WorldContextObject, int32 DelayTicks, FECFActionSettings Settings, FECFHandleBP& Handle)
{
	UECFDelayTicksBP* Proxy = NewObject<UECFDelayTicksBP>();
	if (Proxy)
	{
		Proxy->Init(WorldContextObject, Settings);
		// 使用弱指针捕获，防止回调延迟执行时 Proxy 已被 GC 导致悬空访问
		TWeakObjectPtr<UECFDelayTicksBP> WeakProxy(Proxy);
		Proxy->Proxy_Handle = FFlow::DelayTicks(WorldContextObject, DelayTicks, [WeakProxy](bool bStopped)
		{
			if (UECFDelayTicksBP* StrongProxy = WeakProxy.Get())
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
