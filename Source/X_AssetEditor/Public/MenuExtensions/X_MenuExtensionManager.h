/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

#include "CoreMinimal.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetRegistry/AssetData.h"
#include "Materials/MaterialFunctionInterface.h"

class FExtender;
class AActor;

/**
 * 菜单扩展管理器
 * 负责管理所有的菜单扩展功能
 */
class X_ASSETEDITOR_API FX_MenuExtensionManager
{
public:
    /**
     * 获取单例实例
     */
    static FX_MenuExtensionManager& Get();

    /**
     * 注册所有菜单扩展
     */
    void RegisterMenuExtensions();

    /**
     * 注销所有菜单扩展
     */
    void UnregisterMenuExtensions();

    /**
     * 注册内容浏览器上下文菜单扩展
     */
    void RegisterContentBrowserContextMenuExtender();

    /**
     * 注销内容浏览器上下文菜单扩展
     */
    void UnregisterContentBrowserContextMenuExtender();

    /**
     * 注册关卡编辑器上下文菜单扩展
     */
    void RegisterLevelEditorContextMenuExtender();

    /**
     * 注销关卡编辑器上下文菜单扩展
     */
    void UnregisterLevelEditorContextMenuExtender();

    /**
     * 注册工具菜单
     */
    void RegisterMenus();

private:
    /** 单例实例 */
    static TUniquePtr<FX_MenuExtensionManager> Instance;

    /** 内容浏览器扩展器委托句柄 */
    FDelegateHandle ContentBrowserExtenderDelegateHandle;

    /** 关卡编辑器扩展器委托句柄 */
    FDelegateHandle LevelEditorExtenderDelegateHandle;

    /** 私有构造函数 */
    FX_MenuExtensionManager() = default;

    /**
     * 扩展内容浏览器资产上下文菜单
     * @param SelectedAssets 选中的资产
     * @return 菜单扩展器
     */
    TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets);

    /**
     * 扩展关卡编辑器Actor上下文菜单
     * @param CommandList 命令列表
     * @param SelectedActors 选中的Actor
     * @return 菜单扩展器
     */
    TSharedRef<FExtender> OnExtendLevelEditorActorContextMenu(TSharedRef<FUICommandList> CommandList, TArray<AActor*> SelectedActors);

    /**
     * 添加资产命名菜单项
     * @param MenuBuilder 菜单构建器
     * @param SelectedAssets 选中的资产
     */
    void AddAssetNamingMenuEntry(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets);

    /**
     * 添加材质工具菜单项
     * @param MenuBuilder 菜单构建器
     * @param SelectedAssets 选中的资产
     */
    void AddMaterialFunctionMenuEntry(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets);

    /**
     * 处理添加材质函数到资产
     * @param SelectedAssets 选中的资产
     */
    static void HandleAddMaterialFunctionToAssets(TArray<FAssetData> SelectedAssets);

    /**
     * 材质函数选择回调
     * @param SelectedFunction 选中的材质函数
     * @param SelectedAssets 选中的资产
     */
    static void OnMaterialFunctionSelected(UMaterialFunctionInterface* SelectedFunction, TArray<FAssetData> SelectedAssets);

    /**
     * 添加碰撞管理菜单项
     * @param MenuBuilder 菜单构建器
     * @param SelectedAssets 选中的资产
     */
    void AddCollisionManagementMenuEntry(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets);

    /**
     * 添加Actor材质菜单项
     * @param MenuBuilder 菜单构建器
     * @param SelectedActors 选中的Actor
     */
    void AddActorMaterialMenuEntry(FMenuBuilder& MenuBuilder, TArray<AActor*> SelectedActors);
};
