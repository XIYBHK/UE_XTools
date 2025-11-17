// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BASettingsBase.generated.h"

class FJsonObject;
class FJsonValue;

struct FBASettingsChange
{
	FBASettingsChange() {};
	FBASettingsChange(const FString& PropName, TSharedPtr<FJsonValue> Old, TSharedPtr<FJsonValue> New)
		: PropertyName(PropName)
		, OldValue(Old)
		, NewValue(New)
	{
	}

	virtual ~FBASettingsChange() {};

	FString PropertyName;
	TSharedPtr<FJsonValue> OldValue;
	TSharedPtr<FJsonValue> NewValue;

	static FText GetJsonPropertyDisplayName(const FString& PropName, UObject* Object);
	static FString GetJsonValueAsString(const FString& Name, TSharedPtr<FJsonValue> Value, bool bPrettyPrint = false);
	FString AsString(const FString& Name, TSharedPtr<FJsonValue> Value, bool bPrettyPrint = false);

	bool ResetToDefault(UObject* Object);

	void LogChange();
};

UCLASS()
class XTOOLS_BLUEPRINTASSIST_API UBASettingsBase : public UObject
{
	GENERATED_BODY()

public:
	TSharedPtr<FJsonObject> DefaultsAsJson;

	void SaveSettingsDefaults();
	TArray<FBASettingsChange> GetChanges() const;
	FString GetAllChangesAsString() const;
	void ResetToDefault();
};
