#include "Libraries/MapExtensionsLibrary.h"
#include "Kismet/KismetArrayLibrary.h"
#include "XToolsErrorReporter.h"
#include "XToolsVersionCompat.h"
#include "BlueprintExtensionsRuntime.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MapExtensionsLibrary)

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region GetKey
DEFINE_FUNCTION(UMapExtensionsLibrary::execMap_GetKey)
{
	// Get target map
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FMapProperty>(nullptr);
	const void* MapAddr = Stack.MostRecentPropertyAddress;
	const FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);
	if (!MapProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}
	
	// Get index
	P_GET_PROPERTY(FIntProperty, Index);
	
	// Get key out value
	FProperty* KeyProp = MapProperty->KeyProp;
	const int32 KeyPropertySize = XTOOLS_GET_ELEMENT_SIZE(KeyProp) * KeyProp->ArrayDim;
	void* KeyStorageSpace = FMemory_Alloca(KeyPropertySize);
	KeyProp->InitializeValue(KeyStorageSpace);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FProperty>(KeyStorageSpace);

	const FFieldClass* KeyPropClass = KeyProp->GetClass();
	const FFieldClass* MostRecentPropClass = Stack.MostRecentProperty->GetClass();
	void* ItemPtr;
	if (Stack.MostRecentPropertyAddress != nullptr &&
		KeyPropertySize == XTOOLS_GET_ELEMENT_SIZE(Stack.MostRecentProperty)*Stack.MostRecentProperty->ArrayDim &&
		(MostRecentPropClass->IsChildOf(KeyPropClass) || KeyPropClass->IsChildOf(MostRecentPropClass)))
	{
		ItemPtr = Stack.MostRecentPropertyAddress;
	}
	else
	{
		ItemPtr = KeyStorageSpace;
	}
	
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)RESULT_PARAM = GenericMap_GetKey(MapAddr, MapProperty, Index, ItemPtr);
	P_NATIVE_END
	
	KeyProp->DestroyValue(KeyStorageSpace);
}

bool UMapExtensionsLibrary::GenericMap_GetKey(const void* TargetMap, const FMapProperty* MapProperty, int32 Index, void* OutKeyPtr)
{
	if(TargetMap)
	{
		FScriptMapHelper MapHelper(MapProperty, TargetMap);
		if(Index < 0 || Index > MapHelper.Num()-1) return false;

		MapHelper.KeyProp->CopyCompleteValueFromScriptVM(OutKeyPtr, MapHelper.GetKeyPtr(MapHelper.FindInternalIndex(Index)));
		
		return true;
	}
	
	return false;
}
#pragma endregion 

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region GetValue
DEFINE_FUNCTION(UMapExtensionsLibrary::execMap_GetValue)
{
	// Get target map
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FMapProperty>(nullptr);
	const void* MapAddr = Stack.MostRecentPropertyAddress;
	const FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);
	if (!MapProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}
	
	// Get index
	P_GET_PROPERTY(FIntProperty, Index);
	
	// Get value out value
	FProperty* ValueProp = MapProperty->ValueProp;
	const int32 ValuePropertySize = XTOOLS_GET_ELEMENT_SIZE(ValueProp) * ValueProp->ArrayDim;
	void* ValueStorageSpace = FMemory_Alloca(ValuePropertySize);
	ValueProp->InitializeValue(ValueStorageSpace);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FProperty>(ValueStorageSpace);

	const FFieldClass* ValuePropClass = ValueProp->GetClass();
	const FFieldClass* MostRecentPropClass = Stack.MostRecentProperty->GetClass();

	void* ItemPtr;
	if (Stack.MostRecentPropertyAddress != nullptr &&
		ValuePropertySize == XTOOLS_GET_ELEMENT_SIZE(Stack.MostRecentProperty)*Stack.MostRecentProperty->ArrayDim &&
		(MostRecentPropClass->IsChildOf(ValuePropClass) || ValuePropClass->IsChildOf(MostRecentPropClass)))
	{
		ItemPtr = Stack.MostRecentPropertyAddress;
	}
	else
	{
		ItemPtr = ValueStorageSpace;
	}
	
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)RESULT_PARAM = GenericMap_GetValue(MapAddr, MapProperty, Index, ItemPtr);
	P_NATIVE_END

	ValueProp->DestroyValue(ValueStorageSpace);
}

bool UMapExtensionsLibrary::GenericMap_GetValue(const void* TargetMap, const FMapProperty* MapProperty, int32 Index, void* OutValuePtr)
{
	if(TargetMap)
	{
		FScriptMapHelper MapHelper(MapProperty, TargetMap);
		if(Index < 0 || Index > MapHelper.Num()-1) return false;
			
		MapHelper.ValueProp->CopySingleValueToScriptVM(OutValuePtr, MapHelper.GetValuePtr(MapHelper.FindInternalIndex(Index)));
		return true;
	}
	
	return false;
}
#pragma endregion 

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region GetKeys
DEFINE_FUNCTION(UMapExtensionsLibrary::execMap_Keys)
{
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FMapProperty>(nullptr);
	const void* MapAddr = Stack.MostRecentPropertyAddress;
	const FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);
	if (!MapProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}

	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	const void* ArrayAddr = Stack.MostRecentPropertyAddress;
	const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
	if (!ArrayProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}
	
	P_FINISH;
	P_NATIVE_BEGIN;
	GenericMap_Keys(MapAddr, MapProperty, ArrayAddr, ArrayProperty);
	P_NATIVE_END
}

