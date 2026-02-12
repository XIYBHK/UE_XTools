/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "AssetNaming/X_AssetNamingDelegates.h"
#include "AssetNaming/X_AssetNamingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Editor.h"
#include "Factories/Factory.h"
#include "Subsystems/ImportSubsystem.h"
#include "Misc/CoreDelegates.h"
#include "Settings/X_AssetEditorSettings.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformFilemanager.h"
#include "Internationalization/Regex.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY(LogX_AssetNamingDelegates);

// 需要排除自动重命名的特殊编辑模式（ID 来自 UE 源码）
static const TArray<FEditorModeID> GSpecialEditorModes = {
	TEXT("EM_FractureEditorMode"),        // 破碎模式 (ChaosEditor)
	TEXT("EM_ModelingToolsEditorMode"),   // 建模模式 (ModelingToolsEditorMode)
	TEXT("EM_Landscape"),                 // 地形模式
	TEXT("EM_Foliage"),                   // 植被模式
	TEXT("EM_MeshPaint"),                 // 网格体绘制模式
};

TSharedPtr<FX_AssetNamingDelegates> FX_AssetNamingDelegates::Instance = nullptr;

TSharedPtr<FX_AssetNamingDelegates> FX_AssetNamingDelegates::Get()
{
	if (!Instance.IsValid())
	{
		Instance = TSharedPtr<FX_AssetNamingDelegates>(new FX_AssetNamingDelegates());
	}
	return Instance;
}

void FX_AssetNamingDelegates::Initialize(FOnAssetNeedsRename InRenameCallback)
{
	if (bIsActive)
	{
		UE_LOG(LogX_AssetNamingDelegates, Warning, TEXT("委托已初始化；跳过"));
		return;
	}

	if (!InRenameCallback.IsBound())
	{
		UE_LOG(LogX_AssetNamingDelegates, Error, TEXT("重命名回调未绑定；无法初始化"));
		return;
	}

	RenameCallback = InRenameCallback;

	// 1. 绑定 OnAssetPostImport（用于导入的资产）
	if (GEditor)
	{
		if (UImportSubsystem* ImportSubsystem = GEditor->GetEditorSubsystem<UImportSubsystem>())
		{
			OnAssetPostImportHandle = ImportSubsystem->OnAssetPostImport.AddRaw(
				this, &FX_AssetNamingDelegates::OnAssetPostImport
			);
			UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("已绑定到 OnAssetPostImport 委托"));
		}
		else
		{
			UE_LOG(LogX_AssetNamingDelegates, Warning, TEXT("ImportSubsystem 不可用；OnAssetPostImport 未绑定"));
		}
	}

	// 2. 绑定 OnAssetAdded（资产添加到 AssetRegistry 时触发）
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	OnAssetAddedHandle = AssetRegistry.OnAssetAdded().AddRaw(
		this, &FX_AssetNamingDelegates::OnAssetAdded
	);
	UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("已绑定到 OnAssetAdded 委托"));

	// 3. 绑定 OnFilesLoaded（AssetRegistry 加载完成时触发）
	OnFilesLoadedHandle = AssetRegistry.OnFilesLoaded().AddRaw(
		this, &FX_AssetNamingDelegates::OnFilesLoaded
	);
	UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("已绑定到 OnFilesLoaded 委托"));

	// 4. 绑定 OnNewAssetCreated（用于精准识别 Factory 创建/导入操作）
	OnNewAssetCreatedHandle = FEditorDelegates::OnNewAssetCreated.AddRaw(
		this, &FX_AssetNamingDelegates::OnNewAssetCreated
	);
	UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("已绑定到 OnNewAssetCreated 委托"));

	// ========== 【关键修复】无论 AssetRegistry 是否已加载，都延迟激活 ==========
	bIsAssetRegistryReady = false;

	if (!AssetRegistry.IsLoadingAssets())
	{
		UE_LOG(LogX_AssetNamingDelegates, Log,
			TEXT("AssetRegistry 已加载完成，但仍将通过 OnFilesLoaded 延迟激活以确保安全"));
	}
	else
	{
		UE_LOG(LogX_AssetNamingDelegates, Log,
			TEXT("AssetRegistry 仍在加载中，将在 OnFilesLoaded 触发后延迟激活"));
	}

	// 5. 绑定编辑模式切换回调
	BindEditorModeChangedDelegate();

	bIsActive = true;
	UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("资产命名委托已初始化"));
}

