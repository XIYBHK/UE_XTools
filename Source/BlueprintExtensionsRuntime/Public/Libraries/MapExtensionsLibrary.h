#pragma once

#include "CoreMinimal.h"
#include "UObject/Script.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"
#include "UObject/ScriptMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "XToolsVersionCompat.h"
#include "MapExtensionsLibrary.generated.h"

UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API UMapExtensionsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
#pragma region GetKey
/** 
 * Outputs the key from the map at the specified index.
 *
 * @param	TargetMap		The map to get the value from.
 * @param	Key				The key at the index.
 */
UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "获取键", CompactNodeTitle = "GET KEY", MapParam = "TargetMap", MapKeyParam = "Key"), Category = "XTools|Blueprint Extensions|Map")
static bool Map_GetKey(const TMap<int32, int32>& TargetMap, int32 Index, int32& Key);

DECLARE_FUNCTION(execMap_GetKey);

static bool GenericMap_GetKey(const void* TargetMap, const FMapProperty* MapProperty, int32 Index, void* OutKeyPtr);
#pragma endregion
	
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
#pragma region GetValue
	/** 
	 * Outputs the value from the map at the specified index.
	 *
	 * @param	TargetMap		The map to get the value from.
	 * @param	Value			The value at the index.
	 */
	UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "获取值", CompactNodeTitle = "GET VALUE", MapParam = "TargetMap", MapValueParam = "Value"), Category = "XTools|Blueprint Extensions|Map")
	static bool Map_GetValue(const TMap<int32, int32>& TargetMap, int32 Index, int32& Value);

	DECLARE_FUNCTION(execMap_GetValue);

	static bool GenericMap_GetValue(const void* TargetMap, const FMapProperty* MapProperty, int32 Index, void* OutValuePtr);
#pragma endregion 

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
#pragma region GetKeys
	/** 
	 * Outputs an array of all keys present in the map
	 *
	 * @param	TargetMap		The map to get the list of keys from
	 * @param	Keys			All keys present in the map
	 */
	UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "获取所有键", CompactNodeTitle = "GET KEYS", MapParam = "TargetMap", MapKeyParam = "Keys", AutoCreateRefTerm = "Keys"), Category = "XTools|Blueprint Extensions|Map")
	static void Map_Keys(const TMap<int32, int32>& TargetMap, TArray<int32>& Keys);
	
	DECLARE_FUNCTION(execMap_Keys);
	
	static void GenericMap_Keys(const void* MapAddr, const FMapProperty* MapProperty, const void* ArrayAddr, const FArrayProperty* ArrayProperty);
#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
#pragma region GetValues
	/** 
	 * Outputs an array of all values present in the map.
	 *
	 * @param	TargetMap		The map to get the list of values from.
	 * @param	Values			All values present in the map.
	 */
	UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "获取所有值", CompactNodeTitle = "GET VALUES", MapParam = "TargetMap", MapValueParam = "Values", AutoCreateRefTerm = "Values"), Category = "XTools|Blueprint Extensions|Map")
	static void Map_Values(const TMap<int32, int32>& TargetMap, TArray<int32>& Values);
	
	DECLARE_FUNCTION(execMap_Values);
	
	static void GenericMap_Values(const void* MapAddr, const FMapProperty* MapProperty, const void* ArrayAddr, const FArrayProperty* ArrayProperty);
#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
#pragma region ContainsValue
	/** 
	 * Checks whether a value is in the target map.
	 *
	 * @param	TargetMap		The map to perform the lookup on.
	 * @param	Value			The value that will be used to lookup.
	 * @return	True if the value was found otherwise false.
	 */
	UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "包含值", CompactNodeTitle = "CONTAINS VALUE", MapParam = "TargetMap", MapValueParam = "Value", AutoCreateRefTerm = "Value", BlueprintThreadSafe), Category = "XTools|Blueprint Extensions|Map")
	static bool Map_ContainsValue(const TMap<int32, int32>& TargetMap, const int32& Value);
	
	DECLARE_FUNCTION(execMap_ContainsValue);
	
	static bool GenericMap_FindValue(const void* TargetMap, const FMapProperty* MapProperty, const FProperty* ValueProperty, const void* ValuePtr);