void UMapExtensionsLibrary::GenericMap_Keys(const void* MapAddr, const FMapProperty* MapProperty, const void* ArrayAddr, const FArrayProperty* ArrayProperty)
{
	if(MapAddr && ArrayAddr && ensure(MapProperty->KeyProp->GetID() == ArrayProperty->Inner->GetID()) )
	{
		FScriptMapHelper MapHelper(MapProperty, MapAddr);
		FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayAddr);
		ArrayHelper.EmptyValues();

		const FProperty* InnerProp = ArrayProperty->Inner;

		int32 Size = MapHelper.Num();
		for( int32 I = 0; Size; ++I )
		{
			if(MapHelper.IsValidIndex(I))
			{
				const int32 LastIndex = ArrayHelper.AddValue();
				InnerProp->CopySingleValueToScriptVM(ArrayHelper.GetRawPtr(LastIndex), MapHelper.GetKeyPtr(I));
				--Size;
			}
		}
	}
}
#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region GetValues
DEFINE_FUNCTION(UMapExtensionsLibrary::execMap_Values)
{
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FMapProperty>(nullptr);
	const void* MapAddr = Stack.MostRecentPropertyAddress;
	const FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);
	if (!MapProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}

	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	const void* ArrayAddr = Stack.MostRecentPropertyAddress;
	const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
	if (!ArrayProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}
	
	P_FINISH;
	P_NATIVE_BEGIN;
	GenericMap_Values(MapAddr, MapProperty, ArrayAddr, ArrayProperty);
	P_NATIVE_END
}

void UMapExtensionsLibrary::GenericMap_Values(const void* MapAddr, const FMapProperty* MapProperty, const void* ArrayAddr, const FArrayProperty* ArrayProperty)
{
	if(MapAddr && ArrayAddr && ensure(MapProperty->ValueProp->GetID() == ArrayProperty->Inner->GetID()))
	{
		FScriptMapHelper MapHelper(MapProperty, MapAddr);
		FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayAddr);
		ArrayHelper.EmptyValues();

		const FProperty* InnerProp = ArrayProperty->Inner;
		
		int32 Size = MapHelper.Num();
		for( int32 I = 0; Size; ++I )
		{
			if(MapHelper.IsValidIndex(I))
			{
				const int32 LastIndex = ArrayHelper.AddValue();
				InnerProp->CopySingleValueToScriptVM(ArrayHelper.GetRawPtr(LastIndex), MapHelper.GetValuePtr(I));
				--Size;
			}
		}
	}
}
#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region ContainsValue
DEFINE_FUNCTION(UMapExtensionsLibrary::execMap_ContainsValue)
{
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FMapProperty>(nullptr);
	const void* MapAddr = Stack.MostRecentPropertyAddress;
	const FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);
	if (!MapProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}

	const FProperty* CurrValueProp = MapProperty->ValueProp;
	const int32 ValuePropertySize = XTOOLS_GET_ELEMENT_SIZE(CurrValueProp) * CurrValueProp->ArrayDim;
	void* ValueStorageSpace = FMemory_Alloca(ValuePropertySize);
	CurrValueProp->InitializeValue(ValueStorageSpace);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FProperty>(ValueStorageSpace);

	const FFieldClass* CurrValuePropClass = CurrValueProp->GetClass();
	const FFieldClass* MostRecentPropClass = Stack.MostRecentProperty->GetClass();
	void* ValuePtr;
	// If the destination and the inner type are identical in size and their field classes derive from one another, then permit the writing out of the array element to the destination memory
	if (Stack.MostRecentPropertyAddress != nullptr && (ValuePropertySize == XTOOLS_GET_ELEMENT_SIZE(Stack.MostRecentProperty)*Stack.MostRecentProperty->ArrayDim) &&
		(MostRecentPropClass->IsChildOf(CurrValuePropClass) || CurrValuePropClass->IsChildOf(MostRecentPropClass)))
	{
		ValuePtr = Stack.MostRecentPropertyAddress;
	}
	else
	{
		ValuePtr = ValueStorageSpace;
	}

	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)RESULT_PARAM = GenericMap_FindValue(MapAddr, MapProperty, CurrValueProp, ValuePtr);
	P_NATIVE_END;

	CurrValueProp->DestroyValue(ValueStorageSpace);
}

bool UMapExtensionsLibrary::GenericMap_FindValue(const void* TargetMap, const FMapProperty* MapProperty, const FProperty* ValueProperty, const void* ValuePtr)
{
	if(TargetMap)
	{
		FScriptMapHelper MapHelper(MapProperty, TargetMap);
		for(int32 i = 0; i < MapHelper.Num(); i++)
		{
			const void* MapValuePtr = MapHelper.GetValuePtr(MapHelper.FindInternalIndex(i));
			if(!MapValuePtr) return false;
			if(ValueProperty->Identical(ValuePtr, MapValuePtr, PPF_None)) return true;
		}
	}
	
	return false;
}
#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region GetKeysFromValue
DEFINE_FUNCTION(UMapExtensionsLibrary::execMap_KeysFromValue)
{
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FMapProperty>(nullptr);
	const void* MapAddr = Stack.MostRecentPropertyAddress;
	const FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);
	if (!MapProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}

	const FProperty* CurrValueProp = MapProperty->ValueProp;
	const int32 ValuePropertySize = XTOOLS_GET_ELEMENT_SIZE(CurrValueProp) * CurrValueProp->ArrayDim;
	void* ValueStorageSpace = FMemory_Alloca(ValuePropertySize);
	CurrValueProp->InitializeValue(ValueStorageSpace);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FProperty>(ValueStorageSpace);

	const FFieldClass* CurrValuePropClass = CurrValueProp->GetClass();
	const FFieldClass* MostRecentPropClass = Stack.MostRecentProperty->GetClass();
	void* ValuePtr;
	// If the destination and the inner type are identical in size and their field classes derive from one another, then permit the writing out of the array element to the destination memory
	if (Stack.MostRecentPropertyAddress != nullptr && (ValuePropertySize == XTOOLS_GET_ELEMENT_SIZE(Stack.MostRecentProperty)*Stack.MostRecentProperty->ArrayDim) &&
		(MostRecentPropClass->IsChildOf(CurrValuePropClass) || CurrValuePropClass->IsChildOf(MostRecentPropClass)))
	{
		ValuePtr = Stack.MostRecentPropertyAddress;
	}
	else
	{
		ValuePtr = ValueStorageSpace;
	}
	
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	const void* ArrayAddr = Stack.MostRecentPropertyAddress;
	const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
	if (!ArrayProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}
	
	P_FINISH;
	P_NATIVE_BEGIN;
	GenericMap_KeysFromValue(MapAddr, MapProperty, ArrayAddr, ArrayProperty, CurrValueProp, ValuePtr);
	P_NATIVE_END

	CurrValueProp->DestroyValue(ValueStorageSpace);
}

