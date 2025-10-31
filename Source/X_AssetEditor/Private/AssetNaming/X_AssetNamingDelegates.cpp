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

	// 2. 绑定 OnAssetAdded（用于新建的资产）- 延迟到 OnFilesLoaded 后
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	if (AssetRegistry.IsLoadingAssets())
	{
		// 资产注册表仍在扫描 - 等待 OnFilesLoaded
		OnFilesLoadedHandle = AssetRegistry.OnFilesLoaded().AddRaw(this, &FX_AssetNamingDelegates::OnFilesLoaded);
		UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("资产注册表加载中；将在 OnFilesLoaded 后绑定 OnAssetAdded"));
	}
	else
	{
		// 资产注册表已加载 - 立即绑定
		OnFilesLoaded();
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

	// 2. 解绑 OnFilesLoaded（如果仍然绑定）
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

	// 3. 解绑 OnAssetAdded
	if (OnAssetAddedHandle.IsValid())
	{
		// 使用 GetModulePtr 而不是 LoadModuleChecked 以避免在关闭期间加载模块
		FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry");
		if (AssetRegistryModule)
		{
			IAssetRegistry& AssetRegistry = AssetRegistryModule->Get();
			AssetRegistry.OnAssetAdded().Remove(OnAssetAddedHandle);
		}
		OnAssetAddedHandle.Reset();
	}

	// 4. 解绑 OnAssetRemoved
	if (OnAssetRemovedHandle.IsValid())
	{
		FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry");
		if (AssetRegistryModule)
		{
			IAssetRegistry& AssetRegistry = AssetRegistryModule->Get();
			AssetRegistry.OnAssetRemoved().Remove(OnAssetRemovedHandle);
		}
		OnAssetRemovedHandle.Reset();
	}

	RenameCallback.Unbind();
	bIsActive = false;
	bAssetRegistryLoaded = false;
	RecentlyRemovedAssets.Empty();

	UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("资产命名委托已关闭"));
}

void FX_AssetNamingDelegates::OnFilesLoaded()
{
	if (bAssetRegistryLoaded)
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// 绑定 OnAssetAdded 委托（仅在初始扫描后触发的新资产）
	OnAssetAddedHandle = AssetRegistry.OnAssetAdded().AddRaw(
		this, &FX_AssetNamingDelegates::OnAssetAdded
	);

	// 绑定 OnAssetRemoved 委托以跟踪重命名操作
	OnAssetRemovedHandle = AssetRegistry.OnAssetRemoved().AddRaw(
		this, &FX_AssetNamingDelegates::OnAssetRemoved
	);

	bAssetRegistryLoaded = true;
	UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("OnFilesLoaded 后已绑定到 OnAssetAdded 和 OnAssetRemoved 委托"));
}

