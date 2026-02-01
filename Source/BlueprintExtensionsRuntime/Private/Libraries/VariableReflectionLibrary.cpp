#include "Libraries/VariableReflectionLibrary.h"

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
			Field->ImportText_InContainer(
				*Value,
				OwnerObject,
				OwnerObject,
				PPF_None
			);
		}
	}
}

FString UVariableReflectionLibrary::GetValueByString(UObject* OwnerObject, FString VariableName)
{
	FString Value;

	if (OwnerObject == nullptr)
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