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

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings", ToolTip = "创建一个高度可控的定时器\n\nTickingTime参数：\n- 正数：定时器执行指定秒数后结束\n- -1：无限执行直到手动停止或所属对象销毁\n\nSettings参数：\n- 可配置执行间隔、忽略暂停、忽略时间膨胀等\n\n提供两个事件回调：\n- OnTick: 每次更新时调用，提供DeltaTime参数\n- OnComplete: 定时器结束时调用\n\n较Actor Tick优点：\n1. 不需要启用组件Tick\n2. 可单独暂停/恢复\n3. 拥有更多控制选项", DisplayName = "ECF - 定时器-异步流程"), Category = "ECF")
	static UECFTickerBP* ECFTicker(const UObject* WorldContextObject, float TickingTime, FECFActionSettings Settings, FECFHandleBP& Handle);
};
