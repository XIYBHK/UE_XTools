// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

DECLARE_DELEGATE_OneParam(FBAOnGraphTaskEnded, bool);

DECLARE_LOG_CATEGORY_EXTERN(LogBAGraphTask, NoLogging, VeryVerbose);

class FBAGraphHandler;

class FBAGraphTaskBase
{
public:
	explicit FBAGraphTaskBase(FBAGraphHandler& InGraphHandler) : GraphHandler(InGraphHandler) {}
	virtual ~FBAGraphTaskBase() {}

	FBAGraphHandler& GraphHandler;
	FBAOnGraphTaskEnded OnTaskEnded;

	virtual const FString& GetTaskName() const = 0;

	virtual bool IsRunning() { return false; }

	virtual bool TickTask(float DeltaTime)
	{
		return IsRunning();
	}

	virtual void InitTask() {}
	virtual void ResetTask(bool bActive, bool bCancelled) {}

	void TaskEnded(bool bActive, bool bCancelled)
	{
		OnTaskEnded.ExecuteIfBound(bCancelled);
		ResetTask(bActive, bCancelled);
	}
};
