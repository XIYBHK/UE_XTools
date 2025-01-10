// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFWaitAndExecuteBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnECFWaitAndExecuteBPEvent, class UECFWaitAndExecuteBP*, Action, float, DeltaTime, bool, bTimedOut, bool, bStopped);

UCLASS()
class ENHANCEDCODEFLOW_API UECFWaitAndExecuteBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFWaitAndExecuteBPEvent OnWait;

	UPROPERTY(BlueprintAssignable)
	FOnECFWaitAndExecuteBPEvent OnExecute;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings", ToolTip = "等待特定条件满足后执行代码。\n使用动作的'条件判断'函数来控制。\n设置大于0的超时时间可在指定时间后停止动作。", DisplayName = "ECF - 等待执行-异步流程"), Category = "ECF")
	static UECFWaitAndExecuteBP* ECFWaitAndExecute(const UObject* WorldContextObject, float InTimeOut, FECFActionSettings Settings, FECFHandleBP& Handle);

	UFUNCTION(BlueprintCallable, meta = (ToolTip = "传入'true'来执行'等待执行'动作", DisplayName = "ECF - 条件判断 (等待执行)"), Category = "ECF")
	void Predicate(bool bHasFinished);

protected:

	bool Proxy_HasFinished = false;
};
