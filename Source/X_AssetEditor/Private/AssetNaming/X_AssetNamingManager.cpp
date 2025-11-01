/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "AssetNaming/X_AssetNamingManager.h"
#include "AssetNaming/X_AssetNamingDelegates.h"
#include "Settings/X_AssetEditorSettings.h"
#include "X_AssetEditor.h"
#include "EditorUtilityLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformFilemanager.h"
#include "ScopedTransaction.h"

DEFINE_LOG_CATEGORY(LogX_AssetNaming);

#define LOCTEXT_NAMESPACE "X_AssetNaming"

TUniquePtr<FX_AssetNamingManager> FX_AssetNamingManager::Instance = nullptr;

FX_AssetNamingManager& FX_AssetNamingManager::Get()
{
    if (!Instance.IsValid())
    {
        Instance = TUniquePtr<FX_AssetNamingManager>(new FX_AssetNamingManager());
    }
    return *Instance;
}

bool FX_AssetNamingManager::Initialize()
{
    // Initialize automatic rename delegates if enabled in settings
    const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
    if (!Settings)
    {
        UE_LOG(LogX_AssetNaming, Error, TEXT("Failed to get X_AssetEditorSettings"));
        return false;
    }

    if (Settings->bAutoRenameOnImport || Settings->bAutoRenameOnCreate)
    {
        FX_AssetNamingDelegates::Get().Initialize(
            FX_AssetNamingDelegates::FOnAssetNeedsRename::CreateRaw(this, &FX_AssetNamingManager::OnAssetNeedsRename)
        );
    }

    UE_LOG(LogX_AssetNaming, Log, TEXT("Asset Naming Manager initialized with %d prefix rules"),
        Settings->AssetPrefixMappings.Num());

    return true;
}

bool FX_AssetNamingManager::Shutdown()
{
    // Shutdown delegates
    FX_AssetNamingDelegates::Get().Shutdown();

    UE_LOG(LogX_AssetNaming, Log, TEXT("Asset Naming Manager shut down"));

    return true;
}

void FX_AssetNamingManager::RefreshDelegateBindings()
{
    const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
    if (!Settings)
    {
        return;
    }

    // 先关闭现有委托
    FX_AssetNamingDelegates::Get().Shutdown();

    // 根据设置重新绑定
    if (Settings->bAutoRenameOnImport || Settings->bAutoRenameOnCreate)
    {
        FX_AssetNamingDelegates::Get().Initialize(
            FX_AssetNamingDelegates::FOnAssetNeedsRename::CreateRaw(this, &FX_AssetNamingManager::OnAssetNeedsRename)
        );
        UE_LOG(LogX_AssetNaming, Log, TEXT("Delegate bindings refreshed: Import=%d, Create=%d"),
            Settings->bAutoRenameOnImport, Settings->bAutoRenameOnCreate);
    }
    else
    {
        UE_LOG(LogX_AssetNaming, Log, TEXT("Auto-rename disabled, delegates unbound"));
    }
}

FString FX_AssetNamingManager::GetSimpleClassName(const FAssetData& AssetData) const
{
    FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();

    // 移除 _C 后缀（如果有的话）
    if (ClassName.EndsWith(TEXT("_C")))
    {
        // UE 5.5+ API 变更：LeftChopInline 参数从 bool 改为 EAllowShrinking 枚举
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
        ClassName.LeftChopInline(2, EAllowShrinking::No);
#else
        ClassName.LeftChopInline(2, false);
#endif
    }

    // 如果类名为空，使用资产名称作为备选
    if (ClassName.IsEmpty())
    {
        ClassName = AssetData.AssetName.ToString();
    }

    return ClassName;
}

FString FX_AssetNamingManager::GetAssetClassDisplayName(const FAssetData& AssetData) const
{
    return GetSimpleClassName(AssetData);
}

