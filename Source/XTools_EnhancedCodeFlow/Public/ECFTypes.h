// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ECFTypes.generated.h"

USTRUCT(BlueprintType)
struct XTOOLS_ENHANCEDCODEFLOW_API FECFTimelineEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "时间轴事件")
	float Time = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "时间轴事件")
	FName EventName = NAME_None;
};

using FECFTimelineEventFunc = TUniqueFunction<void(FName EventName, float EventTime)>;

inline void ECFDispatchTimelineEvents(float OldTime, float RawNewTime, float Length, bool bLooping, bool bReverse, const TArray<FECFTimelineEvent>& Events, FECFTimelineEventFunc& EventFunc)
{
	if (Length <= KINDA_SMALL_NUMBER || Events.IsEmpty() || !EventFunc)
	{
		return;
	}

	auto DispatchForward = [&Events, &EventFunc](float Lower, float Upper, bool bIncludeUpper)
	{
		for (const FECFTimelineEvent& Event : Events)
		{
			if (Event.Time < 0.f || Event.Time > Upper + KINDA_SMALL_NUMBER)
			{
				continue;
			}
			if (Event.Time >= Lower && (bIncludeUpper ? Event.Time <= Upper + KINDA_SMALL_NUMBER : Event.Time < Upper))
			{
				EventFunc(Event.EventName, Event.Time);
			}
		}
	};

	auto DispatchReverse = [&Events, &EventFunc](float Lower, float Upper, bool bIncludeLower)
	{
		for (int32 Index = Events.Num() - 1; Index >= 0; --Index)
		{
			const FECFTimelineEvent& Event = Events[Index];
			if (Event.Time > Upper || Event.Time < Lower - KINDA_SMALL_NUMBER)
			{
				continue;
			}
			if ((bIncludeLower ? Event.Time >= Lower - KINDA_SMALL_NUMBER : Event.Time > Lower) && Event.Time <= Upper)
			{
				EventFunc(Event.EventName, Event.Time);
			}
		}
	};

	const float OldPosition = FMath::Clamp(OldTime, 0.f, Length);
	if (!bReverse)
	{
		if (RawNewTime <= OldPosition)
		{
			return;
		}
		if (!bLooping || RawNewTime <= Length)
		{
			const float NewPosition = FMath::Min(RawNewTime, Length);
			DispatchForward(OldPosition, NewPosition, NewPosition >= Length - KINDA_SMALL_NUMBER);
			return;
		}

		float Cursor = OldPosition;
		float Remaining = RawNewTime - OldPosition;
		while (Remaining > KINDA_SMALL_NUMBER)
		{
			const float Segment = FMath::Min(Remaining, Length - Cursor);
			const float SegmentEnd = Cursor + Segment;
			DispatchForward(Cursor, SegmentEnd, SegmentEnd >= Length - KINDA_SMALL_NUMBER);
			Remaining -= Segment;
			if (Remaining <= KINDA_SMALL_NUMBER)
			{
				break;
			}
			Cursor = 0.f;
		}
	}
	else
	{
		if (RawNewTime >= OldPosition)
		{
			return;
		}
		if (!bLooping || RawNewTime >= 0.f)
		{
			const float NewPosition = FMath::Max(RawNewTime, 0.f);
			DispatchReverse(NewPosition, OldPosition, NewPosition <= KINDA_SMALL_NUMBER);
			return;
		}

		float Cursor = OldPosition;
		float Remaining = OldPosition - RawNewTime;
		while (Remaining > KINDA_SMALL_NUMBER)
		{
			const float Segment = FMath::Min(Remaining, Cursor);
			const float SegmentStart = Cursor - Segment;
			DispatchReverse(SegmentStart, Cursor, SegmentStart <= KINDA_SMALL_NUMBER);
			Remaining -= Segment;
			if (Remaining <= KINDA_SMALL_NUMBER)
			{
				break;
			}
			Cursor = Length;
		}
	}
}

// ECF 系统可用的混合函数
UENUM(BlueprintType)
enum class EECFBlendFunc : uint8
{
	ECFBlend_Linear UMETA(DisplayName = "线性"),
	ECFBlend_Cubic UMETA(DisplayName = "三次方"),
	ECFBlend_EaseIn UMETA(DisplayName = "缓入"),
	ECFBlend_EaseOut UMETA(DisplayName = "缓出"),
	ECFBlend_EaseInOut UMETA(DisplayName = "缓入缓出")
};

// ECF 时间轴的播放方向
UENUM(BlueprintType)
enum class EECFPlayDirection : uint8
{
	Forward UMETA(DisplayName = "正向"),
	Reverse UMETA(DisplayName = "反向")
};

// ECF 系统中异步任务的可用优先级
UENUM(BlueprintType)
enum class EECFAsyncPrio : uint8
{
	Normal UMETA(DisplayName = "普通"),
	HiPriority UMETA(DisplayName = "高优先级")
};
