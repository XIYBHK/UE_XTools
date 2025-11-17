// Copyright fpwong. All Rights Reserved.


#include "BlueprintAssistMisc/BASettingsBase.h"

#include "AssetToolsModule.h"
#include "BlueprintAssistGlobals.h"
#include "JsonObjectConverter.h"
#include "ScopedTransaction.h"
#include "BlueprintAssistMisc/FBAScopedPropertySetter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Settings/EditorLoadingSavingSettings.h"

FText FBASettingsChange::GetJsonPropertyDisplayName(const FString& PropName, UObject* Object)
{
	if (FProperty* Prop = Object->GetClass()->FindPropertyByName(FName(PropName)))
	{
		return Prop->GetDisplayNameText();
	}

	return FText::GetEmpty();
}

FString FBASettingsChange::GetJsonValueAsString(const FString& Name, TSharedPtr<FJsonValue> Value, bool bPrettyPrint)
{
	if (!Value)
	{
		return FString();
	}

	FString OutText;

	if (bPrettyPrint)
	{
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutText);
		JsonWriter->WriteObjectStart();
		FJsonSerializer::Serialize(Value, Name, JsonWriter, false);
		JsonWriter->WriteObjectEnd();
		JsonWriter->Close();
	}
	else
	{
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutText);
		JsonWriter->WriteObjectStart();
		FJsonSerializer::Serialize(Value, Name, JsonWriter, false);
		JsonWriter->WriteObjectEnd();
		JsonWriter->Close();

		OutText.RemoveFromStart("{");
		OutText.RemoveFromStart("\"" + Name + "\":");
		OutText.RemoveFromEnd("}");

		// OutText.RemoveFromStart("[");
		// OutText.RemoveFromEnd("]");
	}

	return OutText;
}

FString FBASettingsChange::AsString(const FString& Name, TSharedPtr<FJsonValue> Value, bool bPrettyPrint)
{
	return FBASettingsChange::GetJsonValueAsString(PropertyName, NewValue);
}

bool FBASettingsChange::ResetToDefault(UObject* Object)
{
	if (FProperty* Prop = Object->GetClass()->FindPropertyByName(FName(PropertyName)))
	{
		FText TransactionMsg = FText::Format(INVTEXT("Reset setting \"{0}\""), GetJsonPropertyDisplayName(PropertyName, Object));
		FBAScopedPropertySetter ScopedSetter(Object, Prop, TransactionMsg);

		FText Reason;
#if BA_UE_VERSION_OR_LATER(5, 0)
		if (FJsonObjectConverter::JsonValueToUProperty(OldValue, Prop, Prop->ContainerPtrToValuePtr<void>(Object), 0, 0, false, &Reason))
#else
		if (FJsonObjectConverter::JsonValueToUProperty(OldValue, Prop, Prop->ContainerPtrToValuePtr<void>(Object)))
#endif
		{
			return true;
		}

		UE_LOG(LogBlueprintAssist, Error, TEXT("Failed to reset setting? %s"), *Reason.ToString());
	}

	return false;
}

void FBASettingsChange::LogChange()
{
	const FString OldString = GetJsonValueAsString(PropertyName, OldValue, true);
	const FString NewString = GetJsonValueAsString(PropertyName, NewValue, true);

	UE_LOG(LogBlueprintAssist, Warning, TEXT("===== BEGIN LOG OLD VALUE ====="));
	UE_LOG(LogBlueprintAssist, Log, TEXT("%s"), *OldString);
	UE_LOG(LogBlueprintAssist, Warning, TEXT("===== END LOG OLD VALUE   ====="));
	UE_LOG(LogBlueprintAssist, Warning, TEXT("===== BEGIN LOG NEW VALUE ====="));
	UE_LOG(LogBlueprintAssist, Log, TEXT("%s"), *NewString);
	UE_LOG(LogBlueprintAssist, Warning, TEXT("===== END LOG NEW VALUE   ====="));
}

void UBASettingsBase::SaveSettingsDefaults()
{
	DefaultsAsJson = MakeShareable(new FJsonObject);
	FJsonObjectConverter::UStructToJsonObject(GetClass(), this, DefaultsAsJson.ToSharedRef());
	// UE_LOG(LogTemp, Warning, TEXT("Saved setting! %s"), *GetClass()->GetName());
}

TArray<FBASettingsChange> UBASettingsBase::GetChanges() const
{
	TArray<FBASettingsChange> Changes;

	// load our current state into json to compare with the saved json
	TSharedRef<FJsonObject> Curr = MakeShareable(new FJsonObject);
	FJsonObjectConverter::UStructToJsonObject(GetClass(), this, Curr);

	// iter our current properties
	for (auto& Elem : Curr->Values)
	{
		// find the saved property from our json
		if (TSharedPtr<FJsonValue> DefaultValue = DefaultsAsJson->Values.FindRef(Elem.Key))
		{
			const FJsonValue& Default = DefaultValue.ToSharedRef().Get();
			const FJsonValue& New = Elem.Value.ToSharedRef().Get();
			if (Default != New)
			{
				Changes.Add(FBASettingsChange(Elem.Key, DefaultValue, Elem.Value));
				// UE_LOG(LogTemp, Warning, TEXT("%s changed old %s, new %s"), *Elem.Key, *Elem.Value.Get()->AsString(), *Value->Get()->AsString());
			}
		}
	}

	return Changes;
}

FString UBASettingsBase::GetAllChangesAsString() const
{
	FString Changes;

	// load our current state into json to compare with the saved json
	TSharedRef<FJsonObject> Curr = MakeShareable(new FJsonObject);
	FJsonObjectConverter::UStructToJsonObject(GetClass(), this, Curr);

	// iter our current properties
	for (auto& Elem : Curr->Values)
	{
		// find the saved property from our json
		if (TSharedPtr<FJsonValue> DefaultValue = DefaultsAsJson->Values.FindRef(Elem.Key))
		{
			const FJsonValue& Default = DefaultValue.ToSharedRef().Get();
			const FJsonValue& New = Elem.Value.ToSharedRef().Get();
			if (Default != New)
			{
				FString Value = FBASettingsChange::GetJsonValueAsString(Elem.Key, Elem.Value);
				FString Line = FString::Printf(TEXT("%s: \"%s\""), *Elem.Key, *Value);
				Changes.Append(Line).Append(LINE_TERMINATOR);
			}
		}
	}

	return Changes;
}

void UBASettingsBase::ResetToDefault()
{
	TArray<FBASettingsChange> Changes = GetChanges();
	for (FBASettingsChange& Change : Changes)
	{
		Change.ResetToDefault(this);
	}
}