FString FX_AssetNamingManager::GetCorrectPrefix(const FAssetData& AssetData, const FString& SimpleClassName) const
{
    const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
    if (!Settings)
    {
        return TEXT("");
    }

    // ========== 提前检查排除规则，避免为排除的资产打印无意义的警告 ==========
    if (IsAssetExcluded(AssetData))
    {
        return TEXT("");
    }

    const TMap<FString, FString>& AssetPrefixes = Settings->AssetPrefixMappings;
    const TMap<FString, FString>& ParentClassPrefixes = Settings->ParentClassPrefixMappings;
    FString FullClassPath = AssetData.AssetClassPath.ToString();

    // ========== 智能识别：对于通用资产类型，检查父类来确定具体类型 ==========
    // 支持的通用资产类型：Blueprint, DataAsset, PrimaryDataAsset 等
    bool bNeedsParentClassCheck = (SimpleClassName == TEXT("Blueprint") ||
                                   SimpleClassName == TEXT("DataAsset") ||
                                   SimpleClassName == TEXT("PrimaryDataAsset") ||
                                   SimpleClassName.IsEmpty());

    if (bNeedsParentClassCheck)
    {
        // 特殊处理：Blueprint 类型的 BlueprintType Tag
        if (SimpleClassName == TEXT("Blueprint"))
        {
            FAssetDataTagMapSharedView::FFindTagResult BlueprintTypeTag = AssetData.TagsAndValues.FindTag(TEXT("BlueprintType"));
            if (BlueprintTypeTag.IsSet())
            {
                FString BlueprintType = BlueprintTypeTag.GetValue();

                // BPTYPE_Interface
                if (BlueprintType == TEXT("BPTYPE_Interface"))
                {
                    const FString* PrefixPtr = AssetPrefixes.Find(TEXT("BlueprintInterface"));
                    if (PrefixPtr)
                    {
                        UE_LOG(LogX_AssetNaming, Verbose, TEXT("Detected BlueprintInterface via BlueprintType: %s"), *AssetData.AssetName.ToString());
                        return *PrefixPtr;
                    }
                }

                // BPTYPE_FunctionLibrary
                if (BlueprintType == TEXT("BPTYPE_FunctionLibrary"))
                {
                    const FString* PrefixPtr = AssetPrefixes.Find(TEXT("BlueprintFunctionLibrary"));
                    if (PrefixPtr)
                    {
                        UE_LOG(LogX_AssetNaming, Verbose, TEXT("Detected BlueprintFunctionLibrary via BlueprintType: %s"), *AssetData.AssetName.ToString());
                        return *PrefixPtr;
                    }
                }

                // BPTYPE_MacroLibrary
                if (BlueprintType == TEXT("BPTYPE_MacroLibrary"))
                {
                    const FString* PrefixPtr = AssetPrefixes.Find(TEXT("BlueprintMacroLibrary"));
                    if (PrefixPtr)
                    {
                        UE_LOG(LogX_AssetNaming, Verbose, TEXT("Detected BlueprintMacroLibrary via BlueprintType: %s"), *AssetData.AssetName.ToString());
                        return *PrefixPtr;
                    }
                }
            }
        }

        // 通用方法：通过 ParentClass Tag 识别（适用于所有通用资产类型）
        FAssetDataTagMapSharedView::FFindTagResult ParentClassTag = AssetData.TagsAndValues.FindTag(TEXT("ParentClass"));
        if (ParentClassTag.IsSet())
        {
            FString ParentClassPath = ParentClassTag.GetValue();

            UE_LOG(LogX_AssetNaming, Verbose, TEXT("Asset '%s' (Type: %s) ParentClass: %s"),
                *AssetData.AssetName.ToString(), *SimpleClassName, *ParentClassPath);

            // 按照优先级顺序检查父类映射（更具体的类型优先）
            // 我们需要按照字符串长度倒序排列，确保更具体的匹配优先
            // 例如: "SceneComponent" 应该在 "ActorComponent" 之前检查
            TArray<TPair<FString, FString>> SortedParentClassPrefixes;
            for (const auto& Pair : ParentClassPrefixes)
            {
                SortedParentClassPrefixes.Add(Pair);
            }

            // 按照 Key 的长度倒序排序（更长的类名更具体）
            SortedParentClassPrefixes.Sort([](const TPair<FString, FString>& A, const TPair<FString, FString>& B)
            {
                return A.Key.Len() > B.Key.Len();
            });

            // 使用部分匹配检查父类
            for (const auto& Pair : SortedParentClassPrefixes)
            {
                const FString& ParentClassName = Pair.Key;
                const FString& Prefix = Pair.Value;

                // 支持部分匹配: "ActorComponent" 可以匹配 "/Script/Engine.ActorComponent"
                if (ParentClassPath.Contains(ParentClassName))
                {
                    UE_LOG(LogX_AssetNaming, Verbose, TEXT("Matched ParentClass '%s' for asset '%s' (type: %s), prefix: %s"),
                        *ParentClassName, *AssetData.AssetName.ToString(), *SimpleClassName, *Prefix);
                    return Prefix;
                }
            }

            // 如果没有找到特定的父类映射，检查 AssetPrefixMappings 中是否有对应的条目
            // 提取父类的简单类名 (例如从 "/Script/Engine.Actor" 提取 "Actor")
            FString ParentSimpleClassName;
            int32 LastDotIndex;
            if (ParentClassPath.FindLastChar('.', LastDotIndex))
            {
                ParentSimpleClassName = ParentClassPath.RightChop(LastDotIndex + 1);

                // 移除可能的 _C 后缀
                if (ParentSimpleClassName.EndsWith(TEXT("_C")))
                {
                    // UE 5.5+ API 变更：LeftChopInline 参数从 bool 改为 EAllowShrinking 枚举
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
                    ParentSimpleClassName.LeftChopInline(2, EAllowShrinking::No);
#else
                    ParentSimpleClassName.LeftChopInline(2, false);
#endif
                }

                // 尝试在 AssetPrefixMappings 中查找
                const FString* PrefixPtr = AssetPrefixes.Find(ParentSimpleClassName);
                if (PrefixPtr)
                {
                    UE_LOG(LogX_AssetNaming, Verbose, TEXT("Found prefix in AssetPrefixMappings for parent class '%s': %s"),
                        *ParentSimpleClassName, **PrefixPtr);
                    return *PrefixPtr;
                }
            }
        }

        // 如果仍未找到，返回该资产类型的默认前缀
        const FString* DefaultPrefix = AssetPrefixes.Find(SimpleClassName);
        if (DefaultPrefix)
        {
            UE_LOG(LogX_AssetNaming, Verbose, TEXT("Using default prefix for '%s' (type: %s)"),
                *AssetData.AssetName.ToString(), *SimpleClassName);
            return *DefaultPrefix;
        }
    }

    // ========== 标准查找：直接从类名获取前缀 ==========
    // 1. 尝试直接从类名获取前缀
    const FString* PrefixPtr = AssetPrefixes.Find(SimpleClassName);
    if (PrefixPtr)
    {
        return *PrefixPtr;
    }

    // 2. 尝试从完整类路径中获取类名
    FString Left, Right;
    if (FullClassPath.Split(TEXT("."), &Left, &Right))
    {
        PrefixPtr = AssetPrefixes.Find(Right);
        if (PrefixPtr)
        {
            return *PrefixPtr;
        }
    }

    UE_LOG(LogX_AssetNaming, Warning, TEXT("Unable to determine prefix for asset '%s' (type: %s, path: %s)"),
        *AssetData.AssetName.ToString(), *SimpleClassName, *FullClassPath);

    return TEXT("");
}

