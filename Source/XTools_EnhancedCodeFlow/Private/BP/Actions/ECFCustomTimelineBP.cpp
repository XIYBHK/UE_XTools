// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#include "ECFCustomTimelineBP.h"
#include "EnhancedCodeFlow.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UECFCustomTimelineBP* UECFCustomTimelineBP::ECFCustomTimeline(const UObject* WorldContextObject, UCurveFloat* CurveFloat, FECFActionSettings Settings, FECFHandleBP& Handle, float PlayRate /*= 1.f*/, EECFPlayDirection PlayDirection /*= EECFPlayDirection::Forward*/, TArray<FECFTimelineEvent> Events)
{
	UECFCustomTimelineBP* Proxy = NewObject<UECFCustomTimelineBP>();
	if (Proxy)
	{
		Proxy->Init(WorldContextObject, Settings);
		// 使用弱指针捕获，防止回调延迟执行时 Proxy 已被 GC 导致悬空访问
		TWeakObjectPtr<UECFCustomTimelineBP> WeakProxy(Proxy);
		Proxy->Proxy_Handle = FFlow::AddCustomTimeline(WorldContextObject, CurveFloat,
			[WeakProxy](float Value, float Time)
			{
				if (UECFCustomTimelineBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy))
					{
						StrongProxy->OnTick.Broadcast(Value, Time, false);
					}
				}
			},
			[WeakProxy](float Value, float Time, bool bStopped)
			{
				if (UECFCustomTimelineBP* StrongProxy = WeakProxy.Get())
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
				if (UECFCustomTimelineBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy)) StrongProxy->OnEvent.Broadcast(EventName, EventTime);
				}
			});
		Handle = FECFHandleBP(Proxy->Proxy_Handle);
	}

	return Proxy;
}

ECF_PRAGMA_ENABLE_OPTIMIZATION
