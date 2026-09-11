/* Copyright (c) 2026 XIYBHK. Licensed under UE_XTools License. */
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DebugPrintLibrary.generated.h"

UENUM(BlueprintType)
enum class EXToolsDebugPrintMode : uint8
{
    Inline UMETA(DisplayName="单行追加"),
    Replace UMETA(DisplayName="单行覆盖"),
    NewLine UMETA(DisplayName="逐行"),
    Labels UMETA(DisplayName="逐行带名称"),
    Columns UMETA(DisplayName="列对齐")
};

/** DebugPrint 节点的运行时支持。 */
UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API UDebugPrintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, CustomThunk, Category="XTools|调试", meta=(BlueprintInternalUseOnly="true", CustomStructureParam="Value", DisplayName="调试值转字符串", ToolTip="使用 UE 属性文本格式导出调试值。"))
    static FString ValueToDebugString(const int32& Value);
    DECLARE_FUNCTION(execValueToDebugString);

    UFUNCTION(BlueprintCallable, Category="XTools|调试", meta=(BlueprintInternalUseOnly="true", WorldContext="WorldContextObject", DevelopmentOnly, DisplayName="打印调试值", ToolTip="打印多个已格式化的调试值。"))
    static void PrintDebugValues(const UObject* WorldContextObject, const TArray<FString>& Values,
        const TArray<FString>& Labels, const FString& Separator, bool bPrintToScreen, bool bPrintToLog,
        FLinearColor TextColor, float Duration, FName Key, EXToolsDebugPrintMode Mode, FName NodeKey);

    static FString FormatDebugValues(const TArray<FString>& Values, const TArray<FString>& Labels,
        const FString& Separator, EXToolsDebugPrintMode Mode);
};
