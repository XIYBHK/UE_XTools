// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFTypes.h"
#include "ECFTimelineVectorBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnECFTimelineVectorBPEvent, FVector, Value, float, Time, bool, bStopped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnECFTimelineVectorBPEventTrack, FName, EventName, float, EventTime);

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFTimelineVectorBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFTimelineVectorBPEvent OnTick;

	UPROPERTY(BlueprintAssignable)
	FOnECFTimelineVectorBPEvent OnFinished;

	UPROPERTY(BlueprintAssignable)
	FOnECFTimelineVectorBPEventTrack OnEvent;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings, BlendFunc, BlendExp, PlayRate, PlayDirection", ToolTip = "添加向量时间轴。反向播放会从结束值开始并在起始值完成。", DisplayName = "ECF - 向量时间轴"), Category = "XTools|ECF|时间轴")
	static UECFTimelineVectorBP* ECFTimelineVector(const UObject* WorldContextObject, FVector StartValue, FVector StopValue, float Time, FECFActionSettings Settings, FECFHandleBP& Handle, EECFBlendFunc BlendFunc, float BlendExp, float PlayRate, EECFPlayDirection PlayDirection, TArray<FECFTimelineEvent> Events);
};
