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

			if (It->HasAnyPropertyFlags(CPF_OutParm) && !It->HasAnyPropertyFlags(CPF_ConstParm))
			{
				FXToolsErrorReporter::Error(
					LogBlueprintExtensionsRuntime,
					FString::Printf(TEXT("%s: 目标 '%s' 的参数 '%s' 是输出/引用参数，不支持通过 payload 调用"),
						CallerName, *FunctionName, *It->GetName()),
					CallerName);
				return nullptr;
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
		else if (!PayloadProperty->SameType(FirstParam))
		{
			// 非结构体类型：属性类型及其反射细节须兼容（例如对象类、容器Inner等）
			FXToolsErrorReporter::Error(
				LogBlueprintExtensionsRuntime,
				FString::Printf(TEXT("%s: 参数属性类型不匹配 '%s'（payload=%s, 目标参数=%s）"),
					CallerName, *FunctionName,
					*PayloadProperty->GetClass()->GetName(), *FirstParam->GetClass()->GetName()),
				CallerName);
			return nullptr;
		}

		// 栈分配大小限制：ProcessEvent 需要完整的 UFunction 参数帧，而不是单个 payload 大小。
		if (Function->ParmsSize > MaxStackAllocSize)
		{
			FXToolsErrorReporter::Error(
				LogBlueprintExtensionsRuntime,
				FString::Printf(TEXT("%s: 函数参数帧过大 (%d bytes)，超过栈分配限制 (%d bytes): %s"),
					CallerName, Function->ParmsSize, MaxStackAllocSize, *FunctionName),
				CallerName);
			return nullptr;
		}

		return FirstParam;
	}

	void ProcessEventWithSinglePayload(
		UObject* FunctionOwnerObject,
		UFunction* Function,
		const FProperty* FunctionParam,
		const FProperty* PayloadProperty,
		const void* Payload)
	{
		if (!IsValid(FunctionOwnerObject) || !Function || !FunctionParam || !PayloadProperty || !Payload)
		{
			return;
		}

		uint8* FunctionParams = static_cast<uint8*>(
			FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment()));
		FMemory::Memzero(FunctionParams, Function->ParmsSize);

		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			FProperty* LocalProp = *It;
			if (!LocalProp->HasAnyPropertyFlags(CPF_ZeroConstructor))
			{
				LocalProp->InitializeValue_InContainer(FunctionParams);
			}
		}

		void* TargetParamPtr = FunctionParam->ContainerPtrToValuePtr<void>(FunctionParams);
		FunctionParam->CopyCompleteValueFromScriptVM(TargetParamPtr, Payload);

		FunctionOwnerObject->ProcessEvent(Function, FunctionParams);

		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(FunctionParams);
		}
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
	Stack.StepCompiledIn<FProperty>(nullptr);
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

			ProcessEventWithSinglePayload(
				FunctionOwnerObject,
				Function,
				ValidatedParam,
				Stack.MostRecentProperty,
				EventPayload);
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
	Stack.StepCompiledIn<FProperty>(nullptr);
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

			ProcessEventWithSinglePayload(
				EventOwnerObject,
				EventFunction,
				ValidatedParam,
				Stack.MostRecentProperty,
				EventParamPtr);
		}
	}
	P_NATIVE_END;
}
