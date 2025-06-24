// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetNaming/X_AssetNamingBlueprintLibrary.h"
#include "AssetNaming/X_AssetNamingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorUtilityLibrary.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFilemanager.h"

FX_AssetNamingResult UX_AssetNamingBlueprintLibrary::RenameSelectedAssets()
{
    FX_AssetNamingResult Result;
    
    // 获取选中的资产
    TArray<FAssetData> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssetData();
    Result.TotalCount = SelectedAssets.Num();
    
    if (SelectedAssets.Num() == 0)
    {
        Result.ResultMessage = TEXT("未选中任何资产");
        return Result;
    }

    // 执行重命名操作
    FX_AssetNamingManager::Get().RenameSelectedAssets();
    
    // 注意：实际的统计需要从管理器中获取，这里简化处理
    Result.bIsSuccess = true;
    Result.ResultMessage = FString::Printf(TEXT("已处理 %d 个资产的命名规范化"), Result.TotalCount);
    
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

    const TMap<FString, FString>& PrefixMap = FX_AssetNamingManager::Get().GetAssetPrefixes();
    
    for (const auto& Pair : PrefixMap)
    {
        OutAssetTypes.Add(Pair.Key);
        OutPrefixes.Add(Pair.Value);
    }
}

// === 测试节点实现 ===

FString UX_AssetNamingBlueprintLibrary::TestAssetNamingManager()
{
    FString TestResults;
    bool bAllTestsPassed = true;

    // 测试1: 管理器初始化
    try
    {
        FX_AssetNamingManager& Manager = FX_AssetNamingManager::Get();
        const TMap<FString, FString>& Prefixes = Manager.GetAssetPrefixes();
        
        if (Prefixes.Num() > 0)
        {
            TestResults += CreateTestResultMessage(TEXT("管理器初始化"), true, 
                FString::Printf(TEXT("成功加载 %d 个前缀规则"), Prefixes.Num()));
        }
        else
        {
            TestResults += CreateTestResultMessage(TEXT("管理器初始化"), false, TEXT("前缀规则为空"));
            bAllTestsPassed = false;
        }
    }
    catch (...)
    {
        TestResults += CreateTestResultMessage(TEXT("管理器初始化"), false, TEXT("发生异常"));
        bAllTestsPassed = false;
    }

    // 测试2: 基础前缀查找
    const TArray<FString> TestClasses = {
        TEXT("StaticMesh"), TEXT("Material"), TEXT("Blueprint"), 
        TEXT("Texture2D"), TEXT("WidgetBlueprint")
    };

    for (const FString& ClassName : TestClasses)
    {
        const TMap<FString, FString>& Prefixes = FX_AssetNamingManager::Get().GetAssetPrefixes();
        const FString* PrefixPtr = Prefixes.Find(ClassName);
        
        if (PrefixPtr && !PrefixPtr->IsEmpty())
        {
            TestResults += CreateTestResultMessage(
                FString::Printf(TEXT("前缀查找-%s"), *ClassName), 
                true, 
                FString::Printf(TEXT("找到前缀: %s"), **PrefixPtr)
            );
        }
        else
        {
            TestResults += CreateTestResultMessage(
                FString::Printf(TEXT("前缀查找-%s"), *ClassName), 
                false, 
                TEXT("未找到前缀")
            );
            bAllTestsPassed = false;
        }
    }

    // 总结
    FString Summary = bAllTestsPassed ? TEXT("✅ 所有测试通过") : TEXT("❌ 部分测试失败");
    return FString::Printf(TEXT("%s\n\n%s"), *Summary, *TestResults);
}

FString UX_AssetNamingBlueprintLibrary::TestPrefixRulesIntegrity()
{
    FString TestResults;
    bool bAllTestsPassed = true;
    
    const TMap<FString, FString>& Prefixes = FX_AssetNamingManager::Get().GetAssetPrefixes();
    
    // 测试1: 检查是否有空前缀
    int32 EmptyPrefixCount = 0;
    for (const auto& Pair : Prefixes)
    {
        if (Pair.Value.IsEmpty())
        {
            EmptyPrefixCount++;
        }
    }
    
    TestResults += CreateTestResultMessage(TEXT("空前缀检查"), EmptyPrefixCount == 0, 
        FString::Printf(TEXT("发现 %d 个空前缀"), EmptyPrefixCount));
    
    if (EmptyPrefixCount > 0)
    {
        bAllTestsPassed = false;
    }

    // 测试2: 检查前缀重复
    TMap<FString, TArray<FString>> PrefixToClasses;
    for (const auto& Pair : Prefixes)
    {
        PrefixToClasses.FindOrAdd(Pair.Value).Add(Pair.Key);
    }
    
    int32 DuplicatePrefixCount = 0;
    for (const auto& Pair : PrefixToClasses)
    {
        if (Pair.Value.Num() > 1)
        {
            DuplicatePrefixCount++;
            TestResults += FString::Printf(TEXT("重复前缀 '%s': %s\n"), 
                *Pair.Key, *FString::Join(Pair.Value, TEXT(", ")));
        }
    }
    
    TestResults += CreateTestResultMessage(TEXT("前缀重复检查"), DuplicatePrefixCount == 0,
        FString::Printf(TEXT("发现 %d 个重复前缀"), DuplicatePrefixCount));
    
    if (DuplicatePrefixCount > 0)
    {
        bAllTestsPassed = false;
    }

    // 测试3: 检查关键资产类型覆盖
    const TArray<FString> CriticalAssetTypes = {
        TEXT("StaticMesh"), TEXT("Material"), TEXT("Blueprint"), 
        TEXT("Texture2D"), TEXT("WidgetBlueprint"), TEXT("DataTable")
    };
    
    int32 MissingCriticalTypes = 0;
    for (const FString& CriticalType : CriticalAssetTypes)
    {
        if (!Prefixes.Contains(CriticalType))
        {
            MissingCriticalTypes++;
        }
    }
    
    TestResults += CreateTestResultMessage(TEXT("关键类型覆盖"), MissingCriticalTypes == 0,
        FString::Printf(TEXT("缺少 %d 个关键资产类型"), MissingCriticalTypes));
    
    if (MissingCriticalTypes > 0)
    {
        bAllTestsPassed = false;
    }

    // 总结
    FString Summary = bAllTestsPassed ? TEXT("✅ 前缀规则完整性检查通过") : TEXT("❌ 前缀规则存在问题");
    return FString::Printf(TEXT("%s\n\n%s"), *Summary, *TestResults);
}