void UMapExtensionsLibrary::GenericMap_KeysFromValue(const void* MapAddr, const FMapProperty* MapProperty, const void* ArrayAddr, const FArrayProperty* ArrayProperty,  const FProperty* ValueProperty, const void* ValuePtr)
{
	if(MapAddr)
	{
		FScriptMapHelper MapHelper(MapProperty, MapAddr);
		FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayAddr);
		ArrayHelper.EmptyValues();

		const FProperty* InnerProp = ArrayProperty->Inner;
		
		for(int32 i = 0; i < MapHelper.Num(); i++)
		{
			const void* MapValuePtr = MapHelper.GetValuePtr(MapHelper.FindInternalIndex(i));
			if(!MapValuePtr) continue;
			
			if(ValueProperty->Identical(ValuePtr, MapValuePtr, PPF_None))
			{
				const int32 LastIndex = ArrayHelper.AddValue();
				InnerProp->CopySingleValueToScriptVM(ArrayHelper.GetRawPtr(LastIndex), MapHelper.GetKeyPtr(i));
			}
		}
	}
}
#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region RemoveEntries
DEFINE_FUNCTION(UMapExtensionsLibrary::execMap_RemoveEntries)
{
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FMapProperty>(nullptr);
	const void* MapAddr = Stack.MostRecentPropertyAddress;
	const FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);
	if (!MapProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}

	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	void* ArrayAddr = Stack.MostRecentPropertyAddress;
	const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
	if (!ArrayProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}
	
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)RESULT_PARAM = GenericMap_RemoveEntries(MapAddr, MapProperty, ArrayAddr, ArrayProperty);
	P_NATIVE_END

	ArrayProperty->DestroyValue(ArrayAddr);
}

bool UMapExtensionsLibrary::GenericMap_RemoveEntries(const void* MapAddr, const FMapProperty* MapProperty, const void* ArrayAddr, const FArrayProperty* ArrayProperty)
{
	if(MapAddr)
	{
		bool bResult = true;
		FScriptMapHelper MapHelper(MapProperty, MapAddr);
		FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayAddr);

		for(int32 i = 0; i < ArrayHelper.Num(); i++)
		{
			if(!MapHelper.RemovePair(ArrayHelper.GetRawPtr(i))) bResult = false;
		}

		return bResult;
	}
	
	return false;
}
#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region RemoveEntriesWithValue
DEFINE_FUNCTION(UMapExtensionsLibrary::execMap_RemoveEntriesWithValue)
{
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FMapProperty>(nullptr);
	const void* MapAddr = Stack.MostRecentPropertyAddress;
	const FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);
	if (!MapProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}

	const FProperty* CurrValueProp = MapProperty->ValueProp;
	const int32 ValuePropertySize = XTOOLS_GET_ELEMENT_SIZE(CurrValueProp) * CurrValueProp->ArrayDim;
	void* ValueStorageSpace = FMemory_Alloca(ValuePropertySize);
	CurrValueProp->InitializeValue(ValueStorageSpace);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FProperty>(ValueStorageSpace);

	const FFieldClass* CurrValuePropClass = CurrValueProp->GetClass();
	const FFieldClass* MostRecentPropClass = Stack.MostRecentProperty->GetClass();
	void* ValuePtr;
	// If the destination and the inner type are identical in size and their field classes derive from one another, then permit the writing out of the array element to the destination memory
	if (Stack.MostRecentPropertyAddress != nullptr && (ValuePropertySize == XTOOLS_GET_ELEMENT_SIZE(Stack.MostRecentProperty)*Stack.MostRecentProperty->ArrayDim) &&
		(MostRecentPropClass->IsChildOf(CurrValuePropClass) || CurrValuePropClass->IsChildOf(MostRecentPropClass)))
	{
		ValuePtr = Stack.MostRecentPropertyAddress;
	}
	else
	{
		ValuePtr = ValueStorageSpace;
	}
	
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)RESULT_PARAM = GenericMap_RemoveEntriesWithValue(MapAddr, MapProperty, CurrValueProp, ValuePtr);
	P_NATIVE_END

	CurrValueProp->DestroyValue(ValueStorageSpace);
}

bool UMapExtensionsLibrary::GenericMap_RemoveEntriesWithValue(const void* MapAddr, const FMapProperty* MapProperty, const FProperty* ValueProp, const void* ValuePtr)
{
	if (MapAddr)
	{
		bool bResult = false;
		FScriptMapHelper MapHelper(MapProperty, MapAddr);
		
		TArray<int32> IndicesToRemove;
		for (int32 Index = 0; Index < MapHelper.GetMaxIndex(); ++Index)
		{
			if (MapHelper.IsValidIndex(Index))
			{
				uint8* ValueData = MapHelper.GetValuePtr(Index);
				if (ValueProp->Identical(ValueData, ValuePtr))
				{
					IndicesToRemove.Add(Index);
					bResult = true;
				}
			}
		}

		for (int32 i = IndicesToRemove.Num() - 1; i >= 0; --i)
		{
			MapHelper.RemoveAt(IndicesToRemove[i]);
		}

		return bResult;
	}
	return false;
}
#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region SetValueAt
DEFINE_FUNCTION(UMapExtensionsLibrary::execMap_SetValueAt)
{
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FMapProperty>(nullptr);
	const void* MapAddr = Stack.MostRecentPropertyAddress;
	const FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);
	if (!MapProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}

	P_GET_PROPERTY(FIntProperty, Index);
	
	const FProperty* CurrValueProp = MapProperty->ValueProp;
	const int32 ValuePropertySize = XTOOLS_GET_ELEMENT_SIZE(CurrValueProp) * CurrValueProp->ArrayDim;
	void* ValueStorageSpace = FMemory_Alloca(ValuePropertySize);
	CurrValueProp->InitializeValue(ValueStorageSpace);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FProperty>(ValueStorageSpace);

	const FFieldClass* CurrValuePropClass = CurrValueProp->GetClass();
	const FFieldClass* MostRecentPropClass = Stack.MostRecentProperty->GetClass();
	void* ValuePtr;
	// If the destination and the inner type are identical in size and their field classes derive from one another, then permit the writing out of the array element to the destination memory
	if (Stack.MostRecentPropertyAddress != nullptr && (ValuePropertySize == XTOOLS_GET_ELEMENT_SIZE(Stack.MostRecentProperty)*Stack.MostRecentProperty->ArrayDim) &&
		(MostRecentPropClass->IsChildOf(CurrValuePropClass) || CurrValuePropClass->IsChildOf(MostRecentPropClass)))
	{
		ValuePtr = Stack.MostRecentPropertyAddress;
	}
	else
	{
		ValuePtr = ValueStorageSpace;
	}
	
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*) RESULT_PARAM = GenericMap_SetValueAt(MapAddr, MapProperty, Index, ValuePtr);
	P_NATIVE_END

	CurrValueProp->DestroyValue(ValueStorageSpace);
}

