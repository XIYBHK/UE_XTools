// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#include "ECFDelayBP.h"
#include "EnhancedCodeFlow.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UECFDelayBP* UECFDelayBP::ECFDelay(const UObject* WorldContextObject, float DelayTime, FECFActionSettings Settings, FECFHandleBP& Handle)
{
	UECFDelayBP* Proxy = NewObject<UECFDelayBP>();
	if (Proxy)
	{
		Proxy->Init(WorldContextObject, Settings);
		// 使用弱指针捕获，防止回调延迟执行时 Proxy 已被 GC 导致悬空访问
		TWeakObjectPtr<UECFDelayBP> WeakProxy(Proxy);
		Proxy->Proxy_Handle = FFlow::Delay(WorldContextObject, DelayTime, [WeakProxy](bool bStopped)
		{
			if (UECFDelayBP* StrongProxy = WeakProxy.Get())
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
