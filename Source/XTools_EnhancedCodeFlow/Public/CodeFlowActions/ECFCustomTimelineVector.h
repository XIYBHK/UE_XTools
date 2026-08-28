// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "ECFActionBase.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveVector.h"
#include "ECFCustomTimelineVector.generated.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFCustomTimelineVector : public UECFActionBase
{
	GENERATED_BODY()

	friend class UECFSubsystem;

protected:

	TUniqueFunction<void(FVector, float)> TickFunc;
	TUniqueFunction<void(FVector, float, bool)> CallbackFunc;
	TUniqueFunction<void(FVector, float)> CallbackFunc_NoStopped;
	FTimeline MyTimeline;

	FVector CurrentValue = FVector::ZeroVector;
	float CurrentTime = 0.f;
	float PlayRate = 1.0f;
	EECFPlayDirection PlayDirection = EECFPlayDirection::Forward;


	UPROPERTY(Transient)
	UCurveVector* CurveVector = nullptr;

	bool Setup(UCurveVector* InCurveVector, TUniqueFunction<void(FVector, float)>&& InTickFunc, TUniqueFunction<void(FVector, float, bool)>&& InCallbackFunc, float InPlayRate, EECFPlayDirection InPlayDirection)
	{
		TickFunc = MoveTemp(InTickFunc);
		CallbackFunc = MoveTemp(InCallbackFunc);
		CurveVector = InCurveVector;
		PlayRate = (FMath::Abs(InPlayRate) > KINDA_SMALL_NUMBER) ? FMath::Abs(InPlayRate) : 1.0f;
		PlayDirection = InPlayDirection;

		if (TickFunc && CurveVector)
		{
			FOnTimelineVector ProgressFunction;
			ProgressFunction.BindUFunction(this, FName("HandleProgress"));
			MyTimeline.AddInterpVector(CurveVector, ProgressFunction);

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
			ensureMsgf(false, TEXT("ECF - custom timeline vector failed to start. Are you sure Tick Function and Curve are set properly?"));
			return false;
		}
	}

	bool Setup(UCurveVector* InCurveVector, TUniqueFunction<void(FVector, float)>&& InTickFunc, TUniqueFunction<void(FVector, float)>&& InCallbackFunc, float InPlayRate, EECFPlayDirection InPlayDirection)
	{
		CallbackFunc_NoStopped = MoveTemp(InCallbackFunc);
		return Setup(InCurveVector, MoveTemp(InTickFunc), [this](FVector Value, float Time, bool bStopped)
		{
			if (CallbackFunc_NoStopped)
			{
				CallbackFunc_NoStopped(Value, Time);
			}
		}, InPlayRate, InPlayDirection);
	}

	bool Reset(bool bCallUpdate) override
	{
		float MinTime = 0.f;
		float MaxTime = MyTimeline.GetTimelineLength();
		CurveVector->GetTimeRange(MinTime, MaxTime);
		CurrentTime = PlayDirection == EECFPlayDirection::Reverse ? MaxTime : 0.f;
		MyTimeline.SetPlaybackPosition(CurrentTime, false, false);
		CurrentValue = CurveVector->GetVectorValue(CurrentTime);
		if (bCallUpdate && HasValidOwner() && TickFunc)
		{
			TickFunc(CurrentValue, CurrentTime);
		}
		return true;
	}

	void Tick(float DeltaTime) override
	{
#if STATS
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("CustomTimelineVector - Tick"), STAT_ECFDETAILS_CUSTOMTIMELINEVECTOR, STATGROUP_ECFDETAILS);
#endif
#if ECF_INSIGHT_PROFILING
		TRACE_CPUPROFILER_EVENT_SCOPE("ECF - CustomTimelineVector Tick");
#endif
		// 第一次Tick输出曲线起点值，与UE原生时间轴行为一致
		// 从第二次Tick开始正常调用TickTimeline累加时间
		if (bFirstTick && CurveVector)
		{
			float MinTime = 0.f;
			float MaxTime = MyTimeline.GetTimelineLength();
			CurveVector->GetTimeRange(MinTime, MaxTime);
			CurrentTime = PlayDirection == EECFPlayDirection::Reverse ? MaxTime : MinTime;
			CurrentValue = CurveVector->GetVectorValue(CurrentTime);
			if (HasValidOwner() && TickFunc)
			{
				TickFunc(CurrentValue, CurrentTime);
			}
			bFirstTick = false;
			return;  // 第一次Tick不调用TickTimeline，避免重复触发
		}
		
		MyTimeline.TickTimeline(DeltaTime);
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
		CurrentValue = CurveVector->GetVectorValue(CurrentTime);
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
	void HandleProgress(FVector Value)
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
		if (CurveVector)
		{
			float MinTime = 0.f;
			float MaxTime = MyTimeline.GetTimelineLength();
			CurveVector->GetTimeRange(MinTime, MaxTime);
			CurrentTime = PlayDirection == EECFPlayDirection::Reverse ? MinTime : MaxTime;
			CurrentValue = CurveVector->GetVectorValue(CurrentTime);
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
