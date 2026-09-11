/* Copyright (c) 2026 XIYBHK. Licensed under UE_XTools License. */
#include "Libraries/DebugPrintLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/Stack.h"
#include "UObject/UnrealType.h"

namespace
{
    FString GetDebugLabel(const TArray<FString>& Labels, int32 Index)
    {
        return Labels.IsValidIndex(Index) ? Labels[Index] : FString::FromInt(Index + 1);
    }

    TArray<FString> BuildDebugLines(const TArray<FString>& Values, const TArray<FString>& Labels,
        EXToolsDebugPrintMode Mode, const FString& Separator)
    {
        TArray<FString> Lines;
        Lines.Reserve(Values.Num());
        int32 MaxLabelLength = 0;
        if (Mode == EXToolsDebugPrintMode::Columns)
        {
            for (int32 Index = 0; Index < Values.Num(); ++Index)
            {
                MaxLabelLength = FMath::Max(MaxLabelLength, GetDebugLabel(Labels, Index).Len());
            }
        }
        for (int32 Index = 0; Index < Values.Num(); ++Index)
        {
            const FString Value = Values[Index];
            if (Mode == EXToolsDebugPrintMode::NewLine)
            {
                Lines.Add(Value);
            }
            else
            {
                FString Label = GetDebugLabel(Labels, Index);
                if (Mode == EXToolsDebugPrintMode::Columns)
                {
                    Label = Label + FString::ChrN(MaxLabelLength - Label.Len(), TCHAR(' '));
                }
                Lines.Add(Label + Separator + Value);
            }
        }
        return Lines;
    }
}

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
    const FString& Separator, EXToolsDebugPrintMode Mode)
{
    if (Mode == EXToolsDebugPrintMode::Inline || Mode == EXToolsDebugPrintMode::Replace)
    {
        return FString::Join(Values, *Separator);
    }
    return FString::Join(BuildDebugLines(Values, Labels, Mode, Separator), TEXT("\n"));
}

void UDebugPrintLibrary::PrintDebugValues(const UObject* WorldContextObject, const TArray<FString>& Values,
    const TArray<FString>& Labels, const FString& Separator, bool bPrintToScreen, bool bPrintToLog,
    FLinearColor TextColor, float Duration, FName Key,
    EXToolsDebugPrintMode Mode, FName NodeKey)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    if (Mode == EXToolsDebugPrintMode::Inline || Mode == EXToolsDebugPrintMode::Replace)
    {
        const FName PrintKey = Mode == EXToolsDebugPrintMode::Replace && Key.IsNone() ? NodeKey : Key;
        UKismetSystemLibrary::PrintString(WorldContextObject,
            FormatDebugValues(Values, Labels, Separator, Mode),
            bPrintToScreen, bPrintToLog, TextColor, Duration, PrintKey);
        return;
    }

    const TArray<FString> Lines = BuildDebugLines(Values, Labels, Mode, Separator);
    const FString KeyPrefix = (!Key.IsNone() ? Key : NodeKey).ToString();
    for (int32 Index = 0; Index < Lines.Num(); ++Index)
    {
        const FName LineKey = KeyPrefix.IsEmpty() ? NAME_None : FName(*(KeyPrefix + TEXT("_") + FString::FromInt(Index)));
        UKismetSystemLibrary::PrintString(WorldContextObject, Lines[Index],
            bPrintToScreen, bPrintToLog, TextColor, Duration, LineKey);
    }
#endif
}
