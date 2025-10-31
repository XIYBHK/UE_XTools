/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "X_AssetEditorSettings.generated.h"

/**
 * 资产编辑器插件设置
 * 提供资产命名、碰撞工具等配置选项
 */
UCLASS(config=Editor, defaultconfig, meta=(DisplayName="XTools"))
class X_ASSETEDITOR_API UX_AssetEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 构造函数 */
	UX_AssetEditorSettings();

	/**
	 * 初始化默认前缀映射（在构造函数中调用）
	 * 也可以手动调用以重置为默认值
	 */
	void InitializeDefaultPrefixMappings();

private:
	/** 初始化资产前缀映射 */
	void InitializeAssetPrefixMappings();

	/** 初始化父类前缀映射 */
	void InitializeParentClassPrefixMappings();

public:

	// ========== 通用设置 ==========

	/**
	 * 导入时自动重命名资产以遵循命名规范
	 */
	UPROPERTY(config, EditAnywhere, Category="资产命名通用", meta=(
		DisplayName="导入时自动重命名",
		ToolTip="启用后，资产导入时将根据命名规则自动重命名"))
	bool bAutoRenameOnImport;

	/**
	 * 创建时自动重命名资产以遵循命名规范
	 */
	UPROPERTY(config, EditAnywhere, Category="资产命名通用", meta=(
		DisplayName="创建时自动重命名",
		ToolTip="启用后，新建资产时将根据命名规则自动重命名"))
	bool bAutoRenameOnCreate;

	/**
	 * 重命名后自动清理重定向器
	 */
	UPROPERTY(config, EditAnywhere, Category="资产命名通用", meta=(
		DisplayName="自动修复重定向器",
		ToolTip="启用后，重命名后将自动修复并删除重定向器",
		EditCondition="bAutoRenameOnImport || bAutoRenameOnCreate", EditConditionHides))
	bool bAutoFixupRedirectors;

	// ========== 排除规则 ==========

	/**
	 * 要排除自动重命名的资产类名
	 * 默认：World（关卡地图）
	 */
	UPROPERTY(config, EditAnywhere, Category="资产命名排除规则", meta=(
		DisplayName="排除的资产类",
		ToolTip="这些资产类将被排除自动重命名（例如：World, BlueprintGeneratedClass）",
		EditCondition="bAutoRenameOnImport || bAutoRenameOnCreate", EditConditionHides))
	TArray<FString> ExcludedAssetClasses;

	/**
	 * 要排除自动重命名的文件夹路径
	 * 使用以 /Game/ 或 /Engine/ 开头的相对路径
	 */
	UPROPERTY(config, EditAnywhere, Category="资产命名排除规则", meta=(
		DisplayName="排除的文件夹",
		ToolTip="这些文件夹中的资产将被排除自动重命名（例如：/Game/ThirdParty/）",
		EditCondition="bAutoRenameOnImport || bAutoRenameOnCreate", EditConditionHides))
	TArray<FString> ExcludedFolders;

	// ========== 前缀规则 ==========

	/**
	 * 资产类型到前缀的映射（可编辑）
	 * Key = 资产类名（例如："StaticMesh", "Blueprint"）
	 * Value = 前缀（例如："SM_", "BP_"）
	 *
	 * 此映射包含所有内置规则，可以编辑：
	 * - 修改现有前缀
	 * - 添加新资产类型
	 * - 删除不需要的规则
	 *
	 * 包含86种资产类型，按类别组织（在代码注释中）：
	 * 核心与通用、网格体、物理、Chaos、材质、纹理、UI、数据、
	 * 音频、粒子、Niagara、AI、动画、MetaHuman、相机、媒体、
	 * Sequencer、Paper2D、蓝图特殊类型等
	 */
	UPROPERTY(config, EditAnywhere, Category="资产命名前缀规则", meta=(
		DisplayName="资产前缀映射",
		ToolTip="资产类名到前缀的映射。您可以修改现有规则或添加新规则。",
		EditCondition="bAutoRenameOnImport || bAutoRenameOnCreate", EditConditionHides))
	TMap<FString, FString> AssetPrefixMappings;

	/**
	 * 蓝图资产的父类到前缀映射
	 * 用于基于父C++类智能检测蓝图子类
	 *
	 * Key = 父类名或部分路径（例如："ActorComponent", "PlayerController", "GameModeBase"）
	 * Value = 前缀（例如："AC_", "PC_", "GM_"）
	 *
	 * 当资产的AssetClass为"Blueprint"时，将检查此映射以确定具体类型
	 * 系统使用部分匹配，因此"ActorComponent"将匹配"/Script/Engine.ActorComponent"
	 *
	 * 优先级：更具体的匹配优先检查（例如："SceneComponent"优于"ActorComponent"）
	 */
	UPROPERTY(config, EditAnywhere, Category="资产命名前缀规则", meta=(
		DisplayName="父类前缀映射",
		ToolTip="蓝图父类名到前缀的映射。用于区分蓝图子类（例如：ActorComponent vs SceneComponent）。",
		EditCondition="bAutoRenameOnImport || bAutoRenameOnCreate", EditConditionHides))
	TMap<FString, FString> ParentClassPrefixMappings;

	// ========== 材质工具设置 ==========
	// （材质收集已内置并行优化，无需额外设置）

	// ========== 子系统开关 ==========

	/**
	 * 启用对象池子系统
	 * 
	 * 功能：提供高性能的Actor对象池，减少频繁创建/销毁开销
	 * 性能影响：启用后会在BeginPlay时分帧预热对象池
	 * 建议：仅在需要频繁生成/销毁Actor的项目中启用（如射击游戏、弹幕游戏）
	 */
	UPROPERTY(config, EditAnywhere, Category="子系统开关", meta=(
		DisplayName="启用对象池",
		ToolTip="启用后提供Actor对象池功能，适合需要频繁生成/销毁Actor的项目\n默认关闭，按需启用"))
	bool bEnableObjectPoolSubsystem;

	/**
	 * 启用增强代码流子系统
	 * 
	 * 功能：提供高级异步执行、协程、延迟任务等代码流控制
	 * 性能影响：内存占用极小（<10KB），提供强大的异步编程能力
	 * 建议：默认启用，该子系统非常轻量且功能强大
	 */
	UPROPERTY(config, EditAnywhere, Category="子系统开关", meta=(
		DisplayName="启用增强代码流",
		ToolTip="提供异步执行、协程等高级代码流功能\n默认启用（轻量级子系统）"))
	bool bEnableEnhancedCodeFlowSubsystem;

	/**
	 * 启用蓝图清理工具
	 * 
	 * 提供清理蓝图函数库中多余World Context参数的功能
	 * 使用方法：在蓝图中调用 XBlueprintLibraryCleanupTool 的静态函数
	 */
	UPROPERTY(config, EditAnywhere, Category="工具功能", meta=(
		DisplayName="启用蓝图清理工具",
		ToolTip="启用后可以清理蓝图函数库中复制时自动添加的多余World Context参数\n使用方法：在蓝图中调用 XBlueprintLibraryCleanupTool 的静态函数"))
	bool bEnableBlueprintLibraryCleanup;

	// ========== UDeveloperSettings 接口 ==========

	/**
	 * 获取设置容器名称
	 */
	virtual FName GetContainerName() const override;

	/**
	 * 获取类别名称
	 */
	virtual FName GetCategoryName() const override;

	/**
	 * 获取节名称
	 */
	virtual FName GetSectionName() const override;

#if WITH_EDITOR
	/**
	 * 获取节文本（本地化显示名称）
	 */
	virtual FText GetSectionText() const override;

	/**
	 * 获取节描述
	 */
	virtual FText GetSectionDescription() const override;

	/**
	 * 在编辑器中更改属性时调用
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

