/*
 * 蓝图图表导出器自动化测试
 */

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "BlueprintTools/X_BlueprintGraphExporter.h"

#include "Dom/JsonObject.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_InputKeyEvent.h"
#include "K2Node_MathExpression.h"
#include "K2Node_SetFieldsInStruct.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    bool TestSemanticKind(
        FAutomationTestBase& Test,
        UEdGraphNode* Node,
        const FString& ExpectedKind)
    {
        const TSharedPtr<FJsonObject> Json = XBlueprintGraphExporterTests::BuildNodeSemanticJson(Node);
        if (!Test.TestNotNull(TEXT("受支持节点应生成语义对象"), Json.Get()))
        {
            return false;
        }

        FString ActualKind;
        if (!Test.TestTrue(TEXT("语义对象应包含 kind 字段"), Json->TryGetStringField(TEXT("kind"), ActualKind)))
        {
            return false;
        }

        return Test.TestEqual(TEXT("派生节点应匹配最具体的语义类型"), ActualKind, ExpectedKind);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXBlueprintGraphExporterSemanticClassificationTest,
    "XTools.AssetEditor.BlueprintGraphExporter.SemanticClassification",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXBlueprintGraphExporterSemanticClassificationTest::RunTest(const FString& Parameters)
{
    UK2Node_MathExpression* MathExpression = NewObject<UK2Node_MathExpression>();
    MathExpression->Expression = TEXT("A+B");
    TestTrue(TEXT("数学表达式不应退化为复合节点或隧道节点"),
        TestSemanticKind(*this, MathExpression, TEXT("math_expression")));

    TestTrue(TEXT("设置结构体字段不应退化为结构体成员或变量节点"),
        TestSemanticKind(*this, NewObject<UK2Node_SetFieldsInStruct>(), TEXT("set_fields_in_struct")));
    TestTrue(TEXT("输入按键事件不应退化为通用事件"),
        TestSemanticKind(*this, NewObject<UK2Node_InputKeyEvent>(), TEXT("input_key_event")));
    TestTrue(TEXT("执行序列应保持控制流语义"),
        TestSemanticKind(*this, NewObject<UK2Node_ExecutionSequence>(), TEXT("sequence")));

    TestFalse(TEXT("普通图节点不应伪造语义对象"),
        XBlueprintGraphExporterTests::BuildNodeSemanticJson(NewObject<UEdGraphNode>()).IsValid());
    TestFalse(TEXT("空节点不应生成语义对象"),
        XBlueprintGraphExporterTests::BuildNodeSemanticJson(nullptr).IsValid());
    return true;
}

#endif