#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
#pragma region GetKeysFromValue
	/** 
	 * Outputs an array of all keys that match a value.
	 *
	 * @param	TargetMap		The map to get the list of keys from.
	 * @param	Keys			All keys that match the value provided.
	 */
	UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "根据值获取键", CompactNodeTitle = "GET KEYS FROM VALUE", MapParam = "TargetMap", MapValueParam = "Value", MapKeyParam = "Keys", AutoCreateRefTerm = "Keys"), Category = "XTools|Blueprint Extensions|Map")
	static void Map_KeysFromValue(const TMap<int32, int32>& TargetMap, int32 Value, TArray<int32>& Keys);
	
	DECLARE_FUNCTION(execMap_KeysFromValue);
	
	static void GenericMap_KeysFromValue(const void* MapAddr, const FMapProperty* MapProperty, const void* ArrayAddr, const FArrayProperty* ArrayProperty,  const FProperty* ValueProperty, const void* ValuePtr);
#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
#pragma region RemoveEntries
	/** 
	 * Removes an array of keys and its associated values from the map.
	 *
	 * @param	TargetMap		The map to remove the keys and its associated values from.
	 * @param	Keys			The keys that will be used to match the entries to remove.
	 * @return	True if all the items were removed otherwise false.
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "移除多个条目", CompactNodeTitle = "REMOVE ENTRIES", MapParam = "TargetMap", MapKeyParam = "Keys",  AutoCreateRefTerm = "Keys"), Category = "XTools|Blueprint Extensions|Map")
	static bool Map_RemoveEntries(const TMap<int32, int32>& TargetMap, const TArray<int32>& Keys);

	DECLARE_FUNCTION(execMap_RemoveEntries);

	static bool GenericMap_RemoveEntries(const void* MapAddr, const FMapProperty* MapProperty, const void* ArrayAddr, const FArrayProperty* ArrayProperty);
#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
#pragma region RemoveEntriesWithValue
	/** 
	 * Removes the entries that are associated with the value specified.
	 *
	 * @param	TargetMap		The map to remove the entries and its associated values from.
	 * @param	Value			The value that will be used to look the keys up.
	 * @return	True if at least one item was removed otherwise false.
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "根据值移除条目", CompactNodeTitle = "REMOVE ENTRIES WITH VALUE", MapParam = "TargetMap", MapValueParam = "Value"), Category = "XTools|Blueprint Extensions|Map")
	static bool Map_RemoveEntriesWithValue(const TMap<int32, int32>& TargetMap, const int32 Value);

	DECLARE_FUNCTION(execMap_RemoveEntriesWithValue);

	static bool GenericMap_RemoveEntriesWithValue(const void* MapAddr, const FMapProperty* MapProperty, const FProperty* ValueProperty, const void* ValuePtr);
#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
#pragma region SetValueAt
	/**
	 * Attempts to update a value in the target map at the specified index.
	 * @param TargetMap The map to update the value of.
	 * @param Index The index at which to update the value.
	 * @param Value The new value
	 * @return True if successful otherwise false.
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "按索引设置值", CompactNodeTitle = "SET VALUE AT", MapParam = "TargetMap", MapValueParam = "Value"), Category = "XTools|Blueprint Extensions|Map")
	static bool Map_SetValueAt(const TMap<int32, int32>& TargetMap, const int32 Index, const int32 Value);

	DECLARE_FUNCTION(execMap_SetValueAt);

	static bool GenericMap_SetValueAt(const void* MapAddr, const FMapProperty* MapProperty, const int32 Index, const void* ValuePtr);
#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
#pragma region RandomMapItem
	/**
	 * Returns a random entry from the target map.
	 * @param TargetMap The map to get the random entry from.
	 * @param Key The random entry key.
	 * @param Value The random entry value.
	 */
	UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "随机Map元素", CompactNodeTitle = "RANDOM MAP ITEM", MapParam = "TargetMap", MapKeyParam = "Key", MapValueParam = "Value"), Category = "XTools|Blueprint Extensions|Map")
	static void Map_RandomItem(const TMap<int32, int32>& TargetMap, int32& Key, int32& Value);

	DECLARE_FUNCTION(execMap_RandomItem);

	static void GenericMap_RandomItem(const void* MapAddr, const FMapProperty* MapProperty, void* OutKeyPtr, void* OutValuePtr);
