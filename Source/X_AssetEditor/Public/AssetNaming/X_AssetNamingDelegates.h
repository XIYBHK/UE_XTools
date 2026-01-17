/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "EditorModes.h"

DECLARE_LOG_CATEGORY_EXTERN(LogX_AssetNamingDelegates, Log, All);

class UFactory;
class UObject;
class IAssetRegistry;

/**
 * 资产命名委托管理器
 *
 * 管理引擎委托以实现自动资产重命名：
 * - OnAssetPostImport: 资产导入时触发
 * - OnAssetAdded: 新建资产时触发
 *
 * 职责：
 * - 绑定/解绑引擎委托
 * - 验证委托触发条件
 * - 调用重命名回调
 *
 * 线程安全：使用 TSharedPtr/TWeakPtr 确保异步回调的安全性
 */
class X_ASSETEDITOR_API FX_AssetNamingDelegates : public TSharedFromThis<FX_AssetNamingDelegates>
{
public:
	/**
	 * 资产重命名回调委托签名
	 * @param AssetData - 可能需要重命名的资产数据
	 * @return true 如果尝试了重命名（成功或失败），false 如果跳过
	 */
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnAssetNeedsRename, const FAssetData& /* AssetData */);

	/** 获取单例实例（返回共享指针） */
	static TSharedPtr<FX_AssetNamingDelegates> Get();

	/**
	 * 初始化委托绑定
	 * @param RenameCallback - 资产需要重命名时调用的回调
	 */
	void Initialize(FOnAssetNeedsRename RenameCallback);

	/**
	 * 关闭并清理委托绑定
	 */
	void Shutdown();

	/**
	 * 检查委托是否当前处于活动状态
	 */
	bool IsActive() const { return bIsActive; }

	/**
	 * 设置是否正在处理资产（用于区分系统触发的重命名）
	 */
	void SetProcessingAsset(bool bProcessing) { bIsProcessingAsset = bProcessing; }

private:
	/** 单例实例（使用共享指针以支持弱引用） */
	static TSharedPtr<FX_AssetNamingDelegates> Instance;

	/** 私有构造函数 */
	FX_AssetNamingDelegates() = default;

	/** 重命名回调 */
	FOnAssetNeedsRename RenameCallback;

	/** 委托句柄 */
	FDelegateHandle OnAssetAddedHandle;
	FDelegateHandle OnAssetPostImportHandle;
	FDelegateHandle OnNewAssetCreatedHandle;
	FDelegateHandle OnFilesLoadedHandle;
	FDelegateHandle OnEditorModeChangedHandle;

	/** 激活状态 */
	bool bIsActive = false;

	/** 当前是否处于特殊编辑模式（通过回调跟踪） */
	bool bIsInSpecialMode = false;

	/** AssetRegistry 是否已加载完成 */
	bool bIsAssetRegistryReady = false;

	/** 重入保护标志（防止递归重命名导致崩溃） */
	bool bIsProcessingAsset = false;

	/** 最后一次工厂创建资产的时间戳 */
	double LastFactoryCreationTime = 0.0;

	/** 最后一次工厂支持的资产类（用于类型校验，防止误伤） */
	TWeakObjectPtr<UClass> LastFactorySupportedClass;

	/**
	 * 当资产添加到 AssetRegistry 时调用（新建或导入资产）
	 * @param AssetData - 新添加的资产数据
	 */
	void OnAssetAdded(const FAssetData& AssetData);

	/**
	 * 当通过工厂创建新资产时调用
	 * @param Factory - 用于创建资产的工厂
	 */
	void OnNewAssetCreated(class UFactory* Factory);

	/**
	 * 资产导入或重新导入后调用
	 * @param Factory - 用于导入的工厂
	 * @param CreatedObject - 导入的对象
	 */
	void OnAssetPostImport(class UFactory* Factory, class UObject* CreatedObject);

	/**
	 * 验证资产是否应该被处理以进行重命名
	 * @param AssetData - 要验证的资产
	 * @return true 如果应该处理，否则 false
	 */
	bool ShouldProcessAsset(const FAssetData& AssetData) const;

	/**
	 * 检测用户操作上下文，判断是否为用户主动发起的重命名操作
	 * @return true 如果检测到用户操作上下文，否则 false
	 */
	bool DetectUserOperationContext() const;

	/**
	 * 当 AssetRegistry 完成文件加载时调用
	 * 用于标记 AssetRegistry 已就绪，之后的所有 OnAssetAdded 都是新创建的资产
	 */
	void OnFilesLoaded();

	/**
	 * 检测是否处于特殊编辑模式（破碎模式、建模模式等）
	 * 这些模式会批量创建资产，不应触发自动重命名
	 * @return true 如果处于特殊编辑模式，应跳过自动重命名
	 */
	bool IsInSpecialEditorMode() const;

	/**
	 * 编辑模式切换回调
	 * @param ModeID - 切换的模式 ID
	 * @param bIsEntering - true 表示进入模式，false 表示退出模式
	 */
	void OnEditorModeChanged(const FEditorModeID& ModeID, bool bIsEntering);

	/**
	 * 绑定编辑模式切换回调
	 */
	void BindEditorModeChangedDelegate();

	/**
	 * 解绑编辑模式切换回调
	 */
	void UnbindEditorModeChangedDelegate();

	/**
	 * 检查指定模式 ID 是否为特殊模式
	 */
	static bool IsSpecialModeID(const FEditorModeID& ModeID);
};

