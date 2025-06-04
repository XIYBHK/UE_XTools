// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "X_MaterialToolsSettings.generated.h"

/**
 * 材质工具设置类
 * 存储材质工具的基本配置选项
 */
UCLASS(config=Editor, defaultconfig)
class X_ASSETEDITOR_API UX_MaterialToolsSettings : public UObject
{
    GENERATED_BODY()

public:
    /** 构造函数 */
    UX_MaterialToolsSettings();

    /** 是否启用调试日志 */
    UPROPERTY(config, EditAnywhere, Category="调试")
    bool bEnableDebugLog;

    /** 是否启用并行处理 */
    UPROPERTY(config, EditAnywhere, Category="性能")
    bool bEnableParallelProcessing;

    /** 并行处理批次大小 */
    UPROPERTY(config, EditAnywhere, Category="性能", meta=(ClampMin="1", ClampMax="1000", UIMin="1", UIMax="500", 
        EditCondition="bEnableParallelProcessing"))
    int32 ParallelBatchSize;
}; 