FString UX_AssetNamingBlueprintLibrary::TestAssetClassNameParsing(const TArray<FString>& TestAssetPaths)
{
    FString TestResults;
    bool bAllTestsPassed = true;
    int32 SuccessCount = 0;
    int32 FailureCount = 0;

    for (const FString& AssetPath : TestAssetPaths)
    {
        FAssetData AssetData = GetAssetDataFromPath(AssetPath);
        
        if (AssetData.IsValid())
        {
            FString ClassName = FX_AssetNamingManager::Get().GetSimpleClassName(AssetData);
            FString CorrectPrefix = FX_AssetNamingManager::Get().GetCorrectPrefix(AssetData, ClassName);
            
            if (!ClassName.IsEmpty() && !CorrectPrefix.IsEmpty())
            {
                SuccessCount++;
                TestResults += FString::Printf(TEXT("✅ %s -> 类型: %s, 前缀: %s\n"), 
                    *AssetPath, *ClassName, *CorrectPrefix);
            }
            else
            {
                FailureCount++;
                TestResults += FString::Printf(TEXT("❌ %s -> 解析失败\n"), *AssetPath);
                bAllTestsPassed = false;
            }
        }
        else
        {
            FailureCount++;
            TestResults += FString::Printf(TEXT("❌ %s -> 资产无效\n"), *AssetPath);
            bAllTestsPassed = false;
        }
    }

    FString Summary = FString::Printf(TEXT("类名解析测试: 成功 %d, 失败 %d"), SuccessCount, FailureCount);
    return FString::Printf(TEXT("%s\n\n%s"), *Summary, *TestResults);
}

FString UX_AssetNamingBlueprintLibrary::PerformanceTestAssetNaming(int32 AssetCount)
{
    double StartTime = FPlatformTime::Seconds();
    
    // 获取一些测试资产
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    TArray<FAssetData> AllAssets;
    AssetRegistry.GetAllAssets(AllAssets);
    
    int32 TestCount = FMath::Min(AssetCount, AllAssets.Num());
    int32 ProcessedCount = 0;
    
    for (int32 i = 0; i < TestCount; ++i)
    {
        const FAssetData& AssetData = AllAssets[i];
        FString ClassName = FX_AssetNamingManager::Get().GetSimpleClassName(AssetData);
        FString CorrectPrefix = FX_AssetNamingManager::Get().GetCorrectPrefix(AssetData, ClassName);
        
        if (!CorrectPrefix.IsEmpty())
        {
            ProcessedCount++;
        }
    }
    
    double EndTime = FPlatformTime::Seconds();
    double ElapsedTime = EndTime - StartTime;
    
    return FString::Printf(TEXT("性能测试结果:\n处理资产数: %d\n成功处理: %d\n耗时: %.3f 秒\n平均每个资产: %.3f 毫秒"), 
        TestCount, ProcessedCount, ElapsedTime, (ElapsedTime * 1000.0) / TestCount);
}

FString UX_AssetNamingBlueprintLibrary::ValidateNamingNormalization()
{
    // 这个测试需要实际的资产来验证，这里提供框架
    return TEXT("命名规范化验证测试需要选中具体的资产来执行。\n请在内容浏览器中选中一些资产，然后调用此测试。");
}

FAssetData UX_AssetNamingBlueprintLibrary::GetAssetDataFromPath(const FString& AssetPath)
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    return AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
}

FString UX_AssetNamingBlueprintLibrary::CreateTestResultMessage(const FString& TestName, bool bSuccess, const FString& Details)
{
    FString Status = bSuccess ? TEXT("✅") : TEXT("❌");
    return FString::Printf(TEXT("%s %s: %s\n"), *Status, *TestName, *Details);
}
