// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "X_MaterialToolsSettings.generated.h"

/**
 * 材质工具设置类
 * 存储材质工具的基本配置选项
 */
UCLASS(config=Editor, defaultconfig, meta=(DisplayName="X Asset Editor"))
class X_ASSETEDITOR_API UX_MaterialToolsSettings : public UObject
{
    GENERATED_BODY()

public:
    /** 构造函数 */
    UX_MaterialToolsSettings();

    /** 是否启用调试日志 */
    UPROPERTY(config, EditAnywhere, Category="调试", meta=(DisplayName="启用调试日志", 
        ToolTip="启用后将在输出日志中显示详细的调试信息"))
    bool bEnableDebugLog;

    /** 是否启用并行处理 */
    UPROPERTY(config, EditAnywhere, Category="性能", meta=(DisplayName="启用并行处理", 
        ToolTip="启用后将使用多线程处理大量资产，提高性能"))
    bool bEnableParallelProcessing;

    /** 并行处理批次大小 */
    UPROPERTY(config, EditAnywhere, Category="性能", meta=(ClampMin="1", ClampMax="1000", UIMin="1", UIMax="500", 
        EditCondition="bEnableParallelProcessing", DisplayName="并行批次大小", 
        ToolTip="每个批次处理的资产数量，较大的值可能提高性能但会增加内存使用"))
    int32 ParallelBatchSize;
}; 