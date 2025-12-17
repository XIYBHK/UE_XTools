/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"

class UPhysicsAsset;
class USkeletalMesh;
class IAssetEditorInstance;
class FAssetEditorToolkit;
class FExtender;
class FToolBarBuilder;
class FUICommandList;

/**
 * 物理资产编辑器扩展
 * 
 * 在物理资产编辑器工具栏添加"自动优化"按钮
 */
class PHYSICSASSETOPTIMIZER_API FPhysicsAssetEditorExtension
{
public:
	/** 初始化扩展 */
	static void Initialize();
	
	/** 清理扩展 */
	static void Shutdown();

private:
	/** 资产编辑器打开回调 */
	static void OnAssetOpenedInEditor(UObject* Asset, IAssetEditorInstance* AssetEditor);
	
	/** 扩展工具栏 */
	static void ExtendToolbar(FToolBarBuilder& ToolbarBuilder);
	
	/** 创建下拉菜单内容 */
	static TSharedRef<SWidget> CreateMenuContent();
	
	/** 执行优化 */
	static void OnOptimizeClicked();
	
	/** 打开设置面板 */
	static void OnOpenSettingsPanel();
	
	/** 获取当前编辑的物理资产 */
	static UPhysicsAsset* GetCurrentPhysicsAsset();
	
	/** 获取物理资产对应的骨骼网格体 */
	static USkeletalMesh* GetSkeletalMeshForPhysicsAsset(UPhysicsAsset* PA);
	
	/** 刷新物理资产编辑器视图 */
	static void RefreshPhysicsAssetEditor(UPhysicsAsset* PA);

private:
	/** 引擎初始化完成后的回调 */
	static void OnPostEngineInit();

	/** 工具栏扩展器映射 (Toolkit -> Extender) */
	static TMap<TWeakPtr<FAssetEditorToolkit>, TSharedPtr<FExtender>> ToolbarExtenderMap;
	
	/** 命令列表 */
	static TSharedPtr<FUICommandList> CommandList;
	
	/** 委托句柄 */
	static FDelegateHandle AssetEditorOpenedHandle;
	
	/** 延迟初始化委托句柄 */
	static FDelegateHandle PostEngineInitHandle;
};
