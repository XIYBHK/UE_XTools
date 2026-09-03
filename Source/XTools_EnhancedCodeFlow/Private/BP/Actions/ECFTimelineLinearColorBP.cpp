// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#include "ECFTimelineLinearColorBP.h"
#include "EnhancedCodeFlow.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UECFTimelineLinearColorBP* UECFTimelineLinearColorBP::ECFTimelineLinearColor(const UObject* WorldContextObject, FLinearColor StartValue, FLinearColor StopValue, float Time, FECFActionSettings Settings, FECFHandleBP& Handle, EECFBlendFunc BlendFunc /*= EECFBlendFunc::ECFBlend_Linear*/, float BlendExp /*= 1.f*/, float PlayRate /*= 1.f*/, EECFPlayDirection PlayDirection /*= EECFPlayDirection::Forward*/, TArray<FECFTimelineEvent> Events)
{
	UECFTimelineLinearColorBP* Proxy = NewObject<UECFTimelineLinearColorBP>();
	if (Proxy)
	{
		Proxy->Init(WorldContextObject, Settings);
		// 使用弱指针捕获，防止回调延迟执行时 Proxy 已被 GC 导致悬空访问
		TWeakObjectPtr<UECFTimelineLinearColorBP> WeakProxy(Proxy);
		Proxy->Proxy_Handle = FFlow::AddTimelineLinearColor(WorldContextObject,
			StartValue, StopValue, Time,
			[WeakProxy](FLinearColor Value, float Time)
			{
				if (UECFTimelineLinearColorBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy))
					{
						StrongProxy->OnTick.Broadcast(Value, Time, false, NAME_None, -1.f);
					}
				}
			},
			[WeakProxy](FLinearColor Value, float Time, bool bStopped)
			{
				if (UECFTimelineLinearColorBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy))
					{
						StrongProxy->OnFinished.Broadcast(Value, Time, bStopped, NAME_None, -1.f);
						StrongProxy->ClearAsyncBPAction();
					}
				}
			},
			BlendFunc, BlendExp, PlayRate, Settings, PlayDirection, MoveTemp(Events),
			[WeakProxy](FName EventName, float EventTime)
			{
				if (UECFTimelineLinearColorBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy)) StrongProxy->OnEvent.Broadcast(FLinearColor::Transparent, EventTime, false, EventName, EventTime);
				}
			});
		Handle = FECFHandleBP(Proxy->Proxy_Handle);
	}

	return Proxy;
}

ECF_PRAGMA_ENABLE_OPTIMIZATION