bool UMapExtensionsLibrary::GenericMap_SetValueAt(const void* MapAddr, const FMapProperty* MapProperty, const int32 Index, const void* ValuePtr)
{
	if(MapAddr)
	{
		FScriptMapHelper MapHelper(MapProperty, MapAddr);
		
		if(Index < 0 || Index > MapHelper.Num()-1) return false;
		
		const FProperty* KeyProperty = MapProperty->KeyProp;
		const int32 KeyPropertySize = XTOOLS_GET_ELEMENT_SIZE(KeyProperty) * KeyProperty->ArrayDim;
		void* KeyStorageSpace = FMemory_Alloca(KeyPropertySize);
		KeyProperty->InitializeValue(KeyStorageSpace);
		
		MapHelper.KeyProp->CopyCompleteValueFromScriptVM(KeyStorageSpace, MapHelper.GetKeyPtr(MapHelper.FindInternalIndex(Index)));
		
		MapHelper.AddPair(KeyStorageSpace, ValuePtr);
		return true;
	}
	
	return false;
}
#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region RandomMapItem
DEFINE_FUNCTION(UMapExtensionsLibrary::execMap_RandomItem)
{
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FMapProperty>(nullptr);
	const void* MapAddr = Stack.MostRecentPropertyAddress;
	const FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);
	if (!MapProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}
	
	// Get key out value
	FProperty* KeyProp = MapProperty->KeyProp;
	const int32 KeyPropertySize = XTOOLS_GET_ELEMENT_SIZE(KeyProp) * KeyProp->ArrayDim;
	void* KeyStorageSpace = FMemory_Alloca(KeyPropertySize);
	KeyProp->InitializeValue(KeyStorageSpace);
	
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FProperty>(KeyStorageSpace);
	
	const FFieldClass* KeyPropClass = KeyProp->GetClass();
	const FFieldClass* KeyMostRecentPropClass = Stack.MostRecentProperty->GetClass();
	void* KeyPtr;
	if (Stack.MostRecentPropertyAddress != nullptr &&
		KeyPropertySize == XTOOLS_GET_ELEMENT_SIZE(Stack.MostRecentProperty)*Stack.MostRecentProperty->ArrayDim &&
		(KeyMostRecentPropClass->IsChildOf(KeyPropClass) || KeyPropClass->IsChildOf(KeyMostRecentPropClass)))
	{
		KeyPtr = Stack.MostRecentPropertyAddress;
	}
	else
	{
		KeyPtr = KeyStorageSpace;
	}
	
	// Get value out value
	FProperty* ValueProp = MapProperty->ValueProp;
	const int32 ValuePropertySize = XTOOLS_GET_ELEMENT_SIZE(ValueProp) * ValueProp->ArrayDim;
	void* ValueStorageSpace = FMemory_Alloca(ValuePropertySize);
	ValueProp->InitializeValue(ValueStorageSpace);
	
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FProperty>(ValueStorageSpace);
	
	const FFieldClass* ValuePropClass = ValueProp->GetClass();
	const FFieldClass* ValueMostRecentPropClass = Stack.MostRecentProperty->GetClass();
	
	void* ValuePtr;
	if (Stack.MostRecentPropertyAddress != nullptr &&
		ValuePropertySize == XTOOLS_GET_ELEMENT_SIZE(Stack.MostRecentProperty)*Stack.MostRecentProperty->ArrayDim &&
		(ValueMostRecentPropClass->IsChildOf(ValuePropClass) || ValuePropClass->IsChildOf(ValueMostRecentPropClass)))
	{
		ValuePtr = Stack.MostRecentPropertyAddress;
	}
	else
	{
		ValuePtr = ValueStorageSpace;
	}

	P_FINISH;
	P_NATIVE_BEGIN;
	GenericMap_RandomItem(MapAddr, MapProperty, KeyPtr, ValuePtr);
	P_NATIVE_END
	
	KeyProp->DestroyValue(KeyStorageSpace);
	ValueProp->DestroyValue(ValueStorageSpace);
}

