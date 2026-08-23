// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#include "ECFWhileTrueExecuteBP.h"
#include "EnhancedCodeFlow.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UECFWhileTrueExecuteBP* UECFWhileTrueExecuteBP::ECFWhileTrueExecute(const UObject* WorldContextObject, float TimeOut, FECFActionSettings Settings, FECFHandleBP& Handle)
{
	UECFWhileTrueExecuteBP* Proxy = NewObject<UECFWhileTrueExecuteBP>();
	if (Proxy)
	{
		Proxy->Init(WorldContextObject, Settings);
		Proxy->Proxy_IsTrue = true;
		constexpr bool bEvaluatePredicateOnSetup = false;
		// 使用弱指针捕获，防止回调延迟执行时 Proxy 已被 GC 导致悬空访问
		TWeakObjectPtr<UECFWhileTrueExecuteBP> WeakProxy(Proxy);
		Proxy->Proxy_Handle = FFlow::WhileTrueExecute(WorldContextObject,
			[WeakProxy]()
			{
				if (UECFWhileTrueExecuteBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy))
					{
						StrongProxy->OnWhile.Broadcast(StrongProxy, 0.f, false, false);
						return StrongProxy->Proxy_IsTrue;
					}
				}
				return false;
			},
			[WeakProxy](float DeltaTime)
			{
				if (UECFWhileTrueExecuteBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy))
					{
						StrongProxy->OnExecute.Broadcast(StrongProxy, DeltaTime, false, false);
					}
				}
			},
			[WeakProxy](bool bTimedOut, bool bStopped)
			{
				if (UECFWhileTrueExecuteBP* StrongProxy = WeakProxy.Get())
				{
					if (IsProxyValid(StrongProxy))
					{
						StrongProxy->OnComplete.Broadcast(StrongProxy, 0.f, bTimedOut, bStopped);
						StrongProxy->ClearAsyncBPAction();
					}
				}
			},
		TimeOut, Settings, bEvaluatePredicateOnSetup);
		Handle = FECFHandleBP(Proxy->Proxy_Handle);
	}

	return Proxy;
}

void UECFWhileTrueExecuteBP::Predicate(bool bIsTrue)
{
	Proxy_IsTrue = bIsTrue;
}

ECF_PRAGMA_ENABLE_OPTIMIZATION