void FX_AssetNamingDelegates::Shutdown()
{
	if (!bIsActive)
	{
		return;
	}

	// 1. 解绑 OnAssetPostImport
	if (OnAssetPostImportHandle.IsValid() && GEditor)
	{
		if (UImportSubsystem* ImportSubsystem = GEditor->GetEditorSubsystem<UImportSubsystem>())
		{
			ImportSubsystem->OnAssetPostImport.Remove(OnAssetPostImportHandle);
			OnAssetPostImportHandle.Reset();
		}
	}

	// 2. 解绑 OnAssetAdded
	if (OnAssetAddedHandle.IsValid())
	{
		FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry");
		if (AssetRegistryModule)
		{
			IAssetRegistry& AssetRegistry = AssetRegistryModule->Get();
			AssetRegistry.OnAssetAdded().Remove(OnAssetAddedHandle);
		}
		OnAssetAddedHandle.Reset();
	}

	// 3. 解绑 OnFilesLoaded
	if (OnFilesLoadedHandle.IsValid())
	{
		FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry");
		if (AssetRegistryModule)
		{
			IAssetRegistry& AssetRegistry = AssetRegistryModule->Get();
			AssetRegistry.OnFilesLoaded().Remove(OnFilesLoadedHandle);
		}
		OnFilesLoadedHandle.Reset();
	}

	// 4. 解绑 OnNewAssetCreated
	if (OnNewAssetCreatedHandle.IsValid())
	{
		FEditorDelegates::OnNewAssetCreated.Remove(OnNewAssetCreatedHandle);
		OnNewAssetCreatedHandle.Reset();
	}

	// 5. 解绑编辑模式切换回调
	UnbindEditorModeChangedDelegate();

	// ========== 【关键修复】先禁用标志，阻止新的 Lambda 执行 ==========
	// 必须在 Unbind 回调之前设置 bIsActive = false，避免竞态条件
	// 时间窗口：Lambda 检查 bIsActive 后 → Unbind 前 → Execute 时崩溃
	bIsActive = false;

	// 清空重入标志和缓存
	bIsProcessingAsset = false;
	bIsAssetRegistryReady = false;
	bIsInSpecialMode = false;

	RenameCallback.Unbind();

	UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("资产命名委托已关闭"));
}

