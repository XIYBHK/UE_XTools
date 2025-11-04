#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ProcessExtensionsLibrary.generated.h"

UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API UProcessExtensionsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, CustomThunk, meta = (DisplayName = "按名称调用函数", CustomStructureParam = "EventPayload", Category = "XTools|Blueprint Extensions|Process", CompactNodeTitle = "CallFunctionByName"))
	static void CallFunctionByName(UObject* FunctionOwnerObject, FString FunctionName, const int32& EventPayload);
	DECLARE_FUNCTION(execCallFunctionByName);

	UFUNCTION(BlueprintCallable, CustomThunk, meta = (DisplayName = "按名称调用事件", CustomStructureParam = "EventPayload", Category = "XTools|Blueprint Extensions|Process", CompactNodeTitle = "CallEventByName"))
	static void CallEventByName(UObject* EventOwnerObject, FString EventName, const int32& EventPayload);
	DECLARE_FUNCTION(execCallEventByName);
	
};
