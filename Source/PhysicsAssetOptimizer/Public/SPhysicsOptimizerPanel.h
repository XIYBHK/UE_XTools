/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "PhysicsOptimizerTypes.h"

class UPhysicsAsset;
class USkeletalMesh;
class SWindow;

/**
 * 物理资产优化设置面板
 * 
 * 提供可视化界面配置优化参数并执行优化
 */
class PHYSICSASSETOPTIMIZER_API SPhysicsOptimizerPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPhysicsOptimizerPanel)
		: _PhysicsAsset(nullptr)
		, _SkeletalMesh(nullptr)
	{}
		SLATE_ARGUMENT(UPhysicsAsset*, PhysicsAsset)
		SLATE_ARGUMENT(USkeletalMesh*, SkeletalMesh)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** 创建基础设置区域 */
	TSharedRef<SWidget> CreateBasicSettingsSection();
	
	/** 创建高级设置区域 */
	TSharedRef<SWidget> CreateAdvancedSettingsSection();
	
	/** 创建按钮区域 */
	TSharedRef<SWidget> CreateButtonsSection();
	
	/** 创建统计信息区域 */
	TSharedRef<SWidget> CreateStatsSection();

	/** 执行优化 */
	FReply OnOptimizeClicked();
	
	/** 预览优化结果 */
	FReply OnPreviewClicked();
	
	/** 关闭面板 */
	FReply OnCloseClicked();
	
	/** 重置为默认设置 */
	FReply OnResetClicked();

	/** 更新统计显示 */
	void UpdateStatsDisplay();

private:
	/** 物理资产 */
	TWeakObjectPtr<UPhysicsAsset> PhysicsAsset;
	
	/** 骨骼网格体 */
	TWeakObjectPtr<USkeletalMesh> SkeletalMesh;
	
	/** 父窗口 */
	TWeakPtr<SWindow> ParentWindow;
	
	/** 优化设置 */
	FPhysicsOptimizerSettings Settings;
	
	/** 统计信息 */
	FPhysicsOptimizerStats Stats;
	
	/** 统计文本 */
	TSharedPtr<STextBlock> StatsText;
	
	/** 是否已执行优化 */
	bool bHasOptimized = false;
};
