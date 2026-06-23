#include "Libraries/ObjectExtensionsLibrary.h"

#include "UObject/UnrealType.h"

namespace
{
	bool CanCopyPropertyValue(const FProperty* SourceProperty, const FProperty* TargetProperty)
	{
		return SourceProperty && TargetProperty && SourceProperty->SameType(TargetProperty);
	}

	void CopyPropertyValueInContainer(const FProperty* SourceProperty, const void* SourceObject, FProperty* TargetProperty, void* TargetObject)
	{
		if (!CanCopyPropertyValue(SourceProperty, TargetProperty) || !SourceObject || !TargetObject)
		{
			return;
		}

		const void* SourceValuePtr = SourceProperty->ContainerPtrToValuePtr<void>(SourceObject);
		void* TargetValuePtr = TargetProperty->ContainerPtrToValuePtr<void>(TargetObject);
		TargetProperty->CopyCompleteValue(TargetValuePtr, SourceValuePtr);
	}
}

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region GetObject

	UObject* UObjectExtensionsLibrary::GetObjectFromMap(const TMap<UClass*, UObject*>& FindMap, UClass* FindClass)
	{
		if (FindClass)
		{
			UObject* Result = FindMap.FindRef(FindClass);
			if (Result)
			{
				return Result;
			}
		}

		return nullptr;
	}

#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region ResetObject

	void UObjectExtensionsLibrary::ClearObject(UObject* Object)
	{
		if (!IsValid(Object))
		{
			return;
		}

		UObject* DefaultObject = Object->GetClass()->GetDefaultObject();
		if (DefaultObject != nullptr)
		{
			// 遍历Object的所有属性，并设置为其默认值
			for (FProperty* Property : TFieldRange<FProperty>(Object->GetClass()))
			{
				if (Property->IsValidLowLevel() && !Property->HasAnyPropertyFlags(CPF_Transient | CPF_Config))
				{
					CopyPropertyValueInContainer(Property, DefaultObject, Property, Object);
				}
			}
		}
	}

	void UObjectExtensionsLibrary::ClearObjectByAssetCDO(UObject* Object)
	{
		if (!Object) return;

		FProperty* CDOProperty = Object->GetClass()->FindPropertyByName(TEXT("CDO"));
		if (!CDOProperty) return;

		UObject* CDOSource = nullptr;
		void* PropertyValuePtr = CDOProperty->ContainerPtrToValuePtr<void>(Object);

		if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(CDOProperty))
		{
			UObject* ObjectValue = ObjectProperty->GetObjectPropertyValue(PropertyValuePtr);
			CDOSource = ObjectValue ? ObjectValue->GetClass()->GetDefaultObject() : nullptr;
		}
		else if (FClassProperty* ClassProperty = CastField<FClassProperty>(CDOProperty))
		{
			UClass* ClassValue = Cast<UClass>(ClassProperty->GetObjectPropertyValue(PropertyValuePtr));
			CDOSource = ClassValue ? ClassValue->GetDefaultObject() : nullptr;
		}

		if (!CDOSource) return;

		for (FProperty* Property : TFieldRange<FProperty>(Object->GetClass()))
		{
			if (!Property->IsValidLowLevel() || 
				Property->HasAnyPropertyFlags(CPF_Transient | CPF_Config) || 
				Property == CDOProperty) continue;

			const FProperty* SourceProperty = CDOSource->GetClass()->FindPropertyByName(Property->GetFName());
			CopyPropertyValueInContainer(SourceProperty, CDOSource, Property, Object);
		}
	}

#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region CopyObject

	void UObjectExtensionsLibrary::CopyObjectValues(UObject* Target, UObject* Source)
	{
		if (!Source || !Target) return;

		UClass* Class = Source->GetClass();
		if (!Class) return;

		for (TFieldIterator<FProperty> PropertyIt(Class, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
		{
			FProperty* Property = *PropertyIt;
			if (Property->HasAnyPropertyFlags(CPF_Parm) || !Property->HasAllPropertyFlags(CPF_BlueprintVisible)) continue;

			FProperty* TargetField = Target->GetClass()->FindPropertyByName(Property->GetFName());
			if (CanCopyPropertyValue(Property, TargetField))
			{
				CopyPropertyValueInContainer(Property, Source, TargetField, Target);
			}
		}
	}

	void UObjectExtensionsLibrary::CopyClassValues(UObject* Target, UClass* SourceClass)
	{
		if (!SourceClass || !Target) return;

		UObject* DefaultObject = SourceClass->GetDefaultObject();
		if (!DefaultObject) return;

		for (TFieldIterator<FProperty> PropertyIt(SourceClass, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
		{
			FProperty* Property = *PropertyIt;
			if (Property->HasAnyPropertyFlags(CPF_Parm) || !Property->HasAllPropertyFlags(CPF_BlueprintVisible)) continue;

			FProperty* TargetField = Target->GetClass()->FindPropertyByName(Property->GetFName());
			if (CanCopyPropertyValue(Property, TargetField))
			{
				CopyPropertyValueInContainer(Property, DefaultObject, TargetField, Target);
			}
		}
	}

#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

