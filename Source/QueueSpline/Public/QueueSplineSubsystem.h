#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "QueueSplineSubsystem.generated.h"

class UQueueSplineComponent;

UCLASS()
class QUEUESPLINE_API UQueueSplineSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterQueue(UQueueSplineComponent* QueueComponent);
	void UnregisterQueue(UQueueSplineComponent* QueueComponent);

	UFUNCTION(BlueprintPure, Category = "XTools|排队样条线", meta = (DisplayName = "获取排队样条线数量"))
	int32 GetQueueCount() const;

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线", meta = (DisplayName = "清理无效排队样条线"))
	void CleanupInvalidQueues();

private:
	TArray<TWeakObjectPtr<UQueueSplineComponent>> QueueComponents;
};
