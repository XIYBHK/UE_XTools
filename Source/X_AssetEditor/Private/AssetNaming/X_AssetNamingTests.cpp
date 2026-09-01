/*
 * 自动化测试：资产命名规则
 */

#include "Misc/AutomationTest.h"
#include "AssetNaming/X_AssetNamingManager.h"
#include "Settings/X_AssetEditorSettings.h"

// 简单类名前缀测试
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXAssetNaming_GetCorrectPrefix_SimpleClass,
    "XTools.AssetNaming.GetCorrectPrefix.SimpleClass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXAssetNaming_GetCorrectPrefix_SimpleClass::RunTest(const FString& Parameters)
{
    FX_AssetNamingManager& Manager = FX_AssetNamingManager::Get();
    UX_AssetEditorSettings* Settings = GetMutableDefault<UX_AssetEditorSettings>();

    const TMap<FString, FString> SavedPrefixMappings = Settings->AssetPrefixMappings;
    const TArray<FString> SavedExcludedClasses = Settings->ExcludedAssetClasses;
    const TArray<FString> SavedExcludedFolders = Settings->ExcludedFolders;
    Settings->AssetPrefixMappings.Add(TEXT("StaticMesh"), TEXT("SM_"));
    Settings->ExcludedAssetClasses.Remove(TEXT("StaticMesh"));
    Settings->ExcludedFolders.Reset();

    // 构造一个位于项目内容目录中的 StaticMesh 资产数据
    FAssetData AssetData;
    AssetData.PackageName = TEXT("/Game/Test/TestStaticMesh");
    AssetData.AssetClassPath = FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("StaticMesh"));
    AssetData.AssetName = TEXT("TestStaticMesh");
    AssetData.PackagePath = TEXT("/Game/Test");

    const FString SimpleClassName = Manager.GetSimpleClassName(AssetData);
    const FString Prefix = Manager.GetCorrectPrefix(AssetData, SimpleClassName);

    Settings->AssetPrefixMappings = SavedPrefixMappings;
    Settings->ExcludedAssetClasses = SavedExcludedClasses;
    Settings->ExcludedFolders = SavedExcludedFolders;

    TestEqual(TEXT("StaticMesh 应该返回 SM_ 前缀"), Prefix, FString(TEXT("SM_")));
    return true;
}

