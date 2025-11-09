// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFDelayBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnECFDelayBPEvent, bool, bStopped);

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFDelayBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFDelayBPEvent OnComplete;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings", ToolTip = "增强版延迟节点\n\n相比原生Delay节点优势：\n- 可选择忽略游戏暂停\n- 可选择忽略时间膨胀\n- 可单独停止或暂停\n- 提供bStopped参数判断是否被外部停止\n\n输出参数：\n- Handle：节点操作句柄，用于停止或检查状态\n- OnComplete：延迟完成时触发的回调\n- bStopped：是否被外部停止\n\n当DelayTime为0时会在下一帧执行\n当DelayTime为负数时不会执行", DisplayName = "ECF - 延迟执行-协程"), Category = "ECF")
	static UECFDelayBP* ECFDelay(const UObject* WorldContextObject, float DelayTime, FECFActionSettings Settings, FECFHandleBP& Handle);
};
