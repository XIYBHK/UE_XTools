#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VariableReflectionLibrary.generated.h"

UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API UVariableReflectionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Variable", meta = (DisplayName = "获取变量名列表", CompactNodeTitle = "GetVariableNames"))
	static TArray<FString> GetVariableNames(UClass* Class, bool bIncludeSuper = true);

	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Variable", meta = (DisplayName = "按字符串设置值", CompactNodeTitle = "SetValueByString", BlueprintAuthorityOnly))
	static void SetValueByString(UObject* OwnerObject, FString VariableName, FString Value);

	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Variable", meta = (DisplayName = "按字符串获取值", CompactNodeTitle = "GetValueByString"))
	static FString GetValueByString(UObject* OwnerObject, FString VariableName);
	
};
