/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "SPhysicsOptimizerPanel.h"
#include "PhysicsOptimizerCore.h"
#include "BoneIdentificationSystem.h"

#include "PhysicsEngine/PhysicsAsset.h"
#include "Engine/SkeletalMesh.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "PhysicsAssetOptimizer"

void SPhysicsOptimizerPanel::Construct(const FArguments& InArgs)
{
	PhysicsAsset = InArgs._PhysicsAsset;
	SkeletalMesh = InArgs._SkeletalMesh;
	ParentWindow = InArgs._ParentWindow;
	
	// 默认设置
	Settings = FPhysicsOptimizerSettings();
	
	ChildSlot
	[
		SNew(SVerticalBox)
		
		// 标题
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10, 10, 10, 5)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PanelTitle", "物理资产优化设置"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
		]
		
		// 资产信息
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10, 0, 10, 10)
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				if (PhysicsAsset.IsValid())
				{
					return FText::FromString(FString::Printf(TEXT("资产: %s"), *PhysicsAsset->GetName()));
				}
				return LOCTEXT("NoAsset", "未选择资产");
			})
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10, 0)
		[
			SNew(SSeparator)
		]
		
		// 滚动区域
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(10)
		[
			SNew(SScrollBox)
			
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)
				
				// 基础设置
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					CreateBasicSettingsSection()
				]
				
				// 高级设置
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 10, 0, 0)
				[
					CreateAdvancedSettingsSection()
				]
				
				// 统计信息
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 10, 0, 0)
				[
					CreateStatsSection()
				]
			]
		]
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10, 0)
		[
			SNew(SSeparator)
		]
		
		// 按钮
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10)
		[
			CreateButtonsSection()
		]
	];
}

TSharedRef<SWidget> SPhysicsOptimizerPanel::CreateBasicSettingsSection()
{
	return SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("BasicSettings", "基础设置"))
		.InitiallyCollapsed(false)
		.BodyContent()
		[
			SNew(SVerticalBox)
			
			// 最小骨骼尺寸
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SHorizontalBox)
				
				+ SHorizontalBox::Slot()
				.FillWidth(0.6f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MinBoneSize", "最小骨骼尺寸 (cm)"))
					.ToolTipText(LOCTEXT("MinBoneSizeTooltip", "小于此长度的末端骨骼将被移除"))
				]
				
				+ SHorizontalBox::Slot()
				.FillWidth(0.4f)
				[
					SNew(SSpinBox<float>)
					.MinValue(1.0f)
					.MaxValue(20.0f)
					.Value_Lambda([this]() { return Settings.MinBoneSize; })
					.OnValueChanged_Lambda([this](float Value) { Settings.MinBoneSize = Value; })
				]
			]
			
			// 线性阻尼
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SHorizontalBox)
				
				+ SHorizontalBox::Slot()
				.FillWidth(0.6f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LinearDamping", "线性阻尼"))
					.ToolTipText(LOCTEXT("LinearDampingTooltip", "抑制线性运动的阻尼值"))
				]
				
				+ SHorizontalBox::Slot()
				.FillWidth(0.4f)
				[
					SNew(SSpinBox<float>)
					.MinValue(0.0f)
					.MaxValue(5.0f)
					.Value_Lambda([this]() { return Settings.LinearDamping; })
					.OnValueChanged_Lambda([this](float Value) { Settings.LinearDamping = Value; })
				]
			]
			
			// 角度阻尼
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SHorizontalBox)
				
				+ SHorizontalBox::Slot()
				.FillWidth(0.6f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AngularDamping", "角度阻尼"))
					.ToolTipText(LOCTEXT("AngularDampingTooltip", "抑制旋转运动的阻尼值"))
				]
				
				+ SHorizontalBox::Slot()
				.FillWidth(0.4f)
				[
					SNew(SSpinBox<float>)
					.MinValue(0.0f)
					.MaxValue(5.0f)
					.Value_Lambda([this]() { return Settings.AngularDamping; })
					.OnValueChanged_Lambda([this](float Value) { Settings.AngularDamping = Value; })
				]
			]
			
			// 基础质量
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SHorizontalBox)
				
				+ SHorizontalBox::Slot()
				.FillWidth(0.6f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BaseMass", "基础质量 (kg)"))
					.ToolTipText(LOCTEXT("BaseMassTooltip", "根骨骼的质量，子骨骼按比例递减"))
				]
				
				+ SHorizontalBox::Slot()
				.FillWidth(0.4f)
				[
					SNew(SSpinBox<float>)
					.MinValue(10.0f)
					.MaxValue(200.0f)
					.Value_Lambda([this]() { return Settings.BaseMass; })
					.OnValueChanged_Lambda([this](float Value) { Settings.BaseMass = Value; })
				]
			]
		];
}

