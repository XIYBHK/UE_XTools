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

    // 使用统一的诊断函数
    OutputUnknownAssetDiagnostics(AssetData, SimpleClassName);

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

    // ========== 【参考 UE 源码】检查 AssetRegistry 是否正在加载 ==========
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    if (AssetRegistry.IsLoadingAssets())
    {
        UE_LOG(LogX_AssetNaming, Error, TEXT("Cannot rename assets while AssetRegistry is still loading. Please wait."));
        FMessageDialog::Open(EAppMsgType::Ok, 
            NSLOCTEXT("X_AssetNaming", "AssetRegistryLoading", "Cannot rename assets while the editor is still discovering assets. Please wait and try again."));
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

        // ========== 【安全检查1】如果新名称与当前名称相同，跳过 ==========
        if (NewName == CurrentName)
        {
            UE_LOG(LogX_AssetNaming, Verbose, TEXT("Asset '%s' already has the correct name, skipped"),
                *CurrentName);
            Result.SkippedCount++;
            continue;
        }

        FString FinalNewName = NewName;
        int32 SuffixCounter = 1;

        // ========== 【关键修复】在调用 GetAssetsByPath 前再次检查 AssetRegistry 状态 ==========
        // 防止在检查后、调用前状态发生变化导致崩溃
        if (AssetRegistry.IsLoadingAssets())
        {
            UE_LOG(LogX_AssetNaming, Warning, TEXT("AssetRegistry started loading during rename operation, skipping asset: %s"), *CurrentName);
            Result.FailedCount++;
            Result.FailedRenames.Add(CurrentName);
            continue;
        }

        // 性能优化：缓存资产名称避免重复磁盘查询
        TArray<FAssetData> AllAssetsInFolder;
        AssetRegistry.GetAssetsByPath(FName(*PackagePath), AllAssetsInFolder, false);

        TSet<FString> ExistingNames;
        for (const FAssetData& Asset : AllAssetsInFolder)
        {
            // 排除当前资产自己，避免自己与自己冲突
            if (Asset.PackageName != AssetData.PackageName)
            {
                ExistingNames.Add(Asset.AssetName.ToString());
            }
        }

        // 命名冲突解决：自动添加数字后缀
        while (ExistingNames.Contains(FinalNewName))
        {
            FinalNewName = FString::Printf(TEXT("%s_%d"), *NewName, SuffixCounter++);
        }

        // ========== 【最终安全检查】防御性编程 ==========
        if (FinalNewName == CurrentName)
        {
            UE_LOG(LogX_AssetNaming, Error, 
                TEXT("CRITICAL: Final rename would be same-name operation (%s)! Skipping."),
                *CurrentName);
            Result.SkippedCount++;
            continue;
        }

        // ========== 【参考 UE 源码】检查资产对象有效性 ==========
        UObject* AssetObject = AssetData.GetAsset();
        if (!AssetObject)
        {
            Result.FailedCount++;
            Result.FailedRenames.Add(CurrentName);
            UE_LOG(LogX_AssetNaming, Error, TEXT("Asset object is null for '%s', cannot rename"),
                *CurrentName);
            continue;
        }

        // 执行重命名操作
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

    // ========== 【暂时禁用】自动清理 Redirectors ==========
    // 原因：重命名后立即清理可能导致崩溃，UE 验证系统可能还在使用旧路径
    /*
    const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
    if (Settings && Settings->bAutoFixupRedirectors && Result.SuccessfulRenames.Num() > 0)
    {
        FixupRedirectors(Result.SuccessfulRenames);
    }
    */

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
    // ========== 【诊断】入口日志 ==========
    UE_LOG(LogX_AssetNaming, Verbose, 
        TEXT("RenameAssetInternal 开始 - 资产: %s, 类型: %s, 包路径: %s"),
        *AssetData.AssetName.ToString(),
        *AssetData.AssetClassPath.ToString(),
        *AssetData.PackagePath.ToString());

    if (!AssetData.IsValid())
    {
        UE_LOG(LogX_AssetNaming, Verbose, TEXT("资产数据无效，跳过"));
        return false;
    }

    // ========== 【重要】参考 UE 源码：AssetRenameManager.cpp:246 ==========
    // 如果 AssetRegistry 仍在加载资产，重命名操作可能失败或导致数据不一致
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    if (AssetRegistry.IsLoadingAssets())
    {
        UE_LOG(LogX_AssetNaming, Warning, TEXT("Cannot rename asset while AssetRegistry is still loading assets"));
        return false;
    }

    // 检查是否在排除列表中
    if (IsAssetExcluded(AssetData))
    {
        UE_LOG(LogX_AssetNaming, Verbose, TEXT("资产在排除列表中，跳过: %s"), *AssetData.AssetName.ToString());
        return false;
    }

    // ========== 【关键】在重命名前缓存所有数据（参考 UE 源码） ==========
    // 重命名后 FAssetData 引用会失效，必须提前提取所有需要的信息
    FString CurrentName = AssetData.AssetName.ToString();
    FString OldPackageName = AssetData.PackageName.ToString();
    FString PackagePath = FPackageName::GetLongPackagePath(OldPackageName);

    if (PackagePath.IsEmpty())
    {
        return false;
    }

    FString SimpleClassName = GetSimpleClassName(AssetData);
    FString CorrectPrefix = GetCorrectPrefix(AssetData, SimpleClassName);

    UE_LOG(LogX_AssetNaming, Verbose, 
        TEXT("资产类型分析 - 当前名称: %s, 简单类名: %s, 正确前缀: %s"),
        *CurrentName, *SimpleClassName, *CorrectPrefix);

    if (CorrectPrefix.IsEmpty())
    {
        UE_LOG(LogX_AssetNaming, Verbose, TEXT("无法确定正确前缀，输出诊断信息: %s"), *CurrentName);
        OutputUnknownAssetDiagnostics(AssetData, SimpleClassName);
        return false;
    }

    // 检查是否已符合规范
    if (CurrentName.StartsWith(CorrectPrefix))
    {
        UE_LOG(LogX_AssetNaming, Verbose, TEXT("资产已符合命名规范，跳过: %s"), *CurrentName);
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

    // ========== 【安全检查】防止同名重命名导致崩溃 ==========
    // UE 的 IAssetTools::RenameAssets() 不支持同名重命名（新旧名称相同）
    if (NewName == CurrentName)
    {
        UE_LOG(LogX_AssetNaming, Verbose, TEXT("Asset '%s' already has the correct name, skipping"),
            *CurrentName);
        return false;
    }

    // ========== 【关键修复】在调用 GetAssetsByPath 前再次检查 AssetRegistry 状态 ==========
    // 防止在检查后、调用前状态发生变化导致崩溃
    if (AssetRegistry.IsLoadingAssets())
    {
        UE_LOG(LogX_AssetNaming, Warning, TEXT("AssetRegistry started loading during rename operation, aborting rename for: %s"), *CurrentName);
        return false;
    }

    TArray<FAssetData> AllAssetsInFolder;
    AssetRegistry.GetAssetsByPath(FName(*PackagePath), AllAssetsInFolder, false);

    TSet<FString> ExistingNames;
    for (const FAssetData& Asset : AllAssetsInFolder)
    {
        // 排除当前资产自己，避免自己与自己冲突
        if (Asset.PackageName != AssetData.PackageName)
        {
            ExistingNames.Add(Asset.AssetName.ToString());
        }
    }

    // 命名冲突解决：自动添加数字后缀
    FString FinalNewName = NewName;
    int32 Suffix = 1;
    while (ExistingNames.Contains(FinalNewName))
    {
        FinalNewName = FString::Printf(TEXT("%s_%d"), *NewName, Suffix++);
    }

    // ========== 【最终安全检查】防御性编程 ==========
    // 理论上不应该发生，但作为最后的安全防线
    if (FinalNewName == CurrentName)
    {
        UE_LOG(LogX_AssetNaming, Error, 
            TEXT("CRITICAL: Final rename would be same-name operation (%s)! This is a logic error."),
            *CurrentName);
        return false;  // 直接拒绝执行，而不是强制修改名称
    }

    // ========== 【参考 UE 源码：AssetRenameManager.cpp:1410】检查资产对象有效性 ==========
    UObject* AssetObject = AssetData.GetAsset();
    if (!AssetObject)
    {
        UE_LOG(LogX_AssetNaming, Warning, TEXT("Asset object is null for '%s', cannot rename"), *CurrentName);
        return false;
    }

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    IAssetTools& AssetTools = AssetToolsModule.Get();

    TArray<FAssetRenameData> AssetsToRename;
    AssetsToRename.Add(FAssetRenameData(AssetObject, PackagePath, FinalNewName));

    if (AssetTools.RenameAssets(AssetsToRename))
    {
        OutNewName = FinalNewName;

        // ========== 【暂时禁用】自动清理 Redirector ==========
        // 原因：重命名后立即清理 Redirector 可能导致崩溃
        // UE 内部的验证系统可能还在引用旧的资产路径
        // TODO: 研究 UE 源码中的正确时机，或者提供手动清理功能
        /*
        const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
        if (Settings && Settings->bAutoFixupRedirectors)
        {
            TArray<FString> OldPaths;
            OldPaths.Add(OldPackageName);
            FixupRedirectors(OldPaths);
        }
        */

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
        if (OldPath.IsEmpty())
        {
            continue;
        }

        FAssetData RedirectorData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(OldPath));
        if (RedirectorData.IsValid() && RedirectorData.AssetClassPath.GetAssetName() == TEXT("ObjectRedirector"))
        {
            // ========== 【安全修复】检查 GetAsset() 返回的对象是否有效 ==========
            UObject* RedirectorObject = RedirectorData.GetAsset();
            if (!RedirectorObject)
            {
                UE_LOG(LogX_AssetNaming, Verbose, TEXT("Redirector object is null for path: %s"), *OldPath);
                continue;
            }

            if (UObjectRedirector* Redirector = Cast<UObjectRedirector>(RedirectorObject))
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

    // ========== 【改进】基于事件，而非时间延迟 ==========
    // 此函数现在通过 OnAssetRenamed 委托调用，而不是 OnInMemoryAssetCreated
    // OnAssetRenamed 在 ContentBrowser 完成 DeferredItem 流程后触发，时序完全正确
    // 不再需要 FTSTicker 延迟执行，直接同步处理即可
    
    // 缓存原始名称（在重命名前，因为重命名后 AssetData 引用可能失效）
    FString OldName = AssetData.AssetName.ToString();
    
    FString NewName;
    if (RenameAssetInternal(AssetData, NewName))
    {
        // 使用缓存的旧名称，而不是访问可能已失效的 AssetData
        UE_LOG(LogX_AssetNaming, Log, TEXT("Auto-renamed asset: %s -> %s"),
            *OldName, *NewName);
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
    
    // ========== 核心规则：只允许 /Game/ 路径（项目内容） ==========
    // 排除所有引擎内容、引擎插件、第三方插件
    FString PackagePath = AssetData.PackagePath.ToString();
    // 修复：/Game 根目录的资产，其 PackagePath 是 "/Game"（无末尾斜杠）
    // 所以检查时不应包含末尾斜杠，否则根目录资产会被错误排除
    if (!PackagePath.StartsWith(TEXT("/Game")))
    {
        UE_LOG(LogX_AssetNaming, Verbose, TEXT("Asset excluded (not in /Game): %s (path: %s)"),
            *AssetData.AssetName.ToString(), *PackagePath);
        return true;
    }

    // 检查排除的资产类型
    FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();
    if (Settings->ExcludedAssetClasses.Contains(ClassName))
    {
        UE_LOG(LogX_AssetNaming, Verbose, TEXT("Asset excluded by class: %s (class: %s)"),
            *AssetData.AssetName.ToString(), *ClassName);
        return true;
    }

    // 检查排除的文件夹（在 /Game/ 内的额外排除）
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

void FX_AssetNamingManager::OutputUnknownAssetDiagnostics(const FAssetData& AssetData, const FString& SimpleClassName) const
{
    // ========== 【诊断信息】输出详细的资产信息以便添加新的命名规则 ==========
    UE_LOG(LogX_AssetNaming, Warning, TEXT("========== 未知资产类型需要添加命名规则 =========="));
    UE_LOG(LogX_AssetNaming, Warning, TEXT("资产名称: %s"), *AssetData.AssetName.ToString());
    UE_LOG(LogX_AssetNaming, Warning, TEXT("资产类路径: %s"), *AssetData.AssetClassPath.ToString());
    UE_LOG(LogX_AssetNaming, Warning, TEXT("简单类名: %s"), *SimpleClassName);
    UE_LOG(LogX_AssetNaming, Warning, TEXT("包路径: %s"), *AssetData.PackagePath.ToString());
    
    // 输出重要的标签信息
    UE_LOG(LogX_AssetNaming, Warning, TEXT("重要标签信息:"));
    
    // 检查常见的重要标签
    TArray<FName> ImportantTags = {
        TEXT("BlueprintType"),
        TEXT("ParentClass"), 
        TEXT("GeneratedClass"),
        TEXT("NativeParentClass"),
        TEXT("BlueprintGeneratedClass"),
        TEXT("ClassFlags"),
        TEXT("ModuleRelativePath"),
        TEXT("IncludePath")
    };
    
    for (const FName& TagName : ImportantTags)
    {
        FAssetDataTagMapSharedView::FFindTagResult TagResult = AssetData.TagsAndValues.FindTag(TagName);
        if (TagResult.IsSet())
        {
            UE_LOG(LogX_AssetNaming, Warning, TEXT("  - %s: %s"), 
                *TagName.ToString(), *TagResult.GetValue());
        }
    }
    
    // 检查是否有父类信息
    FAssetDataTagMapSharedView::FFindTagResult ParentClassTag = AssetData.TagsAndValues.FindTag(TEXT("ParentClass"));
    if (ParentClassTag.IsSet())
    {
        FString ParentClassPath = ParentClassTag.GetValue();
        UE_LOG(LogX_AssetNaming, Warning, TEXT("父类路径: %s"), *ParentClassPath);
        
        // 提取父类简单名称
        FString ParentSimpleClassName;
        int32 LastDotIndex;
        if (ParentClassPath.FindLastChar('.', LastDotIndex))
        {
            ParentSimpleClassName = ParentClassPath.RightChop(LastDotIndex + 1);
        }
        else
        {
            ParentSimpleClassName = ParentClassPath;
        }
        UE_LOG(LogX_AssetNaming, Warning, TEXT("父类简单名称: %s"), *ParentSimpleClassName);
    }
    
    // 检查蓝图类型标签
    FAssetDataTagMapSharedView::FFindTagResult BlueprintTypeTag = AssetData.TagsAndValues.FindTag(TEXT("BlueprintType"));
    if (BlueprintTypeTag.IsSet())
    {
        UE_LOG(LogX_AssetNaming, Warning, TEXT("蓝图类型: %s"), *BlueprintTypeTag.GetValue());
    }
    
    // 提供添加建议
    UE_LOG(LogX_AssetNaming, Warning, TEXT("建议添加到 AssetPrefixMappings:"));
    UE_LOG(LogX_AssetNaming, Warning, TEXT("  AssetPrefixMappings.Add(TEXT(\"%s\"), TEXT(\"[前缀]_\"));"), *SimpleClassName);
    
    if (ParentClassTag.IsSet())
    {
        UE_LOG(LogX_AssetNaming, Warning, TEXT("或者添加到 ParentClassPrefixMappings (如果是通用蓝图类型):"));
        FString ParentClassPath = ParentClassTag.GetValue();
        FString ParentSimpleClassName;
        int32 LastDotIndex;
        if (ParentClassPath.FindLastChar('.', LastDotIndex))
        {
            ParentSimpleClassName = ParentClassPath.RightChop(LastDotIndex + 1);
        }
        else
        {
            ParentSimpleClassName = ParentClassPath;
        }
        UE_LOG(LogX_AssetNaming, Warning, TEXT("  ParentClassPrefixMappings.Add(TEXT(\"%s\"), TEXT(\"[前缀]_\"));"), *ParentSimpleClassName);
    }
    
    UE_LOG(LogX_AssetNaming, Warning, TEXT("================================================"));
}

#undef LOCTEXT_NAMESPACE
