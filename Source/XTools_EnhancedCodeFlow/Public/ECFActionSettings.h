// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ECFActionSettings.generated.h"

USTRUCT(BlueprintType)
struct XTOOLS_ENHANCEDCODEFLOW_API FECFActionSettings
{
	GENERATED_BODY()

	FECFActionSettings() :
		TickInterval(0.f),
		FirstDelay(0.f),
		bIgnorePause(false),
		bIgnoreGlobalTimeDilation(false),
		bStartPaused(false),
		bLoop(false)
	{

	}

	FECFActionSettings(float InTickInterval, float InFirstDelay = 0.f, bool InIgnorePause = false, bool InIgnoreTimeDilation = false, bool InStartPaused = false, bool InLoop = false) :
		TickInterval(InTickInterval),
		FirstDelay(InFirstDelay),
		bIgnorePause(InIgnorePause),
		bIgnoreGlobalTimeDilation(InIgnoreTimeDilation),
		bStartPaused(InStartPaused),
		bLoop(InLoop)
	{

	}

	UPROPERTY(BlueprintReadWrite, Category = "XTools|ECF|设置", meta = (DisplayName = "刷新间隔", Tooltip = "动作执行的时间间隔（秒）"))
	float TickInterval = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "XTools|ECF|设置", meta = (DisplayName = "初始延迟", Tooltip = "动作首次执行前的延迟时间（秒）"))
	float FirstDelay = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "XTools|ECF|设置", meta = (DisplayName = "忽略暂停", Tooltip = "是否忽略系统暂停状态"))
	bool bIgnorePause = false;

	UPROPERTY(BlueprintReadWrite, Category = "XTools|ECF|设置", meta = (DisplayName = "忽略全局时间膨胀", Tooltip = "是否忽略全局时间膨胀设置"))
	bool bIgnoreGlobalTimeDilation = false;

	UPROPERTY(BlueprintReadWrite, Category = "XTools|ECF|设置", meta = (DisplayName = "开始时暂停", Tooltip = "是否在创建时就处于暂停状态"))
	bool bStartPaused = false;

	UPROPERTY(BlueprintReadWrite, Category = "XTools|ECF|设置", meta = (DisplayName = "循环", Tooltip = "时间轴到达终点后是否从头开始（无限循环）"))
	bool bLoop = false;
};

#define ECF_TICKINTERVAL(_Interval) FECFActionSettings(_Interval, 0.f, false, false, false)
#define ECF_DELAYFIRST(_Delay) FECFActionSettings(0.f, _Delay, false, false, false)
#define ECF_IGNOREPAUSE FECFActionSettings(0.f, 0.f, true, false, false)
#define ECF_IGNORETIMEDILATION FECFActionSettings(0.f, 0.f, false, true, false)
#define ECF_IGNOREPAUSEDILATION FECFActionSettings(0.f, 0.f, true, true, false)
#define ECF_STARTPAUSED FECFActionSettings(0.f, 0.f, false, false, true)
