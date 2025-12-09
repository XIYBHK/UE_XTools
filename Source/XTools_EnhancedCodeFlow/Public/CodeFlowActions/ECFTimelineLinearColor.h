// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "ECFActionBase.h"
#include "ECFTypes.h"
#include "ECFTimelineLinearColor.generated.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFTimelineLinearColor: public UECFActionBase
{
	GENERATED_BODY()

	friend class UECFSubsystem;

protected:

	TUniqueFunction<void(FLinearColor, float)> TickFunc;
	TUniqueFunction<void(FLinearColor, float, bool)> CallbackFunc;
	TUniqueFunction<void(FLinearColor, float)> CallbackFunc_NoStopped;
	FLinearColor StartValue;
	FLinearColor StopValue;
	float Time;
	EECFBlendFunc BlendFunc;
	float BlendExp;
	float PlayRate = 1.0f;

	float CurrentTime;
	FLinearColor CurrentValue;

	bool Setup(FLinearColor InStartValue, FLinearColor InStopValue, float InTime, TUniqueFunction<void(FLinearColor, float)>&& InTickFunc, TUniqueFunction<void(FLinearColor, float, bool)>&& InCallbackFunc, EECFBlendFunc InBlendFunc, float InBlendExp, float InPlayRate)
	{
		StartValue = InStartValue;
		StopValue = InStopValue;
		Time = InTime;
		const float EffectiveRate = FMath::Abs(InPlayRate);
		PlayRate = (EffectiveRate > KINDA_SMALL_NUMBER) ? EffectiveRate : 1.0f;

		TickFunc = MoveTemp(InTickFunc);
		CallbackFunc = MoveTemp(InCallbackFunc);

		BlendFunc = InBlendFunc;
		BlendExp = InBlendExp;

		if (TickFunc && Time > 0 && BlendExp != 0 && StartValue != StopValue)
		{
			if (!Settings.bLoop)
			{
				SetMaxActionTime(Time / PlayRate);
			}
			CurrentTime = 0.f;
			return true;
		}
		else
		{
			ensureMsgf(false, TEXT("ECF - Timeline Linear Color failed to start. Are you sure the Ticking time is greater than 0 and Ticking Function are set properly? /n Remember, that BlendExp must be different than zero and StartValue and StopValue must not be the same!"));
			return false;
		}
	}

	bool Setup(FLinearColor InStartValue, FLinearColor InStopValue, float InTime, TUniqueFunction<void(FLinearColor, float)>&& InTickFunc, TUniqueFunction<void(FLinearColor, float)>&& InCallbackFunc, EECFBlendFunc InBlendFunc, float InBlendExp, float InPlayRate)
	{
		CallbackFunc_NoStopped = MoveTemp(InCallbackFunc);
		return Setup(InStartValue, InStopValue, InTime, MoveTemp(InTickFunc), [this](FLinearColor FwdValue, float FwdTime, bool bStopped)
		{
			if (CallbackFunc_NoStopped)
			{
				CallbackFunc_NoStopped(FwdValue, FwdTime);
			}
		}, InBlendFunc, InBlendExp, InPlayRate);
	}

	void Tick(float DeltaTime) override
	{
#if STATS
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Timeline Linear Color - Tick"), STAT_ECFDETAILS_TIMELINELINEARCOLOR, STATGROUP_ECFDETAILS);
#endif
		// 第一次Tick不累加时间，直接输出StartValue，与UE原生时间轴行为一致
		if (!bFirstTick)
		{
			CurrentTime = FMath::Clamp(CurrentTime + DeltaTime * PlayRate, 0.f, Time);
		}
		bFirstTick = false;

		const float Alpha = CurrentTime / Time;

		switch (BlendFunc)
		{
		case EECFBlendFunc::ECFBlend_Linear:
			CurrentValue = FMath::Lerp(StartValue, StopValue, Alpha);
			break;
		case EECFBlendFunc::ECFBlend_Cubic:
			CurrentValue = FMath::CubicInterp(StartValue, FLinearColor::Black, StopValue, FLinearColor::Black, Alpha);
			break;
		case EECFBlendFunc::ECFBlend_EaseIn:
			CurrentValue = FMath::Lerp(StartValue, StopValue, FMath::Pow(Alpha, BlendExp));
			break;
		case EECFBlendFunc::ECFBlend_EaseOut:
			CurrentValue = FMath::Lerp(StartValue, StopValue, FMath::Pow(Alpha, 1.f / BlendExp));
			break;
		case EECFBlendFunc::ECFBlend_EaseInOut:
			CurrentValue = FMath::InterpEaseInOut(StartValue, StopValue, Alpha, BlendExp);
			break;
		}

		TickFunc(CurrentValue, CurrentTime);

		if (CurrentTime >= Time)
		{
			if (Settings.bLoop)
			{
				// 参考UE原生FTimeline::TickTimeline的Loop实现
				// 1. 先触发精确的终点值
				CurrentValue = StopValue;
				TickFunc(StopValue, Time);
				
				// 2. 处理溢出时间，计算新位置
				while (CurrentTime > Time)
				{
					CurrentTime -= Time;
				}
			}
			else
			{
				// 确保最终值精确到达StopValue，避免浮点精度问题
				CurrentValue = StopValue;
				CurrentTime = Time;
				TickFunc(CurrentValue, CurrentTime);
				Complete(false);
				MarkAsFinished();
			}
		}
	}

	void Complete(bool bStopped) override
	{
		// 【防御性编程】：确保 Owner 仍然有效
		if (HasValidOwner() && CallbackFunc)
		{
			CallbackFunc(CurrentValue, CurrentTime, bStopped);
		}
		// 注：Owner 已销毁时静默跳过回调，避免崩溃
	}
};

ECF_PRAGMA_ENABLE_OPTIMIZATION
