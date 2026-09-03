// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "Algo/BinarySearch.h"
#include "Algo/Sort.h"
#include "CoreMinimal.h"
#include "ECFLogs.h"
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

inline constexpr int32 ECFMaxTimelineEventSegmentsPerTick = 1000;

inline void ECFPrepareTimelineEvents(TArray<FECFTimelineEvent>& Events)
{
	Events.RemoveAll([](const FECFTimelineEvent& Event) { return !FMath::IsFinite(Event.Time); });
	Algo::StableSort(Events, [](const FECFTimelineEvent& A, const FECFTimelineEvent& B) { return A.Time < B.Time; });
}

enum class EECFTimelineEventDispatchResult : uint8
{
	Completed,
	Interrupted,
	SegmentLimitReached
};

template <typename ContinuePredicateType>
inline EECFTimelineEventDispatchResult ECFDispatchTimelineEvents(float OldTime, float RawNewTime, float Length, bool bLooping, bool bReverse, const TArray<FECFTimelineEvent>& Events, FECFTimelineEventFunc& EventFunc, ContinuePredicateType&& ShouldContinue)
{
	if (Length <= KINDA_SMALL_NUMBER || Events.IsEmpty() || !EventFunc)
	{
		return EECFTimelineEventDispatchResult::Completed;
	}

	auto DispatchForward = [&Events, &EventFunc, &ShouldContinue](float Lower, float Upper, bool bIncludeUpper)
	{
		const int32 StartIndex = Algo::LowerBoundBy(Events, Lower, &FECFTimelineEvent::Time);
		const int32 EndIndex = bIncludeUpper
			? Algo::UpperBoundBy(Events, Upper + KINDA_SMALL_NUMBER, &FECFTimelineEvent::Time)
			: Algo::LowerBoundBy(Events, Upper, &FECFTimelineEvent::Time);
		for (int32 Index = StartIndex; Index < EndIndex; ++Index)
		{
			if (!ShouldContinue())
			{
				return false;
			}
			const FECFTimelineEvent& Event = Events[Index];
			EventFunc(Event.EventName, Event.Time);
			if (!ShouldContinue())
			{
				return false;
			}
		}
		return true;
	};

	auto DispatchReverse = [&Events, &EventFunc, &ShouldContinue](float Lower, float Upper, bool bIncludeLower)
	{
		const int32 StartIndex = bIncludeLower
			? Algo::LowerBoundBy(Events, Lower - KINDA_SMALL_NUMBER, &FECFTimelineEvent::Time)
			: Algo::UpperBoundBy(Events, Lower, &FECFTimelineEvent::Time);
		int32 GroupEnd = Algo::UpperBoundBy(Events, Upper, &FECFTimelineEvent::Time);
		while (GroupEnd > StartIndex)
		{
			const float GroupTime = Events[GroupEnd - 1].Time;
			int32 GroupStart = GroupEnd - 1;
			while (GroupStart > StartIndex && Events[GroupStart - 1].Time == GroupTime)
			{
				--GroupStart;
			}

			for (int32 Index = GroupStart; Index < GroupEnd; ++Index)
			{
				if (!ShouldContinue())
				{
					return false;
				}
				const FECFTimelineEvent& Event = Events[Index];
				EventFunc(Event.EventName, Event.Time);
				if (!ShouldContinue())
				{
					return false;
				}
			}
			GroupEnd = GroupStart;
		}
		return true;
	};

	const float OldPosition = FMath::Clamp(OldTime, 0.f, Length);
	if (!bReverse)
	{
		if (RawNewTime <= OldPosition)
		{
			return EECFTimelineEventDispatchResult::Completed;
		}
		if (!bLooping || RawNewTime <= Length)
		{
			const float NewPosition = FMath::Min(RawNewTime, Length);
			if (NewPosition <= OldPosition)
			{
				return EECFTimelineEventDispatchResult::Completed;
			}
			return DispatchForward(OldPosition, NewPosition, NewPosition >= Length - KINDA_SMALL_NUMBER)
				? EECFTimelineEventDispatchResult::Completed
				: EECFTimelineEventDispatchResult::Interrupted;
		}

		float Cursor = OldPosition;
		float Remaining = RawNewTime - OldPosition;
		if (Cursor >= Length)
		{
			Cursor = 0.f;
		}
		int32 SegmentCount = 0;
		while (Remaining > KINDA_SMALL_NUMBER && SegmentCount < ECFMaxTimelineEventSegmentsPerTick)
		{
			++SegmentCount;
			const float Segment = FMath::Min(Remaining, Length - Cursor);
			const float SegmentEnd = Cursor + Segment;
			if (!DispatchForward(Cursor, SegmentEnd, SegmentEnd >= Length - KINDA_SMALL_NUMBER))
			{
				return EECFTimelineEventDispatchResult::Interrupted;
			}
			Remaining -= Segment;
			if (Remaining <= KINDA_SMALL_NUMBER)
			{
				break;
			}
			Cursor = 0.f;
		}
		if (Remaining > KINDA_SMALL_NUMBER)
		{
			return EECFTimelineEventDispatchResult::SegmentLimitReached;
		}
	}
	else
	{
		if (RawNewTime >= OldPosition)
		{
			return EECFTimelineEventDispatchResult::Completed;
		}
		if (!bLooping || RawNewTime >= 0.f)
		{
			const float NewPosition = FMath::Max(RawNewTime, 0.f);
			if (NewPosition >= OldPosition)
			{
				return EECFTimelineEventDispatchResult::Completed;
			}
			return DispatchReverse(NewPosition, OldPosition, NewPosition <= KINDA_SMALL_NUMBER)
				? EECFTimelineEventDispatchResult::Completed
				: EECFTimelineEventDispatchResult::Interrupted;
		}

		float Cursor = OldPosition;
		float Remaining = OldPosition - RawNewTime;
		if (Cursor <= 0.f)
		{
			Cursor = Length;
		}
		int32 SegmentCount = 0;
		while (Remaining > KINDA_SMALL_NUMBER && SegmentCount < ECFMaxTimelineEventSegmentsPerTick)
		{
			++SegmentCount;
			const float Segment = FMath::Min(Remaining, Cursor);
			const float SegmentStart = Cursor - Segment;
			if (!DispatchReverse(SegmentStart, Cursor, SegmentStart <= KINDA_SMALL_NUMBER))
			{
				return EECFTimelineEventDispatchResult::Interrupted;
			}
			Remaining -= Segment;
			if (Remaining <= KINDA_SMALL_NUMBER)
			{
				break;
			}
			Cursor = Length;
		}
		if (Remaining > KINDA_SMALL_NUMBER)
		{
			return EECFTimelineEventDispatchResult::SegmentLimitReached;
		}
	}

	return EECFTimelineEventDispatchResult::Completed;
}