void FX_AssetNamingDelegates::OnAssetAdded(const FAssetData& AssetData)
{
	UE_LOG(LogX_AssetNamingDelegates, Verbose,
		TEXT("OnAssetAdded 触发 - 资产: %s, 类型: %s, 包路径: %s"),
		*AssetData.AssetName.ToString(),
		*AssetData.AssetClassPath.ToString(),
		*AssetData.PackagePath.ToString());

	if (!bIsActive || !RenameCallback.IsBound())
	{
		return;
	}

	// 防止递归重命名
	if (bIsProcessingAsset)
	{
		return;
	}

	const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
	if (!Settings || !Settings->bAutoRenameOnCreate)
	{
		return;
	}

	// 跳过特殊编辑模式
	if (IsInSpecialEditorMode())
	{
		return;
	}

	// 验证资产
	if (!ShouldProcessAsset(AssetData))
	{
		return;
	}

	// 跳过非用户操作
	if (!DetectUserOperationContext())
	{
		return;
	}

	// 跳过启动时的资产
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	if (!bIsAssetRegistryReady || AssetRegistry.IsLoadingAssets())
	{
		return;
	}

	const float FactoryTimeWindow = Settings ? Settings->FactoryCreationTimeWindow : 5.0f;

	TWeakPtr<FX_AssetNamingDelegates> WeakSelf = AsShared();

	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakSelf, AssetData, FactoryTimeWindow](float DeltaTime) -> bool
	{
		TSharedPtr<FX_AssetNamingDelegates> SharedThis = WeakSelf.Pin();
		if (!SharedThis.IsValid() || !SharedThis->bIsActive || !SharedThis->RenameCallback.IsBound())
		{
			return false;
		}

		bool bIsUserAction = false;
		double CurrentTime = FPlatformTime::Seconds();
		double TimeSinceLastFactory = CurrentTime - SharedThis->LastFactoryCreationTime;

		// 通道 1: Factory 时间窗检测
		if (TimeSinceLastFactory <= FactoryTimeWindow)
		{
			// 类型匹配检查
			if (SharedThis->LastFactorySupportedClass.IsValid())
			{
				UClass* FactoryClass = SharedThis->LastFactorySupportedClass.Get();
				UClass* AssetClass = AssetData.GetClass();
				if (!AssetClass)
				{
					if (UObject* Asset = AssetData.GetAsset())
					{
						AssetClass = Asset->GetClass();
					}
				}
				if (AssetClass && !AssetClass->IsChildOf(FactoryClass))
				{
					UE_LOG(LogX_AssetNamingDelegates, Verbose,
						TEXT("类型不匹配，跳过: %s"), *AssetData.AssetName.ToString());
					return false;
				}
			}
			bIsUserAction = true;
			UE_LOG(LogX_AssetNamingDelegates, Log,
				TEXT("Factory 时间窗命中 (%.2fs): %s"), TimeSinceLastFactory, *AssetData.AssetName.ToString());
		}

		// 通道 2: 文件时间戳检测（备用，捕获拖拽导入等场景）
		if (!bIsUserAction)
		{
			FString PackagePath = AssetData.PackagePath.ToString();
			FString DiskPath = FPackageName::LongPackageNameToFilename(PackagePath, TEXT(".uasset"));
			IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();

			if (PlatformFile.FileExists(*DiskPath))
			{
				FDateTime CreationTime = PlatformFile.GetCreationTime(*DiskPath);
				FDateTime ModifiedTime = PlatformFile.GetTimeStamp(*DiskPath);
				FTimespan Age = FDateTime::Now() - CreationTime;

				// 检查条件：
				// 1. 文件在 10 秒内创建
				// 2. 创建时间等于修改时间（新创建的文件，排除复制操作）
				//    复制的文件通常会有不同的创建和修改时间
				if (Age.GetTotalSeconds() <= 10.0 && CreationTime == ModifiedTime)
				{
					bIsUserAction = true;
					UE_LOG(LogX_AssetNamingDelegates, Log,
						TEXT("文件时间戳命中 (%.1fs): %s"), Age.GetTotalSeconds(), *AssetData.AssetName.ToString());
				}
			}
		}

		if (!bIsUserAction)
		{
			UE_LOG(LogX_AssetNamingDelegates, Verbose,
				TEXT("所有检测通道未命中，跳过: %s"), *AssetData.AssetName.ToString());
			return false;
		}

		// 执行重命名
		SharedThis->bIsProcessingAsset = true;
		SharedThis->RenameCallback.Execute(AssetData);
		SharedThis->bIsProcessingAsset = false;

		return false;
	}), 0.1f);
}

void FX_AssetNamingDelegates::OnNewAssetCreated(UFactory* Factory)
{
	// 更新最后一次工厂创建资产的时间戳
	LastFactoryCreationTime = FPlatformTime::Seconds();
	
	if (Factory)
	{
		LastFactorySupportedClass = Factory->GetSupportedClass();
	}
	else
	{
		LastFactorySupportedClass.Reset();
	}
	
	UE_LOG(LogX_AssetNamingDelegates, Verbose, 
		TEXT("FEditorDelegates::OnNewAssetCreated 触发 (Factory: %s, Class: %s), 更新时间戳"), 
		Factory ? *Factory->GetName() : TEXT("None"),
		LastFactorySupportedClass.IsValid() ? *LastFactorySupportedClass->GetName() : TEXT("None"));
}

