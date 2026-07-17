// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistSettings_Search.generated.h"

UCLASS(config=EditorPerProjectUserSettings, defaultconfig)
class XTOOLS_BLUEPRINTASSIST_API UBlueprintAssistSettings_Search : public UObject
{
	GENERATED_BODY()

public:
	/* Search settings */
	UPROPERTY(config)
	FString File_SavedPath;

	UPROPERTY(config)
	FString File_LastSearch;

	UPROPERTY(config)
	FString Properties_SavedPath;

	UPROPERTY(config)
	FString Properties_LastSearch;

	UPROPERTY(config)
	bool bLargeThumbnail = false;

	/* Config viewer settings */
	UPROPERTY(config)
	bool bReadConstructorDefaults;

	static UBlueprintAssistSettings_Search& Get();
};
