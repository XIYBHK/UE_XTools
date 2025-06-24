// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

DECLARE_LOG_CATEGORY_EXTERN(LogX_AssetNaming, Log, All);

/**
 * 资产命名规范化管理器
 * 负责处理资产的命名规范化操作
 */
class X_ASSETEDITOR_API FX_AssetNamingManager
{
public:
    /**
     * 获取单例实例
     */
    static FX_AssetNamingManager& Get();

    /**
     * 初始化资产前缀映射
     */
    void Initialize();

    /**
     * 重命名选中的资产
     */
    void RenameSelectedAssets();

    /**
     * 获取资产的简单类名
     * @param AssetData 资产数据
     * @return 简单类名
     */
    FString GetSimpleClassName(const FAssetData& AssetData) const;

    /**
     * 获取资产类的显示名称
     * @param AssetData 资产数据
     * @return 显示名称
     */
    FString GetAssetClassDisplayName(const FAssetData& AssetData) const;

    /**
     * 获取正确的资产前缀
     * @param AssetData 资产数据
     * @param SimpleClassName 简单类名
     * @return 正确的前缀
     */
    FString GetCorrectPrefix(const FAssetData& AssetData, const FString& SimpleClassName) const;

    /**
     * 获取所有资产前缀映射
     * @return 前缀映射表
     */
    const TMap<FString, FString>& GetAssetPrefixes() const;

private:
    /** 单例实例 */
    static TUniquePtr<FX_AssetNamingManager> Instance;

    /** 资产前缀映射表 */
    TMap<FString, FString> AssetPrefixes;

    /** 私有构造函数 */
    FX_AssetNamingManager() = default;

    /**
     * 创建资产前缀映射
     */
    void CreateAssetPrefixMap();

    /**
     * 显示重命名操作结果
     * @param SuccessCount 成功数量
     * @param SkippedCount 跳过数量
     * @param FailedCount 失败数量
     * @param OperationDetails 操作详情
     */
    void ShowRenameResult(int32 SuccessCount, int32 SkippedCount, int32 FailedCount, const FString& OperationDetails) const;
};
