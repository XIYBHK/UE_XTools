// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BAGraphTaskBase.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "UObject/WeakObjectPtr.h"
#include "BlueprintAssistDelayedDelegate.h"

class SNotificationItem;
class UEdGraphNode;

class FBAGraphTask_CacheNodeSizes : public FBAGraphTaskBase
{
public:
	using FBAGraphTaskBase::FBAGraphTaskBase;
	FBAGraphTask_CacheNodeSizes(FBAGraphHandler& InGraphHandler);

	int InitialPendingSize = 0;
	float NodeSizeTimeout = 0.f;
	TArray<TWeakObjectPtr<UEdGraphNode>> PendingSize;
	TWeakObjectPtr<UEdGraphNode> FocusedNode;
	bool bFullyZoomed = false;
	TWeakPtr<SNotificationItem> SizeTimeoutNotification;
	FVector2D ViewCache;
	float ZoomCache = 1.0f;

	FBADelayedDelegate DelayedCacheSizeTimeout;
	FBADelayedDelegate DelayedViewportZoomIn;

	virtual const FString& GetTaskName() const override
	{
		static const FString TaskName_CacheNodeSizes = "CacheNodeSizes";
		return TaskName_CacheNodeSizes;
	}

	virtual bool IsRunning() override;
	virtual bool TickTask(float DeltaTime) override;
	virtual void InitTask() override;
	virtual void ResetTask(bool bActive, bool bCancelled) override;

	float GetProgress() const;
	void ShowSizeTimeoutNotification();
	void CancelSizeTimeoutNotification(bool bSaveFocusedNodeSize);
	FText GetSizeTimeoutMessage() const;
	void OnDelayedCacheSizeFinished();
	bool CacheNodeSize(UEdGraphNode* Node);
};
