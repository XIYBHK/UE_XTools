/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#include "AssetNaming/X_AssetNamingBlueprintLibrary.h"
#include "AssetNaming/X_AssetNamingManager.h"
#include "Settings/X_AssetEditorSettings.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorUtilityLibrary.h"

FX_AssetNamingResult UX_AssetNamingBlueprintLibrary::RenameSelectedAssets()
{
    FX_AssetNamingResult Result;
    TArray<FAssetData> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssetData();
    Result.TotalCount = SelectedAssets.Num();
    if (SelectedAssets.Num() == 0)
    {
        Result.ResultMessage = TEXT("未选中任何资产");
        return Result;
    }

    // 获取真实操作结果
    FX_RenameOperationResult OpResult = FX_AssetNamingManager::Get().RenameSelectedAssets();

    Result.SuccessCount = OpResult.SuccessCount;
    Result.SkippedCount = OpResult.SkippedCount;
    Result.FailedCount = OpResult.FailedCount;
    Result.TotalCount = OpResult.SuccessCount + OpResult.SkippedCount + OpResult.FailedCount;
    Result.bIsSuccess = (OpResult.FailedCount == 0);
    Result.ResultMessage = FString::Printf(
        TEXT("重命名: %d | 跳过: %d | 失败: %d"),
        Result.SuccessCount,
        Result.SkippedCount,
        Result.FailedCount
    );

    return Result;
}

FString UX_AssetNamingBlueprintLibrary::GetAssetCorrectPrefix(const FString& AssetPath)
{
    FAssetData AssetData = GetAssetDataFromPath(AssetPath);
    if (!AssetData.IsValid())
    {
        return TEXT("");
    }

    FString SimpleClassName = FX_AssetNamingManager::Get().GetSimpleClassName(AssetData);
    return FX_AssetNamingManager::Get().GetCorrectPrefix(AssetData, SimpleClassName);
}

bool UX_AssetNamingBlueprintLibrary::IsAssetNameValid(const FString& AssetPath)
{
    FAssetData AssetData = GetAssetDataFromPath(AssetPath);
    if (!AssetData.IsValid())
    {
        return false;
    }

    FString CurrentName = AssetData.AssetName.ToString();
    FString SimpleClassName = FX_AssetNamingManager::Get().GetSimpleClassName(AssetData);
    FString CorrectPrefix = FX_AssetNamingManager::Get().GetCorrectPrefix(AssetData, SimpleClassName);
    
    return !CorrectPrefix.IsEmpty() && CurrentName.StartsWith(CorrectPrefix);
}

FString UX_AssetNamingBlueprintLibrary::GetAssetClassName(const FString& AssetPath)
{
    FAssetData AssetData = GetAssetDataFromPath(AssetPath);
    if (!AssetData.IsValid())
    {
        return TEXT("");
    }

    return FX_AssetNamingManager::Get().GetSimpleClassName(AssetData);
}

void UX_AssetNamingBlueprintLibrary::GetAllAssetPrefixRules(TArray<FString>& OutAssetTypes, TArray<FString>& OutPrefixes)
{
    OutAssetTypes.Empty();
    OutPrefixes.Empty();

    const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
    if (Settings)
    {
        for (const auto& Pair : Settings->AssetPrefixMappings)
        {
            OutAssetTypes.Add(Pair.Key);
            OutPrefixes.Add(Pair.Value);
        }
    }
}


FAssetData UX_AssetNamingBlueprintLibrary::GetAssetDataFromPath(const FString& AssetPath)
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    return AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
}