void FX_AssetNamingDelegates::OnAssetAdded(const FAssetData& AssetData)
{
	if (!bIsActive || !RenameCallback.IsBound())
	{
		return;
	}

	// 检查是否启用了"创建时自动重命名"
	const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
	if (!Settings || !Settings->bAutoRenameOnCreate)
	{
		return;
	}

	// 验证资产后再处理
	if (!ShouldProcessAsset(AssetData))
	{
		return;
	}

	// ========== 延迟检测，避免时序问题 ==========
	// 延迟执行可以确保：
	// 1. Redirector 已经创建（重命名操作）
	// 2. 资产注册完成
	// 3. 其他相关资产已经存在（复制操作）
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[this, AssetData](float DeltaTime) -> bool
		{
			if (!RenameCallback.IsBound())
			{
				return false;
			}

			FString AssetName = AssetData.AssetName.ToString();
			FString PackagePath = AssetData.PackagePath.ToString();

			// 重新获取 AssetRegistry（在延迟后）
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

			// ========== 1. 检查是否是重命名操作 ==========
			// 检查最近是否有资产在同一路径下被移除（重命名会先移除旧资产，再添加新资产）
			bool bIsLikelyRename = false;
			double CurrentTime = FPlatformTime::Seconds();
			const double RenameDetectionWindow = 1.0;  // 1秒内的移除操作视为重命名

			if (TArray<FRemovedAssetInfo>* RemovedAssets = RecentlyRemovedAssets.Find(PackagePath))
			{
				for (const FRemovedAssetInfo& RemovedInfo : *RemovedAssets)
				{
					// 检查是否在时间窗口内
					if (CurrentTime - RemovedInfo.RemovalTimestamp < RenameDetectionWindow)
					{
						// 同一路径下有最近移除的资产，这很可能是重命名操作
						bIsLikelyRename = true;
						UE_LOG(LogX_AssetNamingDelegates, Verbose,
							TEXT("跳过自动重命名 '%s': 在相同路径下检测到最近移除的资产 '%s' (%.2f秒前) - 可能是重命名操作"),
							*AssetName, *RemovedInfo.AssetName, CurrentTime - RemovedInfo.RemovalTimestamp);
						break;
					}
				}

				// 清理过期的记录（超过2秒的）
				RemovedAssets->RemoveAll([CurrentTime](const FRemovedAssetInfo& Info) {
					return CurrentTime - Info.RemovalTimestamp > 2.0;
				});

				if (RemovedAssets->Num() == 0)
				{
					RecentlyRemovedAssets.Remove(PackagePath);
				}
			}

			if (bIsLikelyRename)
			{
				return false;  // 不执行重命名
			}

			// ========== 2. 检查是否是复制操作（名称包含 _Copy, _Copy2 等后缀）==========
			bool bIsLikelyCopy = false;

			// 检查常见的复制后缀
			if (AssetName.EndsWith(TEXT("_Copy")) ||
				(AssetName.Contains(TEXT("_Copy")) && AssetName.Len() > 5))
			{
				bIsLikelyCopy = true;
				UE_LOG(LogX_AssetNamingDelegates, Verbose,
					TEXT("跳过自动重命名 '%s': 检测到复制后缀（可能是复制操作）"),
					*AssetName);
			}

			// 检查数字后缀模式（例如 BP_Actor_2, BP_Actor_3）
			if (!bIsLikelyCopy)
			{
				TArray<FAssetData> AssetsInSamePath;
				AssetRegistry.GetAssetsByPath(AssetData.PackagePath, AssetsInSamePath, false);

				FString BaseNameWithoutNumber = AssetName;
				int32 LastUnderscoreIndex = -1;
				if (AssetName.FindLastChar('_', LastUnderscoreIndex))
				{
					FString PotentialNumber = AssetName.RightChop(LastUnderscoreIndex + 1);
					if (PotentialNumber.IsNumeric())
					{
						BaseNameWithoutNumber = AssetName.Left(LastUnderscoreIndex);

						// 检查是否存在基础名称的资产
						for (const FAssetData& Asset : AssetsInSamePath)
						{
							if (Asset.AssetName.ToString() == BaseNameWithoutNumber &&
								Asset.AssetClassPath.GetAssetName() != TEXT("ObjectRedirector"))  // 排除 Redirector
							{
								bIsLikelyCopy = true;
								UE_LOG(LogX_AssetNamingDelegates, Verbose,
									TEXT("跳过自动重命名 '%s': 检测到编号副本（基础名称: %s）"),
									*AssetName, *BaseNameWithoutNumber);
								break;
							}
						}
					}
				}
			}

			if (bIsLikelyCopy)
			{
				return false;  // 不执行重命名
			}

			// ========== 3. 只有真正的新建资产才触发自动重命名 ==========
			UE_LOG(LogX_AssetNamingDelegates, Verbose,
				TEXT("为新创建的资产触发自动重命名: %s"),
				*AssetName);
			RenameCallback.Execute(AssetData);

			return false;  // 执行一次
		}
	), 0.3f);  // 延迟时间 0.3 秒
}

void FX_AssetNamingDelegates::OnAssetRemoved(const FAssetData& AssetData)
{
	if (!bIsActive)
	{
		return;
	}

	// 忽略 Redirector 的移除事件
	if (AssetData.AssetClassPath.GetAssetName() == TEXT("ObjectRedirector"))
	{
		return;
	}

	// 记录移除的资产信息，用于检测重命名操作
	FString PackagePath = AssetData.PackagePath.ToString();
	FString AssetName = AssetData.AssetName.ToString();

	FRemovedAssetInfo RemovedInfo;
	RemovedInfo.AssetName = AssetName;
	RemovedInfo.RemovalTimestamp = FPlatformTime::Seconds();

	TArray<FRemovedAssetInfo>& RemovedAssets = RecentlyRemovedAssets.FindOrAdd(PackagePath);
	RemovedAssets.Add(RemovedInfo);

	UE_LOG(LogX_AssetNamingDelegates, Verbose,
		TEXT("从路径 '%s' 移除资产: %s（跟踪以检测重命名）"),
		*PackagePath, *AssetName);
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

	// 执行重命名回调
	RenameCallback.Execute(AssetData);
}

bool FX_AssetNamingDelegates::ShouldProcessAsset(const FAssetData& AssetData) const
{
	if (!AssetData.IsValid())
	{
		return false;
	}

	// 跳过重定向器（由重命名操作创建）
	if (AssetData.AssetClassPath.GetAssetName() == TEXT("ObjectRedirector"))
	{
		return false;
	}

	// 跳过引擎内容（避免干扰引擎资产）
	FString PackagePath = AssetData.PackagePath.ToString();
	if (PackagePath.StartsWith(TEXT("/Engine/")))
	{
		return false;
	}

	// 跳过临时包（关卡中的临时对象）
	if (PackagePath.StartsWith(TEXT("/Temp/")))
	{
		return false;
	}

	// 跳过关卡中的Actor对象（这些对象的包路径包含关卡路径）
	// 例如：/Game/Maps/MyLevel.MyLevel:PersistentLevel.StaticMeshActor_0
	FString PackageName = AssetData.PackageName.ToString();
	if (PackageName.Contains(TEXT(":")) || PackageName.Contains(TEXT(".")))
	{
		// 包含 ":" 或 "." 通常表示是关卡内的子对象
		return false;
	}

	// 跳过 World 相关的特殊对象
	FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();
	if (ClassName == TEXT("WorldDataLayers") || 
		ClassName == TEXT("ActorFolder") || 
		ClassName == TEXT("WorldPartitionMiniMap"))
	{
		return false;
	}

	return true;
}

