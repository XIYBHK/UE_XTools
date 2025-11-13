/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "AssetNaming/X_AssetNamingDelegates.h"
#include "AssetNaming/X_AssetNamingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Editor.h"
#include "Subsystems/ImportSubsystem.h"
#include "Misc/CoreDelegates.h"
#include "Settings/X_AssetEditorSettings.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/ThreadSafeCounter64.h"
#include "Internationalization/Regex.h"

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

	// 4. 绑定 OnFilesLoaded（AssetRegistry 加载完成时触发）
	// 用于区分启动时扫描的已存在资产和用户新创建的资产
	OnFilesLoadedHandle = AssetRegistry.OnFilesLoaded().AddRaw(
		this, &FX_AssetNamingDelegates::OnFilesLoaded
	);
	UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("已绑定到 OnFilesLoaded 委托"));

	// 检查 AssetRegistry 是否已经加载完成
	bIsAssetRegistryReady = !AssetRegistry.IsLoadingAssets();
	if (bIsAssetRegistryReady)
	{
		UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("AssetRegistry 已加载完成，可以立即处理资产重命名"));
	}
	else
	{
		UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("AssetRegistry 仍在加载中，启动时的资产将被跳过，加载完成后新创建的资产将被处理"));
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

	// 清空重入标志和缓存
	bIsProcessingAsset = false;
	bIsAssetRegistryReady = false;
	RecentManualRenames.Empty();

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

	// ========== 【关键修复】检查是否是最近手动重命名的资产 ==========
	FString PackagePath = AssetData.PackageName.ToString();
	double CurrentTime = FPlatformTime::Seconds();
	
	// 清理过期的缓存条目（超过5秒）
	const double CacheTimeout = 5.0;
	TArray<FString> ExpiredKeys;
	for (const auto& Pair : RecentManualRenames)
	{
		if (CurrentTime - Pair.Value > CacheTimeout)
		{
			ExpiredKeys.Add(Pair.Key);
		}
	}
	for (const FString& Key : ExpiredKeys)
	{
		RecentManualRenames.Remove(Key);
	}
	
	// 检查当前资产是否在手动重命名缓存中
	if (RecentManualRenames.Contains(PackagePath))
	{
		UE_LOG(LogX_AssetNamingDelegates, Log,
			TEXT("跳过最近手动重命名的资产: %s"), *AssetData.AssetName.ToString());
		
		// 从缓存中移除，避免影响后续的正常重命名
		RecentManualRenames.Remove(PackagePath);
		return;
	}

	// ========== 【新增】检查是否是相似名称的新资产（扩展保护范围） ==========
	FString CurrentAssetName = AssetData.AssetName.ToString();
	FString CurrentFolderPath = FPackageName::GetLongPackagePath(PackagePath);
	
	// 检查同一文件夹中是否有最近手动重命名的相似资产
	for (const auto& CachedPair : RecentManualRenames)
	{
		FString CachedPackagePath = CachedPair.Key;
		FString CachedFolderPath = FPackageName::GetLongPackagePath(CachedPackagePath);
		
		// 如果在同一文件夹
		if (CurrentFolderPath == CachedFolderPath)
		{
			FString CachedAssetName = FPackageName::GetShortName(CachedPackagePath);
			
			// 检查是否是相似的变体名称（如 AnimLayerInterface vs ABP_AnimLayerInterface_1）
			// 移除前缀和数字后缀进行比较
			FString CurrentBaseName = CurrentAssetName;
			FString CachedBaseName = CachedAssetName;
			
			// 移除常见前缀
			const UX_AssetEditorSettings* PrefixSettings = GetDefault<UX_AssetEditorSettings>();
			if (PrefixSettings)
			{
				for (const auto& PrefixPair : PrefixSettings->AssetPrefixMappings)
				{
					const FString& Prefix = PrefixPair.Value;
					if (!Prefix.IsEmpty())
					{
						if (CurrentBaseName.StartsWith(Prefix))
						{
							CurrentBaseName = CurrentBaseName.RightChop(Prefix.Len());
						}
						if (CachedBaseName.StartsWith(Prefix))
						{
							CachedBaseName = CachedBaseName.RightChop(Prefix.Len());
						}
					}
				}
			}
			
			// 移除数字后缀（如 _1, _01, _001）
			FRegexPattern NumericSuffixPattern(TEXT("_[0-9]+$"));
			FRegexMatcher CurrentMatcher(NumericSuffixPattern, CurrentBaseName);
			if (CurrentMatcher.FindNext())
			{
				CurrentBaseName = CurrentBaseName.Left(CurrentMatcher.GetMatchBeginning());
			}
			
			FRegexMatcher CachedMatcher(NumericSuffixPattern, CachedBaseName);
			if (CachedMatcher.FindNext())
			{
				CachedBaseName = CachedBaseName.Left(CachedMatcher.GetMatchBeginning());
			}
			
			// 如果基础名称相同，说明是相关的变体资产
			if (CurrentBaseName.Equals(CachedBaseName, ESearchCase::IgnoreCase))
			{
				UE_LOG(LogX_AssetNamingDelegates, Log,
					TEXT("跳过相似名称的新资产（可能是手动重命名的变体）: %s (基础名称: %s, 相关缓存: %s)"), 
					*CurrentAssetName, *CurrentBaseName, *CachedAssetName);
				return;
			}
		}
	}

	// ========== 【优化】启动时跳过已存在的资产 ==========
	// AssetRegistry 加载期间触发的 OnAssetAdded 是已存在的资产，不需要重命名
	// 只处理 AssetRegistry 加载完成后新创建/导入的资产
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	if (!bIsAssetRegistryReady || AssetRegistry.IsLoadingAssets())
	{
		// AssetRegistry 仍在加载中，说明这是启动时扫描到的已存在资产
		// 跳过处理，避免启动时批量检查所有资产
		UE_LOG(LogX_AssetNamingDelegates, Verbose,
			TEXT("AssetRegistry 加载中，跳过已存在资产: %s"),
			*AssetData.AssetName.ToString());
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
		TEXT("OnAssetRenamed triggered: %s（原路径: %s）"),
		*AssetData.AssetName.ToString(), *OldObjectPath);
	
	// 检查是否是用户手动重命名（从规范名称改为非规范名称）
	FString CurrentName = AssetData.AssetName.ToString();
	
	// 从旧路径提取旧名称
	FString OldName;
	int32 LastSlashIndex;
	if (OldObjectPath.FindLastChar('/', LastSlashIndex))
	{
		FString ObjectName = OldObjectPath.RightChop(LastSlashIndex + 1);
		int32 DotIndex;
		if (ObjectName.FindChar('.', DotIndex))
		{
			OldName = ObjectName.Left(DotIndex);
		}
	}
	
	// ========== 【最简方案】基于调用堆栈的手动重命名检测 ==========
	// 参考网络上的成熟方案：如果重命名不是由我们的系统触发的，就认为是手动重命名
	
	UE_LOG(LogX_AssetNamingDelegates, Verbose,
		TEXT("Rename detected: '%s' -> '%s'"), *OldName, *CurrentName);
	
	// 检查调用堆栈，看是否是我们的系统触发的重命名
	bool bIsOurSystemRename = false;
	
	// 简单的检测：如果当前正在处理资产，说明是我们触发的
	if (bIsProcessingAsset)
	{
		bIsOurSystemRename = true;
	}
	
	UE_LOG(LogX_AssetNamingDelegates, Log,
		TEXT("Rename analysis: '%s' -> '%s' | IsOurSystemRename=%s"),
		*OldName, *CurrentName, 
		bIsOurSystemRename ? TEXT("Yes") : TEXT("No"));
	
	// 如果不是我们的系统触发的，认为是手动重命名
	if (!bIsOurSystemRename)
	{
		UE_LOG(LogX_AssetNamingDelegates, Log,
			TEXT("Detected manual rename (not triggered by our system): '%s' -> '%s'"),
			*OldName, *CurrentName);
		
		// 记录到手动重命名缓存中，并设置较长的保护时间
		FString PackagePath = AssetData.PackageName.ToString();
		double CurrentTime = FPlatformTime::Seconds();
		RecentManualRenames.Add(PackagePath, CurrentTime);
		
		UE_LOG(LogX_AssetNamingDelegates, Log,
			TEXT("Added to manual rename cache: %s"), *PackagePath);
		
		return;
	}
	
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

	// ========== 【优化】检查 AssetRegistry 状态 ==========
	// 如果 AssetRegistry 仍在加载，说明这是启动时导入的资产，跳过处理
	if (!bIsAssetRegistryReady || AssetRegistry.IsLoadingAssets())
	{
		UE_LOG(LogX_AssetNamingDelegates, Verbose,
			TEXT("AssetRegistry 加载中，跳过导入资产: %s（包路径: %s）"),
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
	UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("AssetRegistry 文件加载完成，开始处理新创建的资产"));
	bIsAssetRegistryReady = true;
}

