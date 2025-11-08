/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "AssetNaming/X_AssetNamingDelegates.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Editor.h"
#include "Subsystems/ImportSubsystem.h"
#include "Misc/CoreDelegates.h"
#include "Settings/X_AssetEditorSettings.h"
#include "Containers/Ticker.h"

DEFINE_LOG_CATEGORY(LogX_AssetNamingDelegates);

TUniquePtr<FX_AssetNamingDelegates> FX_AssetNamingDelegates::Instance = nullptr;

FX_AssetNamingDelegates& FX_AssetNamingDelegates::Get()
{
	if (!Instance.IsValid())
	{
		Instance = TUniquePtr<FX_AssetNamingDelegates>(new FX_AssetNamingDelegates());
	}
	return *Instance;
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

	// 2. 【修正】绑定 OnAssetAdded（资产添加到 AssetRegistry 时触发）
	// OnAssetAdded 在 ContentBrowser 的 DeferredItem 完成后触发
	// 这是新建资产的正确时机，不是 OnAssetRenamed（因为同名不会触发 Renamed）
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	OnAssetAddedHandle = AssetRegistry.OnAssetAdded().AddRaw(
		this, &FX_AssetNamingDelegates::OnAssetAdded
	);
	UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("已绑定到 OnAssetAdded 委托"));

	// 3. 绑定 OnAssetRenamed（用于手动重命名后的二次检查）
	OnAssetRenamedHandle = AssetRegistry.OnAssetRenamed().AddRaw(
		this, &FX_AssetNamingDelegates::OnAssetRenamed
	);
	UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("已绑定到 OnAssetRenamed 委托"));

	// 4. 【方案1：队列机制】绑定 OnFilesLoaded 委托
	// 当 AssetRegistry 完成文件加载时，处理队列中待重命名的资产
	OnFilesLoadedHandle = AssetRegistry.OnFilesLoaded().AddRaw(
		this, &FX_AssetNamingDelegates::OnFilesLoaded
	);
	UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("已绑定到 OnFilesLoaded 委托"));

	// 检查 AssetRegistry 是否已经加载完成
	bIsAssetRegistryReady = !AssetRegistry.IsLoadingAssets();
	if (bIsAssetRegistryReady)
	{
		UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("AssetRegistry 已加载完成，可以立即处理资产重命名"));
		// 如果已经加载完成，处理队列中的资产（如果有的话）
		ProcessPendingAssets();
	}
	else
	{
		UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("AssetRegistry 仍在加载中，资产将加入队列等待处理"));
	}

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

	// 3. 解绑 OnAssetRenamed
	if (OnAssetRenamedHandle.IsValid())
	{
		FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry");
		if (AssetRegistryModule)
		{
			IAssetRegistry& AssetRegistry = AssetRegistryModule->Get();
			AssetRegistry.OnAssetRenamed().Remove(OnAssetRenamedHandle);
		}
		OnAssetRenamedHandle.Reset();
	}

	// 4. 解绑 OnFilesLoaded
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

	// 清空待处理队列和重入标志
	PendingAssets.Empty();
	bIsProcessingAsset = false;
	bIsAssetRegistryReady = false;

	RenameCallback.Unbind();
	bIsActive = false;

	UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("资产命名委托已关闭"));
}

void FX_AssetNamingDelegates::OnAssetAdded(const FAssetData& AssetData)
{
	// ========== 【诊断】添加详细日志 ==========
	UE_LOG(LogX_AssetNamingDelegates, Verbose, 
		TEXT("OnAssetAdded 触发 - 资产: %s, 类型: %s, 包路径: %s, 包名: %s"),
		*AssetData.AssetName.ToString(),
		*AssetData.AssetClassPath.ToString(),
		*AssetData.PackagePath.ToString(),
		*AssetData.PackageName.ToString());

	if (!bIsActive || !RenameCallback.IsBound())
	{
		UE_LOG(LogX_AssetNamingDelegates, Verbose, 
			TEXT("检测到新资产但委托未激活 - bIsActive=%d, CallbackBound=%d: %s"), 
			bIsActive, RenameCallback.IsBound(), *AssetData.AssetName.ToString());
		return;
	}

	// ========== 【关键修复】防止递归重命名导致崩溃 ==========
	// 重命名操作会触发 OnAssetAdded，如果不加保护会导致无限递归和崩溃
	// 因为所有操作都在 GameThread 上同步执行，使用简单的重入标志即可
	if (bIsProcessingAsset)
	{
		UE_LOG(LogX_AssetNamingDelegates, Log, 
			TEXT("检测到重入调用，跳过以防止递归: %s"), 
			*AssetData.AssetName.ToString());
		return;
	}

	// 检查是否启用了"创建时自动重命名"
	const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
	if (!Settings || !Settings->bAutoRenameOnCreate)
	{
		UE_LOG(LogX_AssetNamingDelegates, Verbose, 
			TEXT("检测到新资产但创建时自动重命名已关闭 - Settings=%p, bAutoRenameOnCreate=%d: %s"), 
			Settings, Settings ? Settings->bAutoRenameOnCreate : false, *AssetData.AssetName.ToString());
		return;
	}

	// 验证资产是否需要处理
	if (!ShouldProcessAsset(AssetData))
	{
		UE_LOG(LogX_AssetNamingDelegates, Verbose, 
			TEXT("资产未通过 ShouldProcessAsset 检查: %s"), *AssetData.AssetName.ToString());
		return;
	}

	// ========== 【方案1：队列机制】检查 AssetRegistry 状态 ==========
	// 如果 AssetRegistry 仍在加载，将资产加入队列等待处理
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	// ========== 【安全修复】双重检查：标志位 + 实时状态 ==========
	// 防止标志位与实际情况不同步
	if (!bIsAssetRegistryReady || AssetRegistry.IsLoadingAssets())
	{
		// AssetRegistry 未准备好，加入队列
		PendingAssets.Add(AssetData);
		UE_LOG(LogX_AssetNamingDelegates, Verbose,
			TEXT("AssetRegistry 未准备好，资产已加入队列 - bReady=%d, IsLoading=%d: %s（包路径: %s）"),
			bIsAssetRegistryReady, AssetRegistry.IsLoadingAssets(),
			*AssetData.AssetName.ToString(), *AssetData.PackageName.ToString());
		return;
	}
	
	// ========== 【正解】OnAssetAdded 在资产添加到 AssetRegistry 后触发 ==========
	// 时序：
	// 1. ContentBrowser: Creating deferred item
	// 2. ContentBrowser: Attempting asset rename: NewMaterial -> NewMaterial
	// 3. ContentBrowser: End creating deferred item
	// 4. OnAssetAdded 触发 ← 此时资产已完全创建，可以安全重命名
	
	UE_LOG(LogX_AssetNamingDelegates, Log,
		TEXT("准备处理新增资产: %s（类型: %s, 包路径: %s）"),
		*AssetData.AssetName.ToString(), *AssetData.AssetClassPath.ToString(), 
		*AssetData.PackageName.ToString());
	
	// ========== 【关键修复】延迟执行重命名，避免在 OnAssetAdded 中直接操作 ==========
	// 在 OnAssetAdded 回调中立即执行重命名可能导致 UE 内部状态不一致
	// 使用 NextTick 延迟到下一帧执行，此时资产已完全稳定
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this, AssetData](float DeltaTime) -> bool
	{
		// 设置重入保护标志
		bIsProcessingAsset = true;
		
		// 触发重命名回调
		RenameCallback.Execute(AssetData);
		
		// 清除重入保护标志
		bIsProcessingAsset = false;
		
		// 返回 false 表示只执行一次
		return false;
	}), 0.0f);
}

