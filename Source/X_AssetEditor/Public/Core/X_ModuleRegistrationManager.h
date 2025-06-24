// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 模块注册管理器
 * 负责管理模块的各种注册和注销操作
 */
class X_ASSETEDITOR_API FX_ModuleRegistrationManager
{
public:
    /**
     * 获取单例实例
     */
    static FX_ModuleRegistrationManager& Get();

    /**
     * 注册所有功能
     */
    void RegisterAll();

    /**
     * 注销所有功能
     */
    void UnregisterAll();

    /**
     * 注册资产工具
     */
    void RegisterAssetTools();

    /**
     * 注册文件夹操作
     */
    void RegisterFolderActions();

    /**
     * 注册网格体操作
     */
    void RegisterMeshActions();

    /**
     * 注册网格体组件操作
     */
    void RegisterMeshComponentActions();

    /**
     * 注册资产编辑器操作
     */
    void RegisterAssetEditorActions();

    /**
     * 注册缩略图渲染器
     */
    void RegisterThumbnailRenderer();

    /**
     * 注册设置
     */
    void RegisterSettings();

    /**
     * 注销设置
     */
    void UnregisterSettings();

private:
    /** 单例实例 */
    static TUniquePtr<FX_ModuleRegistrationManager> Instance;

    /** 私有构造函数 */
    FX_ModuleRegistrationManager() = default;

    /**
     * 注册材质工具设置
     */
    void RegisterMaterialToolsSettings();

    /**
     * 注销材质工具设置
     */
    void UnregisterMaterialToolsSettings();
};
