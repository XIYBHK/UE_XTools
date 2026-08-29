// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFTypes.h"
#include "ECFCustomTimelineVectorBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnECFCustomTimelineVectorBPEvent, FVector, Value, float, Time, bool, bStopped);

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFCustomTimelineVectorBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFCustomTimelineVectorBPEvent OnTick;

	UPROPERTY(BlueprintAssignable)
	FOnECFCustomTimelineVectorBPEvent OnFinished;

	UPROPERTY(BlueprintAssignable)
	FOnECFTimelineEventBP OnEvent;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings, PlayRate, PlayDirection", CPP_Default_PlayRate = "1.0", CPP_Default_PlayDirection = "Forward", ToolTip = "添加向量曲线时间轴。反向播放会从曲线末端开始。", DisplayName = "ECF - 自定义向量时间轴"), Category = "XTools|ECF|时间轴")
	static UECFCustomTimelineVectorBP* ECFCustomTimelineVector(const UObject* WorldContextObject, class UCurveVector* CurveVector, FECFActionSettings Settings, FECFHandleBP& Handle, float PlayRate, EECFPlayDirection PlayDirection, TArray<FECFTimelineEvent> Events);
};