void FX_AssetNamingDelegates::OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
{
	if (!bIsActive || !RenameCallback.IsBound())
	{
		return;
	}

	// 检查是否启用了"创建时自动重命名"
	const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
	if (!Settings || !Settings->bAutoRenameOnCreate)
	{
		UE_LOG(LogX_AssetNamingDelegates, Verbose, TEXT("检测到资产重命名但自动重命名已关闭: %s"), *AssetData.AssetName.ToString());
		return;
	}

	// 验证资产是否需要处理
	if (!ShouldProcessAsset(AssetData))
	{
		return;
	}

	// ========== 【补充】OnAssetRenamed 用于手动重命名后的二次检查 ==========
	// 例如：用户重命名 BP_Test -> Test，我们可以再次检查是否符合规范
	
	UE_LOG(LogX_AssetNamingDelegates, Verbose,
		TEXT("检测到资产重命名: %s（原路径: %s）"),
		*AssetData.AssetName.ToString(), *OldObjectPath);
	
	// 触发重命名回调
	RenameCallback.Execute(AssetData);
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

	// ========== 【方案1：队列机制】检查 AssetRegistry 状态 ==========
	if (!bIsAssetRegistryReady || AssetRegistry.IsLoadingAssets())
	{
		// AssetRegistry 未准备好，加入队列
		PendingAssets.Add(AssetData);
		UE_LOG(LogX_AssetNamingDelegates, Verbose,
			TEXT("AssetRegistry 未准备好，导入资产已加入队列: %s（包路径: %s）"),
			*AssetData.AssetName.ToString(), *AssetData.PackageName.ToString());
		return;
	}

	// 执行重命名回调
	RenameCallback.Execute(AssetData);
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
	UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("AssetRegistry 文件加载完成，开始处理队列中的资产"));
	
	bIsAssetRegistryReady = true;
	
	// 处理队列中待重命名的资产
	ProcessPendingAssets();
}

void FX_AssetNamingDelegates::ProcessPendingAssets()
{
	if (PendingAssets.Num() == 0)
	{
		return;
	}

	UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("开始处理队列中的 %d 个资产"), PendingAssets.Num());

	// 再次确认 AssetRegistry 状态
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	if (AssetRegistry.IsLoadingAssets())
	{
		UE_LOG(LogX_AssetNamingDelegates, Warning, 
			TEXT("AssetRegistry 仍在加载中，延迟处理队列"));
		return;
	}

	// 批量处理队列中的资产
	TArray<FAssetData> AssetsToProcess = PendingAssets;
	PendingAssets.Empty();

	int32 ProcessedCount = 0;
	int32 SkippedCount = 0;

	for (const FAssetData& AssetData : AssetsToProcess)
	{
		// 验证资产是否仍然有效（可能在队列期间被删除）
		if (!AssetData.IsValid())
		{
			SkippedCount++;
			UE_LOG(LogX_AssetNamingDelegates, Verbose, 
				TEXT("跳过无效资产（可能在队列期间被删除）"));
			continue;
		}

		// 再次验证是否需要处理
		if (!ShouldProcessAsset(AssetData))
		{
			SkippedCount++;
			continue;
		}

		// 执行重命名回调
		if (RenameCallback.IsBound())
		{
			RenameCallback.Execute(AssetData);
			ProcessedCount++;
		}
	}

	UE_LOG(LogX_AssetNamingDelegates, Log, 
		TEXT("队列处理完成: 已处理 %d 个资产，跳过 %d 个资产"), 
		ProcessedCount, SkippedCount);
}

