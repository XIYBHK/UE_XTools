// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFTypes.h"
#include "ECFCustomTimelineBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnECFCustomTimelineBPEvent, float, Value, float, Time, bool, bStopped);

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFCustomTimelineBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFCustomTimelineBPEvent OnTick;

	UPROPERTY(BlueprintAssignable)
	FOnECFCustomTimelineBPEvent OnFinished;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings, PlayRate, PlayDirection", ToolTip = "添加浮点曲线时间轴。反向播放会从曲线末端开始。", DisplayName = "ECF - 自定义时间轴"), Category = "XTools|ECF|时间轴")
	static UECFCustomTimelineBP* ECFCustomTimeline(const UObject* WorldContextObject, class UCurveFloat* CurveFloat, FECFActionSettings Settings, FECFHandleBP& Handle, float PlayRate = 1.f, EECFPlayDirection PlayDirection = EECFPlayDirection::Forward);
};
