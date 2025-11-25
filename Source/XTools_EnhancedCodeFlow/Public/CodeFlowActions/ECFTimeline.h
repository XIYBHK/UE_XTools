// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "ECFActionBase.h"
#include "ECFTypes.h"
#include "ECFTimeline.generated.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFTimeline : public UECFActionBase
{
	GENERATED_BODY()

	friend class UECFSubsystem;

protected:

	TUniqueFunction<void(float, float)> TickFunc;
	TUniqueFunction<void(float, float, bool)> CallbackFunc;
	TUniqueFunction<void(float, float)> CallbackFunc_NoStopped;
	float StartValue;
	float StopValue;
	float Time;
	EECFBlendFunc BlendFunc;
	float BlendExp;
	float PlayRate = 1.0f;

	float CurrentTime;
	float CurrentValue;

	bool Setup(float InStartValue, float InStopValue, float InTime, TUniqueFunction<void(float, float)>&& InTickFunc, TUniqueFunction<void(float, float, bool)>&& InCallbackFunc, EECFBlendFunc InBlendFunc, float InBlendExp, float InPlayRate)
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
			SetMaxActionTime(Time / PlayRate);
			CurrentTime = 0.f;
			return true;
		}
		else
		{
			ensureMsgf(false, TEXT("ECF - Timeline failed to start. Are you sure the Ticking time is greater than 0 and Ticking Function are set properly? /n Remember, that BlendExp must be different than zero and StartValue and StopValue must not be the same!"));
			return false;
		}
	}

	bool Setup(float InStartValue, float InStopValue, float InTime, TUniqueFunction<void(float, float)>&& InTickFunc, TUniqueFunction<void(float, float)>&& InCallbackFunc, EECFBlendFunc InBlendFunc, float InBlendExp, float InPlayRate)
	{
		CallbackFunc_NoStopped = MoveTemp(InCallbackFunc);
		return Setup(InStartValue, InStopValue, InTime, MoveTemp(InTickFunc), [this](float FwdValue, float FwdTime, bool bStopped)
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
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Timeline - Tick"), STAT_ECFDETAILS_TIMELINE, STATGROUP_ECFDETAILS);
#endif
		// 【修复】第一次Tick时触发初始值，与UE原生时间轴行为一致
		// 这样FirstDelay会自然生效（延迟后才会进入第一次Tick）
		if (bFirstTick)
		{
			CurrentValue = StartValue;
			if (HasValidOwner() && TickFunc)
			{
				TickFunc(CurrentValue, CurrentTime);
			}
			bFirstTick = false;
		}
		
		CurrentTime = FMath::Clamp(CurrentTime + DeltaTime * PlayRate, 0.f, Time);

		switch (BlendFunc)
		{
		case EECFBlendFunc::ECFBlend_Linear:
			CurrentValue = FMath::Lerp(StartValue, StopValue, CurrentTime / Time);
			break;
		case EECFBlendFunc::ECFBlend_Cubic:
			CurrentValue = FMath::CubicInterp(StartValue, 0.f, StopValue, 0.f, CurrentTime / Time);
			break;
		case EECFBlendFunc::ECFBlend_EaseIn:
			CurrentValue = FMath::Lerp(StartValue, StopValue, FMath::Pow(CurrentTime / Time, BlendExp));
			break;
		case EECFBlendFunc::ECFBlend_EaseOut:
			CurrentValue = FMath::Lerp(StartValue, StopValue, FMath::Pow(CurrentTime / Time, 1.f / BlendExp));
			break;
		case EECFBlendFunc::ECFBlend_EaseInOut:
			CurrentValue = FMath::InterpEaseInOut(StartValue, StopValue, CurrentTime / Time, BlendExp);
			break;
		}

		TickFunc(CurrentValue, CurrentTime);

		if ((StopValue > StartValue && CurrentValue >= StopValue) || (StopValue < StartValue && CurrentValue <= StopValue))
		{
			Complete(false);
			MarkAsFinished();
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
