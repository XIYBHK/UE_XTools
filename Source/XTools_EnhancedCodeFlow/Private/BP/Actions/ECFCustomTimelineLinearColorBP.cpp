// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#include "ECFCustomTimelineLinearColorBP.h"
#include "EnhancedCodeFlow.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UECFCustomTimelineLinearColorBP* UECFCustomTimelineLinearColorBP::ECFCustomTimelineLinearColor(const UObject* WorldContextObject, UCurveLinearColor* CurveLinearColor, FECFActionSettings Settings, FECFHandleBP& Handle, float PlayRate /*= 1.f*/, EECFPlayDirection PlayDirection /*= EECFPlayDirection::Forward*/, TArray<FECFTimelineEvent> Events)
{
	UECFCustomTimelineLinearColorBP* Proxy = NewObject<UECFCustomTimelineLinearColorBP>();
	if (Proxy)
	{
		Proxy->Init(WorldContextObject, Settings);
		// 使用弱指针捕获，防止回调延迟执行时 Proxy 已被 GC 导致悬空访问
		TWeakObjectPtr<UECFCustomTimelineLinearColorBP> WeakProxy(Proxy);
		Proxy->Proxy_Handle = FFlow::AddCustomTimelineLinearColor(WorldContextObject, CurveLinearColor,
			[WeakProxy](FLinearColor Value, float Time)
			{
				if (UECFCustomTimelineLinearColorBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy))
					{
						StrongProxy->OnTick.Broadcast(Value, Time, false);
					}
				}
			},
			[WeakProxy](FLinearColor Value, float Time, bool bStopped)
			{
				if (UECFCustomTimelineLinearColorBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy))
					{
						StrongProxy->OnFinished.Broadcast(Value, Time, bStopped);
						StrongProxy->ClearAsyncBPAction();
					}
				}
			},
			PlayRate, Settings, PlayDirection, MoveTemp(Events),
			[WeakProxy](FName EventName, float EventTime)
			{
				if (UECFCustomTimelineLinearColorBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy)) StrongProxy->OnEvent.Broadcast(EventName, EventTime);
				}
			});
		Handle = FECFHandleBP(Proxy->Proxy_Handle);
	}

	return Proxy;
}

ECF_PRAGMA_ENABLE_OPTIMIZATION
