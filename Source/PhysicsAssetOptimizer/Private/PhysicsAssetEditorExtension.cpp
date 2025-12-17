/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "PhysicsAssetEditorExtension.h"
#include "PhysicsOptimizerCore.h"
#include "BoneIdentificationSystem.h"
#include "SPhysicsOptimizerPanel.h"

#include "PhysicsAssetEditorModule.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Engine/SkeletalMesh.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Docking/TabManager.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWindow.h"
#include "Misc/CoreDelegates.h"

#define LOCTEXT_NAMESPACE "PhysicsAssetOptimizer"

// 静态成员初始化
TMap<TWeakPtr<FAssetEditorToolkit>, TSharedPtr<FExtender>> FPhysicsAssetEditorExtension::ToolbarExtenderMap;
TSharedPtr<FUICommandList> FPhysicsAssetEditorExtension::CommandList;
FDelegateHandle FPhysicsAssetEditorExtension::AssetEditorOpenedHandle;
FDelegateHandle FPhysicsAssetEditorExtension::PostEngineInitHandle;

void FPhysicsAssetEditorExtension::Initialize()
{
	// 延迟到引擎初始化完成后再注册，避免 GEditor 为空
	PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddStatic(
		&FPhysicsAssetEditorExtension::OnPostEngineInit
	);
	
	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 编辑器扩展已注册延迟初始化"));
}

void FPhysicsAssetEditorExtension::OnPostEngineInit()
{
	// 创建命令列表
	CommandList = MakeShareable(new FUICommandList);
	
	// 监听资产编辑器打开事件
	if (GEditor)
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (AssetEditorSubsystem)
		{
			AssetEditorOpenedHandle = AssetEditorSubsystem->OnAssetOpenedInEditor().AddStatic(
				&FPhysicsAssetEditorExtension::OnAssetOpenedInEditor
			);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 编辑器扩展已初始化"));
}

void FPhysicsAssetEditorExtension::Shutdown()
{
	// 移除延迟初始化委托
	if (PostEngineInitHandle.IsValid())
	{
		FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
	}
	
	// 移除资产编辑器委托
	if (AssetEditorOpenedHandle.IsValid())
	{
		if (GEditor)
		{
			UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
			if (AssetEditorSubsystem)
			{
				AssetEditorSubsystem->OnAssetOpenedInEditor().Remove(AssetEditorOpenedHandle);
			}
		}
		AssetEditorOpenedHandle.Reset();
	}
	
	// 清理扩展器
	ToolbarExtenderMap.Empty();
	CommandList.Reset();
	
	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 编辑器扩展已清理"));
}

void FPhysicsAssetEditorExtension::OnAssetOpenedInEditor(UObject* Asset, IAssetEditorInstance* AssetEditor)
{
	if (!Asset || !AssetEditor)
	{
		return;
	}
	
	// 只处理物理资产编辑器
	if (AssetEditor->GetEditorName() != TEXT("PhysicsAssetEditor"))
	{
		return;
	}
	
	FAssetEditorToolkit* AssetEditorToolkit = static_cast<FAssetEditorToolkit*>(AssetEditor);
	if (!AssetEditorToolkit)
	{
		return;
	}
	
	TWeakPtr<FAssetEditorToolkit> WeakToolkit = AssetEditorToolkit->AsShared();
	TSharedRef<FUICommandList> ToolkitCommands = AssetEditorToolkit->GetToolkitCommands();
	
	// 移除旧的扩展器
	TSharedPtr<FExtender> OldExtender = ToolbarExtenderMap.FindRef(WeakToolkit);
	if (OldExtender.IsValid())
	{
		AssetEditorToolkit->RemoveToolbarExtender(OldExtender);
	}
	
	// 创建新的工具栏扩展器
	TSharedRef<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	
	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		ToolkitCommands,
		FToolBarExtensionDelegate::CreateStatic(&FPhysicsAssetEditorExtension::ExtendToolbar)
	);
	
	ToolbarExtenderMap.Add(WeakToolkit, ToolbarExtender);
	AssetEditorToolkit->AddToolbarExtender(ToolbarExtender);
	
	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 已扩展物理资产编辑器工具栏"));
}

void FPhysicsAssetEditorExtension::ExtendToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateStatic(&FPhysicsAssetEditorExtension::CreateMenuContent),
		LOCTEXT("OptimizeButton", "自动优化"),
		LOCTEXT("OptimizeButtonTooltip", "一键优化物理资产，应用12条硬规则"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "PhysicsAssetEditor.EnableCollision")
	);
}