void UMapExtensionsLibrary::GenericMap_RandomItem(const void* MapAddr, const FMapProperty* MapProperty, void* OutKeyPtr, void* OutValuePtr)
{
	if(MapAddr)
	{
		FScriptMapHelper MapHelper(MapProperty, MapAddr);
		const int32 Index = FMath::RandRange(0, MapHelper.Num() - 1);
		
		MapHelper.KeyProp->CopyCompleteValueFromScriptVM(OutKeyPtr, MapHelper.GetKeyPtr(MapHelper.FindInternalIndex(Index)));
		MapHelper.ValueProp->CopyCompleteValueFromScriptVM(OutValuePtr, MapHelper.GetValuePtr(MapHelper.FindInternalIndex(Index)));
	}
}
#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region RandomMapItemFromStream
DEFINE_FUNCTION(UMapExtensionsLibrary::execMap_RandomItemFromStream)
{
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FMapProperty>(nullptr);
	const void* MapAddr = Stack.MostRecentPropertyAddress;
	const FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);
	if (!MapProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}
	
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	FRandomStream* RandomStream = (FRandomStream*)Stack.MostRecentPropertyAddress;
	
	// Get key out value
	FProperty* KeyProp = MapProperty->KeyProp;
	const int32 KeyPropertySize = XTOOLS_GET_ELEMENT_SIZE(KeyProp) * KeyProp->ArrayDim;
	void* KeyStorageSpace = FMemory_Alloca(KeyPropertySize);
	KeyProp->InitializeValue(KeyStorageSpace);
	
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FProperty>(KeyStorageSpace);
	
	const FFieldClass* KeyPropClass = KeyProp->GetClass();
	const FFieldClass* KeyMostRecentPropClass = Stack.MostRecentProperty->GetClass();
	void* KeyPtr;
	if (Stack.MostRecentPropertyAddress != nullptr &&
		KeyPropertySize == XTOOLS_GET_ELEMENT_SIZE(Stack.MostRecentProperty)*Stack.MostRecentProperty->ArrayDim &&
		(KeyMostRecentPropClass->IsChildOf(KeyPropClass) || KeyPropClass->IsChildOf(KeyMostRecentPropClass)))
	{
		KeyPtr = Stack.MostRecentPropertyAddress;
	}
	else
	{
		KeyPtr = KeyStorageSpace;
	}
	
	// Get value out value
	FProperty* ValueProp = MapProperty->ValueProp;
	const int32 ValuePropertySize = XTOOLS_GET_ELEMENT_SIZE(ValueProp) * ValueProp->ArrayDim;
	void* ValueStorageSpace = FMemory_Alloca(ValuePropertySize);
	ValueProp->InitializeValue(ValueStorageSpace);
	
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FProperty>(ValueStorageSpace);
	
	const FFieldClass* ValuePropClass = ValueProp->GetClass();
	const FFieldClass* ValueMostRecentPropClass = Stack.MostRecentProperty->GetClass();
	
	void* ValuePtr;
	if (Stack.MostRecentPropertyAddress != nullptr &&
		ValuePropertySize == XTOOLS_GET_ELEMENT_SIZE(Stack.MostRecentProperty)*Stack.MostRecentProperty->ArrayDim &&
		(ValueMostRecentPropClass->IsChildOf(ValuePropClass) || ValuePropClass->IsChildOf(ValueMostRecentPropClass)))
	{
		ValuePtr = Stack.MostRecentPropertyAddress;
	}
	else
	{
		ValuePtr = ValueStorageSpace;
	}

	P_FINISH;
	P_NATIVE_BEGIN;
	GenericMap_RandomItemFromStream(MapAddr, MapProperty, RandomStream, KeyPtr, ValuePtr);
	P_NATIVE_END
	
	KeyProp->DestroyValue(KeyStorageSpace);
	ValueProp->DestroyValue(ValueStorageSpace);
}

void UMapExtensionsLibrary::GenericMap_RandomItemFromStream(const void* MapAddr, const FMapProperty* MapProperty, FRandomStream* RandomStream, void* OutKeyPtr, void* OutValuePtr)
{
	if(MapAddr)
	{
		FScriptMapHelper MapHelper(MapProperty, MapAddr);
		const int32 Index = RandomStream->RandRange(0, MapHelper.Num() - 1);
		
		MapHelper.KeyProp->CopyCompleteValueFromScriptVM(OutKeyPtr, MapHelper.GetKeyPtr(MapHelper.FindInternalIndex(Index)));
		MapHelper.ValueProp->CopyCompleteValueFromScriptVM(OutValuePtr, MapHelper.GetValuePtr(MapHelper.FindInternalIndex(Index)));
	}
}
#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region MapIdentical
DEFINE_FUNCTION(UMapExtensionsLibrary::execMap_Identical)
{
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FMapProperty>(nullptr);
	const void* MapAddr= Stack.MostRecentPropertyAddress;
	const FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);
	if (!MapProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}

	// Retrieve the first array
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FArrayProperty>(NULL);
	void* ArrayKeysAddr = Stack.MostRecentPropertyAddress;
	FArrayProperty* ArrayKeysProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
	if (!ArrayKeysProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}

	// Retrieve the second array
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FArrayProperty>(NULL);
	void* ArrayValuesAddr = Stack.MostRecentPropertyAddress;
	FArrayProperty* ArrayValuesProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
	if (!ArrayValuesProperty)
	{
		Stack.bArrayContextFailed = true;
		return;
	}
	
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)RESULT_PARAM = GenericMap_Identical(MapAddr, MapProperty, ArrayKeysAddr, ArrayKeysProperty, ArrayValuesAddr, ArrayValuesProperty);
	P_NATIVE_END;
}

