// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#include "ECFTimelineVectorBP.h"
#include "EnhancedCodeFlow.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UECFTimelineVectorBP* UECFTimelineVectorBP::ECFTimelineVector(const UObject* WorldContextObject, FVector StartValue, FVector StopValue, float Time, FECFActionSettings Settings, FECFHandleBP& Handle, EECFBlendFunc BlendFunc /*= EECFBlendFunc::ECFBlend_Linear*/, float BlendExp /*= 1.f*/, float PlayRate /*= 1.f*/)
{
	UECFTimelineVectorBP* Proxy = NewObject<UECFTimelineVectorBP>();
	if (Proxy)
	{
		Proxy->Init(WorldContextObject, Settings);
		// 使用弱指针捕获，防止回调延迟执行时 Proxy 已被 GC 导致悬空访问
		TWeakObjectPtr<UECFTimelineVectorBP> WeakProxy(Proxy);
		Proxy->Proxy_Handle = FFlow::AddTimelineVector(WorldContextObject,
			StartValue, StopValue, Time,
			[WeakProxy](FVector Value, float Time)
			{
				if (UECFTimelineVectorBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy))
					{
						StrongProxy->OnTick.Broadcast(Value, Time, false);
					}
				}
			},
			[WeakProxy](FVector Value, float Time, bool bStopped)
			{
				if (UECFTimelineVectorBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy))
					{
						StrongProxy->OnFinished.Broadcast(Value, Time, bStopped);
						StrongProxy->ClearAsyncBPAction();
					}
				}
			},
			BlendFunc, BlendExp, PlayRate, Settings);
		Handle = FECFHandleBP(Proxy->Proxy_Handle);
	}

	return Proxy;
}

ECF_PRAGMA_ENABLE_OPTIMIZATION
