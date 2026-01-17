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
 * 资产命名模式结构体（支持变体命名）
 * 用于解析和生成符合变体命名规范的资产名称
 * 格式示例: BP_PlayerCharacter_Combat_01
 *          ^前缀^   ^基础名称^    ^变体^   ^后缀^
 */
struct X_ASSETEDITOR_API FAssetNamingPattern
{
	/** 前缀（如 BP_, SM_, M_） */
	FString Prefix;

	/** 基础资产名称（如 PlayerCharacter, WoodFloor） */
	FString BaseAssetName;

	/** 变体名称（如 Combat, Damaged, Red - 可选） */
	FString Variant;

	/** 后缀（非数字部分，可选） */
	FString Suffix;

	/** 数字后缀（自动格式化为两位数） */
	int32 NumericSuffix;

	FAssetNamingPattern()
		: NumericSuffix(0)
	{}

	/**
	 * 生成完整的资产名称
	 * 格式: Prefix + BaseAssetName + [Variant] + [Suffix] + _NumericSuffix
	 */
	FString GenerateFullName() const;

	/**
	 * 从现有名称解析命名模式
	 * @param AssetName 资产名称
	 * @param KnownPrefix 已知的前缀（如果有）
	 * @return 解析后的命名模式
	 */
	static FAssetNamingPattern ParseFromName(const FString& AssetName, const FString& KnownPrefix = TEXT(""));
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

    /**
     * 输出未知资产类型的诊断信息
     * @param AssetData 资产数据
     * @param SimpleClassName 简单类名
     */
    void OutputUnknownAssetDiagnostics(const FAssetData& AssetData, const FString& SimpleClassName) const;

    /**
     * 规范化数字后缀（转换为两位数格式）
     * @param AssetName 资产名称
     * @return 规范化后的名称
     */
    FString NormalizeNumericSuffix(const FString& AssetName) const;

private:
    /** 单例实例 */
    static TUniquePtr<FX_AssetNamingManager> Instance;

    /** 数字后缀正则表达式模式缓存（避免重复编译） */
    static FRegexPattern NumericSuffixPattern;

    /** 私有构造函数 */
    FX_AssetNamingManager() = default;

    /** 正在进行自动重命名的资产集合（用于防止递归重命名） */
    TSet<FString> AssetsBeingRenamed;

    /** 用户明确表示不需要自动重命名的资产集合 */
    TSet<FString> UserExcludedAssets;

    /**
     * 已按 Key 长度倒序排序的父类前缀映射缓存
     * 来自 UX_AssetEditorSettings::ParentClassPrefixMappings，只在 Initialize() 时构建
     */
    TArray<TPair<FString, FString>> SortedParentClassPrefixes;

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
     * 处理单个资产的命名规范化（供 RenameSelectedAssets 使用）
     * 封装原本在 RenameSelectedAssets 中的 per-asset 逻辑，便于维护
     */
    void ProcessSingleAssetRename(const FAssetData& AssetData,
        FX_RenameOperationResult& Result,
        class IAssetRegistry& AssetRegistry,
        class IAssetTools& AssetTools,
        const TMap<FString, TSet<FString>>& FolderNameCache);

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

    // ========== 变体命名和重名冲突处理 ==========

    /**
     * 检测重命名后的命名冲突
     * @param ProposedName 建议的新名称
     * @param PackagePath 包路径
     * @param AssetRegistry 资产注册表
     * @param ExcludePackageName 要排除的包名（当前资产）
     * @return true 如果存在冲突
     */
    bool DetectNamingConflict(
        const FString& ProposedName,
        const FString& PackagePath,
        class IAssetRegistry& AssetRegistry,
        const FString& ExcludePackageName) const;

    /**
     * 生成唯一的资产名称（处理重名冲突）
     * @param BaseName 基础名称
     * @param PackagePath 包路径
     * @param AssetRegistry 资产注册表
     * @param ExcludePackageName 要排除的包名（当前资产）
     * @return 唯一的资产名称
     */
    FString GenerateUniqueAssetName(
        const FString& BaseName,
        const FString& PackagePath,
        class IAssetRegistry& AssetRegistry,
        const FString& ExcludePackageName) const;

    /**
     * 应用变体命名规范
     * @param AssetData 资产数据
     * @param Pattern 命名模式
     * @return 符合规范的名称
     */
    FString ApplyVariantNaming(const FAssetData& AssetData, const FAssetNamingPattern& Pattern) const;

    // ========== 重构：公共辅助函数 ==========

    /**
     * 资产重命名上下文（用于传递重命名所需的所有信息）
     */
    struct FAssetRenameContext
    {
        FString CurrentName;
        FString PackagePath;
        FString PackageName;
        FString SimpleClassName;
        FString CorrectPrefix;
        UObject* AssetObject = nullptr;

        bool IsValid() const
        {
            return !CurrentName.IsEmpty() && !PackagePath.IsEmpty() && !SimpleClassName.IsEmpty();
        }
    };

    /**
     * 构建资产重命名上下文
     * @param AssetData 资产数据
     * @return 重命名上下文
     */
    FAssetRenameContext BuildRenameContext(const FAssetData& AssetData) const;

    /**
     * 计算新的资产名称（不处理冲突）
     * @param Context 重命名上下文
     * @return 建议的新名称
     */
    FString ComputeNewAssetName(const FAssetRenameContext& Context) const;

    /**
     * 解析资产名称并移除错误前缀
     * @param CurrentName 当前名称
     * @param CorrectPrefix 正确的前缀
     * @return 处理后的基础名称
     */
    static FString StripIncorrectPrefix(const FString& CurrentName, const FString& CorrectPrefix);

    /**
     * 解析资产名称并提取基础名称（移除前缀、变体、后缀、数字）
     * @param AssetName 资产名称
     * @param Prefix 已知的前缀
     * @return 基础名称
     */
    static FString ExtractBaseAssetName(const FString& AssetName, const FString& Prefix);
};