bool UMapExtensionsLibrary::GenericMap_Identical(const void* MapAddr, const FMapProperty* MapProperty, const void* KeysBAddr, const FArrayProperty* KeysBProp, const void* ValuesBAddr, const FArrayProperty* ValuesBProp)
{
	if(MapAddr && KeysBAddr && ValuesBAddr)
	{
		FScriptMapHelper MapHelper(MapProperty, MapAddr);
		FScriptArrayHelper KeysBHelper(KeysBProp, KeysBAddr);
		FScriptArrayHelper ValuesBHelper(ValuesBProp, ValuesBAddr);

		if(MapHelper.Num() == KeysBHelper.Num() && MapHelper.Num() == ValuesBHelper.Num())
		{
			for(int32 MapIndex = 0; MapIndex < MapHelper.Num(); MapIndex++)
			{
				bool bFound = false;
				const FProperty* KeyProperty = MapHelper.GetKeyProperty();
				const FProperty* ValueProperty = MapHelper.GetValueProperty();
				const int32 InternalIndex = MapHelper.FindInternalIndex(MapIndex);
				for(int32 KeyIndex = 0; KeyIndex < KeysBHelper.Num(); KeyIndex++)
				{
					if(KeyProperty->Identical(MapHelper.GetKeyPtr(InternalIndex), KeysBHelper.GetRawPtr(KeyIndex)) &&
						ValueProperty->Identical(MapHelper.GetValuePtr(InternalIndex), ValuesBHelper.GetRawPtr(KeyIndex)))
					{
						bFound = true;
					}
				}

				if(!bFound) return false;
			}
			
			return true;
		}
	}
	
	return false;
}
#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region ArrayItem

    void UMapExtensionsLibrary::GenericMap_AddArrayItem(const void* TargetMap, const FMapProperty* MapProperty, const void* KeyPtr, const void* ValuePtr)
    {
        if (!TargetMap)
        {
            return;
        }

        FScriptMapHelper MapHelper(MapProperty, TargetMap);
        
        // 获取Map的Value类型（应该是个Struct）
        const FStructProperty* StructProp = CastField<FStructProperty>(MapProperty->ValueProp);
        if (!StructProp)
        {
            return;
        }

        // 检查Map中是否已存在此Key
        void* ExistingValuePtr = MapHelper.FindValueFromHash(KeyPtr);
        
        if (ExistingValuePtr)
        {
            // 情况2：Map中已存在此Key
            // 获取结构体中的Array属性
            FArrayProperty* ArrayProp = nullptr;
            for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
            {
                if (FArrayProperty* Prop = CastField<FArrayProperty>(*PropIt))
                {
                    ArrayProp = Prop;
                    break;
                }
            }

            if (ArrayProp)
            {
                // 获取已存在结构体中Array成员的指针
                void* ExistingArrayPtr = static_cast<uint8*>(ExistingValuePtr) + ArrayProp->GetOffset_ForInternal();
                
                // 创建Array助手
                FScriptArrayHelper ArrayHelper(ArrayProp, ExistingArrayPtr);
                
                // 检查是否已存在相同的值
                bool bValueExists = false;
                for (int32 i = 0; i < ArrayHelper.Num(); ++i)
                {
                    if (ArrayProp->Inner->Identical(ArrayHelper.GetRawPtr(i), ValuePtr))
                    {
                        bValueExists = true;
                        break;
                    }
                }
                
                // 如果值不存在，则添加
                if (!bValueExists)
                {
                    const int32 NewIndex = ArrayHelper.AddValue();
                    ArrayProp->Inner->CopyCompleteValue(ArrayHelper.GetRawPtr(NewIndex), ValuePtr);
                }
            }
        }
        else
        {
            // 情况1：Map中没有此Key
            if (MapHelper.Num() >= MaxSupportedMapSize)
            {
                FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Attempted add to map '%s' beyond the maximum supported capacity!"), 
                    *MapProperty->GetName()), ELogVerbosity::Warning, 
                    UKismetArrayLibrary::ReachedMaximumContainerSizeWarning);
                return;
            }

            // 创建临时的结构体空间
            void* ValueStructure = FMemory::Malloc(StructProp->GetSize(), StructProp->GetMinAlignment());
            StructProp->InitializeValue(ValueStructure);

            // 获取结构体中的Array属性
            FArrayProperty* ArrayProp = nullptr;
            for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
            {
                if (FArrayProperty* Prop = CastField<FArrayProperty>(*PropIt))
                {
                    ArrayProp = Prop;
                    break;
                }
            }

            if (ArrayProp)
            {
                // 获取Array在结构体中的偏移
                void* ArrayPtr = static_cast<uint8*>(ValueStructure) + ArrayProp->GetOffset_ForInternal();
                
                // 初始化Array并添加ValuePtr指向的元素
                FScriptArrayHelper ArrayHelper(ArrayProp, ArrayPtr);
                const int32 NewIndex = ArrayHelper.AddValue();
                ArrayProp->Inner->CopyCompleteValue(ArrayHelper.GetRawPtr(NewIndex), ValuePtr);

                // 将构建好的结构体添加到Map中
                MapHelper.AddPair(KeyPtr, ValueStructure);
            }

            // 清理临时结构体
            StructProp->DestroyValue(ValueStructure);
            FMemory::Free(ValueStructure);
        }
    }

    void UMapExtensionsLibrary::GenericMap_RemoveArrayItem(const void* TargetMap, const FMapProperty* MapProperty, const void* KeyPtr, const void* ValuePtr)
    {
        if (!TargetMap)
        {
            return;
        }

        FScriptMapHelper MapHelper(MapProperty, TargetMap);
        
        // 获取Map的Value类型（应该是个Struct）
        const FStructProperty* StructProp = CastField<FStructProperty>(MapProperty->ValueProp);
        if (!StructProp)
        {
            return;
        }

        // 检查Map中是否已存在此Key
        void* ExistingValuePtr = MapHelper.FindValueFromHash(KeyPtr);
        
        if (ExistingValuePtr)
        {
            // 情况2：Map中已存在此Key
            // 获取结构体中的Array属性
            FArrayProperty* ArrayProp = nullptr;
            for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
            {
                if (FArrayProperty* Prop = CastField<FArrayProperty>(*PropIt))
                {
                    ArrayProp = Prop;
                    break;
                }
            }

            if (ArrayProp)
            {
                // 获取已存在结构体中Array成员的指针
                void* ExistingArrayPtr = static_cast<uint8*>(ExistingValuePtr) + ArrayProp->GetOffset_ForInternal();

                // 创建Array助手
                FScriptArrayHelper ArrayHelper(ArrayProp, ExistingArrayPtr);

                // 查找并移除匹配的元素
                for (int32 i = ArrayHelper.Num() - 1; i >= 0; --i)
                {
                    if (ArrayProp->Inner->Identical(ArrayHelper.GetRawPtr(i), ValuePtr))
                    {
                        ArrayHelper.RemoveValues(i, 1);
                    }
                }
            }
        }
    }