void FX_AssetNamingDelegates::OnAssetPostImport(UFactory* Factory, UObject* CreatedObject)
{
	if (!bIsActive || !RenameCallback.IsBound() || !CreatedObject)
	{
		return;
	}

	// 检查是否启用了"导入时自动重命名"
	const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
	if (!Settings || !Settings->bAutoRenameOnImport)
	{
		UE_LOG(LogX_AssetNamingDelegates, Verbose, TEXT("检测到资产导入但导入时自动重命名已关闭: %s"), *CreatedObject->GetName());
		return;
	}

	// 检查是否处于特殊编辑模式（破碎模式、建模模式等）
	if (IsInSpecialEditorMode())
	{
		UE_LOG(LogX_AssetNamingDelegates, Verbose,
			TEXT("处于特殊编辑模式，跳过导入资产自动重命名: %s"), *CreatedObject->GetName());
		return;
	}

	// 从创建的对象获取资产数据
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(CreatedObject));
	if (!AssetData.IsValid())
	{
		return;
	}

	// 验证资产后再处理
	if (!ShouldProcessAsset(AssetData))
	{
		return;
	}

	// ========== 【新增】检测是否为用户主动操作 ==========
	if (!DetectUserOperationContext())
	{
		UE_LOG(LogX_AssetNamingDelegates, Log, 
			TEXT("检测到非用户操作上下文，跳过导入资产自动重命名: %s"), 
			*AssetData.AssetName.ToString());
		return;
	}

	// ========== 【优化】检查 AssetRegistry 状态 ==========
	// 如果 AssetRegistry 仍在加载，说明这是启动时导入的资产，跳过处理
	if (!bIsAssetRegistryReady || AssetRegistry.IsLoadingAssets())
	{
		UE_LOG(LogX_AssetNamingDelegates, Verbose,
			TEXT("AssetRegistry 加载中，跳过导入资产: %s（包路径: %s）"),
			*AssetData.AssetName.ToString(), *AssetData.PackageName.ToString());
		return;
	}

	// ========== 【关键修复】添加重入保护 ==========
	// 重命名操作可能触发导入事件，需要防止递归调用
	if (bIsProcessingAsset)
	{
		UE_LOG(LogX_AssetNamingDelegates, Log,
			TEXT("检测到重入调用（导入），跳过以防止递归: %s"),
			*AssetData.AssetName.ToString());
		return;
	}

	// 设置重入保护标志
	bIsProcessingAsset = true;

	// 执行重命名回调
	RenameCallback.Execute(AssetData);

	// 清除重入保护标志
	bIsProcessingAsset = false;
}

bool FX_AssetNamingDelegates::ShouldProcessAsset(const FAssetData& AssetData) const
{
	if (!AssetData.IsValid())
	{
		UE_LOG(LogX_AssetNamingDelegates, Verbose, TEXT("资产数据无效"));
		return false;
	}

	// 跳过重定向器（由重命名操作创建）
	if (AssetData.AssetClassPath.GetAssetName() == TEXT("ObjectRedirector"))
	{
		UE_LOG(LogX_AssetNamingDelegates, Verbose, 
			TEXT("跳过重定向器: %s"), *AssetData.AssetName.ToString());
		return false;
	}

	// 跳过引擎内容（避免干扰引擎资产）
	FString PackagePath = AssetData.PackagePath.ToString();
	if (PackagePath.StartsWith(TEXT("/Engine/")))
	{
		UE_LOG(LogX_AssetNamingDelegates, Verbose, 
			TEXT("跳过引擎内容: %s"), *AssetData.AssetName.ToString());
		return false;
	}

	// 跳过临时包（关卡中的临时对象）
	if (PackagePath.StartsWith(TEXT("/Temp/")))
	{
		UE_LOG(LogX_AssetNamingDelegates, Verbose, 
			TEXT("跳过临时包: %s"), *AssetData.AssetName.ToString());
		return false;
	}

	// 【重要】仅处理 /Game 路径下的资产
	// 跳过插件内容、StarterContent 等，避免启动时批量处理非项目资产
	// 【修复】/Game 根目录的资产，其 PackagePath 是 "/Game"（无末尾斜杠）
	// 所以检查时不应包含末尾斜杠，否则根目录资产会被错误排除
	// 【安全】同时排除 /Game/Developers 等特殊路径
	if (!PackagePath.StartsWith(TEXT("/Game")) || PackagePath.StartsWith(TEXT("/Game/Developers")))
	{
		UE_LOG(LogX_AssetNamingDelegates, Verbose, 
			TEXT("资产不在 /Game 路径下或在特殊路径中，跳过: %s (路径: %s)"),
			*AssetData.AssetName.ToString(), *PackagePath);
		return false;
	}

	// 跳过关卡中的Actor对象（这些对象的包路径包含关卡路径）
	// 例如：/Game/Maps/MyLevel.MyLevel:PersistentLevel.StaticMeshActor_0
	FString PackageName = AssetData.PackageName.ToString();
	if (PackageName.Contains(TEXT(":")) || PackageName.Contains(TEXT(".")))
	{
		// 包含 ":" 或 "." 通常表示是关卡内的子对象
		UE_LOG(LogX_AssetNamingDelegates, Verbose, 
			TEXT("跳过关卡内子对象: %s (包名: %s)"),
			*AssetData.AssetName.ToString(), *PackageName);
		return false;
	}

	// 跳过 World 相关的特殊对象
	FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();
	if (ClassName == TEXT("WorldDataLayers") || 
		ClassName == TEXT("ActorFolder") || 
		ClassName == TEXT("WorldPartitionMiniMap"))
	{
		UE_LOG(LogX_AssetNamingDelegates, Verbose, 
			TEXT("跳过 World 特殊对象: %s (类型: %s)"),
			*AssetData.AssetName.ToString(), *ClassName);
		return false;
	}

	UE_LOG(LogX_AssetNamingDelegates, Verbose, 
		TEXT("资产通过 ShouldProcessAsset 检查: %s"), *AssetData.AssetName.ToString());
	return true;
}

