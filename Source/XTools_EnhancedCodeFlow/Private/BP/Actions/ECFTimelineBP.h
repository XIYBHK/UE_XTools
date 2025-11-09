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
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings, BlendFunc, BlendExp, PlayRate", ToolTip = "时间轴动画控制器\n\n参数说明：\n- StartValue/StopValue：起始和结束值\n- Time：持续时间（秒）\n- PlayRate：播放速率（>1加速，<1减速）\n- BlendFunc：插值方式（线性、缓入缓出等）\n- BlendExp：缓动曲线指数\n\n输出参数：\n- Handle：节点操作句柄，用于停止或检查状态\n- OnTick：每帧更新时触发，提供当前值和时间\n- OnFinished：时间线结束时触发\n- Value：当前插值计算的数值\n- Time：当前时间线已执行的时间\n- bStopped：时间线是否被外部停止\n\n常用于：UI动画、相机移动、颜色过渡、平滑数值变化", DisplayName = "ECF - 时间轴"), Category = "XTools|ECF|时间轴")
	static UECFTimelineBP* ECFTimeline(const UObject* WorldContextObject, float StartValue, float StopValue, float Time, FECFActionSettings Settings, FECFHandleBP& Handle, EECFBlendFunc BlendFunc = EECFBlendFunc::ECFBlend_Linear, float BlendExp = 1.f, float PlayRate = 1.f);
};