#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region SetItem

    void UMapExtensionsLibrary::GenericMap_AddSetItem(const void* TargetMap, const FMapProperty* MapProperty, const void* KeyPtr, const void* ValuePtr)
    {
        if (!TargetMap)
        {
            return;
        }

        FScriptMapHelper MapHelper(MapProperty, TargetMap);
        
        // 获取Map的Value类型（应该是个Struct）
        const FStructProperty* StructProp = CastField<FStructProperty>(MapProperty->ValueProp);
        if (!StructProp)
        {
            return;
        }

        // 检查Map中是否已存在此Key
        void* ExistingValuePtr = MapHelper.FindValueFromHash(KeyPtr);
        
        if (ExistingValuePtr)
        {
            // 情况2：Map中已存在此Key
            // 获取结构体中的Set属性
            FSetProperty* SetProp = nullptr;
            for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
            {
                if (FSetProperty* Prop = CastField<FSetProperty>(*PropIt))
                {
                    SetProp = Prop;
                    break;
                }
            }

            if (SetProp)
            {
                // 获取已存在结构体中Set成员的指针
                void* ExistingSetPtr = static_cast<uint8*>(ExistingValuePtr) + SetProp->GetOffset_ForInternal();
        
                // 创建Set助手
                FScriptSetHelper SetHelper(SetProp, ExistingSetPtr);
        
                // 直接添加到Set中(Set会自动处理重复元素)
                SetHelper.AddElement(ValuePtr);
            }
        }
        else
        {
            // 情况1：Map中没有此Key
            if (MapHelper.Num() >= MaxSupportedMapSize)
            {
                FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Attempted add to map '%s' beyond the maximum supported capacity!"), 
                    *MapProperty->GetName()), ELogVerbosity::Warning, 
                    UKismetArrayLibrary::ReachedMaximumContainerSizeWarning);
                return;
            }

            // 创建临时的结构体空间
            void* ValueStructure = FMemory::Malloc(StructProp->GetSize(), StructProp->GetMinAlignment());
            StructProp->InitializeValue(ValueStructure);

            // 获取结构体中的Set属性
            FSetProperty* SetProp = nullptr;
            for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
            {
                if (FSetProperty* Prop = CastField<FSetProperty>(*PropIt))
                {
                    SetProp = Prop;
                    break;
                }
            }

            if (SetProp)
            {
                // 获取Set在结构体中的偏移
                void* SetPtr = static_cast<uint8*>(ValueStructure) + SetProp->GetOffset_ForInternal();
        
                // 初始化Set并添加元素
                FScriptSetHelper SetHelper(SetProp, SetPtr);
                SetHelper.AddElement(ValuePtr);

                // 将构建好的结构体添加到Map中
                MapHelper.AddPair(KeyPtr, ValueStructure);
            }

            // 清理临时结构体
            StructProp->DestroyValue(ValueStructure);
            FMemory::Free(ValueStructure);
        }
    }

    void UMapExtensionsLibrary::GenericMap_RemoveSetItem(const void* TargetMap, const FMapProperty* MapProperty, const void* KeyPtr, const void* ValuePtr)
    {
        if (!TargetMap)
        {
            return;
        }

        FScriptMapHelper MapHelper(MapProperty, TargetMap);
        
        // 获取Map的Value类型（应该是个Struct）
        const FStructProperty* StructProp = CastField<FStructProperty>(MapProperty->ValueProp);
        if (!StructProp)
        {
            return;
        }

        // 检查Map中是否已存在此Key
        void* ExistingValuePtr = MapHelper.FindValueFromHash(KeyPtr);
        
        if (ExistingValuePtr)
        {
            // 情况2：Map中已存在此Key
            // 获取结构体中的Set属性
            FSetProperty* SetProp = nullptr;
            for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
            {
                if (FSetProperty* Prop = CastField<FSetProperty>(*PropIt))
                {
                    SetProp = Prop;
                    break;
                }
            }

            if (SetProp)
            {
                // 获取已存在结构体中Set成员的指针
                void* ExistingSetPtr = static_cast<uint8*>(ExistingValuePtr) + SetProp->GetOffset_ForInternal();

                // 创建Set助手
                FScriptSetHelper SetHelper(SetProp, ExistingSetPtr);

                // 从Set中移除元素
                SetHelper.RemoveElement(ValuePtr);
            }
        }
    }