bool FX_AssetNamingDelegates::DetectUserOperationContext() const
{
	// ========== 【用户操作上下文检测】遵循UE最佳实践 ==========
	
	// 1. 基础检查：确保在编辑器环境中
	if (!GIsEditor || IsRunningCommandlet())
	{
		return false;
	}

	// 2. 检查编辑器状态（遵循UE最佳实践）
	bool bInValidEditorMode = true;
	if (GEditor)
	{
		// 检查是否在PIE模式、烘焙或其他非编辑状态
		bInValidEditorMode = !GEditor->IsPlayingSessionInEditor() && 
							 !GEditor->GetPIEWorldContext() &&
							 !GEditor->IsSimulatingInEditor();
	}

	if (!bInValidEditorMode)
	{
		return false;
	}

	// 3. 检查ContentBrowser模块可用性
	if (!FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
	{
		return false;
	}

	// 4. 简化的用户交互检测（避免复杂的Slate API调用）
	bool bHasUserInteraction = false;
	
	// 检查是否在游戏线程（用户操作必须在游戏线程）
	if (IsInGameThread())
	{
		// 使用更安全的Slate检测方式
		if (FSlateApplication::IsInitialized())
		{
			// 只检查基本的用户交互状态，避免版本兼容性问题
			const FSlateApplication& SlateApp = FSlateApplication::Get();
			
			// 检查是否有鼠标捕获（表示用户正在交互）
			bHasUserInteraction = SlateApp.HasAnyMouseCaptor();
			
			// UE 5.3+版本兼容性检查
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
			// 在较新版本中，可以安全地检查焦点状态
			if (!bHasUserInteraction)
			{
				bHasUserInteraction = SlateApp.GetUserFocusedWidget(0).IsValid();
			}
#endif
		}
	}

	// 5. 时间窗口检测（使用线程安全的方式）
	static FThreadSafeCounter64 LastInteractionFrame;
	const uint64 CurrentFrame = GFrameCounter;
	const uint64 LastFrame = LastInteractionFrame.GetValue();
	
	// 如果在最近几帧内有用户交互，认为可能是用户操作
	const bool bRecentUserInteraction = (CurrentFrame - LastFrame) < 120; // 2秒@60fps
	
	// 更新交互帧计数（线程安全）
	if (bHasUserInteraction)
	{
		LastInteractionFrame.Set(CurrentFrame);
	}

	// 6. 综合判断（保守策略）
	const bool bIsUserContext = bHasUserInteraction && bInValidEditorMode && bRecentUserInteraction;

	// 7. 详细日志（仅在详细模式下输出）
	UE_LOG(LogX_AssetNamingDelegates, VeryVerbose,
		TEXT("User context detection: Editor=%s, ValidMode=%s, ContentBrowser=%s, UserInteraction=%s, RecentInteraction=%s -> Result=%s"),
		GIsEditor ? TEXT("Yes") : TEXT("No"),
		bInValidEditorMode ? TEXT("Yes") : TEXT("No"),
		FModuleManager::Get().IsModuleLoaded("ContentBrowser") ? TEXT("Yes") : TEXT("No"),
		bHasUserInteraction ? TEXT("Yes") : TEXT("No"),
		bRecentUserInteraction ? TEXT("Yes") : TEXT("No"),
		bIsUserContext ? TEXT("Yes") : TEXT("No"));

	return bIsUserContext;
}

bool FX_AssetNamingDelegates::IsRecentlyManuallyRenamed(const FString& PackagePath) const
{
	double CurrentTime = FPlatformTime::Seconds();
	const double CacheTimeout = 5.0;
	
	if (const double* RenameTime = RecentManualRenames.Find(PackagePath))
	{
		return (CurrentTime - *RenameTime) <= CacheTimeout;
	}
	
	return false;
}

bool FX_AssetNamingDelegates::IsSimilarToRecentlyRenamed(const FAssetData& AssetData) const
{
	FString CurrentAssetName = AssetData.AssetName.ToString();
	FString CurrentFolderPath = FPackageName::GetLongPackagePath(AssetData.PackageName.ToString());
	double CurrentTime = FPlatformTime::Seconds();
	const double CacheTimeout = 5.0;
	
	// 检查同一文件夹中是否有最近手动重命名的相似资产
	for (const auto& CachedPair : RecentManualRenames)
	{
		// 检查缓存是否过期
		if ((CurrentTime - CachedPair.Value) > CacheTimeout)
		{
			continue;
		}
		
		FString CachedPackagePath = CachedPair.Key;
		FString CachedFolderPath = FPackageName::GetLongPackagePath(CachedPackagePath);
		
		// 如果在同一文件夹
		if (CurrentFolderPath == CachedFolderPath)
		{
			FString CachedAssetName = FPackageName::GetShortName(CachedPackagePath);
			
			// 检查是否是相似的变体名称
			FString CurrentBaseName = CurrentAssetName;
			FString CachedBaseName = CachedAssetName;
			
			// 移除常见前缀
			const UX_AssetEditorSettings* PrefixSettings = GetDefault<UX_AssetEditorSettings>();
			if (PrefixSettings)
			{
				for (const auto& PrefixPair : PrefixSettings->AssetPrefixMappings)
				{
					const FString& Prefix = PrefixPair.Value;
					if (!Prefix.IsEmpty())
					{
						if (CurrentBaseName.StartsWith(Prefix))
						{
							CurrentBaseName = CurrentBaseName.RightChop(Prefix.Len());
						}
						if (CachedBaseName.StartsWith(Prefix))
						{
							CachedBaseName = CachedBaseName.RightChop(Prefix.Len());
						}
					}
				}
			}
			
			// 移除数字后缀（如 _1, _01, _001）
			FRegexPattern NumericSuffixPattern(TEXT("_[0-9]+$"));
			FRegexMatcher CurrentMatcher(NumericSuffixPattern, CurrentBaseName);
			if (CurrentMatcher.FindNext())
			{
				CurrentBaseName = CurrentBaseName.Left(CurrentMatcher.GetMatchBeginning());
			}
			
			FRegexMatcher CachedMatcher(NumericSuffixPattern, CachedBaseName);
			if (CachedMatcher.FindNext())
			{
				CachedBaseName = CachedBaseName.Left(CachedMatcher.GetMatchBeginning());
			}
			
			// 如果基础名称相同，说明是相关的变体资产
			if (CurrentBaseName.Equals(CachedBaseName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
	}
	
	return false;
}