FX_RenameOperationResult FX_AssetNamingManager::RenameSelectedAssets()
{
    FX_RenameOperationResult Result;

    // 获取选中的资产数据
    TArray<FAssetData> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssetData();
    if (SelectedAssets.Num() == 0)
    {
        UE_LOG(LogX_AssetNaming, Warning, TEXT("No assets selected; cannot perform rename"));
        return Result;
    }

    // 获取AssetTools模块
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    IAssetTools& AssetTools = AssetToolsModule.Get();

    // 开始资产重命名操作
    FScopedTransaction Transaction(NSLOCTEXT("X_AssetNaming", "RenameAssets", "Rename Assets"));

    UE_LOG(LogX_AssetNaming, Log, TEXT("Start normalizing names for %d assets"), SelectedAssets.Num());

    // 添加进度条
    FScopedSlowTask SlowTask(
        SelectedAssets.Num(),
        FText::Format(NSLOCTEXT("X_AssetNaming", "NormalizingAssetNames", "Normalizing names for {0} assets..."),
        FText::AsNumber(SelectedAssets.Num()))
    );
    SlowTask.MakeDialog(true);

    for (const FAssetData& AssetData : SelectedAssets)
    {
        SlowTask.EnterProgressFrame(1.0f);

        if (SlowTask.ShouldCancel())
        {
            UE_LOG(LogX_AssetNaming, Warning, TEXT("User canceled name normalization"));
            break;
        }

        if (!AssetData.IsValid())
        {
            UE_LOG(LogX_AssetNaming, Warning, TEXT("Invalid asset data found; skipped"));
            Result.FailedCount++;
            Result.FailedRenames.Add(LOCTEXT("InvalidAsset", "Invalid Asset").ToString());
            continue;
        }

        // 检查资产包是否仍然存在（避免处理已被删除或重命名的资产）
        FString PackageName = AssetData.PackageName.ToString();
        if (!FPackageName::DoesPackageExist(PackageName))
        {
            UE_LOG(LogX_AssetNaming, Warning, TEXT("Asset package no longer exists: %s (may have been renamed or deleted)"),
                *AssetData.AssetName.ToString());
            Result.SkippedCount++;
            continue;
        }

        // 检查是否在排除列表中
        if (IsAssetExcluded(AssetData))
        {
            Result.SkippedCount++;
            UE_LOG(LogX_AssetNaming, Verbose, TEXT("Asset '%s' is excluded; skipped"),
                *AssetData.AssetName.ToString());
            continue;
        }

        FString CurrentName = AssetData.AssetName.ToString();
        FString PackagePath = FPackageName::GetLongPackagePath(AssetData.PackageName.ToString());

        if (PackagePath.IsEmpty())
        {
            UE_LOG(LogX_AssetNaming, Warning, TEXT("Asset '%s' has invalid package path"), *CurrentName);
            Result.FailedCount++;
            Result.FailedRenames.Add(CurrentName);
            continue;
        }

        FString SimpleClassName = GetSimpleClassName(AssetData);

        // 输出调试信息
        UE_LOG(LogX_AssetNaming, Verbose, TEXT("Processing asset: %s, Class: %s, ClassPath: %s"),
            *CurrentName, *SimpleClassName, *AssetData.AssetClassPath.ToString());

        FString CorrectPrefix = GetCorrectPrefix(AssetData, SimpleClassName);

        if (CorrectPrefix.IsEmpty())
        {
            UE_LOG(LogX_AssetNaming, Warning, TEXT("Cannot determine prefix for asset '%s' (class: %s)"),
                *CurrentName, *SimpleClassName);
            Result.FailedCount++;
            Result.FailedRenames.Add(CurrentName);
            continue;
        }

        UE_LOG(LogX_AssetNaming, Verbose, TEXT("Asset '%s': Current name='%s', Determined prefix='%s'"),
            *CurrentName, *CurrentName, *CorrectPrefix);

        // 检查当前名称是否已经符合规范
        if (CurrentName.StartsWith(CorrectPrefix))
        {
            Result.SkippedCount++;
            continue;
        }

        // 构建新名称
        FString BaseName = CurrentName;

        // 只移除错误的前缀（优化后的逻辑）
        const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
        if (Settings)
        {
            const TMap<FString, FString>& AssetPrefixes = Settings->AssetPrefixMappings;
            for (const auto& Pair : AssetPrefixes)
            {
                const FString& ExistingPrefix = Pair.Value;
                if (!ExistingPrefix.IsEmpty() &&
                    ExistingPrefix != CorrectPrefix &&
                    CurrentName.StartsWith(ExistingPrefix))
                {
                    BaseName = CurrentName.RightChop(ExistingPrefix.Len());
                    UE_LOG(LogX_AssetNaming, Verbose, TEXT("Removing incorrect prefix '%s' from '%s'"),
                        *ExistingPrefix, *CurrentName);
                    break;
                }
            }
        }

        FString NewName = CorrectPrefix + BaseName;
        FString FinalNewName = NewName;
        int32 SuffixCounter = 1;

        // 性能优化：缓存资产名称避免重复磁盘查询
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

        TArray<FAssetData> AllAssetsInFolder;
        AssetRegistry.GetAssetsByPath(FName(*PackagePath), AllAssetsInFolder, false);

        TSet<FString> ExistingNames;
        for (const FAssetData& Asset : AllAssetsInFolder)
        {
            ExistingNames.Add(Asset.AssetName.ToString());
        }

        // 检查是否存在同名资产
        while (ExistingNames.Contains(FinalNewName))
        {
            FinalNewName = FString::Printf(TEXT("%s_%d"), *NewName, SuffixCounter++);
        }

        if (FinalNewName != CurrentName)
        {
            // 执行重命名操作 - 先检查资产是否可以加载
            UObject* AssetObject = AssetData.GetAsset();
            if (!AssetObject)
            {
                Result.FailedCount++;
                Result.FailedRenames.Add(CurrentName);
                UE_LOG(LogX_AssetNaming, Error, TEXT("Unable to load asset: %s (PackageName: %s)"),
                    *CurrentName, *AssetData.PackageName.ToString());
                continue;
            }

            TArray<FAssetRenameData> AssetsToRename;
            AssetsToRename.Add(FAssetRenameData(AssetObject, PackagePath, FinalNewName));

            if (AssetTools.RenameAssets(AssetsToRename))
            {
                Result.SuccessCount++;
                Result.SuccessfulRenames.Add(AssetData.PackageName.ToString());
                UE_LOG(LogX_AssetNaming, Log, TEXT("Rename succeeded: %s -> %s"), *CurrentName, *FinalNewName);
            }
            else
            {
                Result.FailedCount++;
                Result.FailedRenames.Add(CurrentName);
                UE_LOG(LogX_AssetNaming, Error, TEXT("Rename failed: %s"), *CurrentName);
            }
        }
        else
        {
            Result.SkippedCount++;
        }
    }

    // 自动清理 Redirectors（如果启用）
    const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
    if (Settings && Settings->bAutoFixupRedirectors && Result.SuccessfulRenames.Num() > 0)
    {
        FixupRedirectors(Result.SuccessfulRenames);
    }

    // 显示操作结果
    ShowRenameResult(Result);

    return Result;
}