#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region MapItem

    void UMapExtensionsLibrary::GenericMap_AddMapItem(const void* TargetMap, const FMapProperty* MapProperty, const void* KeyPtr, const void* SubKeyPtr, const void* ValuePtr)
    {
        // 1. 增加全面的参数检查
        if (!TargetMap || !MapProperty || !KeyPtr || !SubKeyPtr || !ValuePtr)
        {
            FXToolsErrorReporter::Warning(LogBlueprintExtensionsRuntime, TEXT("GenericMap_AddMapItem: 无效的参数"), TEXT("GenericMap_AddMapItem"));
            return;
        }

        // 2. 确保Map属性有效
        if (!MapProperty->IsValidLowLevel())
        {
            FXToolsErrorReporter::Warning(LogBlueprintExtensionsRuntime, TEXT("GenericMap_AddMapItem: MapProperty无效"), TEXT("GenericMap_AddMapItem"));
            return;
        }
        
        FScriptMapHelper MapHelper(MapProperty, TargetMap);
        
        // 4. 获取并检查Value类型
        const FStructProperty* StructProp = CastField<FStructProperty>(MapProperty->ValueProp);
        if (!StructProp || !StructProp->IsValidLowLevel())
        {
            FXToolsErrorReporter::Warning(LogBlueprintExtensionsRuntime, TEXT("GenericMap_AddMapItem: Value类型无效"), TEXT("GenericMap_AddMapItem"));
            return;
        }

        // 5. 使用RAII方式管理临时结构体
        struct FScopedValueStructure
        {
            void* ValuePtr;
            const FStructProperty* Prop;

            FScopedValueStructure(const FStructProperty* InProp)
                : ValuePtr(nullptr)
                , Prop(InProp)
            {
                if (Prop)
                {
                    ValuePtr = FMemory::Malloc(Prop->GetSize(), Prop->GetMinAlignment());
                    if (ValuePtr)
                    {
                        Prop->InitializeValue(ValuePtr);
                    }
                }
            }

            ~FScopedValueStructure()
            {
                if (ValuePtr && Prop)
                {
                    Prop->DestroyValue(ValuePtr);
                    FMemory::Free(ValuePtr);
                }
            }

            void* Get() const { return ValuePtr; }
        };

        // 6. 检查现有值
        void* ExistingValuePtr = MapHelper.FindValueFromHash(KeyPtr);
        
        if (ExistingValuePtr)
        {
            // 处理已存在的Key
            FMapProperty* InnerMapProp = nullptr;
            for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
            {
                InnerMapProp = CastField<FMapProperty>(*PropIt);
                if (InnerMapProp && InnerMapProp->IsValidLowLevel())
                {
                    break;
                }
            }

            if (!InnerMapProp)
            {
                FXToolsErrorReporter::Warning(LogBlueprintExtensionsRuntime, TEXT("GenericMap_AddMapItem: 找不到内部Map属性"), TEXT("GenericMap_AddMapItem"));
                return;
            }

            void* ExistingMapPtr = static_cast<uint8*>(ExistingValuePtr) + InnerMapProp->GetOffset_ForInternal();
            if (!ExistingMapPtr)
            {
                FXToolsErrorReporter::Warning(LogBlueprintExtensionsRuntime, TEXT("GenericMap_AddMapItem: 无效的内部Map指针"), TEXT("GenericMap_AddMapItem"));
                return;
            }

            FScriptMapHelper InnerMapHelper(InnerMapProp, ExistingMapPtr);
            if (InnerMapHelper.Num() < MaxSupportedMapSize)
            {
                InnerMapHelper.AddPair(SubKeyPtr, ValuePtr);
            }
        }
        else
        {
            // 处理新Key
            if (MapHelper.Num() >= MaxSupportedMapSize)
            {
                FXToolsErrorReporter::Warning(LogBlueprintExtensionsRuntime, TEXT("GenericMap_AddMapItem: 超出最大容量"), TEXT("GenericMap_AddMapItem"));
                return;
            }

            FScopedValueStructure ScopedValue(StructProp);
            if (!ScopedValue.Get())
            {
                FXToolsErrorReporter::Warning(LogBlueprintExtensionsRuntime, TEXT("GenericMap_AddMapItem: 无法创建临时结构体"), TEXT("GenericMap_AddMapItem"));
                return;
            }

            FMapProperty* InnerMapProp = nullptr;
            for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
            {
                InnerMapProp = CastField<FMapProperty>(*PropIt);
                if (InnerMapProp && InnerMapProp->IsValidLowLevel())
                {
                    break;
                }
            }

            if (!InnerMapProp)
            {
                FXToolsErrorReporter::Warning(LogBlueprintExtensionsRuntime, TEXT("GenericMap_AddMapItem: 找不到内部Map属性"), TEXT("GenericMap_AddMapItem"));
                return;
            }

            void* InnerMapPtr = static_cast<uint8*>(ScopedValue.Get()) + InnerMapProp->GetOffset_ForInternal();
            if (!InnerMapPtr)
            {
                FXToolsErrorReporter::Warning(LogBlueprintExtensionsRuntime, TEXT("GenericMap_AddMapItem: 无效的内部Map指针"), TEXT("GenericMap_AddMapItem"));
                return;
            }

            FScriptMapHelper InnerMapHelper(InnerMapProp, InnerMapPtr);
            InnerMapHelper.AddPair(SubKeyPtr, ValuePtr);

            MapHelper.AddPair(KeyPtr, ScopedValue.Get());
        }
    }

    void UMapExtensionsLibrary::GenericMap_RemoveMapItem(const void* TargetMap, const FMapProperty* MapProperty, const void* KeyPtr, const void* SubKeyPtr)
    {
        // 1. 增加全面的参数检查
        if (!TargetMap || !MapProperty || !KeyPtr || !SubKeyPtr)
        {
            FXToolsErrorReporter::Warning(LogBlueprintExtensionsRuntime, TEXT("GenericMap_AddMapItem: 无效的参数"), TEXT("GenericMap_AddMapItem"));
            return;
        }

        // 2. 确保Map属性有效
        if (!MapProperty->IsValidLowLevel())
        {
            FXToolsErrorReporter::Warning(LogBlueprintExtensionsRuntime, TEXT("GenericMap_AddMapItem: MapProperty无效"), TEXT("GenericMap_AddMapItem"));
            return;
        }
        
        FScriptMapHelper MapHelper(MapProperty, TargetMap);
        
        // 4. 获取并检查Value类型
        const FStructProperty* StructProp = CastField<FStructProperty>(MapProperty->ValueProp);
        if (!StructProp || !StructProp->IsValidLowLevel())
        {
            FXToolsErrorReporter::Warning(LogBlueprintExtensionsRuntime, TEXT("GenericMap_AddMapItem: Value类型无效"), TEXT("GenericMap_AddMapItem"));
            return;
        }
        
        // 6. 检查现有值
        void* ExistingValuePtr = MapHelper.FindValueFromHash(KeyPtr);
        
        if (ExistingValuePtr)
        {
            // 处理已存在的Key
            FMapProperty* InnerMapProp = nullptr;
            for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
            {
                InnerMapProp = CastField<FMapProperty>(*PropIt);
                if (InnerMapProp && InnerMapProp->IsValidLowLevel())
                {
                    break;
                }
            }

            if (!InnerMapProp)
            {
                FXToolsErrorReporter::Warning(LogBlueprintExtensionsRuntime, TEXT("GenericMap_RemoveMapItem: 找不到内部Map属性"), TEXT("GenericMap_RemoveMapItem"));
                return;
            }

            void* ExistingMapPtr = static_cast<uint8*>(ExistingValuePtr) + InnerMapProp->GetOffset_ForInternal();
            if (!ExistingMapPtr)
            {
                FXToolsErrorReporter::Warning(LogBlueprintExtensionsRuntime, TEXT("GenericMap_RemoveMapItem: 无效的内部Map指针"), TEXT("GenericMap_RemoveMapItem"));
                return;
            }

            FScriptMapHelper InnerMapHelper(InnerMapProp, ExistingMapPtr);
            InnerMapHelper.RemovePair(SubKeyPtr);
        }
    }

#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————