#pragma endregion
	
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
#pragma region RandomMapItemFromStream
	/**
	 * Returns a random entry from the target map using a stream.
	 * @param TargetMap The map to get the random entry from.
	 * @param RandomStream The random stream.
	 * @param Key The random entry key.
	 * @param Value The random entry value.
	 */
	UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "随机Map元素(流)", CompactNodeTitle = "RANDOM FROM STREAM", MapParam = "TargetMap", MapKeyParam = "Key", MapValueParam = "Value"), Category = "XTools|Blueprint Extensions|Map")
	static void Map_RandomItemFromStream(const TMap<int32, int32>& TargetMap, UPARAM(Ref) FRandomStream& RandomStream, int32& Key, int32& Value);

	DECLARE_FUNCTION(execMap_RandomItemFromStream);

	static void GenericMap_RandomItemFromStream(const void* MapAddr, const FMapProperty* MapProperty, FRandomStream* RandomStream, void* OutKeyPtr, void* OutValuePtr);
#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
#pragma region MapIdentical
	UFUNCTION(BlueprintPure, BlueprintInternalUseOnly, CustomThunk, meta = (MapParam = "MapA", MapKeyParam = "KeysB", MapValueParam = "ValuesB"))
	static bool Map_Identical(const TMap<int32, int32>& MapA, const TArray<int32>& KeysB, const TArray<int32>& ValuesB);
	
	DECLARE_FUNCTION(execMap_Identical);
	
	static bool GenericMap_Identical(const void* MapAddr, const FMapProperty* MapProperty, const void* KeysBAddr, const FArrayProperty* KeysBProp, const void* ValuesBAddr, const FArrayProperty* ValuesBProp);
