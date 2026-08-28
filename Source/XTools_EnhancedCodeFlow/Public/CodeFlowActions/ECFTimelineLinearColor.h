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
	TArray<FECFTimelineEvent> TimelineEvents;
	FECFTimelineEventFunc EventFunc;
	FLinearColor StartValue;
	FLinearColor StopValue;
	float Time;
	EECFBlendFunc BlendFunc;
	float BlendExp;
	float PlayRate = 1.0f;
	EECFPlayDirection PlayDirection = EECFPlayDirection::Forward;

	float CurrentTime;
	FLinearColor CurrentValue;

	bool Setup(FLinearColor InStartValue, FLinearColor InStopValue, float InTime, TUniqueFunction<void(FLinearColor, float)>&& InTickFunc, TUniqueFunction<void(FLinearColor, float, bool)>&& InCallbackFunc, EECFBlendFunc InBlendFunc, float InBlendExp, float InPlayRate, EECFPlayDirection InPlayDirection, TArray<FECFTimelineEvent>&& InEvents, FECFTimelineEventFunc&& InEventFunc)
	{
		StartValue = InStartValue;
		StopValue = InStopValue;
		Time = InTime;
		const float EffectiveRate = FMath::Abs(InPlayRate);
		PlayRate = (EffectiveRate > KINDA_SMALL_NUMBER) ? EffectiveRate : 1.0f;
		PlayDirection = InPlayDirection;

		TickFunc = MoveTemp(InTickFunc);
		CallbackFunc = MoveTemp(InCallbackFunc);
		TimelineEvents = MoveTemp(InEvents);
		EventFunc = MoveTemp(InEventFunc);
		Algo::StableSort(TimelineEvents, [](const FECFTimelineEvent& A, const FECFTimelineEvent& B) { return A.Time < B.Time; });

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
			ensureMsgf(false, TEXT("ECF - Timeline Linear Color failed to start. Are you sure the Ticking time is greater than 0 and Ticking Function are set properly? /n Remember, that BlendExp must be different than zero and StartValue and StopValue must not be the same!"));
			return false;
		}
	}

	bool Setup(FLinearColor InStartValue, FLinearColor InStopValue, float InTime, TUniqueFunction<void(FLinearColor, float)>&& InTickFunc, TUniqueFunction<void(FLinearColor, float)>&& InCallbackFunc, EECFBlendFunc InBlendFunc, float InBlendExp, float InPlayRate, EECFPlayDirection InPlayDirection, TArray<FECFTimelineEvent>&& InEvents, FECFTimelineEventFunc&& InEventFunc)
	{
		CallbackFunc_NoStopped = MoveTemp(InCallbackFunc);
		return Setup(InStartValue, InStopValue, InTime, MoveTemp(InTickFunc), [this](FLinearColor FwdValue, float FwdTime, bool bStopped)
		{
			if (CallbackFunc_NoStopped)
			{
				CallbackFunc_NoStopped(FwdValue, FwdTime);
			}
		}, InBlendFunc, InBlendExp, InPlayRate, InPlayDirection, MoveTemp(InEvents), MoveTemp(InEventFunc));
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
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Timeline Linear Color - Tick"), STAT_ECFDETAILS_TIMELINELINEARCOLOR, STATGROUP_ECFDETAILS);
#endif
#if ECF_INSIGHT_PROFILING
		TRACE_CPUPROFILER_EVENT_SCOPE("ECF - Timeline Linear Color Tick");
#endif
		bool bDispatchEvents = false;
		float PreviousTime = CurrentTime;
		float UnclampedTime = CurrentTime;
		// 第一次 Tick 直接输出起点值，与 UE 时间轴首帧行为对齐。
		if (bFirstTick)
		{
			bFirstTick = false;
		}
		else
		{
			const float DirectionMultiplier = PlayDirection == EECFPlayDirection::Reverse ? -1.f : 1.f;
			UnclampedTime = CurrentTime + DeltaTime * PlayRate * DirectionMultiplier;
			PreviousTime = CurrentTime;
			bDispatchEvents = true;
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

		const bool bReachedEnd = PlayDirection == EECFPlayDirection::Reverse ? CurrentTime <= 0.f : CurrentTime >= Time;
		if (bReachedEnd)
		{
			if (Settings.bLoop)
			{
				TickFunc(CurrentValue, CurrentTime);
				if (bDispatchEvents)
				{
					ECFDispatchTimelineEvents(PreviousTime, UnclampedTime, Time, true, PlayDirection == EECFPlayDirection::Reverse, TimelineEvents, EventFunc);
				}
				return;
			}
			else
			{
				// 确保最终值精确到达StopValue，避免浮点精度问题
				CurrentValue = PlayDirection == EECFPlayDirection::Reverse ? StartValue : StopValue;
				CurrentTime = PlayDirection == EECFPlayDirection::Reverse ? 0.f : Time;
				TickFunc(CurrentValue, CurrentTime);
				if (bDispatchEvents)
				{
					ECFDispatchTimelineEvents(PreviousTime, UnclampedTime, Time, false, PlayDirection == EECFPlayDirection::Reverse, TimelineEvents, EventFunc);
				}
				MarkAsFinished();
				Complete(false);
				return;
			}
		}

		TickFunc(CurrentValue, CurrentTime);
		if (bDispatchEvents)
		{
			ECFDispatchTimelineEvents(PreviousTime, UnclampedTime, Time, Settings.bLoop, PlayDirection == EECFPlayDirection::Reverse, TimelineEvents, EventFunc);
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
		case EECFBlendFunc::ECFBlend_Cubic: CurrentValue = FMath::CubicInterp(StartValue, FLinearColor::Black, StopValue, FLinearColor::Black, Alpha); break;
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
