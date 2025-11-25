// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "ECFActionBase.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveLinearColor.h"
#include "ECFCustomTimelineLinearColor.generated.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFCustomTimelineLinearColor : public UECFActionBase
{
	GENERATED_BODY()

	friend class UECFSubsystem;

protected:

	TUniqueFunction<void(FLinearColor, float)> TickFunc;
	TUniqueFunction<void(FLinearColor, float, bool)> CallbackFunc;
	TUniqueFunction<void(FLinearColor, float)> CallbackFunc_NoStopped;
	FTimeline MyTimeline;

	FLinearColor CurrentValue = FLinearColor::Black;
	float CurrentTime = 0.f;
	float PlayRate = 1.0f;
	bool bSuppressCallback = false;  // 用于阻止Setup中PlayFromStart触发的过早回调

	UPROPERTY(Transient)
	UCurveLinearColor* CurveLinearColor = nullptr;

	bool Setup(UCurveLinearColor* InCurveLinearColor, TUniqueFunction<void(FLinearColor, float)>&& InTickFunc, TUniqueFunction<void(FLinearColor, float, bool)>&& InCallbackFunc, float InPlayRate)
	{
		TickFunc = MoveTemp(InTickFunc);
		CallbackFunc = MoveTemp(InCallbackFunc);
		CurveLinearColor = InCurveLinearColor;
		PlayRate = (FMath::Abs(InPlayRate) > KINDA_SMALL_NUMBER) ? FMath::Abs(InPlayRate) : 1.0f;

		if (TickFunc && CurveLinearColor)
		{
			FOnTimelineLinearColor ProgressFunction;
			ProgressFunction.BindUFunction(this, FName("HandleProgress"));
			MyTimeline.AddInterpLinearColor(CurveLinearColor, ProgressFunction);

			FOnTimelineEvent FinishFunction;
			FinishFunction.BindUFunction(this, FName("HandleFinish"));
			MyTimeline.SetTimelineFinishedFunc(FinishFunction);
			MyTimeline.SetPlayRate(PlayRate);
			SetMaxActionTime(MyTimeline.GetTimelineLength() / FMath::Max(FMath::Abs(PlayRate), KINDA_SMALL_NUMBER));

			// 阻止PlayFromStart触发的回调，因为此时时机太早
			// 初始值会在第一次Tick时手动触发
			bSuppressCallback = true;
			MyTimeline.PlayFromStart();
			bSuppressCallback = false;

			return true;
		}
		else
		{
			ensureMsgf(false, TEXT("ECF - custom timeline LinearColor failed to start. Are you sure Tick Function and Curve are set properly?"));
			return false;
		}
	}

	bool Setup(UCurveLinearColor* InCurveLinearColor, TUniqueFunction<void(FLinearColor, float)>&& InTickFunc, TUniqueFunction<void(FLinearColor, float)>&& InCallbackFunc, float InPlayRate)
	{
		CallbackFunc_NoStopped = MoveTemp(InCallbackFunc);
		return Setup(InCurveLinearColor, MoveTemp(InTickFunc), [this](FLinearColor Value, float Time, bool bStopped)
		{
			if (CallbackFunc_NoStopped)
			{
				CallbackFunc_NoStopped(Value, Time);
			}
		}, InPlayRate);
	}

	void Tick(float DeltaTime) override
	{
#if STATS
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("CustomTimelineLinearColor - Tick"), STAT_ECFDETAILS_CUSTOMTIMELINELINEARCOLOR, STATGROUP_ECFDETAILS);
#endif
		// 【修复】第一次Tick时触发初始值，与UE原生时间轴行为一致
		if (bFirstTick && CurveLinearColor)
		{
			float MinTime, MaxTime;
			CurveLinearColor->GetTimeRange(MinTime, MaxTime);
			const FLinearColor InitialValue = CurveLinearColor->GetLinearColorValue(MinTime);
			CurrentValue = InitialValue;
			CurrentTime = MinTime;
			if (HasValidOwner() && TickFunc)
			{
				TickFunc(CurrentValue, CurrentTime);
			}
			bFirstTick = false;
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

private:

	UFUNCTION()
	void HandleProgress(FLinearColor Value)
	{
		// 如果回调被阻止（Setup阶段），直接返回
		if (bSuppressCallback) return;
		
		CurrentValue = Value;
		CurrentTime = MyTimeline.GetPlaybackPosition();
		if (HasValidOwner())
		{
			TickFunc(CurrentValue, CurrentTime);
		}
	}

	UFUNCTION()
	void HandleFinish()
	{
		if (HasValidOwner())
		{
			Complete(false);
		}
		MarkAsFinished();
	}
};

ECF_PRAGMA_ENABLE_OPTIMIZATION
