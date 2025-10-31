// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFTypes.h"
#include "ECFTimelineVectorBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnECFTimelineVectorBPEvent, FVector, Value, float, Time, bool, bStopped);

UCLASS()
class ENHANCEDCODEFLOW_API UECFTimelineVectorBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFTimelineVectorBPEvent OnTick;

	UPROPERTY(BlueprintAssignable)
	FOnECFTimelineVectorBPEvent OnFinished;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings, BlendFunc, BlendExp, PlayRate", ToolTip = "添加一个在指定时间内在给定范围内运行的向量时间轴。\nPlayRate 可用于整体加速或减缓时间轴播放。", DisplayName = "ECF - 向量时间轴"), Category = "XTools|ECF|时间轴")
	static UECFTimelineVectorBP* ECFTimelineVector(const UObject* WorldContextObject, FVector StartValue, FVector StopValue, float Time, FECFActionSettings Settings, FECFHandleBP& Handle, EECFBlendFunc BlendFunc = EECFBlendFunc::ECFBlend_Linear, float BlendExp = 1.f, float PlayRate = 1.f);
};
