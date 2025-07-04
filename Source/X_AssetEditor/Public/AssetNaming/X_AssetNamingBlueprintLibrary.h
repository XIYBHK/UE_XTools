// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AssetRegistry/AssetData.h"
#include "X_AssetNamingBlueprintLibrary.generated.h"

/**
 * 资产命名规范化结果结构体
 */
USTRUCT(BlueprintType)
struct FX_AssetNamingResult
{
    GENERATED_BODY()

    /** 成功重命名的资产数量 */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    int32 SuccessCount = 0;

    /** 跳过的资产数量（已符合规范） */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    int32 SkippedCount = 0;

    /** 失败的资产数量 */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    int32 FailedCount = 0;

    /** 处理的总资产数量 */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    int32 TotalCount = 0;

    /** 操作是否完全成功 */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    bool bIsSuccess = false;

    /** 操作结果消息 */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FString ResultMessage;
};

/**
 * 资产命名蓝图函数库
 * 提供蓝图可调用的资产命名功能和测试节点
 */
UCLASS()
class X_ASSETEDITOR_API UX_AssetNamingBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * 重命名选中的资产（使其符合命名规范）
     * @return 重命名操作结果
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|资产命名", meta = (DisplayName = "重命名选中资产"))
    static FX_AssetNamingResult RenameSelectedAssets();

    /**
     * 获取资产的正确前缀
     * @param AssetPath 资产路径
     * @return 正确的前缀，如果无法确定则返回空字符串
     */
    UFUNCTION(BlueprintPure, Category = "XTools|资产命名", meta = (DisplayName = "获取资产正确前缀"))
    static FString GetAssetCorrectPrefix(const FString& AssetPath);

    /**
     * 检查资产名称是否符合命名规范
     * @param AssetPath 资产路径
     * @return 是否符合命名规范
     */
    UFUNCTION(BlueprintPure, Category = "XTools|资产命名", meta = (DisplayName = "检查资产命名规范"))
    static bool IsAssetNameValid(const FString& AssetPath);

    /**
     * 获取资产的简单类名
     * @param AssetPath 资产路径
     * @return 简单类名
     */
    UFUNCTION(BlueprintPure, Category = "XTools|资产命名", meta = (DisplayName = "获取资产类名"))
    static FString GetAssetClassName(const FString& AssetPath);

    /**
     * 获取所有支持的资产类型和对应前缀
     * @param OutAssetTypes 输出：资产类型数组
     * @param OutPrefixes 输出：对应的前缀数组
     */
    UFUNCTION(BlueprintPure, Category = "XTools|资产命名", meta = (DisplayName = "获取所有资产前缀规则"))
    static void GetAllAssetPrefixRules(TArray<FString>& OutAssetTypes, TArray<FString>& OutPrefixes);

    // === 测试节点 ===

    /**
     * 测试资产命名管理器的基础功能
     * @return 测试结果消息
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|测试", meta = (DisplayName = "测试资产命名管理器"))
    static FString TestAssetNamingManager();

    /**
     * 测试前缀规则的完整性
     * @return 测试结果消息
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|测试", meta = (DisplayName = "测试前缀规则完整性"))
    static FString TestPrefixRulesIntegrity();

    /**
     * 测试资产类名解析功能
     * @param TestAssetPaths 测试用的资产路径数组
     * @return 测试结果消息
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|测试", meta = (DisplayName = "测试资产类名解析"))
    static FString TestAssetClassNameParsing(const TArray<FString>& TestAssetPaths);

    /**
     * 性能测试：批量处理资产命名
     * @param AssetCount 测试的资产数量
     * @return 性能测试结果
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|测试", meta = (DisplayName = "性能测试资产命名"))
    static FString PerformanceTestAssetNaming(int32 AssetCount = 100);

    /**
     * 验证命名规范化的正确性
     * @return 验证结果消息
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|测试", meta = (DisplayName = "验证命名规范化正确性"))
    static FString ValidateNamingNormalization();

private:
    /**
     * 从资产路径获取资产数据
     * @param AssetPath 资产路径
     * @return 资产数据，如果无效则返回空
     */
    static FAssetData GetAssetDataFromPath(const FString& AssetPath);

    /**
     * 创建测试结果消息
     * @param TestName 测试名称
     * @param bSuccess 是否成功
     * @param Details 详细信息
     * @return 格式化的测试结果消息
     */
    static FString CreateTestResultMessage(const FString& TestName, bool bSuccess, const FString& Details);
};