void FX_AssetNamingManager::ShowRenameResult(const FX_RenameOperationResult& Result) const
{
    int32 TotalCount = Result.SuccessCount + Result.SkippedCount + Result.FailedCount;

    // 构建详细的操作结果信息
    static FString LastOperationDetails;
    LastOperationDetails.Empty();
    LastOperationDetails.Append(FString::Printf(TEXT("Asset name normalization details (%s)\n\n"), *FDateTime::Now().ToString()));
    LastOperationDetails.Append(LOCTEXT("NormalizationHeader", "==================== Normalization Completed ====================\n").ToString());
    LastOperationDetails.Append(LOCTEXT("SummaryLabel", "Summary:\n").ToString());
    LastOperationDetails.Append(FText::Format(LOCTEXT("TotalLabel", "- Total: {0}\n"), FText::AsNumber(TotalCount)).ToString());
    LastOperationDetails.Append(FText::Format(LOCTEXT("RenamedLabel", "- Renamed: {0}\n"), FText::AsNumber(Result.SuccessCount)).ToString());
    LastOperationDetails.Append(FText::Format(LOCTEXT("AlreadyOkLabel", "- Already OK: {0}\n"), FText::AsNumber(Result.SkippedCount)).ToString());
    LastOperationDetails.Append(FText::Format(LOCTEXT("FailedLabel", "- Failed: {0}\n"), FText::AsNumber(Result.FailedCount)).ToString());
    LastOperationDetails.Append(LOCTEXT("SeparatorLine", "====================================================\n").ToString());

    // 添加提示信息
    if (Result.SkippedCount > 0)
    {
        const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
        if (Settings && (Settings->bAutoRenameOnImport || Settings->bAutoRenameOnCreate))
        {
            LastOperationDetails.Append(TEXT("\n"));
            LastOperationDetails.Append(LOCTEXT("AutoRenameNote", "Note: Some assets may have been skipped because they no longer exist.\n").ToString());
            LastOperationDetails.Append(LOCTEXT("AutoRenameHint", "This can happen when 'Auto-Rename on Create/Import' is enabled and\n").ToString());
            LastOperationDetails.Append(LOCTEXT("AutoRenameHint2", "assets were already automatically renamed. Check the Output Log for details.\n").ToString());
        }
    }

    // 显示可点击的通知
    FNotificationInfo Info(FText::Format(
        NSLOCTEXT("X_AssetNaming", "AssetRenameNotification", "Asset name normalization completed\nTotal: {0} | Renamed: {1} | Already OK: {2} | Failed: {3}\nClick to view details"),
        FText::AsNumber(TotalCount),
        FText::AsNumber(Result.SuccessCount),
        FText::AsNumber(Result.SkippedCount),
        FText::AsNumber(Result.FailedCount)
    ));

    Info.bUseLargeFont = false;
    Info.bUseSuccessFailIcons = false;
    Info.bUseThrobber = false;
    Info.FadeOutDuration = 1.0f;
    Info.ExpireDuration = Result.FailedCount > 0 ? 8.0f : 5.0f;
    Info.bFireAndForget = true;
    Info.bAllowThrottleWhenFrameRateIsLow = true;
    Info.Image = nullptr;

    // 添加点击查看详情功能
    Info.Hyperlink = FSimpleDelegate::CreateLambda([=]()
    {
        FMessageDialog::Open(EAppMsgType::Ok,
            FText::FromString(LastOperationDetails),
            NSLOCTEXT("X_AssetNaming", "ViewDetailsHyperlink", "View Details"));
    });
    Info.HyperlinkText = NSLOCTEXT("X_AssetNaming", "ViewDetailsHyperlink", "View Details");

    // 显示通知
    TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);

    if (NotificationItem.IsValid())
    {
        if (Result.FailedCount == 0)
        {
            NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
        }
        else
        {
            NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
        }
    }

    // 如果失败数量较多，自动显示详细信息
    if (Result.FailedCount > 0 && Result.FailedCount > TotalCount / 3)
    {
        FMessageDialog::Open(EAppMsgType::Ok,
            FText::FromString(LastOperationDetails),
            NSLOCTEXT("X_AssetNaming", "AssetRenameDetails", "Asset Name Normalization Details"));
    }

    UE_LOG(LogX_AssetNaming, Log, TEXT("Asset renaming finished: Renamed %d, Skipped %d, Failed %d"),
        Result.SuccessCount, Result.SkippedCount, Result.FailedCount);
}

