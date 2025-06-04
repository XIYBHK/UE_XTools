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

// 获取资产前缀映射
const TMap<FString, FString>& GetAssetPrefixes();

class FX_AssetEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** 单例访问器 */
	static FX_AssetEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FX_AssetEditorModule>("X_AssetEditor");
	}

	/** 检查模块是否已加载 */
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("X_AssetEditor");
	}

	/** 用于注册菜单扩展 */
	void RegisterMenus();

	/** 用于获取资产的简化类名 */
	static FString GetSimpleClassName(const FAssetData& AssetData);

	/** 用于确定正确的前缀 */
	static FString GetCorrectPrefix(const FAssetData& AssetData, const FString& SimpleClassName);

	/** 重命名选中资产 */
	static void RenameSelectedAssets();

	/** 获取资产类名显示 */
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

	/** 添加材质菜单项 */
	void AddMaterialMenuEntry(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets);
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

	/** 菜单扩展器句柄 */
	FDelegateHandle ContentBrowserExtenderDelegateHandle;
	FDelegateHandle LevelEditorExtenderDelegateHandle;

	/** 菜单扩展器列表 */
	TArray<TSharedPtr<FExtender>> MenuExtenders;

	/** 命令列表 */
	TSharedPtr<FUICommandList> PluginCommands;
};
