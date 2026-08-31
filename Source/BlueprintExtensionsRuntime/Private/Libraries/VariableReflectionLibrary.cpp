#include "Libraries/VariableReflectionLibrary.h"

#include "BlueprintExtensionsRuntime.h"
#include "XToolsBlueprintHelpers.h"
#include "XToolsErrorReporter.h"

#include "Misc/OutputDeviceNull.h"

TArray<FString> UVariableReflectionLibrary::GetVariableNames(UClass* Class, bool bIncludeSuper)
{
	TArray<FString> VariableName;
	if (!Class)
	{
		ensureAlwaysMsgf(false, TEXT("Class is Null on UVariableReflectionLibrary::GetVariableNames"));
		return VariableName;
	}

	EFieldIteratorFlags::SuperClassFlags IteratorFlags = bIncludeSuper ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper;
	for (TFieldIterator<FProperty> PropertyIt(Class, IteratorFlags); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;

		if ((!Property->HasAnyPropertyFlags(CPF_Parm) && Property->HasAllPropertyFlags(CPF_BlueprintVisible)))
		{
			VariableName.Add(Property->GetName());
		}
	}
	return VariableName;
}

void UVariableReflectionLibrary::SetValueByString(UObject* OwnerObject, FString VariableName, FString Value)
{
	if (!IsValid(OwnerObject)) return;

	if (FProperty* Field = OwnerObject->GetClass()->FindPropertyByName(FName(*VariableName)))
	{
		if (Field->HasAnyPropertyFlags(CPF_BlueprintVisible) &&
			!Field->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
		{
			XToolsBlueprintHelpers::FScopedPropertyStorage ParsedValue(Field);
			if (!ParsedValue.IsValid())
			{
				return;
			}

			FOutputDeviceNull ImportErrors;
			const TCHAR* ImportResult = Field->ImportText_Direct(
				*Value,
				ParsedValue.Get(),
				OwnerObject,
				PPF_None,
				&ImportErrors
			);
			while (ImportResult && FChar::IsWhitespace(*ImportResult))
			{
				++ImportResult;
			}

			if (!ImportResult || *ImportResult != TEXT('\0'))
			{
				FXToolsErrorReporter::Warning(
					LogBlueprintExtensionsRuntime,
					FString::Printf(TEXT("按字符串设置变量失败：属性 '%s' 无法导入值 '%s'"),
						*VariableName, *Value),
					TEXT("SetValueByString"));
				return;
			}

#if WITH_EDITOR
			OwnerObject->PreEditChange(Field);
#endif
			Field->SetValue_InContainer(OwnerObject, ParsedValue.Get());
#if WITH_EDITOR
			FPropertyChangedEvent PropertyChangedEvent(Field, EPropertyChangeType::ValueSet);
			OwnerObject->PostEditChangeProperty(PropertyChangedEvent);
#endif
		}
	}
}

FString UVariableReflectionLibrary::GetValueByString(UObject* OwnerObject, FString VariableName)
{
	FString Value;

	if (!IsValid(OwnerObject))
	{
		return Value;
	}

	FProperty* Field = OwnerObject->GetClass()->FindPropertyByName(FName(*VariableName));

	if (Field != nullptr)
	{
		Field->ExportTextItem_InContainer(Value, OwnerObject, nullptr, nullptr, 0);
	}
	return Value;
}
