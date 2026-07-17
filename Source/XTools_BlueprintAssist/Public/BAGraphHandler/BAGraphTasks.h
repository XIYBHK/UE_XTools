#pragma once

#include "CoreMinimal.h"
#include "BAGraphTaskBase.h"
#include "BAGraphTasks.h"
#include "BAGraphTask_NodeSize.h"
#include "BAGraphTask_WaitForRename.h"

class SNotificationItem;
class UEdGraphNode;
class FBAGraphHandler;

class FBAGraphTasks
{
public:
	FBAGraphTask_WaitForRename RenameTask;
	FBAGraphTask_CacheNodeSizes CacheSizeTask;

	FBAGraphTaskBase* ActiveTask = nullptr;

	TArray<FBAGraphTaskBase*> Tasks;

	explicit FBAGraphTasks(FBAGraphHandler& InGraphHandler);

	void TickTasks(float DeltaTime);

	void ClearTasks();
	bool HasTaskRunning();
};