// 数字后缀规范化测试
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXAssetNaming_NormalizeNumericSuffix,
    "XTools.AssetNaming.NormalizeNumericSuffix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXAssetNaming_NormalizeNumericSuffix::RunTest(const FString& Parameters)
{
    FX_AssetNamingManager& Manager = FX_AssetNamingManager::Get();

    TestEqual(TEXT("_1 应该规范化为 _01"),
        Manager.NormalizeNumericSuffix(TEXT("BP_角色_1")),
        FString(TEXT("BP_角色_01")));

    TestEqual(TEXT("已是两位数的后缀应保持不变"),
        Manager.NormalizeNumericSuffix(TEXT("BP_角色_10")),
        FString(TEXT("BP_角色_10")));

    TestEqual(TEXT("没有数字后缀的名称应保持不变"),
        Manager.NormalizeNumericSuffix(TEXT("BP_角色")),
        FString(TEXT("BP_角色")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXAssetNaming_LongestPrefixWins,
    "XTools.AssetNaming.Prefix.LongestMatchWins",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXAssetNaming_LongestPrefixWins::RunTest(const FString& Parameters)
{
    TMap<FString, FString> PrefixMappings;
    PrefixMappings.Add(TEXT("Short"), TEXT("T_"));
    PrefixMappings.Add(TEXT("Long"), TEXT("T_UI_"));
    PrefixMappings.Add(TEXT("Excluded"), TEXT("T_UI_Icon_"));

    TestEqual(TEXT("应选择最长的非排除匹配前缀"),
        XAssetNaming::FindLongestMatchingPrefix(TEXT("T_UI_Icon_Close"), TEXT("T_UI_Icon_"), PrefixMappings),
        FString(TEXT("T_UI_")));
    TestTrue(TEXT("没有匹配前缀时应返回空字符串"),
        XAssetNaming::FindLongestMatchingPrefix(TEXT("SM_Chair"), FString(), PrefixMappings).IsEmpty());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXAssetNaming_ResolveNameCollisionUsesNormalizedSuffix,
    "XTools.AssetNaming.Collision.UsesTwoDigitSuffix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXAssetNaming_ResolveNameCollisionUsesNormalizedSuffix::RunTest(const FString& Parameters)
{
    TSet<FString> ExistingNames;
    ExistingNames.Add(TEXT("SM_Chair"));
    ExistingNames.Add(TEXT("SM_Chair_01"));
    ExistingNames.Add(TEXT("SM_Chair_02"));

    TestEqual(TEXT("冲突名称应跳过已占用的两位数字后缀"),
        XAssetNaming::ResolveNameCollision(TEXT("SM_Chair"), ExistingNames),
        FString(TEXT("SM_Chair_03")));
    TestEqual(TEXT("未冲突名称应保持不变"),
        XAssetNaming::ResolveNameCollision(TEXT("SM_Table"), ExistingNames),
        FString(TEXT("SM_Table")));

    return true;
}

// 失败记录 helper：计数、旧名称数组、详情数组必须同步写入
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXAssetNaming_RecordRenameFailureSyncsAllFields,
    "XTools.AssetNaming.RecordRenameFailure.SyncsAllFields",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXAssetNaming_RecordRenameFailureSyncsAllFields::RunTest(const FString& Parameters)
{
    FX_RenameOperationResult Result;

    XAssetNaming::RecordRenameFailure(Result, TEXT("SM_Chair"), TEXT("无法确定资产前缀"));
    XAssetNaming::RecordRenameFailure(Result, TEXT("BP_Door"), TEXT("资产对象为空"));
    XAssetNaming::RecordRenameFailure(Result, TEXT("M_Wood"), TEXT("主路径重命名失败"));

    TestEqual(TEXT("失败计数应为3"), Result.FailedCount, 3);
    TestEqual(TEXT("旧名称数组应同步记录3条"), Result.FailedRenames.Num(), 3);
    TestEqual(TEXT("详情数组应同步记录3条"), Result.FailedDetails.Num(), 3);

    TestEqual(TEXT("第1条名称应写入旧数组"), Result.FailedRenames[0], FString(TEXT("SM_Chair")));
    TestEqual(TEXT("第1条详情名称应一致"), Result.FailedDetails[0].AssetName, FString(TEXT("SM_Chair")));
    TestEqual(TEXT("第1条详情原因应保留"), Result.FailedDetails[0].Reason, FString(TEXT("无法确定资产前缀")));
    TestEqual(TEXT("第2条详情原因应可区分"), Result.FailedDetails[1].Reason, FString(TEXT("资产对象为空")));
    TestEqual(TEXT("第3条详情原因应可区分"), Result.FailedDetails[2].Reason, FString(TEXT("主路径重命名失败")));

    // 旧数组与详情数组的名称必须逐条一致（兼容性契约）
    for (int32 Index = 0; Index < Result.FailedRenames.Num(); ++Index)
    {
        TestEqual(TEXT("旧数组与详情数组名称应逐条一致"),
            Result.FailedRenames[Index], Result.FailedDetails[Index].AssetName);
    }

    return true;
}

// 失败详情格式化：输出必须包含资产名与原因，并在超限时注明剩余数量
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXAssetNaming_FormatFailureDetailsContainsNameAndReason,
    "XTools.AssetNaming.FormatFailureDetails.NameReasonAndTruncation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXAssetNaming_FormatFailureDetailsContainsNameAndReason::RunTest(const FString& Parameters)
{
    FX_RenameOperationResult EmptyResult;
    TestTrue(TEXT("无失败时格式化结果应为空字符串"),
        XAssetNaming::FormatFailureDetails(EmptyResult, 50).IsEmpty());

    FX_RenameOperationResult Result;
    XAssetNaming::RecordRenameFailure(Result, TEXT("SM_Chair"), TEXT("无法确定资产前缀"));
    XAssetNaming::RecordRenameFailure(Result, TEXT("BP_Door"), TEXT("资产对象为空"));
    XAssetNaming::RecordRenameFailure(Result, TEXT("M_Wood"), TEXT("主路径重命名失败"));

    const FString FullText = XAssetNaming::FormatFailureDetails(Result, MAX_int32);
    TestTrue(TEXT("完整输出应包含第1条资产名"), FullText.Contains(TEXT("SM_Chair")));
    TestTrue(TEXT("完整输出应包含第1条原因"), FullText.Contains(TEXT("无法确定资产前缀")));
    TestTrue(TEXT("完整输出应包含第2条资产名"), FullText.Contains(TEXT("BP_Door")));
    TestTrue(TEXT("完整输出应包含第2条原因"), FullText.Contains(TEXT("资产对象为空")));
    TestTrue(TEXT("完整输出应包含第3条资产名"), FullText.Contains(TEXT("M_Wood")));
    TestTrue(TEXT("完整输出应包含第3条原因"), FullText.Contains(TEXT("主路径重命名失败")));
    TestFalse(TEXT("未截断时不应出现剩余提示"), FullText.Contains(TEXT("其余")));

    const FString TruncatedText = XAssetNaming::FormatFailureDetails(Result, 2);
    TestTrue(TEXT("截断输出应包含前2条"), TruncatedText.Contains(TEXT("SM_Chair")) && TruncatedText.Contains(TEXT("BP_Door")));
    TestFalse(TEXT("截断输出不应包含第3条资产名"), TruncatedText.Contains(TEXT("M_Wood")));
    TestTrue(TEXT("截断输出应注明剩余1条"), TruncatedText.Contains(TEXT("其余 1 条")));

    return true;
}
