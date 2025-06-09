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

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings", ToolTip = "在后台线程执行任务，完成后返回主线程\n\n使用流程：\n1. 在AsyncTask回调中编写要在后台线程执行的代码\n2. 在OnExecute回调中处理结果\n\n参数：\n- InTimeOut：超时时间（秒），超时后会停止等待并触发回调\n- Priority：线程优先级\n\n输出参数：\n- Handle：节点操作句柄，用于停止或检查状态\n- AsyncTask：在后台线程中执行的任务回调\n- OnExecute：后台任务完成后在主线程执行的回调\n- bTimedOut：是否因超时而结束\n- bStopped：是否被外部停止\n\n注意事项：\n- 后台线程不能访问UObject或GameThread内容\n- 适用于复杂计算、文件操作等\n- 即使停止动作，后台线程仍会继续运行", DisplayName = "ECF - 异步执行-异步"), Category = "ECF")
	static UECFRunAsyncThenBP* ECFRunAsyncThen(const UObject* WorldContextObject, float InTimeOut, EECFAsyncPrio Priority, FECFActionSettings Settings, FECFHandleBP& Handle);
};
