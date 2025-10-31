// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFCustomTimelineVectorBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnECFCustomTimelineVectorBPEvent, FVector, Value, float, Time, bool, bStopped);

UCLASS()
class ENHANCEDCODEFLOW_API UECFCustomTimelineVectorBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFCustomTimelineVectorBPEvent OnTick;

	UPROPERTY(BlueprintAssignable)
	FOnECFCustomTimelineVectorBPEvent OnFinished;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings, PlayRate", ToolTip = "添加一个由向量曲线驱动的自定义时间轴，可调节 PlayRate", DisplayName = "ECF - 自定义向量时间轴"), Category = "XTools|ECF|时间轴")
	static UECFCustomTimelineVectorBP* ECFCustomTimelineVector(const UObject* WorldContextObject, class UCurveVector* CurveVector, FECFActionSettings Settings, FECFHandleBP& Handle, float PlayRate = 1.f);
};
