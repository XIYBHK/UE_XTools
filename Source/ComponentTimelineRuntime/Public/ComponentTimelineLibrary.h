// Copyright 2023 Tomasz Klin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ComponentTimelineLibrary.generated.h"

UCLASS(BlueprintType)
class COMPONENTTIMELINERUNTIME_API UComponentTimelineLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Component Timeline", meta = (UnsafeDuringActorConstruction = "true", DefaultToSelf="Component"))
	static void InitializeComponentTimelines(UActorComponent* Component);

	UFUNCTION(BlueprintCallable, Category = "Component Timeline", meta = (UnsafeDuringActorConstruction = "true", DefaultToSelf="BlueprintOwner"))
	static void InitializeTimelines(UObject* BlueprintOwner, AActor* ActorOwner);

};
