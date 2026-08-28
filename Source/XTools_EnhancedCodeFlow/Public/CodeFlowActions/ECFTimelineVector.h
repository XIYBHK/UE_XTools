// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "ECFActionBase.h"
#include "ECFTypes.h"
#include "ECFTimelineVector.generated.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFTimelineVector : public UECFActionBase
{
	GENERATED_BODY()

	friend class UECFSubsystem;

protected:

	TUniqueFunction<void(FVector, float)> TickFunc;
	TUniqueFunction<void(FVector, float, bool)> CallbackFunc;
	TUniqueFunction<void(FVector, float)> CallbackFunc_NoStopped;
	FVector StartValue;
	FVector StopValue;
	float Time;
	EECFBlendFunc BlendFunc;
	float BlendExp;
	float PlayRate = 1.0f;
	EECFPlayDirection PlayDirection = EECFPlayDirection::Forward;

	float CurrentTime;
	FVector CurrentValue;

	bool Setup(FVector InStartValue, FVector InStopValue, float InTime, TUniqueFunction<void(FVector, float)>&& InTickFunc, TUniqueFunction<void(FVector, float, bool)>&& InCallbackFunc, EECFBlendFunc InBlendFunc, float InBlendExp, float InPlayRate, EECFPlayDirection InPlayDirection)
	{
		StartValue = InStartValue;
		StopValue = InStopValue;
		Time = InTime;
		const float EffectiveRate = FMath::Abs(InPlayRate);
		PlayRate = (EffectiveRate > KINDA_SMALL_NUMBER) ? EffectiveRate : 1.0f;
		PlayDirection = InPlayDirection;

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
			CurrentTime = PlayDirection == EECFPlayDirection::Reverse ? Time : 0.f;
			CurrentValue = PlayDirection == EECFPlayDirection::Reverse ? StopValue : StartValue;
			return true;
		}
		else
		{
			ensureMsgf(false, TEXT("ECF - Timeline Vector failed to start. Are you sure the Ticking time is greater than 0 and Ticking Function are set properly? /n Remember, that BlendExp must be different than zero and StartValue and StopValue must not be the same!"));
			return false;
		}
	}

	bool Setup(FVector InStartValue, FVector InStopValue, float InTime, TUniqueFunction<void(FVector, float)>&& InTickFunc, TUniqueFunction<void(FVector, float)>&& InCallbackFunc, EECFBlendFunc InBlendFunc, float InBlendExp, float InPlayRate, EECFPlayDirection InPlayDirection)
	{
		CallbackFunc_NoStopped = MoveTemp(InCallbackFunc);
		return Setup(InStartValue, InStopValue, InTime, MoveTemp(InTickFunc), [this](FVector FwdValue, float FwdTime, bool bStopped)
		{
			if (CallbackFunc_NoStopped)
			{
				CallbackFunc_NoStopped(FwdValue, FwdTime);
			}
		}, InBlendFunc, InBlendExp, InPlayRate, InPlayDirection);
	}

	bool Reset(bool bCallUpdate) override
	{
		CurrentTime = PlayDirection == EECFPlayDirection::Reverse ? Time : 0.f;
		CurrentValue = PlayDirection == EECFPlayDirection::Reverse ? StopValue : StartValue;
		if (bCallUpdate && HasValidOwner() && TickFunc)
		{
			TickFunc(CurrentValue, CurrentTime);
		}
		return true;
	}

	void Tick(float DeltaTime) override
	{
#if STATS
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Timeline Vector - Tick"), STAT_ECFDETAILS_TIMELINEVECTOR, STATGROUP_ECFDETAILS);
#endif
#if ECF_INSIGHT_PROFILING
		TRACE_CPUPROFILER_EVENT_SCOPE("ECF - Timeline Vector Tick");
#endif
		// 第一次 Tick 直接输出起点值，与 UE 时间轴首帧行为对齐。
		if (bFirstTick)
		{
			bFirstTick = false;
		}
		else
		{
			const float DirectionMultiplier = PlayDirection == EECFPlayDirection::Reverse ? -1.f : 1.f;
			const float UnclampedTime = CurrentTime + DeltaTime * PlayRate * DirectionMultiplier;
			const bool bCrossedBoundary = PlayDirection == EECFPlayDirection::Reverse ? UnclampedTime < 0.f : UnclampedTime > Time;
			if (Settings.bLoop && bCrossedBoundary)
			{
				CurrentTime = PlayDirection == EECFPlayDirection::Reverse ? 0.f : Time;
				CurrentValue = PlayDirection == EECFPlayDirection::Reverse ? StartValue : StopValue;
				TickFunc(CurrentValue, CurrentTime);

				const float WrappedTime = FMath::Fmod(UnclampedTime, Time);
				CurrentTime = WrappedTime < 0.f ? WrappedTime + Time : WrappedTime;
			}
			else
			{
				CurrentTime = FMath::Clamp(UnclampedTime, 0.f, Time);
			}
		}

		const float Alpha = CurrentTime / Time;

		switch (BlendFunc)
		{
		case EECFBlendFunc::ECFBlend_Linear:
			CurrentValue = FMath::Lerp(StartValue, StopValue, Alpha);
			break;
		case EECFBlendFunc::ECFBlend_Cubic:
			CurrentValue = FMath::CubicInterp(StartValue, FVector::ZeroVector, StopValue, FVector::ZeroVector, Alpha);
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

		const bool bReachedEnd = PlayDirection == EECFPlayDirection::Reverse ? CurrentTime <= 0.f : CurrentTime >= Time;
		if (bReachedEnd)
		{
			if (Settings.bLoop)
			{
				TickFunc(CurrentValue, CurrentTime);
				return;
			}
			else
			{
				// 确保最终值精确到达StopValue，避免浮点精度问题
				CurrentValue = PlayDirection == EECFPlayDirection::Reverse ? StartValue : StopValue;
				CurrentTime = PlayDirection == EECFPlayDirection::Reverse ? 0.f : Time;
				TickFunc(CurrentValue, CurrentTime);
				MarkAsFinished();
				Complete(false);
				return;
			}
		}

		TickFunc(CurrentValue, CurrentTime);
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

	float GetActionTime() const override
	{
		return CurrentTime;
	}

	bool SetActionTime(float NewTime, bool bCallUpdate) override
	{
		CurrentTime = FMath::Clamp(NewTime, 0.f, Time);
		const float Alpha = CurrentTime / Time;
		switch (BlendFunc)
		{
		case EECFBlendFunc::ECFBlend_Linear: CurrentValue = FMath::Lerp(StartValue, StopValue, Alpha); break;
		case EECFBlendFunc::ECFBlend_Cubic: CurrentValue = FMath::CubicInterp(StartValue, FVector::ZeroVector, StopValue, FVector::ZeroVector, Alpha); break;
		case EECFBlendFunc::ECFBlend_EaseIn: CurrentValue = FMath::Lerp(StartValue, StopValue, FMath::Pow(Alpha, BlendExp)); break;
		case EECFBlendFunc::ECFBlend_EaseOut: CurrentValue = FMath::Lerp(StartValue, StopValue, FMath::Pow(Alpha, 1.f / BlendExp)); break;
		case EECFBlendFunc::ECFBlend_EaseInOut: CurrentValue = FMath::InterpEaseInOut(StartValue, StopValue, Alpha, BlendExp); break;
		}

		if (bCallUpdate && HasValidOwner() && TickFunc)
		{
			TickFunc(CurrentValue, CurrentTime);
			const bool bReachedEnd = PlayDirection == EECFPlayDirection::Reverse ? CurrentTime <= 0.f : CurrentTime >= Time;
			if (!Settings.bLoop && bReachedEnd)
			{
				MarkAsFinished();
				Complete(false);
			}
		}
		return true;
	}

	bool SetPlayDirection(EECFPlayDirection NewDirection) override
	{
		PlayDirection = NewDirection;
		return true;
	}

	bool ReversePlayDirection() override
	{
		PlayDirection = PlayDirection == EECFPlayDirection::Forward ? EECFPlayDirection::Reverse : EECFPlayDirection::Forward;
		return true;
	}
};

ECF_PRAGMA_ENABLE_OPTIMIZATION
