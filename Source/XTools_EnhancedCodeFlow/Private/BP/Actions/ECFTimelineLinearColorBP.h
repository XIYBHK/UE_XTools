// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFTypes.h"
#include "ECFTimelineLinearColorBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnECFTimelineLinearColorBPEvent, FLinearColor, Value, float, Time, bool, bStopped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnECFTimelineLinearColorBPEventTrack, FName, EventName, float, EventTime);

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFTimelineLinearColorBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFTimelineLinearColorBPEvent OnTick;

	UPROPERTY(BlueprintAssignable)
	FOnECFTimelineLinearColorBPEvent OnFinished;

	UPROPERTY(BlueprintAssignable)
	FOnECFTimelineLinearColorBPEventTrack OnEvent;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings, BlendFunc, BlendExp, PlayRate, PlayDirection", ToolTip = "添加颜色时间轴。反向播放会从结束值开始并在起始值完成。", DisplayName = "ECF - 颜色时间轴"), Category = "XTools|ECF|时间轴")
	static UECFTimelineLinearColorBP* ECFTimelineLinearColor(const UObject* WorldContextObject, FLinearColor StartValue, FLinearColor StopValue, float Time, FECFActionSettings Settings, FECFHandleBP& Handle, EECFBlendFunc BlendFunc, float BlendExp, float PlayRate, EECFPlayDirection PlayDirection, TArray<FECFTimelineEvent> Events);
};
