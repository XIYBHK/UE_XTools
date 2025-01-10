// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFTickerBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnECFTickerBPEvent, float, DeltaTime, bool, bStopped);

UCLASS()
class ENHANCEDCODEFLOW_API UECFTickerBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFTickerBPEvent OnTick;

	UPROPERTY(BlueprintAssignable)
	FOnECFTickerBPEvent OnComplete;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings", ToolTip = "创建一个定时器。可以在指定时间内执行，直到被停止或所属对象被销毁。\n设置TickingTime为-1可以无限执行。", DisplayName = "ECF - 定时器-异步流程"), Category = "ECF")
	static UECFTickerBP* ECFTicker(const UObject* WorldContextObject, float TickingTime, FECFActionSettings Settings, FECFHandleBP& Handle);
};
