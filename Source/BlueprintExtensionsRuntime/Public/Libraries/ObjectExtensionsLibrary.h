#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ObjectExtensionsLibrary.generated.h"

UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API UObjectExtensionsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region GetObject
	
	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Object", meta = (DisplayName = "从Map获取对象", CompactNodeTitle = "GetObjectFromMap", DeterminesOutputType = "FindClass"))
	static UObject* GetObjectFromMap(const TMap<UClass*, UObject*>& FindMap, UClass* FindClass);

#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region ResetObject
	
	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Object", meta = (DisplayName = "清空对象", CompactNodeTitle = "ClearObject"))
	static void ClearObject(UObject* Object);

	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Object", meta = (DisplayName = "清空对象(CDO)", CompactNodeTitle = "ClearObject.CDO"))
	static void ClearObjectByAssetCDO(UObject* Object);

#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region CopyObject
	
	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Object", meta = (DisplayName = "复制对象值", CompactNodeTitle = "CopyObjectValues"))
	static void CopyObjectValues(UObject* Target, UObject* Source);

	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Object", meta = (DisplayName = "复制类值", CompactNodeTitle = "CopyClassValues"))
	static void CopyClassValues(UObject* Target, UClass* SourceClass);

#pragma endregion
	
//————————————————————————————————————————————————————————————————————————————————————————————————————
	
};
