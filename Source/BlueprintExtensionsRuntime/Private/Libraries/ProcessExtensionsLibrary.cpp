#include "Libraries/ProcessExtensionsLibrary.h"

#include "UObject/UnrealType.h"

void UProcessExtensionsLibrary::CallFunctionByName(UObject* FunctionOwnerObject, FString FunctionName, const int32& EventPayload)
{
	checkNoEntry();
}
DEFINE_FUNCTION(UProcessExtensionsLibrary::execCallFunctionByName) {
	// 修正参数解析
	P_GET_OBJECT(UObject, FunctionOwnerObject);
	P_GET_PROPERTY(FStrProperty, FunctionName); // 使用 FStrProperty 替代 FString

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	void* EventPayload = Stack.MostRecentPropertyAddress;
	P_FINISH;

	P_NATIVE_BEGIN;
	if (IsValid(FunctionOwnerObject) && Stack.MostRecentProperty != nullptr) {
		UFunction* Function = FunctionOwnerObject->FindFunction(FName(*FunctionName));
		if (IsValid(Function)) {
			// 验证参数类型是否匹配
			if (Function->ParmsSize != Stack.MostRecentProperty->GetSize()) {
				UE_LOG(LogTemp, Error, TEXT("Parameter size mismatch for function %s!"), *FunctionName);
				return;
			}

			void* FunctionDataPtr = FMemory_Alloca_Aligned(
				Stack.MostRecentProperty->GetSize(),
				Stack.MostRecentProperty->GetMinAlignment()
			);
			Stack.MostRecentProperty->InitializeValue(FunctionDataPtr);
			Stack.MostRecentProperty->CopyCompleteValue(FunctionDataPtr, EventPayload);

			// 直接使用原对象指针（已通过 IsValid 检查）
			FunctionOwnerObject->ProcessEvent(Function, FunctionDataPtr);
			Stack.MostRecentProperty->DestroyValue(FunctionDataPtr);
		}
	}
	P_NATIVE_END;
}

void UProcessExtensionsLibrary::CallEventByName(UObject* EventOwnerObject, FString EventName, const int32& EventPayload)
{
	checkNoEntry();
}
DEFINE_FUNCTION(UProcessExtensionsLibrary::execCallEventByName)
{
	// 解析输入参数
	P_GET_OBJECT(UObject, EventOwnerObject);
	P_GET_PROPERTY(FStrProperty, EventName);

	// 处理动态结构体参数
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	void* EventParamPtr = Stack.MostRecentPropertyAddress;
	P_FINISH;

	P_NATIVE_BEGIN;
	if (IsValid(EventOwnerObject) && Stack.MostRecentProperty != nullptr) {
		// 通过反射查找事件（BlueprintImplementableEvent 或 BlueprintNativeEvent）
		UFunction* EventFunction = EventOwnerObject->FindFunction(FName(*EventName));

		if (EventFunction && EventFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent)) {
			// 分配临时内存并复制结构体数据
			void* EventData = FMemory_Alloca_Aligned(
				Stack.MostRecentProperty->GetSize(),
				Stack.MostRecentProperty->GetMinAlignment()
			);
			Stack.MostRecentProperty->InitializeValue(EventData);
			Stack.MostRecentProperty->CopyCompleteValue(EventData, EventParamPtr);

			// 触发事件（无返回值）
			EventOwnerObject->ProcessEvent(EventFunction, EventData);

			// 清理临时数据
			Stack.MostRecentProperty->DestroyValue(EventData);
		}
	}
	P_NATIVE_END;
}