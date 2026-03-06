// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFTypes.h"
#include "ECFRunAsyncThenBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnECFRunAsyncThenBPEvent, bool, bTimedOut, bool, bStopped);

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFRunAsyncThenBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFRunAsyncThenBPEvent AsyncTask;

	UPROPERTY(BlueprintAssignable)
	FOnECFRunAsyncThenBPEvent OnExecute;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings", ToolTip = "启动一个后台任务，并在主线程上接收时序通知。\n\n使用流程：\n1. 后台任务由底层异步系统执行\n2. AsyncTask 和 OnExecute 委托都会在主线程广播，适合更新蓝图状态或界面\n\n参数：\n- InTimeOut：超时时间（秒），超时后会停止等待并触发回调\n- Priority：线程优先级\n\n输出参数：\n- Handle：节点操作句柄，用于停止或检查状态\n- AsyncTask：后台任务开始后的主线程通知\n- OnExecute：后台任务完成后的主线程回调\n- bTimedOut：是否因超时而结束\n- bStopped：是否被外部停止\n\n注意事项：\n- Blueprint 委托不会在后台线程执行，避免非游戏线程访问 UObject/蓝图 VM\n- 即使停止动作，后台线程仍可能继续运行到底层任务结束", DisplayName = "ECF - 异步执行-异步"), Category = "ECF")
	static UECFRunAsyncThenBP* ECFRunAsyncThen(const UObject* WorldContextObject, float InTimeOut, EECFAsyncPrio Priority, FECFActionSettings Settings, FECFHandleBP& Handle);
};
