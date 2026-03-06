// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#include "ECFActionBP.h"
#include "EnhancedCodeFlow.h"
#include "ECFStats.h"
#include "Containers/Ticker.h"
#include "Runtime/Launch/Resources/Version.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

DEFINE_STAT(STAT_ECF_AsyncBPObjectsCount);

UECFActionBP::UECFActionBP()
{
#if STATS
	INC_DWORD_STAT(STAT_ECF_AsyncBPObjectsCount);
#endif
}

UECFActionBP::~UECFActionBP()
{
#if STATS
	DEC_DWORD_STAT(STAT_ECF_AsyncBPObjectsCount);
#endif
}

void UECFActionBP::Init(const UObject* WorldContextObject, FECFActionSettings& Settings)
{
	Proxy_WorldContextObject = WorldContextObject;
	RegisterWithGameInstance(WorldContextObject);
	Proxy_IsPausedAtStart = Settings.bStartPaused;
	Settings.bStartPaused = true;
}

UWorld* UECFActionBP::GetWorld() const
{
	return Proxy_WorldContextObject ? Proxy_WorldContextObject->GetWorld() : nullptr;
}

void UECFActionBP::Activate()
{
	bActivated = true;
	if (!Proxy_Handle.IsValid())
	{
		ClearAsyncBPAction();
		return;
	}

	if (Proxy_IsPausedAtStart == false)
	{
		FFlow::ResumeAction(Proxy_WorldContextObject, Proxy_Handle);
	}

	StartCompletionWatch();
}

void UECFActionBP::ClearAsyncBPAction()
{
	if (Proxy_CompletionTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Proxy_CompletionTickerHandle);
		Proxy_CompletionTickerHandle.Reset();
	}

	SetReadyToDestroy();
#if (ENGINE_MAJOR_VERSION == 5)
	MarkAsGarbage();
#else
	MarkPendingKill();
#endif
}

void UECFActionBP::StartCompletionWatch()
{
	if (Proxy_CompletionTickerHandle.IsValid() || !Proxy_Handle.IsValid())
	{
		return;
	}

	TWeakObjectPtr<UECFActionBP> WeakThis(this);
	Proxy_CompletionTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([WeakThis](float DeltaTime)
		{
			if (UECFActionBP* StrongThis = WeakThis.Get())
			{
				return StrongThis->TickCompletionWatch(DeltaTime);
			}

			return false;
		}),
		0.0f);
}

bool UECFActionBP::TickCompletionWatch(float DeltaTime)
{
	if (!Proxy_Handle.IsValid())
	{
		ClearAsyncBPAction();
		return false;
	}

	if (!FFlow::IsActionRunning(Proxy_WorldContextObject.Get(), Proxy_Handle))
	{
		ClearAsyncBPAction();
		return false;
	}

	return true;
}

bool UECFActionBP::IsProxyValid(const UObject* ProxyObject)
{
	const bool bIsValid = (IsValid(ProxyObject) && (ProxyObject->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) == false));
	ensureAlwaysMsgf(bIsValid, TEXT("Invalid ProxyObject!"));
	return bIsValid;
}

ECF_PRAGMA_ENABLE_OPTIMIZATION
