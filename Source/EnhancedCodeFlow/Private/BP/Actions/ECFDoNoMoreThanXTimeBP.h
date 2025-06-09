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

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings,MaxExecsEnqueued", ToolTip = "节流阀功能(实例化)：限制相同实例ID的动作执行频率\n\nTime参数：冷却时间，每次执行后必须等待此时间才能执行下一次\nMaxExecsEnqueued参数：最大排队执行数，超出此数量的调用会被丢弃\n\n使用方法：\n1. 首次调用会立即执行\n2. 冷却期间的调用会排队等待\n3. 冷却时间结束后执行队列中的下一个动作\n4. 必须使用相同的实例ID才能正确跟踪", DisplayName = "ECF - 限制执行次数-异步流程"), Category = "ECF")
	static UECFDoNoMoreThanXTimeBP* ECFDoNoMoreThanXTime(const UObject* WorldContextObject, float Time, FECFHandleBP& Handle, UPARAM(Ref) FECFInstanceIdBP& InstanceId, FECFActionSettings Settings, int32 MaxExecsEnqueued = 1);

	bool bExecuteOnActivation = false;
	

	void Activate() override;
};
