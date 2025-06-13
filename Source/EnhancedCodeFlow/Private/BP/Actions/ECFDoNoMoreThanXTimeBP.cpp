// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#include "ECFDoNoMoreThanXTimeBP.h"
#include "EnhancedCodeFlow.h"
#include "BP/ECFBPLibrary.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UECFDoNoMoreThanXTimeBP* UECFDoNoMoreThanXTimeBP::ECFDoNoMoreThanXTime(const UObject* WorldContextObject, float Time, FECFHandleBP& Handle, FECFInstanceIdBP& InstanceId, FECFActionSettings Settings, int32 MaxExecsEnqueued /*= 1*/)
{
	UECFDoNoMoreThanXTimeBP* Proxy = NewObject<UECFDoNoMoreThanXTimeBP>();
	if (Proxy)
	{
		Proxy->Init(WorldContextObject, Settings);

		if (InstanceId.InstanceId.IsValid() == false)
		{
			UECFBPLibrary::ECFGetNewInstanceId(InstanceId);
		}

		Proxy->Proxy_Handle = FFlow::DoNoMoreThanXTime(WorldContextObject, [Proxy]()
		{
			// 因为动作将在第一次调用时执行，检查异步动作是否已激活
			// 未激活的动作没有委托绑定！
			// 为激活排队 OnExecute 广播
			if (IsProxyValid(Proxy))
			{
				if (Proxy->bActivated)
				{
					Proxy->OnExecute.Broadcast();
				}
				else
				{
					Proxy->bExecuteOnActivation = true;
				}
			}
		}, Time, MaxExecsEnqueued, InstanceId.InstanceId, Settings);
		Handle = FECFHandleBP(Proxy->Proxy_Handle);
	}

	return Proxy;
}

void UECFDoNoMoreThanXTimeBP::Activate()
{
	Super::Activate();

	// 如果已排队，则广播 OnExecute 事件
	if (bExecuteOnActivation)
	{
		bExecuteOnActivation = false;
		OnExecute.Broadcast();
	}
}

ECF_PRAGMA_ENABLE_OPTIMIZATION