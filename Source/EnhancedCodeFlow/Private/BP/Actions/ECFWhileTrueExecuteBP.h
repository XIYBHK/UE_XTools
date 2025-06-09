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

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings", ToolTip = "持续执行代码直到条件为假\n\n使用方法：\n1. 连接Predicate函数控制循环条件\n2. 当条件为true时，每帧都执行OnExecute\n3. 条件变为false或超时时执行OnComplete\n\n参数：\n- TimeOut：超时时间（秒），设为0表示无限循环\n\n输出参数：\n- Handle：节点操作句柄，用于停止或检查状态\n- OnWhile：条件检查回调，在循环过程中触发\n- OnExecute：每帧执行的主要回调\n- OnComplete：循环结束时触发的回调\n- Action：节点自身引用，用于后续操作\n- Delta Time：当前帧与上一帧的时间间隔\n- Timed Out：是否因超时而结束\n- Stopped：是否被外部停止\n\n常见用途：\n- 持续检查游戏状态\n- 持续更新UI元素\n- 实现自定义的游戏循环", DisplayName = "ECF - 循环执行-异步流程"), Category = "ECF")
	static UECFWhileTrueExecuteBP* ECFWhileTrueExecute(const UObject* WorldContextObject, float TimeOut, FECFActionSettings Settings, FECFHandleBP& Handle);

	UFUNCTION(BlueprintCallable, meta = (ToolTip = "传入'true'继续'循环执行'动作，传入'false'停止执行", DisplayName = "ECF - 条件判断 (循环执行)"), Category = "ECF")
	void Predicate(bool bIsTrue);

protected:

	bool Proxy_IsTrue = true;
};
