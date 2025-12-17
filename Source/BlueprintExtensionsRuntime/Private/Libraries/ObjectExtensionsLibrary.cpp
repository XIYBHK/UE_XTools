#include "Libraries/ObjectExtensionsLibrary.h"

#include "UObject/UnrealType.h"

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
					// 获取属性值的指针
					void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(Object);

					// 获取默认对象的属性值
					void* DefalutValuePtr = Property->ContainerPtrToValuePtr<void>(DefaultObject);

					FString defaultValueStr;
					Property->ExportTextItem_Direct(defaultValueStr, DefalutValuePtr, nullptr, DefaultObject, 0, nullptr);
					// 设置Object的属性值为默认值
					Property->ImportText_Direct(*defaultValueStr, PropertyValuePtr, Object, 0, nullptr);
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

			void* ObjectValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
			void* SourceValuePtr = Property->ContainerPtrToValuePtr<void>(CDOSource);

			FString ValueStr;
			Property->ExportTextItem_Direct(ValueStr, SourceValuePtr, nullptr, CDOSource, 0, nullptr);
			Property->ImportText_Direct(*ValueStr, ObjectValuePtr, Object, 0, nullptr);
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

			FString ValueStr;
			Property->ExportTextItem_InContainer(ValueStr, Source, nullptr, nullptr, 0);

			FProperty* TargetField = Target->GetClass()->FindPropertyByName(Property->GetFName());
			if (TargetField)
			{
				TargetField->ImportText_InContainer(*ValueStr, Target, Target, 0);
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

			FString ValueStr;
			Property->ExportTextItem_InContainer(ValueStr, DefaultObject, nullptr, nullptr, 0);

			FProperty* TargetField = Target->GetClass()->FindPropertyByName(Property->GetFName());
			if (TargetField)
			{
				TargetField->ImportText_InContainer(*ValueStr, Target, Target, 0);
			}
		}
	}

#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

