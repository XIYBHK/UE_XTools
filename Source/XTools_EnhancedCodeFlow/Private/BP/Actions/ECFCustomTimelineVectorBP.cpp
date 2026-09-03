// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#include "ECFCustomTimelineVectorBP.h"
#include "EnhancedCodeFlow.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UECFCustomTimelineVectorBP* UECFCustomTimelineVectorBP::ECFCustomTimelineVector(const UObject* WorldContextObject, UCurveVector* CurveVector, FECFActionSettings Settings, FECFHandleBP& Handle, float PlayRate /*= 1.f*/, EECFPlayDirection PlayDirection /*= EECFPlayDirection::Forward*/, TArray<FECFTimelineEvent> Events)
{
    UECFCustomTimelineVectorBP* Proxy = NewObject<UECFCustomTimelineVectorBP>();
    if (Proxy)
    {
        Proxy->Init(WorldContextObject, Settings);
        // 使用弱指针捕获，防止回调延迟执行时 Proxy 已被 GC 导致悬空访问
        TWeakObjectPtr<UECFCustomTimelineVectorBP> WeakProxy(Proxy);
        Proxy->Proxy_Handle = FFlow::AddCustomTimelineVector(WorldContextObject, CurveVector,
            [WeakProxy](FVector Value, float Time)
            {
                if (UECFCustomTimelineVectorBP* StrongProxy = WeakProxy.Get())
                {
                    if (IsProxyValid(StrongProxy))
                    {
                        StrongProxy->OnTick.Broadcast(Value, Time, false, NAME_None, -1.f);
                    }
                }
            },
            [WeakProxy](FVector Value, float Time, bool bStopped)
            {
                if (UECFCustomTimelineVectorBP* StrongProxy = WeakProxy.Get())
                {
                    if (IsProxyValid(StrongProxy))
                    {
                        StrongProxy->OnFinished.Broadcast(Value, Time, bStopped, NAME_None, -1.f);
                        StrongProxy->ClearAsyncBPAction();
                    }
				}
			},
			PlayRate, Settings, PlayDirection, MoveTemp(Events),
			[WeakProxy](FName EventName, float EventTime)
			{
				if (UECFCustomTimelineVectorBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy)) StrongProxy->OnEvent.Broadcast(FVector::ZeroVector, EventTime, false, EventName, EventTime);
				}
			});
        Handle = FECFHandleBP(Proxy->Proxy_Handle);
    }

    return Proxy;
}

ECF_PRAGMA_ENABLE_OPTIMIZATION