void FX_AssetNamingDelegates::OnFilesLoaded()
{
	const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
	const float ActivationDelay = Settings ? Settings->StartupActivationDelay : 30.0f;

	UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("AssetRegistry 文件加载完成，延迟 %.0f 秒后开始处理新创建的资产"), ActivationDelay);

	// ========== 【关键修复】延迟激活，避免启动时的资产操作被误处理 ==========
	// OnFilesLoaded 只表示初始文件扫描完成，但引擎可能仍在加载资产并触发各种事件
	// 延迟激活确保所有启动时的资产加载、内部重命名操作和编辑器初始化都已完成
	// 这是一个保守的延迟时间，避免在启动阶段的任何资产操作被误认为用户操作

	// ========== 【关键修复】使用弱指针确保 Lambda 安全性 ==========
	TWeakPtr<FX_AssetNamingDelegates> WeakSelf = AsShared();

	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakSelf, ActivationDelay](float DeltaTime) -> bool
	{
		// ========== 【安全检查】尝试提升弱指针为强指针 ==========
		TSharedPtr<FX_AssetNamingDelegates> SharedThis = WeakSelf.Pin();
		if (!SharedThis.IsValid() || !SharedThis->bIsActive)
		{
			UE_LOG(LogX_AssetNamingDelegates, Verbose,
				TEXT("延迟激活 Lambda 执行时委托已失效，跳过"));
			return false;
		}

		// 再次检查 AssetRegistry 状态，确保真的完成加载
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		if (AssetRegistry.IsLoadingAssets())
		{
			// 如果延迟后 AssetRegistry 仍在加载，再延迟 10 秒
			UE_LOG(LogX_AssetNamingDelegates, Warning,
				TEXT("延迟激活超时但 AssetRegistry 仍在加载，再延迟 10 秒"));

			// 再次使用弱指针确保安全性
			TWeakPtr<FX_AssetNamingDelegates> WeakThis2 = SharedThis->AsShared();
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis2](float DT) -> bool
			{
				TSharedPtr<FX_AssetNamingDelegates> SharedThis2 = WeakThis2.Pin();
				if (!SharedThis2.IsValid())
				{
					UE_LOG(LogX_AssetNamingDelegates, Verbose,
						TEXT("二次延迟激活 Lambda 执行时委托已失效，跳过"));
					return false;
				}

				SharedThis2->bIsAssetRegistryReady = true;
				UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("延迟激活完成（强制），现在开始处理新创建的资产"));
				return false;
			}), 10.0f);
		}
		else
		{
			SharedThis->bIsAssetRegistryReady = true;
			UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("延迟激活完成，现在开始处理新创建的资产"));
		}

		return false; // 只执行一次
	}), ActivationDelay);
}