inline EECFTimelineEventDispatchResult ECFDispatchTimelineEvents(float OldTime, float RawNewTime, float Length, bool bLooping, bool bReverse, const TArray<FECFTimelineEvent>& Events, FECFTimelineEventFunc& EventFunc)
{
	return ECFDispatchTimelineEvents(OldTime, RawNewTime, Length, bLooping, bReverse, Events, EventFunc, []() { return true; });
}

class XTOOLS_ENHANCEDCODEFLOW_API FECFTimelineEventTrack
{
public:
	void Initialize(TArray<FECFTimelineEvent>&& InEvents, FECFTimelineEventFunc&& InEventFunc)
	{
		Events = MoveTemp(InEvents);
		EventFunc = MoveTemp(InEventFunc);
		ECFPrepareTimelineEvents(Events);
		bHasLoggedDispatchLimit = false;
	}

	template <typename ContinuePredicateType>
	EECFTimelineEventDispatchResult Dispatch(float OldTime, float RawNewTime, float Length, bool bLooping, bool bReverse, ContinuePredicateType&& ShouldContinue)
	{
		const EECFTimelineEventDispatchResult Result = ECFDispatchTimelineEvents(
			OldTime, RawNewTime, Length, bLooping, bReverse, Events, EventFunc, Forward<ContinuePredicateType>(ShouldContinue));
		if (Result == EECFTimelineEventDispatchResult::SegmentLimitReached && !bHasLoggedDispatchLimit)
		{
			bHasLoggedDispatchLimit = true;
#if ECF_LOGS
			UE_LOG(LogECF, Warning, TEXT("ECF timeline event dispatch exceeded the per-Tick segment limit (%d); remaining events were skipped."), ECFMaxTimelineEventSegmentsPerTick);
#endif
		}
		return Result;
	}

private:
	TArray<FECFTimelineEvent> Events;
	FECFTimelineEventFunc EventFunc;
	bool bHasLoggedDispatchLimit = false;
};

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
