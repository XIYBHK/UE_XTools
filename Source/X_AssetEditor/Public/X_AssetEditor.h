// Copyright 1998-2023 Epic Games, Inc. All Rights Reserved.
#pragma once

// Engine includes
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Framework/Commands/UICommandList.h"
#include "AssetRegistry/AssetData.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogX_AssetEditor, Log, All);

// 前向声明
class FAssetToolsModule;
class UToolMenu;
class FToolBarBuilder;
class FMenuBuilder;
class AActor;
class UMaterialFunctionInterface;
class UX_MaterialToolsSettings;

/**
 * 获取资产前缀映射
 * 返回资产类型到命名前缀的映射表
 * @return 资产前缀映射表
 */
const TMap<FString, FString>& GetAssetPrefixes();

/**
 * X资产编辑器模块类
 * 提供资产编辑和管理的扩展功能
 */
class FX_AssetEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface实现 */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** 
	 * 单例访问器
	 * @return 模块实例的引用
	 */
	static FX_AssetEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FX_AssetEditorModule>("X_AssetEditor");
	}

	/**
	 * 检查模块是否已加载
	 * @return 如果模块已加载则返回true
	 */
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("X_AssetEditor");
	}

	/**
	 * 用于注册菜单扩展
	 * 在工具菜单系统初始化后调用
	 */
	void RegisterMenus();

	/**
	 * 获取资产的简化类名
	 * 移除类名中的路径和后缀
	 * @param AssetData 资产数据
	 * @return 简化的类名
	 */
	static FString GetSimpleClassName(const FAssetData& AssetData);

	/**
	 * 确定资产的正确前缀
	 * 根据资产类型查找命名规范中的前缀
	 * @param AssetData 资产数据
	 * @param SimpleClassName 简化的类名
	 * @return 资产应使用的正确前缀
	 */
	static FString GetCorrectPrefix(const FAssetData& AssetData, const FString& SimpleClassName);

	/**
	 * 重命名选中的资产
	 * 根据命名规范重命名当前选中的资产
	 */
	static void RenameSelectedAssets();

	/**
	 * 获取资产类名的显示名称
	 * @param AssetData 资产数据
	 * @return 用于显示的类名
	 */
	static FString GetAssetClassDisplayName(const FAssetData& AssetData);

protected:
	/** 注册内容浏览器菜单扩展 */
	void RegisterContentBrowserContextMenuExtender();
	void UnregisterContentBrowserContextMenuExtender();

	/** 注册关卡编辑器菜单扩展 */
	void RegisterLevelEditorContextMenuExtender();
	void UnregisterLevelEditorContextMenuExtender();

	/** 扩展内容浏览器资产选择菜单 */
	TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets);

	/** 扩展关卡编辑器Actor上下文菜单 */
	TSharedRef<FExtender> OnExtendLevelEditorActorContextMenu(TSharedRef<FUICommandList> CommandList, TArray<AActor*> SelectedActors);

	/** 添加资产命名菜单项 */
	void AddAssetNamingMenuEntry(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets);

	/** 添加材质函数菜单项 */
	void AddMaterialFunctionMenuEntry(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets);

	/** 添加材质菜单项 */
	void AddActorMaterialMenuEntry(FMenuBuilder& MenuBuilder, TArray<AActor*> SelectedActors);

	/** 材质函数功能 */
	void AddFresnelToAssets(TArray<FAssetData> SelectedAssets);

	void AddFresnelToActors(TArray<AActor*> SelectedActors);
	bool ValidateAssetPath(const FString& AssetPath);

	/** 通用材质函数功能 */
	void OnAddMaterialFunctionToAsset(TArray<FAssetData> SelectedAssets);
	void OnAddMaterialFunctionToActor(TArray<AActor*> SelectedActors);
	void ShowMaterialFunctionPicker(
		const TArray<FAssetData>& SelectedAssets,
		const TArray<AActor*>& SelectedActors,
		bool bIsActor);
	void ProcessAssetMaterialFunction(
		const TArray<FAssetData>& SelectedAssets,
		UMaterialFunctionInterface* MaterialFunction);
	void ProcessActorMaterialFunction(
		const TArray<AActor*>& SelectedActors,
		UMaterialFunctionInterface* MaterialFunction);

private:
	/** 资产选择器窗口 */
	TSharedPtr<SWindow> PickerWindow;

	/** 注册资产工具 */
	void RegisterAssetTools();

	/** 注册文件夹操作 */
	void RegisterFolderActions();

	/** 注册网格体操作 */
	void RegisterMeshActions();

	/** 注册网格体组件操作 */
	void RegisterMeshComponentActions();

	/** 注册资产编辑器操作 */
	void RegisterAssetEditorActions();

	/** 注册缩略图渲染器 */
	void RegisterThumbnailRenderer();

	/** 注册菜单扩展 */
	void RegisterMenuExtensions();

	/** 注销菜单扩展 */
	void UnregisterMenuExtensions();

	/** 添加菜单扩展 */
	void AddMenuExtension(FMenuBuilder& Builder);

	/** 注册自定义设置 */
	void RegisterSettings();

	/** 注销自定义设置 */
	void UnregisterSettings();

	/** 处理插件设置变更 */
	bool HandleSettingsSaved();

	/** 获取当前插件名称 */
	const TCHAR* GetTypeName() const { return TEXT("X_AssetEditor"); }

	/** 命令列表 */
	TSharedPtr<FUICommandList> PluginCommands;

	/** 菜单扩展器句柄 */
	FDelegateHandle ContentBrowserExtenderDelegateHandle;
	FDelegateHandle LevelEditorExtenderDelegateHandle;

	/** 菜单扩展器列表 */
	TArray<TSharedPtr<FExtender>> MenuExtenders;
};
