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
			// 循环模式不设置MaxActionTime，由Tick自行管理
			if (!Settings.bLoop)
			{
				SetMaxActionTime(Time / PlayRate);
			}
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
		// 第一次 Tick 直接输出起点值，与 UE 时间轴首帧行为对齐。
		if (bFirstTick)
		{
			bFirstTick = false;
		}
		else
		{
			const float UnclampedTime = CurrentTime + DeltaTime * PlayRate;
			if (Settings.bLoop && UnclampedTime > Time)
			{
				// 对齐 UE 原生 FTimeline::TickTimeline：先精确走到终点，再回到起点并消化溢出时间。
				CurrentTime = Time;
				CurrentValue = StopValue;
				TickFunc(CurrentValue, CurrentTime);

				CurrentTime = 0.f;
				// 使用 FMath::Fmod 替代 while 循环消化溢出时间，避免大溢出时循环次数过多
				CurrentTime = (Time > 0.f) ? FMath::Fmod(UnclampedTime, Time) : 0.f;
			}
			else
			{
				CurrentTime = FMath::Clamp(UnclampedTime, 0.f, Time);
			}
		}

		// 计算插值比例
		const float Alpha = CurrentTime / Time;

		switch (BlendFunc)
		{
		case EECFBlendFunc::ECFBlend_Linear:
			CurrentValue = FMath::Lerp(StartValue, StopValue, Alpha);
			break;
		case EECFBlendFunc::ECFBlend_Cubic:
			CurrentValue = FMath::CubicInterp(StartValue, 0.f, StopValue, 0.f, Alpha);
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

		// 检查是否到达终点
		if (CurrentTime >= Time)
		{
			if (Settings.bLoop)
			{
				// 循环模式：正常调用 TickFunc（溢出已在上方通过 Fmod 处理）
				TickFunc(CurrentValue, CurrentTime);
				return;
			}
			else
			{
				// 非循环模式：确保最终值精确到达 StopValue，只调用一次 TickFunc 避免重复
				CurrentValue = StopValue;
				CurrentTime = Time;
				TickFunc(CurrentValue, CurrentTime);
				Complete(false);
				MarkAsFinished();
			}
		}
		else
		{
			// 未到达终点：正常输出当前插值
			TickFunc(CurrentValue, CurrentTime);
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
