// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFDoNoMoreThanXTimeBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnECFDoNoMoreThanXTimeBPEvent);

UCLASS()
class ENHANCEDCODEFLOW_API UECFDoNoMoreThanXTimeBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFDoNoMoreThanXTimeBPEvent OnExecute;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings,MaxExecsEnqueued", ToolTip = "限制动作在指定时间内执行的次数，可以设置最大排队次数", DisplayName = "ECF - 限制执行次数-异步流程"), Category = "ECF")
	static UECFDoNoMoreThanXTimeBP* ECFDoNoMoreThanXTime(const UObject* WorldContextObject, float Time, FECFHandleBP& Handle, UPARAM(Ref) FECFInstanceIdBP& InstanceId, FECFActionSettings Settings, int32 MaxExecsEnqueued = 1);

	bool bExecuteOnActivation = false;
	

	void Activate() override;
};
