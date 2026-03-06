// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "Kismet/BlueprintAsyncActionBase.h"
#include "Containers/Ticker.h"
#include "ECFActionSettings.h"
#include "BP/ECFHandleBP.h"
#include "ECFActionBP.generated.h"

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFActionBP : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

	/**
	 * Default constructor and destructor. Used only for stat counting.
	 */
	UECFActionBP();
	virtual ~UECFActionBP();

	/**
	 * Initializes the Async Action with project WorldContext Object and Settings.
	 * Set the action to start paused, as unreal will run it in next tick.
	 */
	virtual void Init(const UObject* WorldContextObject, FECFActionSettings& Settings);

	/**
	 * Getting world from World Context Object.
	 */
	class UWorld* GetWorld() const override;

	/**
	 * Activate the action by resuming it (we always start it as paused).
	 */
	void Activate() override;

protected:

	// The World Context Object that started this action.
	UPROPERTY(Transient)
	TObjectPtr<const UObject> Proxy_WorldContextObject;

	// Handle of the Action to control.
	FECFHandle Proxy_Handle;

	// If the action was supposed to be paused - to not resume it on Activate!
	bool Proxy_IsPausedAtStart;

	// Just a handy flag to check if the action has been activated already.
	bool bActivated = false;

	// Watches the underlying handle and clears this proxy when the action disappears without a callback.
	FTSTicker::FDelegateHandle Proxy_CompletionTickerHandle;

	// Mark this async node as ready to destroy.
	void ClearAsyncBPAction();

	// Start polling the underlying handle lifetime after activation.
	void StartCompletionWatch();

	// Per-frame completion watch used for externally stopped or silently finished actions.
	bool TickCompletionWatch(float DeltaTime);

	// Checks if the given proxy object is still valid and safe to use.
	static bool IsProxyValid(const UObject* ProxyObject);
};
