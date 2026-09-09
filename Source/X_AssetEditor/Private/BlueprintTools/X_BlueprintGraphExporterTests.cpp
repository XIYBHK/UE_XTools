/*
 * 蓝图图表导出器自动化测试
 */

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "BlueprintTools/X_BlueprintGraphExporter.h"

#include "Dom/JsonObject.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Knot.h"
#include "HAL/PlatformTime.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXBlueprintGraphExporterGraphSnapshotTest,
    "XTools.AssetEditor.BlueprintGraphExporter.GraphSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXBlueprintGraphExporterGraphSnapshotTest::RunTest(const FString& Parameters)
{
    UBlueprint* Blueprint = NewObject<UBlueprint>(GetTransientPackage(), TEXT("XToolsExportBlueprint"));
    Blueprint->ParentClass = AActor::StaticClass();
    UEdGraph* Graph = NewObject<UEdGraph>(Blueprint, TEXT("XToolsExportFixture"));
    Blueprint->UbergraphPages.Add(Graph);
    Graph->Schema = UEdGraphSchema_K2::StaticClass();
    const UEdGraphSchema* Schema = Graph->GetSchema();
    TArray<UK2Node_CustomEvent*> Entries;
    for (int32 I = 0; I < 3; ++I)
    {
        UK2Node_CustomEvent* Entry = NewObject<UK2Node_CustomEvent>(Graph,
            *FString::Printf(TEXT("Entry%d"), I));
        Entry->CustomFunctionName = *FString::Printf(TEXT("Event%d"), I);
        Graph->AddNode(Entry);
        Entry->AllocateDefaultPins();
        Entries.Add(Entry);
    }
    UK2Node_Knot* Knot = NewObject<UK2Node_Knot>(Graph, TEXT("Reroute"));
    Graph->AddNode(Knot);
    Knot->AllocateDefaultPins();
    UK2Node_ExecutionSequence* Previous = nullptr;
    UK2Node_ExecutionSequence* First = nullptr;
    for (int32 I = 0; I < 128; ++I)
    {
        UK2Node_ExecutionSequence* Node = NewObject<UK2Node_ExecutionSequence>(Graph,
            *FString::Printf(TEXT("Sequence%d"), I));
        Graph->AddNode(Node);
        Node->AllocateDefaultPins();
        Node->NodePosY = I * 10;
        if (Previous)
        {
            TestTrue(TEXT("Connect chain"), Schema->TryCreateConnection(
                Previous->GetThenPinGivenIndex(0), Node->GetExecPin()));
        }
        else
        {
            First = Node;
        }
        Previous = Node;
    }
    TestTrue(TEXT("Connect reroute"), Schema->TryCreateConnection(Knot->GetOutputPin(), First->GetExecPin()));
    for (UK2Node_CustomEvent* Entry : Entries)
    {
        TestTrue(TEXT("Shared chain from each entry"), Schema->TryCreateConnection(Entry->FindPinChecked(UEdGraphSchema_K2::PN_Then), Knot->GetInputPin()));
    }
    TestTrue(TEXT("Execution cycle"), Schema->TryCreateConnection(Previous->GetThenPinGivenIndex(0), First->GetExecPin()));
    // 固定 GUID，便于两版实现之间比较完整序列化结果。
    int32 GuidIndex = 1;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        Node->NodeGuid = FGuid(1, 0, 0, GuidIndex++);
        for (UEdGraphPin* Pin : Node->Pins)
        {
            Pin->PinId = FGuid(2, 0, 0, GuidIndex++);
        }
    }
    const double Start = FPlatformTime::Seconds();
    const TSharedPtr<FJsonObject> Json = XBlueprintGraphExporterTests::BuildGraphJson(Graph);
    const FString Markdown = XBlueprintGraphExporterTests::BuildGraphMarkdown(Graph);
    const double ElapsedMs = (FPlatformTime::Seconds() - Start) * 1000;
    TestEqual(TEXT("All nodes exported"), static_cast<int32>(Json->GetNumberField(TEXT("node_count"))), 132);
    TestEqual(TEXT("All entry sections retained"), Json->GetArrayField(TEXT("entry_nodes")).Num(), 3);
    TestEqual(TEXT("Stable repeated Markdown"), XBlueprintGraphExporterTests::BuildGraphMarkdown(Graph), Markdown);
    FString JsonText;
    FJsonSerializer::Serialize(Json.ToSharedRef(), TJsonWriterFactory<>::Create(&JsonText));
    AddInfo(FString::Printf(TEXT("GraphSnapshot JSON=%s Markdown=%s elapsed=%.3fms"),
        *FMD5::HashBytes(reinterpret_cast<const uint8*>(*JsonText), JsonText.Len() * sizeof(TCHAR)),
        *FMD5::HashBytes(reinterpret_cast<const uint8*>(*Markdown), Markdown.Len() * sizeof(TCHAR)), ElapsedMs));
    Entries[0]->CustomFunctionName = TEXT("ChangedEntry");
    Entries[0]->NodePosY = 2000;
    Entries[0]->FindPinChecked(UEdGraphSchema_K2::PN_Then)->BreakAllPinLinks();
    const FString Changed = XBlueprintGraphExporterTests::BuildGraphMarkdown(Graph);
    TestTrue(TEXT("Next export observes title mutation"), Changed.Contains(TEXT("ChangedEntry")));
    TestTrue(TEXT("Next export observes changed layout and links"), Changed != Markdown);
    TestTrue(TEXT("Empty graph export"), XBlueprintGraphExporterTests::BuildGraphJson(
        NewObject<UEdGraph>())->GetArrayField(TEXT("nodes")).IsEmpty());
    return true;
}

#endif
