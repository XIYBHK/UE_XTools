// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFCustomTimelineBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnECFCustomTimelineBPEvent, float, Value, float, Time, bool, bStopped);

UCLASS()
class ENHANCEDCODEFLOW_API UECFCustomTimelineBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFCustomTimelineBPEvent OnTick;

	UPROPERTY(BlueprintAssignable)
	FOnECFCustomTimelineBPEvent OnFinished;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings", ToolTip = "添加一个由浮点曲线定义的自定义时间轴", DisplayName = "ECF - 自定义时间轴"), Category = "XTools|ECF|时间轴")
	static UECFCustomTimelineBP* ECFCustomTimeline(const UObject* WorldContextObject, class UCurveFloat* CurveFloat, FECFActionSettings Settings, FECFHandleBP& Handle);
};
