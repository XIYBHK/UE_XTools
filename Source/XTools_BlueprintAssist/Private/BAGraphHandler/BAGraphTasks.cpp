#include "BAGraphHandler/BAGraphTasks.h"

#include "SGraphPanel.h"

FBAGraphTasks::FBAGraphTasks(FBAGraphHandler& InGraphHandler)
	: RenameTask(InGraphHandler)
	, CacheSizeTask(InGraphHandler)
{
	Tasks = { &RenameTask, &CacheSizeTask };
}

void FBAGraphTasks::TickTasks(float DeltaTime)
{
	for (FBAGraphTaskBase* Task : Tasks)
	{
		if (!Task->IsRunning())
		{
			continue;
		}

		if (ActiveTask != Task)
		{
			if (ActiveTask)
			{
				UE_LOG(LogBAGraphTask, Log, TEXT("[%hs] Task '%s' ended"), __FUNCTION__, *ActiveTask->GetTaskName());
				ActiveTask->TaskEnded(true, false);
			}

			UE_LOG(LogBAGraphTask, Log, TEXT("[%hs] Task '%s' started"), __FUNCTION__, *Task->GetTaskName());
			Task->InitTask();
			ActiveTask = Task;
		}

		// tick this task then skip the other tasks
		if (!Task->TickTask(DeltaTime))
		{
			Task->TaskEnded(true, false);
			ActiveTask = nullptr;
		}

		UE_LOG(LogBAGraphTask, VeryVerbose, TEXT("[%hs] Ticking '%s'"), __FUNCTION__, *Task->GetTaskName());
		return;
	}

	if (ActiveTask)
	{
		UE_LOG(LogBAGraphTask, Log, TEXT("[%hs] Task '%s' ended"), __FUNCTION__, *ActiveTask->GetTaskName());
		ActiveTask->TaskEnded(true, false);
		ActiveTask = nullptr;
	}
}

void FBAGraphTasks::ClearTasks()
{
	for (FBAGraphTaskBase* Task : Tasks)
	{
		Task->TaskEnded(Task == ActiveTask, true);
	}

	ActiveTask = nullptr;
}

bool FBAGraphTasks::HasTaskRunning()
{
	for (FBAGraphTaskBase* Task : Tasks)
	{
		if (Task->IsRunning())
		{
			return true;
		}
	}

	return false;
}
