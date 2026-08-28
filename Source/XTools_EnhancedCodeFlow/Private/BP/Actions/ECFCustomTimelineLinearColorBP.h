// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFTypes.h"
#include "ECFCustomTimelineLinearColorBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnECFCustomTimelineLinearColorBPEvent, FLinearColor, Value, float, Time, bool, bStopped, FName, EventName, float, EventTime);

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFCustomTimelineLinearColorBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFCustomTimelineLinearColorBPEvent OnTick;

	UPROPERTY(BlueprintAssignable)
	FOnECFCustomTimelineLinearColorBPEvent OnFinished;

	UPROPERTY(BlueprintAssignable)
	FOnECFCustomTimelineLinearColorBPEvent OnEvent;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings, PlayRate, PlayDirection", ToolTip = "添加颜色曲线时间轴。反向播放会从曲线末端开始。", DisplayName = "ECF - 自定义颜色时间轴"), Category = "XTools|ECF|时间轴")
	static UECFCustomTimelineLinearColorBP* ECFCustomTimelineLinearColor(const UObject* WorldContextObject, class UCurveLinearColor* CurveLinearColor, FECFActionSettings Settings, FECFHandleBP& Handle, float PlayRate, EECFPlayDirection PlayDirection, TArray<FECFTimelineEvent> Events);
};
