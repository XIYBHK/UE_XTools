/* Copyright (c) 2026 XIYBHK. Licensed under UE_XTools License. */
#include "Libraries/DebugPrintLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/Stack.h"
#include "UObject/UnrealType.h"

FString UDebugPrintLibrary::ValueToDebugString(const int32& Value)
{
    return FString::FromInt(Value);
}

DEFINE_FUNCTION(UDebugPrintLibrary::execValueToDebugString)
{
    Stack.MostRecentProperty = nullptr;
    Stack.MostRecentPropertyAddress = nullptr;
    Stack.StepCompiledIn<FProperty>(nullptr);
    const FProperty* Property = Stack.MostRecentProperty;
    const void* Address = Stack.MostRecentPropertyAddress;
    P_FINISH;
    P_NATIVE_BEGIN;
    FString& Result = *static_cast<FString*>(RESULT_PARAM);
    Result.Reset();
    if (Property && Address)
    {
        Property->ExportTextItem_Direct(Result, Address, nullptr, Stack.Object, PPF_None);
    }
    P_NATIVE_END;
}

FString UDebugPrintLibrary::FormatDebugValues(const TArray<FString>& Values, const TArray<FString>& Labels,
    bool bShowLabels, bool bNewLine, const FString& Separator)
{
    TArray<FString> Parts;
    Parts.Reserve(Values.Num());
    for (int32 Index = 0; Index < Values.Num(); ++Index)
    {
        const FString Label = Labels.IsValidIndex(Index) ? Labels[Index] : FString::FromInt(Index + 1);
        Parts.Add(bShowLabels ? Label + TEXT(" = ") + Values[Index] : Values[Index]);
    }
    return FString::Join(Parts, bNewLine ? TEXT("\n") : *Separator);
}

void UDebugPrintLibrary::PrintDebugValues(const UObject* WorldContextObject, const TArray<FString>& Values,
    const TArray<FString>& Labels, bool bShowLabels, bool bNewLine, const FString& Separator,
    bool bPrintToScreen, bool bPrintToLog, FLinearColor TextColor, float Duration, FName Key)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    UKismetSystemLibrary::PrintString(WorldContextObject,
        FormatDebugValues(Values, Labels, bShowLabels, bNewLine, Separator),
        bPrintToScreen, bPrintToLog, TextColor, Duration, Key);
#endif
}
