/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

DECLARE_LOG_CATEGORY_EXTERN(LogX_AssetNaming, Log, All);

/**
 * 资产重命名操作结果结构体
 */
struct FX_RenameOperationResult
{
	int32 SuccessCount = 0;
	int32 SkippedCount = 0;
	int32 FailedCount = 0;
	TArray<FString> SuccessfulRenames;  // 旧包路径
	TArray<FString> FailedRenames;      // 失败的资产名称
};

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
     * 初始化资产前缀映射和委托
     * @return true 如果初始化成功
     */
    bool Initialize();

    /**
     * 关闭并清理
     * @return true 如果关闭成功
     */
    bool Shutdown();

    /**
     * 根据当前设置刷新委托绑定
     * 当自动重命名设置更改时调用
     */
    void RefreshDelegateBindings();

    /**
     * 重命名选中的资产
     * @return 操作结果（包含计数和详情）
     */
    FX_RenameOperationResult RenameSelectedAssets();

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
     * 检查资产是否应该被排除重命名
     * @param AssetData 要检查的资产
     * @return true 如果应该排除，否则 false
     */
    bool IsAssetExcluded(const FAssetData& AssetData) const;

private:
    /** 单例实例 */
    static TUniquePtr<FX_AssetNamingManager> Instance;

    /** 私有构造函数 */
    FX_AssetNamingManager() = default;

    /**
     * 显示重命名操作结果
     * @param Result 操作结果
     */
    void ShowRenameResult(const FX_RenameOperationResult& Result) const;

    /**
     * 重命名单个资产（内部实现）
     * @param AssetData 要重命名的资产
     * @param OutNewName 如果重命名，输出新名称
     * @return true 如果尝试了重命名（成功或失败），false 如果跳过
     */
    bool RenameAssetInternal(const FAssetData& AssetData, FString& OutNewName);

    /**
     * 重命名后修复重定向器
     * @param OldPackagePaths 可能有重定向器的旧包路径
     */
    void FixupRedirectors(const TArray<FString>& OldPackagePaths);

    /**
     * 委托管理器在资产需要重命名时的回调
     * @param AssetData 要重命名的资产
     * @return true 如果尝试了重命名
     */
    bool OnAssetNeedsRename(const FAssetData& AssetData);
};
