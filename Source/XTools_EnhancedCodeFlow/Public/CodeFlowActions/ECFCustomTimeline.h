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
	FTimeline MyTimeline;

	float CurrentValue = 0.f;
	float CurrentTime = 0.f;
	float PlayRate = 1.0f;


	UPROPERTY(Transient)
	UCurveFloat* CurveFloat = nullptr;

	bool Setup(UCurveFloat* InCurveFloat, TUniqueFunction<void(float, float)>&& InTickFunc, TUniqueFunction<void(float, float, bool)>&& InCallbackFunc, float InPlayRate)
	{
		TickFunc = MoveTemp(InTickFunc);
		CallbackFunc = MoveTemp(InCallbackFunc);
		CurveFloat = InCurveFloat;
		PlayRate = (FMath::Abs(InPlayRate) > KINDA_SMALL_NUMBER) ? FMath::Abs(InPlayRate) : 1.0f;

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
			
			// PlayFromStart设置Position=0并开始播放，不会触发回调
			MyTimeline.PlayFromStart();

			return true;
		}
		else
		{
			ensureMsgf(false, TEXT("ECF - custom timeline failed to start. Are you sure Tick Function and Curve are set properly?"));
			return false;
		}
	}

	bool Setup(UCurveFloat* InCurveFloat, TUniqueFunction<void(float, float)>&& InTickFunc, TUniqueFunction<void(float, float)>&& InCallbackFunc, float InPlayRate)
	{
		CallbackFunc_NoStopped = MoveTemp(InCallbackFunc);
		return Setup(InCurveFloat, MoveTemp(InTickFunc), [this](float Value, float Time, bool bStopped)
		{
			if (CallbackFunc_NoStopped)
			{
				CallbackFunc_NoStopped(Value, Time);
			}
		}, InPlayRate);
	}

	bool Reset(bool bCallUpdate) override
	{
		MyTimeline.SetPlaybackPosition(0.f, false, false);
		CurrentTime = 0.f;
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
			float MinTime, MaxTime;
			CurveFloat->GetTimeRange(MinTime, MaxTime);
			CurrentValue = CurveFloat->GetFloatValue(MinTime);
			CurrentTime = MinTime;
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
		CurrentValue = CurveFloat->GetFloatValue(CurrentTime);
		if (bCallUpdate && HasValidOwner() && TickFunc)
		{
			TickFunc(CurrentValue, CurrentTime);
			if (!Settings.bLoop && CurrentTime >= TimelineLength)
			{
				MarkAsFinished();
				Complete(false);
			}
		}
		return true;
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
			float MinTime, MaxTime;
			CurveFloat->GetTimeRange(MinTime, MaxTime);
			CurrentTime = MaxTime;
			CurrentValue = CurveFloat->GetFloatValue(MaxTime);
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
