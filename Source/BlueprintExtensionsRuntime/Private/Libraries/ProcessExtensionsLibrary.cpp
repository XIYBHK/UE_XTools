#include "Libraries/ProcessExtensionsLibrary.h"
#include "BlueprintExtensionsRuntime.h"
#include "XToolsErrorReporter.h"

#include "UObject/UnrealType.h"

namespace
{
	/** 栈分配大小上限 */
	constexpr int32 MaxStackAllocSize = 65536;

	/**
	 * 验证目标 UFunction 的参数布局是否与 payload 兼容
	 * 要求：只有一个非返回值参数，且大小与 payload 匹配，结构体类型兼容
	 * @return 目标函数的第一个参数属性（验证通过），或 nullptr（验证失败）
	 */
	const FProperty* ValidateFunctionParams(
		const UFunction* Function,
		const FProperty* PayloadProperty,
		const FString& FunctionName,
		const TCHAR* CallerName)
	{
		if (!Function || !PayloadProperty)
		{
			return nullptr;
		}

		// 统计非返回值参数数量，找到第一个参数
		const FProperty* FirstParam = nullptr;
		int32 ParamCount = 0;
		for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			if (It->PropertyFlags & CPF_ReturnParm)
			{
				continue;
			}
			++ParamCount;
			if (!FirstParam)
			{
				FirstParam = *It;
			}
		}

		// 只支持单参数函数/事件
		if (ParamCount != 1 || !FirstParam)
		{
			FXToolsErrorReporter::Error(
				LogBlueprintExtensionsRuntime,
				FString::Printf(TEXT("%s: 目标 '%s' 有 %d 个参数，仅支持单参数调用"),
					CallerName, *FunctionName, ParamCount),
				CallerName);
			return nullptr;
		}

		// 参数大小匹配
		const int32 PayloadSize = PayloadProperty->GetSize();
		const int32 ParamSize = FirstParam->GetSize();
		if (PayloadSize != ParamSize)
		{
			FXToolsErrorReporter::Error(
				LogBlueprintExtensionsRuntime,
				FString::Printf(TEXT("%s: 参数大小不匹配 '%s'（payload=%d, 目标参数=%d bytes）"),
					CallerName, *FunctionName, PayloadSize, ParamSize),
				CallerName);
			return nullptr;
		}

		// 结构体类型兼容性检查
		const FStructProperty* PayloadStruct = CastField<FStructProperty>(PayloadProperty);
		const FStructProperty* ParamStruct = CastField<FStructProperty>(FirstParam);
		if (PayloadStruct && ParamStruct)
		{
			if (PayloadStruct->Struct != ParamStruct->Struct)
			{
				FXToolsErrorReporter::Error(
					LogBlueprintExtensionsRuntime,
					FString::Printf(TEXT("%s: 结构体类型不匹配 '%s'（payload=%s, 目标参数=%s）"),
						CallerName, *FunctionName,
						*PayloadStruct->Struct->GetName(), *ParamStruct->Struct->GetName()),
					CallerName);
				return nullptr;
			}
		}
		else if (PayloadProperty->GetClass() != FirstParam->GetClass())
		{
			// 非结构体类型：属性类型须一致
			FXToolsErrorReporter::Error(
				LogBlueprintExtensionsRuntime,
				FString::Printf(TEXT("%s: 参数属性类型不匹配 '%s'（payload=%s, 目标参数=%s）"),
					CallerName, *FunctionName,
					*PayloadProperty->GetClass()->GetName(), *FirstParam->GetClass()->GetName()),
				CallerName);
			return nullptr;
		}

		// 栈分配大小限制
		if (PayloadSize > MaxStackAllocSize)
		{
			FXToolsErrorReporter::Error(
				LogBlueprintExtensionsRuntime,
				FString::Printf(TEXT("%s: 参数过大 (%d bytes)，超过栈分配限制 (%d bytes): %s"),
					CallerName, PayloadSize, MaxStackAllocSize, *FunctionName),
				CallerName);
			return nullptr;
		}

		return FirstParam;
	}
}

void UProcessExtensionsLibrary::CallFunctionByName(UObject* FunctionOwnerObject, FString FunctionName, const int32& EventPayload)
{
	checkNoEntry();
}
DEFINE_FUNCTION(UProcessExtensionsLibrary::execCallFunctionByName) {
	P_GET_OBJECT(UObject, FunctionOwnerObject);
	P_GET_PROPERTY(FStrProperty, FunctionName);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	void* EventPayload = Stack.MostRecentPropertyAddress;
	P_FINISH;

	P_NATIVE_BEGIN;
	if (IsValid(FunctionOwnerObject) && Stack.MostRecentProperty != nullptr) {
		UFunction* Function = FunctionOwnerObject->FindFunction(FName(*FunctionName));
		if (IsValid(Function)) {
			const FProperty* ValidatedParam = ValidateFunctionParams(
				Function, Stack.MostRecentProperty, FunctionName, TEXT("CallFunctionByName"));
			if (!ValidatedParam)
			{
				return;
			}

			const int32 PropertySize = Stack.MostRecentProperty->GetSize();
			void* FunctionDataPtr = FMemory_Alloca_Aligned(
				PropertySize,
				Stack.MostRecentProperty->GetMinAlignment()
			);
			Stack.MostRecentProperty->InitializeValue(FunctionDataPtr);
			Stack.MostRecentProperty->CopyCompleteValue(FunctionDataPtr, EventPayload);

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
	P_GET_OBJECT(UObject, EventOwnerObject);
	P_GET_PROPERTY(FStrProperty, EventName);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	void* EventParamPtr = Stack.MostRecentPropertyAddress;
	P_FINISH;

	P_NATIVE_BEGIN;
	if (IsValid(EventOwnerObject) && Stack.MostRecentProperty != nullptr) {
		UFunction* EventFunction = EventOwnerObject->FindFunction(FName(*EventName));

		if (EventFunction && EventFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent)) {
			const FProperty* ValidatedParam = ValidateFunctionParams(
				EventFunction, Stack.MostRecentProperty, EventName, TEXT("CallEventByName"));
			if (!ValidatedParam)
			{
				return;
			}

			const int32 PropertySize = Stack.MostRecentProperty->GetSize();
			void* EventData = FMemory_Alloca_Aligned(
				PropertySize,
				Stack.MostRecentProperty->GetMinAlignment()
			);
			Stack.MostRecentProperty->InitializeValue(EventData);
			Stack.MostRecentProperty->CopyCompleteValue(EventData, EventParamPtr);

			EventOwnerObject->ProcessEvent(EventFunction, EventData);
			Stack.MostRecentProperty->DestroyValue(EventData);
		}
	}
	P_NATIVE_END;
}