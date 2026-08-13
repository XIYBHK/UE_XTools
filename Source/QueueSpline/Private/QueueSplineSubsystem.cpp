#include "QueueSplineSubsystem.h"

#include "QueueSplineComponent.h"

void UQueueSplineSubsystem::RegisterQueue(UQueueSplineComponent* QueueComponent)
{
	if (!IsValid(QueueComponent))
	{
		return;
	}

	for (const TWeakObjectPtr<UQueueSplineComponent>& ExistingQueue : QueueComponents)
	{
		if (ExistingQueue.Get() == QueueComponent)
		{
			return;
		}
	}

	QueueComponents.Add(QueueComponent);
}

void UQueueSplineSubsystem::UnregisterQueue(UQueueSplineComponent* QueueComponent)
{
	for (int32 Index = QueueComponents.Num() - 1; Index >= 0; --Index)
	{
		UQueueSplineComponent* ExistingQueue = QueueComponents[Index].Get();
		if (!ExistingQueue || ExistingQueue == QueueComponent)
		{
			QueueComponents.RemoveAt(Index);
		}
	}
}

int32 UQueueSplineSubsystem::GetQueueCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<UQueueSplineComponent>& QueueComponent : QueueComponents)
	{
		if (QueueComponent.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

void UQueueSplineSubsystem::CleanupInvalidQueues()
{
	for (int32 Index = QueueComponents.Num() - 1; Index >= 0; --Index)
	{
		if (!QueueComponents[Index].IsValid())
		{
			QueueComponents.RemoveAt(Index);
		}
	}
}
