// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFWhileTrueExecuteBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnECFWhileTrueExecuteBPEvent, class UECFWhileTrueExecuteBP*, Action, float, DeltaTime, bool, bTimedOut, bool, bStopped);

UCLASS()
class ENHANCEDCODEFLOW_API UECFWhileTrueExecuteBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFWhileTrueExecuteBPEvent OnWhile;

	UPROPERTY(BlueprintAssignable)
	FOnECFWhileTrueExecuteBPEvent OnExecute;

	UPROPERTY(BlueprintAssignable)
	FOnECFWhileTrueExecuteBPEvent OnComplete;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings", ToolTip = "当特定条件为真时持续执行函数。\n使用动作的'条件判断'函数来控制。\n设置大于0的超时时间可在指定时间后停止动作。", DisplayName = "ECF - 循环执行-异步流程"), Category = "ECF")
	static UECFWhileTrueExecuteBP* ECFWhileTrueExecute(const UObject* WorldContextObject, float TimeOut, FECFActionSettings Settings, FECFHandleBP& Handle);

	UFUNCTION(BlueprintCallable, meta = (ToolTip = "传入'true'继续'循环执行'动作，传入'false'停止执行", DisplayName = "ECF - 条件判断 (循环执行)"), Category = "ECF")
	void Predicate(bool bIsTrue);

protected:

	bool Proxy_IsTrue = true;
};
