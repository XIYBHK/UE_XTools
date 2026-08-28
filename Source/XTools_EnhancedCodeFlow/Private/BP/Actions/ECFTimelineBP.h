// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFTypes.h"
#include "ECFTimelineBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnECFTimelineBPEvent, float, Value, float, Time, bool, bStopped);

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFTimelineBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFTimelineBPEvent OnTick;

	UPROPERTY(BlueprintAssignable)
	FOnECFTimelineBPEvent OnFinished;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings, BlendFunc, BlendExp, PlayRate, PlayDirection", ToolTip = "时间轴动画控制器。反向播放会从结束值开始并在起始值完成。", DisplayName = "ECF - 时间轴"), Category = "XTools|ECF|时间轴")
	static UECFTimelineBP* ECFTimeline(const UObject* WorldContextObject, float StartValue, float StopValue, float Time, FECFActionSettings Settings, FECFHandleBP& Handle, EECFBlendFunc BlendFunc = EECFBlendFunc::ECFBlend_Linear, float BlendExp = 1.f, float PlayRate = 1.f, EECFPlayDirection PlayDirection = EECFPlayDirection::Forward);
};
