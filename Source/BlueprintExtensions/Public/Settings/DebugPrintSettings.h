/* Copyright (c) 2026 XIYBHK. Licensed under UE_XTools License. */
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Libraries/DebugPrintLibrary.h"
#include "DebugPrintSettings.generated.h"

/** 只用于新节点的默认值，不改写已放置节点。 */
UCLASS(Config=EditorPerProjectUserSettings, meta=(DisplayName="调试打印（DebugPrint）"))
class BLUEPRINTEXTENSIONS_API UDebugPrintSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    virtual FName GetContainerName() const override { return TEXT("Editor"); }
    virtual FName GetCategoryName() const override { return TEXT("XTools"); }

    UPROPERTY(Config, EditAnywhere, Category="新节点默认值", meta=(DisplayName="打印模式", ToolTip="新创建节点的打印模式。"))
    EXToolsDebugPrintMode Mode = EXToolsDebugPrintMode::Columns;
    UPROPERTY(Config, EditAnywhere, Category="新节点默认值", meta=(DisplayName="文字颜色"))
    FLinearColor TextColor = FLinearColor(1.f, 0.f, 1.f, 1.f);
    UPROPERTY(Config, EditAnywhere, Category="新节点默认值", meta=(DisplayName="分隔符"))
    FString Separator = TEXT(" | ");
    UPROPERTY(Config, EditAnywhere, Category="新节点默认值", meta=(DisplayName="持续时间"))
    float Duration = 5.f;
    UPROPERTY(Config, EditAnywhere, Category="新节点默认值", meta=(DisplayName="输出到屏幕"))
    bool bPrintToScreen = true;
    UPROPERTY(Config, EditAnywhere, Category="新节点默认值", meta=(DisplayName="输出到日志"))
    bool bPrintToLog = false;
};
