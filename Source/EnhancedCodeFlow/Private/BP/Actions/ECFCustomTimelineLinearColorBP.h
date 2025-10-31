// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFCustomTimelineLinearColorBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnECFCustomTimelineLinearColorBPEvent, FLinearColor, Value, float, Time, bool, bStopped);

UCLASS()
class ENHANCEDCODEFLOW_API UECFCustomTimelineLinearColorBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFCustomTimelineLinearColorBPEvent OnTick;

	UPROPERTY(BlueprintAssignable)
	FOnECFCustomTimelineLinearColorBPEvent OnFinished;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings, PlayRate", ToolTip = "添加一个由线性色彩曲线驱动的自定义时间轴，支持 PlayRate 调节", DisplayName = "ECF - 自定义颜色时间轴"), Category = "XTools|ECF|时间轴")
	static UECFCustomTimelineLinearColorBP* ECFCustomTimelineLinearColor(const UObject* WorldContextObject, class UCurveLinearColor* CurveLinearColor, FECFActionSettings Settings, FECFHandleBP& Handle, float PlayRate = 1.f);
};
