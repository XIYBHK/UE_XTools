// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "ECFActionBase.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "ECFCustomTimeline.generated.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFCustomTimeline : public UECFActionBase
{
	GENERATED_BODY()

	friend class UECFSubsystem;

protected:

	TUniqueFunction<void(float, float)> TickFunc;
	TUniqueFunction<void(float, float, bool)> CallbackFunc;
	TUniqueFunction<void(float, float)> CallbackFunc_NoStopped;
	TArray<FECFTimelineEvent> TimelineEvents;
	FECFTimelineEventFunc EventFunc;
	FTimeline MyTimeline;

	float CurrentValue = 0.f;
	float CurrentTime = 0.f;
	float PlayRate = 1.0f;
	EECFPlayDirection PlayDirection = EECFPlayDirection::Forward;


	UPROPERTY(Transient)
	UCurveFloat* CurveFloat = nullptr;

	bool Setup(UCurveFloat* InCurveFloat, TUniqueFunction<void(float, float)>&& InTickFunc, TUniqueFunction<void(float, float, bool)>&& InCallbackFunc, float InPlayRate, EECFPlayDirection InPlayDirection, TArray<FECFTimelineEvent>&& InEvents, FECFTimelineEventFunc&& InEventFunc)
	{
		TickFunc = MoveTemp(InTickFunc);
		CallbackFunc = MoveTemp(InCallbackFunc);
		TimelineEvents = MoveTemp(InEvents);
		EventFunc = MoveTemp(InEventFunc);
		Algo::StableSort(TimelineEvents, [](const FECFTimelineEvent& A, const FECFTimelineEvent& B) { return A.Time < B.Time; });
		CurveFloat = InCurveFloat;
		PlayRate = (FMath::Abs(InPlayRate) > KINDA_SMALL_NUMBER) ? FMath::Abs(InPlayRate) : 1.0f;
		PlayDirection = InPlayDirection;

		if (TickFunc && CurveFloat)
		{
			FOnTimelineFloat ProgressFunction;
			ProgressFunction.BindUFunction(this, FName("HandleProgress"));
			MyTimeline.AddInterpFloat(CurveFloat, ProgressFunction);

			FOnTimelineEvent FinishFunction;
			FinishFunction.BindUFunction(this, FName("HandleFinish"));
			MyTimeline.SetTimelineFinishedFunc(FinishFunction);
			MyTimeline.SetPlayRate(PlayRate);
			
			if (!Settings.bLoop)
			{
				SetMaxActionTime(MyTimeline.GetTimelineLength() / FMath::Max(FMath::Abs(PlayRate), KINDA_SMALL_NUMBER));
			}

			// 设置Loop模式，让FTimeline内部处理循环
			MyTimeline.SetLooping(Settings.bLoop);
			
			if (PlayDirection == EECFPlayDirection::Reverse)
			{
				MyTimeline.ReverseFromEnd();
			}
			else
			{
				MyTimeline.PlayFromStart();
			}

			return true;
		}
		else
		{
			ensureMsgf(false, TEXT("ECF - custom timeline failed to start. Are you sure Tick Function and Curve are set properly?"));
			return false;
		}
	}

	bool Setup(UCurveFloat* InCurveFloat, TUniqueFunction<void(float, float)>&& InTickFunc, TUniqueFunction<void(float, float)>&& InCallbackFunc, float InPlayRate, EECFPlayDirection InPlayDirection, TArray<FECFTimelineEvent>&& InEvents, FECFTimelineEventFunc&& InEventFunc)
	{
		CallbackFunc_NoStopped = MoveTemp(InCallbackFunc);
		return Setup(InCurveFloat, MoveTemp(InTickFunc), [this](float Value, float Time, bool bStopped)
		{
			if (CallbackFunc_NoStopped)
			{
				CallbackFunc_NoStopped(Value, Time);
			}
		}, InPlayRate, InPlayDirection, MoveTemp(InEvents), MoveTemp(InEventFunc));
	}

	bool Reset(bool bCallUpdate) override
	{
		CurrentTime = PlayDirection == EECFPlayDirection::Reverse ? MyTimeline.GetTimelineLength() : 0.f;
		MyTimeline.SetPlaybackPosition(CurrentTime, false, false);
		CurrentValue = CurveFloat->GetFloatValue(CurrentTime);
		if (bCallUpdate && HasValidOwner() && TickFunc)
		{
			TickFunc(CurrentValue, CurrentTime);
		}
		return true;
	}

	void Tick(float DeltaTime) override
	{
#if STATS
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("CustomTimeline - Tick"), STAT_ECFDETAILS_CUSTOMTIMELINE, STATGROUP_ECFDETAILS);
#endif
#if ECF_INSIGHT_PROFILING
		TRACE_CPUPROFILER_EVENT_SCOPE("ECF - CustomTimeline Tick");
#endif
		// 第一次Tick输出曲线起点值，与UE原生时间轴行为一致
		// 从第二次Tick开始正常调用TickTimeline累加时间
		if (bFirstTick && CurveFloat)
		{
			CurrentTime = PlayDirection == EECFPlayDirection::Reverse ? MyTimeline.GetTimelineLength() : 0.f;
			CurrentValue = CurveFloat->GetFloatValue(CurrentTime);
			if (HasValidOwner() && TickFunc)
			{
				TickFunc(CurrentValue, CurrentTime);
			}
			bFirstTick = false;
			return;  // 第一次Tick不调用TickTimeline，避免重复触发
		}
		
		const float PreviousTime = CurrentTime;
		const float TimelineLength = MyTimeline.GetTimelineLength();
		const float RawNewTime = PreviousTime + DeltaTime * PlayRate * (PlayDirection == EECFPlayDirection::Reverse ? -1.f : 1.f);
		MyTimeline.TickTimeline(DeltaTime);
		ECFDispatchTimelineEvents(PreviousTime, RawNewTime, TimelineLength, Settings.bLoop, PlayDirection == EECFPlayDirection::Reverse, TimelineEvents, EventFunc);
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
		const float TimelineLength = MyTimeline.GetTimelineLength();
		CurrentTime = FMath::Clamp(NewTime, 0.f, TimelineLength);
		MyTimeline.SetPlaybackPosition(CurrentTime, false, false);
		CurrentValue = CurveFloat->GetFloatValue(CurrentTime);
		if (bCallUpdate && HasValidOwner() && TickFunc)
		{
			TickFunc(CurrentValue, CurrentTime);
			const bool bReachedEnd = PlayDirection == EECFPlayDirection::Reverse ? CurrentTime <= 0.f : CurrentTime >= TimelineLength;
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
		if (PlayDirection == EECFPlayDirection::Reverse)
		{
			MyTimeline.Reverse();
		}
		else
		{
			MyTimeline.Play();
		}
		return true;
	}

	bool ReversePlayDirection() override
	{
		return SetPlayDirection(PlayDirection == EECFPlayDirection::Forward ? EECFPlayDirection::Reverse : EECFPlayDirection::Forward);
	}

private:

	UFUNCTION()
	void HandleProgress(float Value)
	{
		CurrentValue = Value;
		CurrentTime = MyTimeline.GetPlaybackPosition();
		if (HasValidOwner() && TickFunc)
		{
			TickFunc(CurrentValue, CurrentTime);
		}
	}

	UFUNCTION()
	void HandleFinish()
	{
		// Loop模式由FTimeline内部处理，不会触发HandleFinish
		// 这里只处理非Loop模式的结束
		
		// 确保最终值精确到达曲线终点，避免浮点精度问题
		if (CurveFloat)
		{
			CurrentTime = PlayDirection == EECFPlayDirection::Reverse ? 0.f : MyTimeline.GetTimelineLength();
			CurrentValue = CurveFloat->GetFloatValue(CurrentTime);
			if (HasValidOwner() && TickFunc)
			{
				TickFunc(CurrentValue, CurrentTime);
			}
		}
		MarkAsFinished();
		Complete(false);
	}
};

ECF_PRAGMA_ENABLE_OPTIMIZATION
