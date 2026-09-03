// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFTypes.h"
#include "ECFTimelineVectorBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnECFTimelineVectorBPEvent, FVector, Value, float, Time, bool, bStopped, FName, EventName);

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
	FOnECFTimelineVectorBPEvent OnEvent;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings, BlendFunc, BlendExp, PlayRate, PlayDirection, Events", AutoCreateRefTerm = "Events", CPP_Default_BlendFunc = "ECFBlend_Linear", CPP_Default_BlendExp = "1.0", CPP_Default_PlayRate = "1.0", CPP_Default_PlayDirection = "Forward", ToolTip = "添加向量时间轴。反向播放会从结束值开始并在起始值完成。", DisplayName = "ECF - 向量时间轴"), Category = "XTools|ECF|时间轴")
	static UECFTimelineVectorBP* ECFTimelineVector(const UObject* WorldContextObject, FVector StartValue, FVector StopValue, float Time, FECFActionSettings Settings, FECFHandleBP& Handle, EECFBlendFunc BlendFunc, float BlendExp, float PlayRate, EECFPlayDirection PlayDirection, const TArray<FECFTimelineEvent>& Events);
};