TSharedRef<SWidget> FPhysicsAssetEditorExtension::CreateMenuContent()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, CommandList);
	
	MenuBuilder.BeginSection("QuickActions", LOCTEXT("QuickActionsSection", "快速操作"));
	{
		// 一键优化（使用默认设置）
		MenuBuilder.AddMenuEntry(
			LOCTEXT("QuickOptimize", "一键优化"),
			LOCTEXT("QuickOptimizeTooltip", "使用默认设置快速优化物理资产"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "PhysicsAssetEditor.EnableCollision"),
			FUIAction(FExecuteAction::CreateStatic(&FPhysicsAssetEditorExtension::OnOptimizeClicked))
		);
		
		// 打开设置面板
		MenuBuilder.AddMenuEntry(
			LOCTEXT("OpenSettings", "优化设置..."),
			LOCTEXT("OpenSettingsTooltip", "打开优化设置面板，自定义优化参数"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "PhysicsAssetEditor.Properties"),
			FUIAction(FExecuteAction::CreateStatic(&FPhysicsAssetEditorExtension::OnOpenSettingsPanel))
		);
	}
	MenuBuilder.EndSection();
	
	return MenuBuilder.MakeWidget();
}

void FPhysicsAssetEditorExtension::OnOptimizeClicked()
{
	UPhysicsAsset* PA = GetCurrentPhysicsAsset();
	if (!PA)
	{
		UE_LOG(LogTemp, Warning, TEXT("[物理资产优化器] 未找到当前物理资产"));
		return;
	}
	
	USkeletalMesh* Mesh = GetSkeletalMeshForPhysicsAsset(PA);
	if (!Mesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[物理资产优化器] 未找到对应的骨骼网格体"));
		return;
	}
	
	// 使用默认设置执行优化
	FPhysicsOptimizerSettings Settings;
	FPhysicsOptimizerStats Stats;
	
	bool bSuccess = FPhysicsOptimizerCore::OptimizePhysicsAsset(PA, Mesh, Settings, Stats);
	
	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 优化完成: Body %d->%d, 碰撞对 %d->%d, 耗时 %.2fms"),
			Stats.OriginalBodyCount, Stats.FinalBodyCount,
			Stats.OriginalCollisionPairs, Stats.FinalCollisionPairs,
			Stats.OptimizationTimeMs);
		
		// 刷新编辑器视图
		RefreshPhysicsAssetEditor(PA);
		
		// 标记资产已修改
		PA->MarkPackageDirty();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[物理资产优化器] 优化失败"));
	}
}

void FPhysicsAssetEditorExtension::OnOpenSettingsPanel()
{
	UPhysicsAsset* PA = GetCurrentPhysicsAsset();
	USkeletalMesh* Mesh = PA ? GetSkeletalMeshForPhysicsAsset(PA) : nullptr;
	
	// 创建设置窗口
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("SettingsWindowTitle", "物理资产优化设置"))
		.ClientSize(FVector2D(450, 500))
		.SupportsMinimize(false)
		.SupportsMaximize(false);
	
	Window->SetContent(
		SNew(SPhysicsOptimizerPanel)
		.PhysicsAsset(PA)
		.SkeletalMesh(Mesh)
		.ParentWindow(Window)
	);
	
	FSlateApplication::Get().AddWindow(Window);
}

UPhysicsAsset* FPhysicsAssetEditorExtension::GetCurrentPhysicsAsset()
{
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		return nullptr;
	}
	
	// 获取所有打开的资产
	TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
	for (UObject* Asset : EditedAssets)
	{
		if (UPhysicsAsset* PA = Cast<UPhysicsAsset>(Asset))
		{
			return PA;
		}
	}
	
	return nullptr;
}

USkeletalMesh* FPhysicsAssetEditorExtension::GetSkeletalMeshForPhysicsAsset(UPhysicsAsset* PA)
{
	if (!PA)
	{
		return nullptr;
	}
	
	// 通过 PreviewMesh 获取
	TSoftObjectPtr<USkeletalMesh> PreviewMesh = PA->PreviewSkeletalMesh;
	if (PreviewMesh.IsValid())
	{
		return PreviewMesh.Get();
	}
	
	// 尝试加载
	if (!PreviewMesh.IsNull())
	{
		return PreviewMesh.LoadSynchronous();
	}
	
	return nullptr;
}

void FPhysicsAssetEditorExtension::RefreshPhysicsAssetEditor(UPhysicsAsset* PA)
{
	if (!PA)
	{
		return;
	}
	
	// 通知物理资产已更改，触发编辑器刷新
	PA->RefreshPhysicsAssetChange();
	
	// 广播属性变更通知
	PA->PostEditChange();
	
	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 已刷新编辑器视图"));
}

#undef LOCTEXT_NAMESPACE