bool FX_AssetNamingDelegates::DetectUserOperationContext() const
{
	// 基础环境检查
	if (!GIsEditor || IsRunningCommandlet())
	{
		return false;
	}

	if (!GEditor)
	{
		return false;
	}

	// 排除自动化测试
	if (GIsAutomationTesting)
	{
		return false;
	}

	// 排除 Cooker
	if (GIsCookerLoadingPackage)
	{
		return false;
	}

	// 排除 PIE/SIE 模式
	if (GEditor->IsPlayingSessionInEditor() ||
		GEditor->GetPIEWorldContext() ||
		GEditor->IsSimulatingInEditor())
	{
		return false;
	}

	// 检查 Slate 应用是否就绪（用户交互环境）
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	// 检查是否有活动窗口（用户正在使用编辑器）
	const FSlateApplication& SlateApp = FSlateApplication::Get();
	if (!SlateApp.GetActiveTopLevelWindow().IsValid())
	{
		return false;
	}

	return true;
}

bool FX_AssetNamingDelegates::IsInSpecialEditorMode() const
{
	// 直接返回通过回调跟踪的状态
	return bIsInSpecialMode;
}

bool FX_AssetNamingDelegates::IsSpecialModeID(const FEditorModeID& ModeID)
{
	return GSpecialEditorModes.Contains(ModeID);
}

void FX_AssetNamingDelegates::OnEditorModeChanged(const FEditorModeID& ModeID, bool bIsEntering)
{
	if (!IsSpecialModeID(ModeID))
	{
		return;
	}

	bIsInSpecialMode = bIsEntering;

	UE_LOG(LogX_AssetNamingDelegates, Log,
		TEXT("编辑模式切换: %s %s，自动重命名%s"),
		*ModeID.ToString(),
		bIsEntering ? TEXT("进入") : TEXT("退出"),
		bIsInSpecialMode ? TEXT("已禁用") : TEXT("已启用"));
}

void FX_AssetNamingDelegates::BindEditorModeChangedDelegate()
{
	if (OnEditorModeChangedHandle.IsValid())
	{
		return;
	}

	// 延迟绑定，确保 GLevelEditorModeTools 已初始化
	// 使用弱引用避免模块卸载时的悬空指针
	TWeakPtr<FX_AssetNamingDelegates> WeakSelf = AsShared();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakSelf](float) -> bool
	{
		TSharedPtr<FX_AssetNamingDelegates> SharedThis = WeakSelf.Pin();
		if (!SharedThis.IsValid())
		{
			return false; // 对象已销毁，停止 Ticker
		}

		if (!SharedThis->bIsActive)
		{
			return false;
		}

		if (!GEditor || GEditor->GetWorldContexts().Num() == 0)
		{
			return true; // 编辑器尚未就绪，下一帧重试
		}

		FEditorModeTools& ModeTools = GLevelEditorModeTools();
		SharedThis->OnEditorModeChangedHandle = ModeTools.OnEditorModeIDChanged().AddRaw(
			SharedThis.Get(), &FX_AssetNamingDelegates::OnEditorModeChanged
		);

		// 检查当前是否已在特殊模式中
		for (const FEditorModeID& ModeID : GSpecialEditorModes)
		{
			if (ModeTools.IsModeActive(ModeID))
			{
				SharedThis->bIsInSpecialMode = true;
				UE_LOG(LogX_AssetNamingDelegates, Log,
					TEXT("检测到当前已在特殊模式 %s 中，自动重命名已禁用"), *ModeID.ToString());
				break;
			}
		}

		UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("已绑定到 OnEditorModeIDChanged 委托"));
		return false;
	}), 0.1f);
}

void FX_AssetNamingDelegates::UnbindEditorModeChangedDelegate()
{
	if (!OnEditorModeChangedHandle.IsValid())
	{
		return;
	}

	if (GEditor && GEditor->GetWorldContexts().Num() > 0)
	{
		FEditorModeTools& ModeTools = GLevelEditorModeTools();
		ModeTools.OnEditorModeIDChanged().Remove(OnEditorModeChangedHandle);
	}

	OnEditorModeChangedHandle.Reset();
}