bool FX_AssetNamingManager::RenameAssetInternal(const FAssetData& AssetData, FString& OutNewName)
{
    if (!AssetData.IsValid())
    {
        return false;
    }

    // 检查是否在排除列表中
    if (IsAssetExcluded(AssetData))
    {
        return false;
    }

    FString CurrentName = AssetData.AssetName.ToString();
    FString PackagePath = FPackageName::GetLongPackagePath(AssetData.PackageName.ToString());

    if (PackagePath.IsEmpty())
    {
        return false;
    }

    FString SimpleClassName = GetSimpleClassName(AssetData);
    FString CorrectPrefix = GetCorrectPrefix(AssetData, SimpleClassName);

    if (CorrectPrefix.IsEmpty())
    {
        return false;
    }

    // 检查是否已符合规范
    if (CurrentName.StartsWith(CorrectPrefix))
    {
        return false;
    }

    // 移除错误的前缀
    FString BaseName = CurrentName;
    const UX_AssetEditorSettings* PrefixSettings = GetDefault<UX_AssetEditorSettings>();
    if (PrefixSettings)
    {
        const TMap<FString, FString>& AssetPrefixes = PrefixSettings->AssetPrefixMappings;
        for (const auto& Pair : AssetPrefixes)
        {
            const FString& ExistingPrefix = Pair.Value;
            if (!ExistingPrefix.IsEmpty() &&
                ExistingPrefix != CorrectPrefix &&
                CurrentName.StartsWith(ExistingPrefix))
            {
                BaseName = CurrentName.RightChop(ExistingPrefix.Len());
                break;
            }
        }
    }

    // 构建新名称
    FString NewName = CorrectPrefix + BaseName;

    // 检查命名冲突
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FAssetData> AllAssetsInFolder;
    AssetRegistry.GetAssetsByPath(FName(*PackagePath), AllAssetsInFolder, false);

    TSet<FString> ExistingNames;
    for (const FAssetData& Asset : AllAssetsInFolder)
    {
        ExistingNames.Add(Asset.AssetName.ToString());
    }

    FString FinalNewName = NewName;
    int32 Suffix = 1;
    while (ExistingNames.Contains(FinalNewName))
    {
        FinalNewName = FString::Printf(TEXT("%s_%d"), *NewName, Suffix++);
    }

    // 执行重命名
    UObject* AssetObject = AssetData.GetAsset();
    if (!AssetObject)
    {
        return false;
    }

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    IAssetTools& AssetTools = AssetToolsModule.Get();

    TArray<FAssetRenameData> AssetsToRename;
    AssetsToRename.Add(FAssetRenameData(AssetObject, PackagePath, FinalNewName));

    if (AssetTools.RenameAssets(AssetsToRename))
    {
        OutNewName = FinalNewName;

        // 自动清理 Redirector（如果启用）
        const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
        if (Settings && Settings->bAutoFixupRedirectors)
        {
            TArray<FString> OldPaths;
            OldPaths.Add(AssetData.PackageName.ToString());
            FixupRedirectors(OldPaths);
        }

        return true;
    }

    return false;
}

