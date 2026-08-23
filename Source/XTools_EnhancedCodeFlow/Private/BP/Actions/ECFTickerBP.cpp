// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#include "ECFTickerBP.h"
#include "EnhancedCodeFlow.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UECFTickerBP* UECFTickerBP::ECFTicker(const UObject* WorldContextObject, float TickingTime, FECFActionSettings Settings, FECFHandleBP& Handle)
{
	UECFTickerBP* Proxy = NewObject<UECFTickerBP>();
	if (Proxy)
	{
		Proxy->Init(WorldContextObject, Settings);
		// 使用弱指针捕获，防止回调延迟执行时 Proxy 已被 GC 导致悬空访问
		TWeakObjectPtr<UECFTickerBP> WeakProxy(Proxy);
		Proxy->Proxy_Handle = FFlow::AddTicker(WorldContextObject, TickingTime,
			[WeakProxy](float DeltaTime)
			{
				if (UECFTickerBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy))
					{
						StrongProxy->OnTick.Broadcast(DeltaTime, false);
					}
				}
			},
			[WeakProxy](bool bStopped)
			{
				if (UECFTickerBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy))
					{
						StrongProxy->OnComplete.Broadcast(0.f, bStopped);
						StrongProxy->ClearAsyncBPAction();
					}
				}
			},
		Settings);
		Handle = FECFHandleBP(Proxy->Proxy_Handle);
	}

	return Proxy;
}

ECF_PRAGMA_ENABLE_OPTIMIZATION
