// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFWaitAndExecuteBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnECFWaitAndExecuteBPEvent, class UECFWaitAndExecuteBP*, Action, float, DeltaTime, bool, bTimedOut, bool, bStopped);

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFWaitAndExecuteBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFWaitAndExecuteBPEvent OnWait;

	UPROPERTY(BlueprintAssignable)
	FOnECFWaitAndExecuteBPEvent OnExecute;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings", ToolTip = "等待条件满足后执行代码\n\n使用方法：\n1. 连接Predicate函数设置条件\n2. 当条件返回true时执行OnExecute回调\n\n参数：\n- InTimeOut：超时时间（秒），设为0表示无限等待\n- Settings：可控制是否忽略游戏暂停等\n\n输出参数：\n- Handle：节点操作句柄，用于停止或检查状态\n- On Wait：等待过程中每帧触发的回调\n- On Execute：条件满足时触发的回调\n- Action：节点自身引用，用于后续操作\n- Delta Time：当前帧与上一帧的时间间隔\n- Timed Out：是否因超时而结束\n- Stopped：是否被外部停止\n\n特别适用：\n- 等待资源加载完成\n- 等待玩家到达某位置\n- 条件触发的游戏逻辑", DisplayName = "ECF - 等待执行-异步流程"), Category = "ECF")
	static UECFWaitAndExecuteBP* ECFWaitAndExecute(const UObject* WorldContextObject, float InTimeOut, FECFActionSettings Settings, FECFHandleBP& Handle);

	UFUNCTION(BlueprintCallable, meta = (ToolTip = "传入'true'来执行'等待执行'动作", DisplayName = "ECF - 条件判断 (等待执行)"), Category = "ECF")
	void Predicate(bool bHasFinished);

protected:

	bool Proxy_HasFinished = false;
};
