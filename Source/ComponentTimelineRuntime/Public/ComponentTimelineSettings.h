// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ComponentTimelineSettings.generated.h"

/**
 * 
 */
UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "Component Timeline"))
class COMPONENTTIMELINERUNTIME_API UComponentTimelineSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, config, Category = "Experimental", meta = (DisplayName = "Enable timelines in all blueprints", ConfigRestartRequired = true))
	bool bEnableObjectTimeline = false;

	virtual FName GetCategoryName() const { return FName("Plugins"); }
};