TSharedRef<SWidget> SPhysicsOptimizerPanel::CreateAdvancedSettingsSection()
{
	return SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("AdvancedSettings", "高级设置"))
		.InitiallyCollapsed(true)
		.BodyContent()
		[
			SNew(SVerticalBox)
			
			// Sleep 阈值
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SHorizontalBox)
				
				+ SHorizontalBox::Slot()
				.FillWidth(0.6f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SleepThreshold", "Sleep 阈值"))
					.ToolTipText(LOCTEXT("SleepThresholdTooltip", "物体静止时进入休眠的速度阈值"))
				]
				
				+ SHorizontalBox::Slot()
				.FillWidth(0.4f)
				[
					SNew(SSpinBox<float>)
					.MinValue(0.01f)
					.MaxValue(1.0f)
					.Value_Lambda([this]() { return Settings.SleepThreshold; })
					.OnValueChanged_Lambda([this](float Value) { Settings.SleepThreshold = Value; })
				]
			]
			
			// 长骨阈值
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SHorizontalBox)
				
				+ SHorizontalBox::Slot()
				.FillWidth(0.6f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LongBoneThreshold", "长骨阈值 (cm)"))
					.ToolTipText(LOCTEXT("LongBoneThresholdTooltip", "超过此长度的骨骼使用凸包替代胶囊"))
				]
				
				+ SHorizontalBox::Slot()
				.FillWidth(0.4f)
				[
					SNew(SSpinBox<float>)
					.MinValue(20.0f)
					.MaxValue(100.0f)
					.Value_Lambda([this]() { return Settings.LongBoneThreshold; })
					.OnValueChanged_Lambda([this](float Value) { Settings.LongBoneThreshold = Value; })
				]
			]
			
			// 配置 Sleep
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return Settings.bConfigureSleep ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { Settings.bConfigureSleep = (State == ECheckBoxState::Checked); })
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ConfigureSleep", "配置 Sleep 设置"))
					.ToolTipText(LOCTEXT("ConfigureSleepTooltip", "启用后物体落地会快速进入休眠，节省 CPU"))
				]
			]
			
			// 使用凸包
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return Settings.bUseConvexForLongBones ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { Settings.bUseConvexForLongBones = (State == ECheckBoxState::Checked); })
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("UseConvex", "长骨使用凸包"))
					.ToolTipText(LOCTEXT("UseConvexTooltip", "对长骨使用凸包碰撞体，更贴合肌肉形状"))
				]
			]
			
			// 使用 LSV (实验性)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return Settings.bUseLSV ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { Settings.bUseLSV = (State == ECheckBoxState::Checked); })
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("UseLSV", "生成 LSV (实验性)"))
					.ToolTipText(LOCTEXT("UseLSVTooltip", "为袖口/领口生成 Level Set Volume，减少布料穿模"))
				]
			]
		];
}

TSharedRef<SWidget> SPhysicsOptimizerPanel::CreateStatsSection()
{
	return SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("Statistics", "统计信息"))
		.InitiallyCollapsed(false)
		.BodyContent()
		[
			SAssignNew(StatsText, STextBlock)
			.Text_Lambda([this]()
			{
				if (!bHasOptimized)
				{
					return LOCTEXT("NoStats", "点击\"应用优化\"后显示统计信息");
				}
				
				return FText::FromString(FString::Printf(
					TEXT("Body: %d -> %d (减少 %d)\n")
					TEXT("碰撞对: %d -> %d (减少 %d)\n")
					TEXT("移除小骨骼: %d\n")
					TEXT("耗时: %.2f ms"),
					Stats.OriginalBodyCount, Stats.FinalBodyCount, 
					Stats.OriginalBodyCount - Stats.FinalBodyCount,
					Stats.OriginalCollisionPairs, Stats.FinalCollisionPairs,
					Stats.OriginalCollisionPairs - Stats.FinalCollisionPairs,
					Stats.RemovedSmallBones,
					Stats.OptimizationTimeMs
				));
			})
		];
}

TSharedRef<SWidget> SPhysicsOptimizerPanel::CreateButtonsSection()
{
	return SNew(SHorizontalBox)
		
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(0, 0, 5, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("ResetButton", "重置"))
			.OnClicked(this, &SPhysicsOptimizerPanel::OnResetClicked)
		]
		
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(5, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("OptimizeButton", "应用优化"))
			.OnClicked(this, &SPhysicsOptimizerPanel::OnOptimizeClicked)
			.IsEnabled_Lambda([this]() { return PhysicsAsset.IsValid() && SkeletalMesh.IsValid(); })
		]
		
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(5, 0, 0, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("CloseButton", "关闭"))
			.OnClicked(this, &SPhysicsOptimizerPanel::OnCloseClicked)
		];
}

FReply SPhysicsOptimizerPanel::OnOptimizeClicked()
{
	if (!PhysicsAsset.IsValid() || !SkeletalMesh.IsValid())
	{
		return FReply::Handled();
	}
	
	bool bSuccess = FPhysicsOptimizerCore::OptimizePhysicsAsset(
		PhysicsAsset.Get(),
		SkeletalMesh.Get(),
		Settings,
		Stats
	);
	
	if (bSuccess)
	{
		bHasOptimized = true;
		
		// 刷新编辑器视图
		PhysicsAsset->RefreshPhysicsAssetChange();
		PhysicsAsset->PostEditChange();
		PhysicsAsset->MarkPackageDirty();
		
		UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 优化完成"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[物理资产优化器] 优化失败"));
	}
	
	return FReply::Handled();
}

FReply SPhysicsOptimizerPanel::OnPreviewClicked()
{
	// TODO: 实现预览功能
	return FReply::Handled();
}

FReply SPhysicsOptimizerPanel::OnCloseClicked()
{
	if (ParentWindow.IsValid())
	{
		ParentWindow.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

FReply SPhysicsOptimizerPanel::OnResetClicked()
{
	Settings = FPhysicsOptimizerSettings();
	return FReply::Handled();
}

void SPhysicsOptimizerPanel::UpdateStatsDisplay()
{
	// 统计显示通过 Lambda 自动更新
}

#undef LOCTEXT_NAMESPACE
