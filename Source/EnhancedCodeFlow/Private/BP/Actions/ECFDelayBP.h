// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFDelayBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnECFDelayBPEvent, bool, bStopped);

UCLASS()
class ENHANCEDCODEFLOW_API UECFDelayBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFDelayBPEvent OnComplete;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings", ToolTip = "延迟指定时间后执行动作", DisplayName = "ECF - 延迟执行-协程"), Category = "ECF")
	static UECFDelayBP* ECFDelay(const UObject* WorldContextObject, float DelayTime, FECFActionSettings Settings, FECFHandleBP& Handle);
};
