// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistDelayedDelegate.h"

void FBADelayedDelegate::SetOnDelayEnded(FBAOnDelayEnded OnDelayEnded)
{
	Delegate = OnDelayEnded;
}

void FBADelayedDelegate::StartDelay(int32 NumTicks)
{
	TicksRemaining = NumTicks;
}

bool FBADelayedDelegate::Tick()
{
	if (TicksRemaining == 0)
	{
		Delegate.ExecuteIfBound();
	}

	if (TicksRemaining >= 0)
	{
		TicksRemaining -= 1;
	}

	return IsComplete();
}

void FBADelayedDelegate::Cancel()
{
	TicksRemaining = -1;
}
