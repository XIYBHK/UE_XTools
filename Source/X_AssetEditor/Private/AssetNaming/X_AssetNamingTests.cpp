/*
 * 自动化测试：资产命名规则
 */

#include "Misc/AutomationTest.h"
#include "AssetNaming/X_AssetNamingManager.h"

// 简单类名前缀测试
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXAssetNaming_GetCorrectPrefix_SimpleClass,
    "XTools.资产命名.GetCorrectPrefix.简单类名",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXAssetNaming_GetCorrectPrefix_SimpleClass::RunTest(const FString& Parameters)
{
    FX_AssetNamingManager& Manager = FX_AssetNamingManager::Get();

    // 构造一个模拟的 StaticMesh 资产数据（只关心 AssetClassPath）
    FAssetData AssetData;
    AssetData.AssetClassPath = FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("StaticMesh"));

    const FString SimpleClassName = Manager.GetSimpleClassName(AssetData);
    const FString Prefix = Manager.GetCorrectPrefix(AssetData, SimpleClassName);

    TestEqual(TEXT("StaticMesh 应该返回 SM_ 前缀"), Prefix, FString(TEXT("SM_")));
    return true;
}

// 数字后缀规范化测试
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXAssetNaming_NormalizeNumericSuffix,
    "XTools.资产命名.NormalizeNumericSuffix",
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