#pragma endregion 

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
#pragma region ArrayItem

	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, CustomThunk,meta=
	(
		Category = "XTools|Blueprint Extensions|Map",
		DisplayName = "AddItem",
		CompactNodeTitle = "ADDITEM",
		MapParam = "TargetMap",
		MapKeyParam = "Key",
		CustomStructureParam = "Value",
		AutoCreateRefTerm = "Key, Value"
	))
	static void Map_AddArrayItem(const TMap<int32, int32>& TargetMap, const int32& Key, const int32& Value);

	DECLARE_FUNCTION(execMap_AddArrayItem)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FMapProperty>(NULL);
		void* MapAddr = Stack.MostRecentPropertyAddress;
		FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);
		if (!MapProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// 获取Map的Key属性
		const FProperty* CurrKeyProp = MapProperty->KeyProp;
#if XTOOLS_ENGINE_5_5_OR_LATER
		const int32 KeyPropertySize = CurrKeyProp->GetElementSize() * CurrKeyProp->ArrayDim;
#else
		const int32 KeyPropertySize = CurrKeyProp->ElementSize * CurrKeyProp->ArrayDim;
#endif
		void* KeyStorageSpace = FMemory_Alloca(KeyPropertySize);
		CurrKeyProp->InitializeValue(KeyStorageSpace);

		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentPropertyContainer = nullptr;
		Stack.StepCompiledIn<FProperty>(KeyStorageSpace);
		
		// 获取结构体属性
		const FStructProperty* StructProp = CastField<FStructProperty>(MapProperty->ValueProp);
		if (!StructProp)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// 从结构体中查找Array属性
		FArrayProperty* ArrayProp = nullptr;
		for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
		{
			if (FArrayProperty* Prop = CastField<FArrayProperty>(*PropIt))
			{
				ArrayProp = Prop;
				break;
			}
		}

		if (!ArrayProp)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// 使用Array的元素类型作为Value属性
		const FProperty* CurrValueProp = ArrayProp->Inner;
#if XTOOLS_ENGINE_5_5_OR_LATER
		const int32 ValuePropertySize = CurrValueProp->GetElementSize() * CurrValueProp->ArrayDim;
#else
		const int32 ValuePropertySize = CurrValueProp->ElementSize * CurrValueProp->ArrayDim;
#endif
		void* ValueStorageSpace = FMemory_Alloca(ValuePropertySize);
		CurrValueProp->InitializeValue(ValueStorageSpace);

		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentPropertyContainer = nullptr;
		Stack.StepCompiledIn<FProperty>(ValueStorageSpace);

		P_FINISH;

		P_NATIVE_BEGIN;
		GenericMap_AddArrayItem(MapAddr, MapProperty, KeyStorageSpace, ValueStorageSpace);
		P_NATIVE_END;
		
		CurrValueProp->DestroyValue(ValueStorageSpace);
		CurrKeyProp->DestroyValue(KeyStorageSpace);
	}

	static void GenericMap_AddArrayItem(const void* TargetMap, const FMapProperty* MapProperty, const void* KeyPtr, const void* ValuePtr);

	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, CustomThunk,meta=
	(
		Category = "XTools|Blueprint Extensions|Map",
		DisplayName = "RemoveItem",
		CompactNodeTitle = "REMOVEITEM",
		MapParam = "TargetMap",
		MapKeyParam = "Key",
		CustomStructureParam = "Value",
		AutoCreateRefTerm = "Key, Value"
	))
	static void Map_RemoveArrayItem(const TMap<int32, int32>& TargetMap, const int32& Key, const int32& Value);

	DECLARE_FUNCTION(execMap_RemoveArrayItem)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FMapProperty>(NULL);
		void* MapAddr = Stack.MostRecentPropertyAddress;
		FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);
		if (!MapProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// 获取Map的Key属性
		const FProperty* CurrKeyProp = MapProperty->KeyProp;
#if XTOOLS_ENGINE_5_5_OR_LATER
		const int32 KeyPropertySize = CurrKeyProp->GetElementSize() * CurrKeyProp->ArrayDim;
#else
		const int32 KeyPropertySize = CurrKeyProp->ElementSize * CurrKeyProp->ArrayDim;
#endif
		void* KeyStorageSpace = FMemory_Alloca(KeyPropertySize);
		CurrKeyProp->InitializeValue(KeyStorageSpace);

		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentPropertyContainer = nullptr;
		Stack.StepCompiledIn<FProperty>(KeyStorageSpace);
		
		// 获取结构体属性
		const FStructProperty* StructProp = CastField<FStructProperty>(MapProperty->ValueProp);
		if (!StructProp)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// 从结构体中查找Array属性
		FArrayProperty* ArrayProp = nullptr;
		for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
		{
			if (FArrayProperty* Prop = CastField<FArrayProperty>(*PropIt))
			{
				ArrayProp = Prop;
				break;
			}
		}

		if (!ArrayProp)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// 使用Array的元素类型作为Value属性
		const FProperty* CurrValueProp = ArrayProp->Inner;
#if XTOOLS_ENGINE_5_5_OR_LATER
		const int32 ValuePropertySize = CurrValueProp->GetElementSize() * CurrValueProp->ArrayDim;
#else
		const int32 ValuePropertySize = CurrValueProp->ElementSize * CurrValueProp->ArrayDim;
#endif
		void* ValueStorageSpace = FMemory_Alloca(ValuePropertySize);
		CurrValueProp->InitializeValue(ValueStorageSpace);

		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentPropertyContainer = nullptr;
		Stack.StepCompiledIn<FProperty>(ValueStorageSpace);

		P_FINISH;

		P_NATIVE_BEGIN;
		GenericMap_RemoveArrayItem(MapAddr, MapProperty, KeyStorageSpace, ValueStorageSpace);
		P_NATIVE_END;
		
		CurrValueProp->DestroyValue(ValueStorageSpace);
		CurrKeyProp->DestroyValue(KeyStorageSpace);
	}

	static void GenericMap_RemoveArrayItem(const void* TargetMap, const FMapProperty* MapProperty, const void* KeyPtr, const void* ValuePtr);

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
#pragma region SetItem

	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, CustomThunk,meta=
	(
		Category = "XTools|Blueprint Extensions|Map",
		DisplayName = "AddItem",
		CompactNodeTitle = "ADDITEM",
		MapParam = "TargetMap",
		MapKeyParam = "Key",
		CustomStructureParam = "Value",
		AutoCreateRefTerm = "Key, Value"
	))
	static void Map_AddSetItem(const TMap<int32, int32>& TargetMap, const int32& Key, const int32& Value);

	DECLARE_FUNCTION(execMap_AddSetItem)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FMapProperty>(NULL);
		void* MapAddr = Stack.MostRecentPropertyAddress;
		FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);
		if (!MapProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// 获取Map的Key属性
		const FProperty* CurrKeyProp = MapProperty->KeyProp;
#if XTOOLS_ENGINE_5_5_OR_LATER
		const int32 KeyPropertySize = CurrKeyProp->GetElementSize() * CurrKeyProp->ArrayDim;
#else
		const int32 KeyPropertySize = CurrKeyProp->ElementSize * CurrKeyProp->ArrayDim;
#endif
		void* KeyStorageSpace = FMemory_Alloca(KeyPropertySize);
		CurrKeyProp->InitializeValue(KeyStorageSpace);

		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentPropertyContainer = nullptr;
		Stack.StepCompiledIn<FProperty>(KeyStorageSpace);
		
		// 获取结构体属性
		const FStructProperty* StructProp = CastField<FStructProperty>(MapProperty->ValueProp);
		if (!StructProp)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// 从结构体中查找Set属性
		FSetProperty* SetProp = nullptr;
		for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
		{
			if (FSetProperty* Prop = CastField<FSetProperty>(*PropIt))
			{
				SetProp = Prop;
				break;
			}
		}

		if (!SetProp)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// 使用Set的元素类型作为Value属性
		const FProperty* CurrValueProp = SetProp->ElementProp;
#if XTOOLS_ENGINE_5_5_OR_LATER
		const int32 ValuePropertySize = CurrValueProp->GetElementSize() * CurrValueProp->ArrayDim;
#else
		const int32 ValuePropertySize = CurrValueProp->ElementSize * CurrValueProp->ArrayDim;
#endif
		void* ValueStorageSpace = FMemory_Alloca(ValuePropertySize);
		CurrValueProp->InitializeValue(ValueStorageSpace);

		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentPropertyContainer = nullptr;
		Stack.StepCompiledIn<FProperty>(ValueStorageSpace);

		P_FINISH;

		P_NATIVE_BEGIN;
		GenericMap_AddSetItem(MapAddr, MapProperty, KeyStorageSpace, ValueStorageSpace);
		P_NATIVE_END;
		
		CurrValueProp->DestroyValue(ValueStorageSpace);
		CurrKeyProp->DestroyValue(KeyStorageSpace);
	}

	static void GenericMap_AddSetItem(const void* TargetMap, const FMapProperty* MapProperty, const void* KeyPtr, const void* ValuePtr);

	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, CustomThunk,meta=
	(
		Category = "XTools|Blueprint Extensions|Map",
		DisplayName = "RemoveItem",
		CompactNodeTitle = "REMOVEITEM",
		MapParam = "TargetMap",
		MapKeyParam = "Key",
		CustomStructureParam = "Value",
		AutoCreateRefTerm = "Key, Value"
	))
	static void Map_RemoveSetItem(const TMap<int32, int32>& TargetMap, const int32& Key, const int32& Value);

	DECLARE_FUNCTION(execMap_RemoveSetItem)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FMapProperty>(NULL);
		void* MapAddr = Stack.MostRecentPropertyAddress;
		FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);
		if (!MapProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// 获取Map的Key属性
		const FProperty* CurrKeyProp = MapProperty->KeyProp;
#if XTOOLS_ENGINE_5_5_OR_LATER
		const int32 KeyPropertySize = CurrKeyProp->GetElementSize() * CurrKeyProp->ArrayDim;
#else
		const int32 KeyPropertySize = CurrKeyProp->ElementSize * CurrKeyProp->ArrayDim;
#endif
		void* KeyStorageSpace = FMemory_Alloca(KeyPropertySize);
		CurrKeyProp->InitializeValue(KeyStorageSpace);

		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentPropertyContainer = nullptr;
		Stack.StepCompiledIn<FProperty>(KeyStorageSpace);
		
		// 获取结构体属性
		const FStructProperty* StructProp = CastField<FStructProperty>(MapProperty->ValueProp);
		if (!StructProp)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// 从结构体中查找Set属性
		FSetProperty* SetProp = nullptr;
		for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
		{
			if (FSetProperty* Prop = CastField<FSetProperty>(*PropIt))
			{
				SetProp = Prop;
				break;
			}
		}

		if (!SetProp)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// 使用Set的元素类型作为Value属性
		const FProperty* CurrValueProp = SetProp->ElementProp;
#if XTOOLS_ENGINE_5_5_OR_LATER
		const int32 ValuePropertySize = CurrValueProp->GetElementSize() * CurrValueProp->ArrayDim;
#else
		const int32 ValuePropertySize = CurrValueProp->ElementSize * CurrValueProp->ArrayDim;
#endif
		void* ValueStorageSpace = FMemory_Alloca(ValuePropertySize);
		CurrValueProp->InitializeValue(ValueStorageSpace);

		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentPropertyContainer = nullptr;
		Stack.StepCompiledIn<FProperty>(ValueStorageSpace);

		P_FINISH;

		P_NATIVE_BEGIN;
		GenericMap_RemoveSetItem(MapAddr, MapProperty, KeyStorageSpace, ValueStorageSpace);
		P_NATIVE_END;
		
		CurrValueProp->DestroyValue(ValueStorageSpace);
		CurrKeyProp->DestroyValue(KeyStorageSpace);
	}

	static void GenericMap_RemoveSetItem(const void* TargetMap, const FMapProperty* MapProperty, const void* KeyPtr, const void* ValuePtr);

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
#pragma region MapItem

	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, CustomThunk,meta=
	(
		Category = "XTools|Blueprint Extensions|Map",
		DisplayName = "AddItem",
		CompactNodeTitle = "ADDITEM",
		MapParam = "TargetMap",
		MapKeyParam = "Key",
		CustomStructureParam = "SubKey,Value",
		AutoCreateRefTerm = "Key, SubKey, Value"
	))
	static void Map_AddMapItem(const TMap<int32, int32>& TargetMap, const int32& Key, const int32& SubKey, const int32& Value);

	DECLARE_FUNCTION(execMap_AddMapItem)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FMapProperty>(NULL);
		void* MapAddr = Stack.MostRecentPropertyAddress;
		FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);
		if (!MapProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// Since Key and Value aren't really an int, step the stack manually
		const FProperty* CurrKeyProp = MapProperty->KeyProp;
#if XTOOLS_ENGINE_5_5_OR_LATER
		const int32 KeyPropertySize = CurrKeyProp->GetElementSize() * CurrKeyProp->ArrayDim;
#else
		const int32 KeyPropertySize = CurrKeyProp->ElementSize * CurrKeyProp->ArrayDim;
#endif
		void* KeyStorageSpace = FMemory_Alloca(KeyPropertySize);
		CurrKeyProp->InitializeValue(KeyStorageSpace);

		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentPropertyContainer = nullptr;
		Stack.StepCompiledIn<FProperty>(KeyStorageSpace);

		// 获取结构体属性
		const FStructProperty* StructProp = CastField<FStructProperty>(MapProperty->ValueProp);
		if (!StructProp)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// 从结构体中查找内部Map属性
		FMapProperty* InnerMapProp = nullptr;
		for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
		{
			if (FMapProperty* Prop = CastField<FMapProperty>(*PropIt))
			{
				InnerMapProp = Prop;
				break;
			}
		}
    
		if (!InnerMapProp)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// 获取内部Map的Key属性（SubKey）
		const FProperty* CurrSubKeyProp = InnerMapProp->KeyProp;
#if XTOOLS_ENGINE_5_5_OR_LATER
		const int32 SubKeyPropertySize = CurrSubKeyProp->GetElementSize() * CurrSubKeyProp->ArrayDim;
#else
		const int32 SubKeyPropertySize = CurrSubKeyProp->ElementSize * CurrSubKeyProp->ArrayDim;
#endif
		void* SubKeyStorageSpace = FMemory_Alloca(SubKeyPropertySize);
		CurrSubKeyProp->InitializeValue(SubKeyStorageSpace);

		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentPropertyContainer = nullptr;
		Stack.StepCompiledIn<FProperty>(SubKeyStorageSpace);
    
		// 获取内部Map的Value属性
		const FProperty* CurrValueProp = InnerMapProp->ValueProp;
#if XTOOLS_ENGINE_5_5_OR_LATER
		const int32 ValuePropertySize = CurrValueProp->GetElementSize() * CurrValueProp->ArrayDim;
#else
		const int32 ValuePropertySize = CurrValueProp->ElementSize * CurrValueProp->ArrayDim;
#endif
		void* ValueStorageSpace = FMemory_Alloca(ValuePropertySize);
		CurrValueProp->InitializeValue(ValueStorageSpace);
    
		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentPropertyContainer = nullptr;
		Stack.StepCompiledIn<FProperty>(ValueStorageSpace);

		P_FINISH;

		P_NATIVE_BEGIN;
		GenericMap_AddMapItem(MapAddr, MapProperty, KeyStorageSpace, SubKeyStorageSpace, ValueStorageSpace);
		P_NATIVE_END;
		
		CurrValueProp->DestroyValue(ValueStorageSpace);
		CurrSubKeyProp->DestroyValue(SubKeyStorageSpace);
		CurrKeyProp->DestroyValue(KeyStorageSpace);
	}

	static void GenericMap_AddMapItem(const void* TargetMap, const FMapProperty* MapProperty, const void* KeyPtr, const void* SubKeyPtr, const void* ValuePtr);
	
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, CustomThunk,meta=
	(
		Category = "XTools|Blueprint Extensions|Map",
		DisplayName = "RemoveItem",
		CompactNodeTitle = "REMOVEITEM",
		MapParam = "TargetMap",
		MapKeyParam = "Key",
		CustomStructureParam = "SubKey",
		AutoCreateRefTerm = "Key,SubKey"
	))
	static void Map_RemoveMapItem(const TMap<int32, int32>& TargetMap, const int32& Key, const int32& SubKey);

	DECLARE_FUNCTION(execMap_RemoveMapItem)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FMapProperty>(NULL);
		void* MapAddr = Stack.MostRecentPropertyAddress;
		FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);
		if (!MapProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// Since Key and Value aren't really an int, step the stack manually
		const FProperty* CurrKeyProp = MapProperty->KeyProp;
#if XTOOLS_ENGINE_5_5_OR_LATER
		const int32 KeyPropertySize = CurrKeyProp->GetElementSize() * CurrKeyProp->ArrayDim;
#else
		const int32 KeyPropertySize = CurrKeyProp->ElementSize * CurrKeyProp->ArrayDim;
#endif
		void* KeyStorageSpace = FMemory_Alloca(KeyPropertySize);
		CurrKeyProp->InitializeValue(KeyStorageSpace);

		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentPropertyContainer = nullptr;
		Stack.StepCompiledIn<FProperty>(KeyStorageSpace);

		// 获取结构体属性
		const FStructProperty* StructProp = CastField<FStructProperty>(MapProperty->ValueProp);
		if (!StructProp)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// 从结构体中查找内部Map属性
		FMapProperty* InnerMapProp = nullptr;
		for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
		{
			if (FMapProperty* Prop = CastField<FMapProperty>(*PropIt))
			{
				InnerMapProp = Prop;
				break;
			}
		}
    
		if (!InnerMapProp)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		// 获取内部Map的Key属性（SubKey）
		const FProperty* CurrSubKeyProp = InnerMapProp->KeyProp;
#if XTOOLS_ENGINE_5_5_OR_LATER
		const int32 SubKeyPropertySize = CurrSubKeyProp->GetElementSize() * CurrSubKeyProp->ArrayDim;
#else
		const int32 SubKeyPropertySize = CurrSubKeyProp->ElementSize * CurrSubKeyProp->ArrayDim;
#endif
		void* SubKeyStorageSpace = FMemory_Alloca(SubKeyPropertySize);
		CurrSubKeyProp->InitializeValue(SubKeyStorageSpace);

		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentPropertyContainer = nullptr;
		Stack.StepCompiledIn<FProperty>(SubKeyStorageSpace);

		P_FINISH;

		P_NATIVE_BEGIN;
		GenericMap_RemoveMapItem(MapAddr, MapProperty, KeyStorageSpace, SubKeyStorageSpace);
		P_NATIVE_END;

		CurrSubKeyProp->DestroyValue(SubKeyStorageSpace);
		CurrKeyProp->DestroyValue(KeyStorageSpace);
	}

	static void GenericMap_RemoveMapItem(const void* TargetMap, const FMapProperty* MapProperty, const void* KeyPtr, const void* SubKeyPtr);

#pragma endregion
	
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
private:
	static constexpr int32 MaxSupportedMapSize = TNumericLimits<int32>::Max();
	
};
