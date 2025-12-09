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


	UPROPERTY(Transient)
	UCurveVector* CurveVector = nullptr;

	bool Setup(UCurveVector* InCurveVector, TUniqueFunction<void(FVector, float)>&& InTickFunc, TUniqueFunction<void(FVector, float, bool)>&& InCallbackFunc, float InPlayRate)
	{
		TickFunc = MoveTemp(InTickFunc);
		CallbackFunc = MoveTemp(InCallbackFunc);
		CurveVector = InCurveVector;
		PlayRate = (FMath::Abs(InPlayRate) > KINDA_SMALL_NUMBER) ? FMath::Abs(InPlayRate) : 1.0f;

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
			
			// PlayFromStart设置Position=0并开始播放，不会触发回调
			MyTimeline.PlayFromStart();

			return true;
		}
		else
		{
			ensureMsgf(false, TEXT("ECF - custom timeline vector failed to start. Are you sure Tick Function and Curve are set properly?"));
			return false;
		}
	}

	bool Setup(UCurveVector* InCurveVector, TUniqueFunction<void(FVector, float)>&& InTickFunc, TUniqueFunction<void(FVector, float)>&& InCallbackFunc, float InPlayRate)
	{
		CallbackFunc_NoStopped = MoveTemp(InCallbackFunc);
		return Setup(InCurveVector, MoveTemp(InTickFunc), [this](FVector Value, float Time, bool bStopped)
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
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("CustomTimelineVector - Tick"), STAT_ECFDETAILS_CUSTOMTIMELINEVECTOR, STATGROUP_ECFDETAILS);
#endif
		// 第一次Tick输出曲线起点值，与UE原生时间轴行为一致
		// 从第二次Tick开始正常调用TickTimeline累加时间
		if (bFirstTick && CurveVector)
		{
			float MinTime, MaxTime;
			CurveVector->GetTimeRange(MinTime, MaxTime);
			CurrentValue = CurveVector->GetVectorValue(MinTime);
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
			float MinTime, MaxTime;
			CurveVector->GetTimeRange(MinTime, MaxTime);
			CurrentTime = MaxTime;
			CurrentValue = CurveVector->GetVectorValue(MaxTime);
			if (HasValidOwner() && TickFunc)
			{
				TickFunc(CurrentValue, CurrentTime);
			}
		}
		if (HasValidOwner())
		{
			Complete(false);
		}
		MarkAsFinished();
	}
};

ECF_PRAGMA_ENABLE_OPTIMIZATION