void FX_AssetNamingManager::FixupRedirectors(const TArray<FString>& OldPackagePaths)
{
    if (OldPackagePaths.Num() == 0)
    {
        return;
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<UObjectRedirector*> Redirectors;

    for (const FString& OldPath : OldPackagePaths)
    {
        FAssetData RedirectorData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(OldPath));
        if (RedirectorData.IsValid() && RedirectorData.AssetClassPath.GetAssetName() == TEXT("ObjectRedirector"))
        {
            if (UObjectRedirector* Redirector = Cast<UObjectRedirector>(RedirectorData.GetAsset()))
            {
                Redirectors.Add(Redirector);
            }
        }
    }

    if (Redirectors.Num() > 0)
    {
        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
        AssetToolsModule.Get().FixupReferencers(
            Redirectors,
            true,  // bCheckoutDialogPrompt
            ERedirectFixupMode::DeleteFixedUpRedirectors
        );

        UE_LOG(LogX_AssetNaming, Log, TEXT("Fixed up and deleted %d redirectors"), Redirectors.Num());
    }
}

bool FX_AssetNamingManager::OnAssetNeedsRename(const FAssetData& AssetData)
{
    // Check settings to determine which events are enabled
    const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
    if (!Settings)
    {
        return false;
    }

    // Skip if both settings are disabled
    if (!Settings->bAutoRenameOnImport && !Settings->bAutoRenameOnCreate)
    {
        return false;
    }

    FString NewName;
    if (RenameAssetInternal(AssetData, NewName))
    {
        UE_LOG(LogX_AssetNaming, Log, TEXT("Auto-renamed asset: %s -> %s"),
            *AssetData.AssetName.ToString(), *NewName);
        return true;
    }

    return false;
}

bool FX_AssetNamingManager::IsAssetExcluded(const FAssetData& AssetData) const
{
    if (!AssetData.IsValid())
    {
        return true;
    }

    const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
    if (!Settings)
    {
        return false;
    }

    // 检查排除的资产类型
    FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();
    if (Settings->ExcludedAssetClasses.Contains(ClassName))
    {
        UE_LOG(LogX_AssetNaming, Verbose, TEXT("Asset excluded by class: %s (class: %s)"),
            *AssetData.AssetName.ToString(), *ClassName);
        return true;
    }

    // 检查排除的文件夹
    FString PackagePath = AssetData.PackagePath.ToString();
    for (const FString& ExcludedFolder : Settings->ExcludedFolders)
    {
        if (!ExcludedFolder.IsEmpty() && PackagePath.StartsWith(ExcludedFolder))
        {
            UE_LOG(LogX_AssetNaming, Verbose, TEXT("Asset excluded by folder: %s (folder: %s)"),
                *AssetData.AssetName.ToString(), *ExcludedFolder);
            return true;
        }
    }

    return false;
}

#undef LOCTEXT_NAMESPACE
