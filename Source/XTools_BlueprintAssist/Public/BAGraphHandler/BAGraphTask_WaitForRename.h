// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BAGraphTaskBase.h"

class UEdGraphNode;

class FBAGraphTask_WaitForRename : public FBAGraphTaskBase
{
public:
	using FBAGraphTaskBase::FBAGraphTaskBase;

	TArray<TWeakObjectPtr<UEdGraphNode>> Nodes;

	virtual const FString& GetTaskName() const override
	{
		static const FString TaskName_WaitForRename = "TaskWaitForRename";
		return TaskName_WaitForRename;
	}

	virtual bool IsRunning() override;

	virtual bool TickTask(float DeltaTime) override;

	virtual void ResetTask(bool bActive, bool bCancelled) override;

	void AddNodes(const TArray<UEdGraphNode*>& NodesToAdd);

	bool IsNodeBeingRenamed(UEdGraphNode* Node);
};
