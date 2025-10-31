/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "XBlueprintLibraryCleanupTool.generated.h"

/**
 * 蓝图函数库清理工具
 * 
 * 提供清理蓝图函数库中多余World Context参数的功能
 */
UCLASS()
class XTOOLS_API UXBlueprintLibraryCleanupTool : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * 扫描并预览需要清理的World Context参数
     * 
     * @param bLogToConsole 是否输出到控制台日志
     * @return 找到的需要清理的参数数量
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|蓝图清理", CallInEditor, meta = (
        DisplayName = "预览清理World Context参数",
        Keywords = "cleanup world context preview scan",
        ToolTip = "扫描所有蓝图函数库，预览哪些【未连接】的World Context参数将被清理\n安全特性：只处理未连接的参数，已连接的参数会被保留"))
    static int32 PreviewCleanupWorldContextParams(bool bLogToConsole = true);

    /**
     * 执行清理World Context参数
     * 
     * @param bLogToConsole 是否输出到控制台日志
     * @return 成功清理的参数数量
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|蓝图清理", CallInEditor, meta = (
        DisplayName = "执行清理World Context参数",
        Keywords = "cleanup world context execute remove",
        ToolTip = "实际清理蓝图函数库中【未连接】的World Context参数\n安全特性：只处理未连接的参数，已连接的参数会被保留\n警告：会修改蓝图资产，建议先备份"))
    static int32 ExecuteCleanupWorldContextParams(bool bLogToConsole = true);

private:
#if WITH_EDITOR
    // 内部实现函数
    static class UBlueprint* GetBlueprintFromAssetData(const struct FAssetData& AssetData);
    static bool IsBlueprintFunctionLibrary(class UBlueprint* Blueprint);
    static TArray<class UBlueprint*> GetAllBlueprintFunctionLibraries();
    static bool IsWorldContextPin(const class UEdGraphPin* Pin);
    
    struct FWorldContextScanResult
    {
        class UBlueprint* Blueprint;
        FString FunctionName;
        FString PinName;
        class UEdGraphNode* Node; // 可能是入口节点或调用节点
        bool bIsCallNode; // 标记是否是调用节点
    };
    
    static TArray<FWorldContextScanResult> ScanWorldContextParams(const TArray<class UBlueprint*>& Blueprints);
#endif // WITH_EDITOR
};
