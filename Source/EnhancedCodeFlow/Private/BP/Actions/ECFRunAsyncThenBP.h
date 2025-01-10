// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFTypes.h"
#include "ECFRunAsyncThenBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnECFRunAsyncThenBPEvent, bool, bTimedOut, bool, bStopped);

UCLASS()
class ENHANCEDCODEFLOW_API UECFRunAsyncThenBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFRunAsyncThenBPEvent AsyncTask;

	UPROPERTY(BlueprintAssignable)
	FOnECFRunAsyncThenBPEvent OnExecute;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings", ToolTip = "在单独的线程上运行任务函数，并在任务结束时调用回调函数。\n设置大于0的超时时间可在指定时间后停止动作。", DisplayName = "ECF - 异步执行-异步"), Category = "ECF")
	static UECFRunAsyncThenBP* ECFRunAsyncThen(const UObject* WorldContextObject, float InTimeOut, EECFAsyncPrio Priority, FECFActionSettings Settings, FECFHandleBP& Handle);
};
