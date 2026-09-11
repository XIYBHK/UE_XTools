/* Copyright (c) 2025 XIYBHK. Licensed under UE_XTools License. */
#pragma once
#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "X_AssetFlattenLibrary.generated.h"

USTRUCT(BlueprintType)
struct X_ASSETEDITOR_API FX_AssetFlattenResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "XTools|资产整理", meta = (DisplayName = "全部完成"))
    bool bSuccess = false;
    UPROPERTY(BlueprintReadOnly, Category = "XTools|资产整理", meta = (DisplayName = "收集资产数"))
    int32 CollectedCount = 0;
    UPROPERTY(BlueprintReadOnly, Category = "XTools|资产整理", meta = (DisplayName = "已移动数"))
    int32 MovedCount = 0;
    UPROPERTY(BlueprintReadOnly, Category = "XTools|资产整理", meta = (DisplayName = "已在目标目录数"))
    int32 SkippedCount = 0;
    /** 显式选中的外部资产，去重后保留原位；不计入已在目标目录数。 */
    UPROPERTY(BlueprintReadOnly, Category = "XTools|资产整理", meta = (DisplayName = "已跳过外部资产"))
    TArray<FString> SkippedExternalAssets;
    UPROPERTY(BlueprintReadOnly, Category = "XTools|资产整理", meta = (DisplayName = "自动改名记录"))
    TArray<FString> AutoRenamedAssets;
    UPROPERTY(BlueprintReadOnly, Category = "XTools|资产整理", meta = (DisplayName = "未修复重定向器"))
    TArray<FString> RemainingRedirectors;
    UPROPERTY(BlueprintReadOnly, Category = "XTools|资产整理", meta = (DisplayName = "问题详情"))
    TArray<FString> Errors;
};

/** 仅编辑器：将已保存的选中资产及 /Game 硬、软包依赖移动到同一目录。 */
UCLASS()
class X_ASSETEDITOR_API UX_AssetFlattenLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "XTools|资产整理", meta = (DisplayName = "扁平移动资产及依赖", Keywords = "移动 依赖 引用 重定向器 Flatten", ToolTip = "保持名称，递归收集 /Game 硬软包依赖并批量移动、保存、修复本次重定向器。请先保存源资产。同名冲突会阻止移动；执行阶段可能部分成功，请检查结果。"))
    static FX_AssetFlattenResult FlattenAssets(UPARAM(DisplayName = "资产") const TArray<FAssetData>& Assets, UPARAM(DisplayName = "目标目录") const FString& TargetFolder);
    UFUNCTION(BlueprintCallable, Category = "XTools|资产整理", meta = (DisplayName = "扁平移动资产及依赖（自动改名冲突）"))
    static FX_AssetFlattenResult FlattenAssetsAutoRename(UPARAM(DisplayName = "资产") const TArray<FAssetData>& Assets, UPARAM(DisplayName = "目标目录") const FString& TargetFolder);

    UFUNCTION(BlueprintCallable, Category = "XTools|资产整理", meta = (DisplayName = "扁平移动选中资产及依赖", Keywords = "移动 选中 依赖 Flatten", ToolTip = "将内容浏览器选中资产及 /Game 依赖移动到目标目录，例如 /Game/Gathered。保持原名，冲突时不移动。"))
    static FX_AssetFlattenResult FlattenSelectedAssets(UPARAM(DisplayName = "目标目录") const FString& TargetFolder);

    UFUNCTION(BlueprintCallable, Category = "XTools|资产整理", meta = (DisplayName = "按类型移动资产及依赖", Keywords = "移动 分类 依赖 引用", ToolTip = "递归收集 /Game 硬软依赖，保持原名，按真实资产类型移动到目标目录的分类子目录。冲突会阻止移动；请检查结果及遗留重定向器。"))
    static FX_AssetFlattenResult OrganizeAssets(UPARAM(DisplayName = "资产") const TArray<FAssetData>& Assets, UPARAM(DisplayName = "目标目录") const FString& TargetFolder);
    UFUNCTION(BlueprintCallable, Category = "XTools|资产整理", meta = (DisplayName = "按类型移动资产及依赖（自动改名冲突）"))
    static FX_AssetFlattenResult OrganizeAssetsAutoRename(UPARAM(DisplayName = "资产") const TArray<FAssetData>& Assets, UPARAM(DisplayName = "目标目录") const FString& TargetFolder);

    UFUNCTION(BlueprintCallable, Category = "XTools|资产整理", meta = (DisplayName = "按类型移动选中资产及依赖", Keywords = "移动 分类 选中 依赖", ToolTip = "将选中资产及依赖按类型移入目标目录下的 Blueprints、Materials、Meshes 等子目录。"))
    static FX_AssetFlattenResult OrganizeSelectedAssets(UPARAM(DisplayName = "目标目录") const FString& TargetFolder);

    static void ShowDialog(const TArray<FAssetData>& Assets, bool bOrganizeByType = false);

private:
    static FX_AssetFlattenResult MoveAssets(const TArray<FAssetData>& Assets, const FString& TargetFolder, bool bOrganizeByType, bool bAutoRename = false);
};
