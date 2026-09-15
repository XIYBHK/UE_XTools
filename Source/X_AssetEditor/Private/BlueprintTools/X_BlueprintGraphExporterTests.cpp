/*
 * 蓝图图表导出器自动化测试
 */

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "BlueprintTools/X_BlueprintGraphExporter.h"
#include "BlueprintTools/X_BlueprintReadPack.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Components/StaticMeshComponent.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "GameFramework/Actor.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Knot.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_InputKeyEvent.h"
#include "K2Node_MathExpression.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_AssignmentStatement.h"
#include "K2Node_TemporaryVariable.h"
#include "K2Node_SetFieldsInStruct.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

namespace
{
    TMap<FString, FString> BuildReadPackForGraph(const TSharedRef<FJsonObject>& Graph)
    {
        const TSharedRef<FJsonObject> Snapshot = MakeShared<FJsonObject>();
        Snapshot->SetStringField(TEXT("asset_path"), TEXT("/Temp/ReadPackFixture.ReadPackFixture"));
        Snapshot->SetArrayField(TEXT("graphs"), { MakeShared<FJsonValueObject>(Graph) });
        return XBlueprintReadPack::Build(Snapshot, FString());
    }

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
    const TMap<FString, FString> ReadPack = BuildReadPackForGraph(Json.ToSharedRef());
    const FString& Logic = ReadPack.FindChecked(TEXT("10_Logic/G0001.pseudo.md"));
    const double ElapsedMs = (FPlatformTime::Seconds() - Start) * 1000;
    TestEqual(TEXT("All nodes exported"), static_cast<int32>(Json->GetNumberField(TEXT("node_count"))), 132);
    TestEqual(TEXT("All entry sections retained"), Json->GetArrayField(TEXT("entry_nodes")).Num(), 3);
    const TMap<FString, FString> Repeated = BuildReadPackForGraph(XBlueprintGraphExporterTests::BuildGraphJson(Graph).ToSharedRef());
    TestEqual(TEXT("Stable repeated logic"), Repeated.FindChecked(TEXT("10_Logic/G0001.pseudo.md")), Logic);
    TestEqual(TEXT("Stable repeated evidence"), Repeated.FindChecked(TEXT("20_Evidence/G0001.json")), ReadPack.FindChecked(TEXT("20_Evidence/G0001.json")));
    FString JsonText;
    FJsonSerializer::Serialize(Json.ToSharedRef(), TJsonWriterFactory<>::Create(&JsonText));
    AddInfo(FString::Printf(TEXT("GraphSnapshot JSON=%s Logic=%s elapsed=%.3fms"),
        *FMD5::HashBytes(reinterpret_cast<const uint8*>(*JsonText), JsonText.Len() * sizeof(TCHAR)),
        *FMD5::HashBytes(reinterpret_cast<const uint8*>(*Logic), Logic.Len() * sizeof(TCHAR)), ElapsedMs));
    Entries[0]->CustomFunctionName = TEXT("ChangedEntry");
    Entries[0]->NodePosY = 2000;
    Entries[0]->FindPinChecked(UEdGraphSchema_K2::PN_Then)->BreakAllPinLinks();
    const TMap<FString, FString> ChangedPack = BuildReadPackForGraph(XBlueprintGraphExporterTests::BuildGraphJson(Graph).ToSharedRef());
    const FString& Changed = ChangedPack.FindChecked(TEXT("10_Logic/G0001.pseudo.md"));
    TestTrue(TEXT("Next export observes title mutation"), Changed.Contains(TEXT("ChangedEntry")));
    TestTrue(TEXT("Next export observes changed layout and links"), Changed != Logic);
    TestTrue(TEXT("Empty graph export"), XBlueprintGraphExporterTests::BuildGraphJson(
        NewObject<UEdGraph>())->GetArrayField(TEXT("nodes")).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXBlueprintGraphExporterPinIndicesTest,
    "XTools.AssetEditor.BlueprintGraphExporter.PinIndices",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXBlueprintGraphExporterPinIndicesTest::RunTest(const FString& Parameters)
{
    UBlueprint* Blueprint = NewObject<UBlueprint>();
    Blueprint->ParentClass = AActor::StaticClass();
    UEdGraph* Graph = NewObject<UEdGraph>(Blueprint);
    Blueprint->UbergraphPages.Add(Graph);
    Graph->Schema = UEdGraphSchema_K2::StaticClass();
    UEdGraphNode* Source = NewObject<UK2Node_ExecutionSequence>(Graph);
    UEdGraphNode* Target = NewObject<UK2Node_ExecutionSequence>(Graph);
    Graph->AddNode(Source);
    Graph->AddNode(Target);
    UEdGraphPin* Output = Source->CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_String, TEXT("Value"));
    for (int32 Index = 0; Index < 256; ++Index)
    {
        UEdGraphPin* Input = Target->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, TEXT("SameName"));
        TestTrue(TEXT("Wide node connection"), Graph->GetSchema()->TryCreateConnection(Output, Input));
    }
    // Valid owning node deliberately absent from Graph->Nodes: retain its pin reference.
    UEdGraphNode* External = NewObject<UK2Node_ExecutionSequence>(Graph);
    External->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, TEXT("Unused"));
    UEdGraphPin* ExternalInput = External->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, TEXT("Linked"));
    TestTrue(TEXT("Non-enumerated owner connection"), Graph->GetSchema()->TryCreateConnection(Output, ExternalInput));
    auto Verify = [this, Graph, Source, Target, External, Output, ExternalInput]()
    {
        const auto Json = XBlueprintGraphExporterTests::BuildGraphJson(Graph);
        TestEqual(TEXT("All links retained"), Json->GetArrayField(TEXT("edges")).Num(), 257);
        for (const auto& Value : Json->GetArrayField(TEXT("nodes")))
        {
            const auto Node = Value->AsObject();
            for (const auto& PinValue : Node->GetArrayField(TEXT("pins")))
            {
                const auto Pin = PinValue->AsObject();
                if (Pin->GetStringField(TEXT("name")) != TEXT("Value")) { continue; }
                TestEqual(TEXT("Source index matches native array lookup"), static_cast<int32>(Pin->GetNumberField(TEXT("index"))), Source->Pins.IndexOfByKey(Output));
                const auto& Links = Pin->GetArrayField(TEXT("linked_to"));
                if (!TestEqual(TEXT("All linked pin references retained"), Links.Num(), 257)) { return; }
                for (int32 Index = 0; Index < Links.Num(); ++Index)
                {
                    const UEdGraphPin* Linked = Output->LinkedTo[Index];
                    TestEqual(TEXT("Linked index matches native array lookup"), static_cast<int32>(Links[Index]->AsObject()->GetNumberField(TEXT("pin_index"))), Linked->GetOwningNode()->Pins.IndexOfByKey(Linked));
                }
                TestEqual(TEXT("Non-enumerated node keeps its actual pin index"), static_cast<int32>(Links.Last()->AsObject()->GetNumberField(TEXT("pin_index"))), External->Pins.IndexOfByKey(ExternalInput));
            }
        }
    };
    Verify();
    Target->Pins.Swap(0, 255);
    External->Pins.Swap(0, 1);
    Verify(); // A new serialization must rebuild indices, even for the same UObject pointers.
    Output->BreakAllPinLinks();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXBlueprintGraphExporterAIFidelityTest,
    "XTools.AssetEditor.BlueprintGraphExporter.AIFidelity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXBlueprintGraphExporterAIFidelityTest::RunTest(const FString& Parameters)
{
    UBlueprint* Blueprint = NewObject<UBlueprint>();
    Blueprint->ParentClass = AActor::StaticClass();
    UEdGraph* Graph = NewObject<UEdGraph>(Blueprint);
    Graph->Schema = UEdGraphSchema_K2::StaticClass();
    Blueprint->UbergraphPages.Add(Graph);
    UEdGraphNode* Source = NewObject<UK2Node_ExecutionSequence>(Graph);
    UEdGraphNode* Target = NewObject<UK2Node_ExecutionSequence>(Graph);
    Graph->AddNode(Source);
    Graph->AddNode(Target);
    Graph->AddNode(NewObject<UEdGraphNode>(Graph));
    Source->NodeGuid = FGuid(3, 0, 0, 1);
    Target->NodeGuid = FGuid(3, 0, 0, 2);
    Source->NodeComment = TEXT("作者注释\n```\n含引号\"和中文");
    Source->SetEnabledState(ENodeEnabledState::Disabled, true);
    UEdGraphPin* Output = Source->CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_String, TEXT("Value"));
    for (int32 Index = 0; Index < 105; ++Index)
    {
        UEdGraphPin* Input = Target->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String,
            *FString::Printf(TEXT("Input%d"), Index));
        TestTrue(TEXT("Data fanout connection"), Graph->GetSchema()->TryCreateConnection(Output, Input));
    }
    UEdGraphPin* Parent = Target->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, TEXT("Parent"));
    UEdGraphPin* Child = Target->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, TEXT("Parent_Field"));
    Parent->SubPins.Add(Child);
    Child->ParentPin = Parent;
    Child->bOrphanedPin = true;
    Child->PinType.bIsReference = true;
    Child->PinType.bIsConst = true;
    Child->PinType.PinSubCategoryMemberReference.MemberName = TEXT("DelegateSignature");
    Child->DefaultValue = TEXT("");
    UEdGraphPin* Map = Target->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Name, TEXT("Map"));
    Map->PinType.ContainerType = EPinContainerType::Map;
    Map->PinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Object;
    Map->PinType.PinValueType.TerminalSubCategoryObject = AActor::StaticClass();
    Map->PinType.PinValueType.bTerminalIsConst = true;
    Map->PinType.PinValueType.bTerminalIsWeakPointer = true;
    int32 PinIndex = 1;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            Pin->PinId = FGuid(4, 0, 0, PinIndex++);
        }
    }
    // 异常资产的 GUID 不可依赖；快照索引仍须保留精确父子关系。
    Parent->PinId.Invalidate();
    Child->PinId.Invalidate();
    const TSharedPtr<FJsonObject> Snapshot = XBlueprintGraphExporterTests::BuildBlueprintJson(Blueprint);
    const TSharedPtr<FJsonObject> GraphJson = Snapshot->GetArrayField(TEXT("graphs"))[0]->AsObject();
    TestEqual(TEXT("All data fanout edges"), GraphJson->GetArrayField(TEXT("edges")).Num(), 105);
    TestEqual(TEXT("Unknown nodes explicitly listed"), GraphJson->GetArrayField(TEXT("unclassified_node_ids")).Num(), 1);
    TSharedPtr<FJsonObject> ChildJson;
    TSharedPtr<FJsonObject> MapJson;
    for (const TSharedPtr<FJsonValue>& NodeValue : GraphJson->GetArrayField(TEXT("nodes")))
    {
        for (const TSharedPtr<FJsonValue>& PinValue : NodeValue->AsObject()->GetArrayField(TEXT("pins")))
        {
            if (PinValue->AsObject()->GetStringField(TEXT("name")) == Child->PinName.ToString()) ChildJson = PinValue->AsObject();
            if (PinValue->AsObject()->GetStringField(TEXT("name")) == Map->PinName.ToString()) MapJson = PinValue->AsObject();
        }
    }
    if (!TestNotNull(TEXT("Child pin exported"), ChildJson.Get()) || !TestNotNull(TEXT("Map pin exported"), MapJson.Get())) return false;
    TestEqual(TEXT("Split parent identity"), ChildJson->GetStringField(TEXT("parent_pin_id")), Parent->PinId.ToString());
    TestEqual(TEXT("Split parent remains unambiguous with invalid GUIDs"),
        static_cast<int32>(ChildJson->GetNumberField(TEXT("parent_pin_index"))), Target->Pins.IndexOfByKey(Parent));
    TestTrue(TEXT("Orphan retained"), ChildJson->GetBoolField(TEXT("orphaned")));
    TestTrue(TEXT("Reference retained"), ChildJson->GetObjectField(TEXT("type"))->GetBoolField(TEXT("is_reference")));
    TestTrue(TEXT("Const retained"), ChildJson->GetObjectField(TEXT("type"))->GetBoolField(TEXT("is_const")));
    TestTrue(TEXT("Empty default exists"), ChildJson->HasField(TEXT("default")));
    TestEqual(TEXT("Empty default value"), ChildJson->GetStringField(TEXT("default")), FString());
    TestTrue(TEXT("Map value qualifier retained"), MapJson->GetObjectField(TEXT("type"))->GetObjectField(TEXT("value_type_details"))->GetBoolField(TEXT("is_weak_pointer")));

    FString Before;
    FJsonSerializer::Serialize(Snapshot.ToSharedRef(), TJsonWriterFactory<>::Create(&Before));
    const TMap<FString, FString> ReadPack = XBlueprintReadPack::Build(Snapshot.ToSharedRef(), FString());
    FString After;
    FJsonSerializer::Serialize(Snapshot.ToSharedRef(), TJsonWriterFactory<>::Create(&After));
    TestEqual(TEXT("Writer does not mutate snapshot"), After, Before);
    const TMap<FString, FString> Repeated = XBlueprintReadPack::Build(Snapshot.ToSharedRef(), FString());
    const FString& Evidence = ReadPack.FindChecked(TEXT("20_Evidence/G0001.json"));
    const FString& Logic = ReadPack.FindChecked(TEXT("10_Logic/G0001.pseudo.md"));
    TestEqual(TEXT("Repeated evidence stable"), Repeated.FindChecked(TEXT("20_Evidence/G0001.json")), Evidence);
    TestEqual(TEXT("Repeated logic stable"), Repeated.FindChecked(TEXT("10_Logic/G0001.pseudo.md")), Logic);
    TSharedPtr<FJsonObject> EvidenceGraph;
    if (!TestTrue(TEXT("Evidence parseable with escaped comments"),
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Evidence), EvidenceGraph))) return false;
    for (const TSharedPtr<FJsonValue>& Edge : EvidenceGraph->GetArrayField(TEXT("edges")))
    {
        TestEqual(TEXT("Fanout source pin identity"), Edge->AsObject()->GetStringField(TEXT("from_pin_id")), Output->PinId.ToString());
    }
    for (const TSharedPtr<FJsonValue>& Node : EvidenceGraph->GetArrayField(TEXT("nodes")))
    {
        const TSharedPtr<FJsonObject> Record = Node->AsObject();
        if (Record->GetStringField(TEXT("node_guid")) == Source->NodeGuid.ToString())
        {
            TestEqual(TEXT("Author comment preserved"), Record->GetStringField(TEXT("comment")), Source->NodeComment);
            TestFalse(TEXT("Disabled state preserved"), Record->GetBoolField(TEXT("is_enabled")));
        }
        TestTrue(TEXT("Logic contains every classified or unknown node"), Logic.Contains(TEXT("@") + Record->GetStringField(TEXT("id")) + TEXT(":")));
    }
    TestEqual(TEXT("ReadPack does not truncate at 100 edges"), EvidenceGraph->GetArrayField(TEXT("edges")).Num(), 105);
    TestEqual(TEXT("ReadPack exports classified and unknown nodes"), EvidenceGraph->GetArrayField(TEXT("nodes")).Num(), 3);
    TestEqual(TEXT("Original graph links unchanged"), Output->LinkedTo.Num(), 105);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXBlueprintGraphExporterAIControlFlowTest,
    "XTools.AssetEditor.BlueprintGraphExporter.AIControlFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXBlueprintGraphExporterAIControlFlowTest::RunTest(const FString& Parameters)
{
    UBlueprint* Blueprint = NewObject<UBlueprint>();
    Blueprint->ParentClass = AActor::StaticClass();
    UEdGraph* Graph = NewObject<UEdGraph>(Blueprint);
    Graph->Schema = UEdGraphSchema_K2::StaticClass();
    Blueprint->UbergraphPages.Add(Graph);
    UK2Node_ExecutionSequence* First = NewObject<UK2Node_ExecutionSequence>(Graph);
    UK2Node_ExecutionSequence* Second = NewObject<UK2Node_ExecutionSequence>(Graph);
    Graph->AddNode(First);
    Graph->AddNode(Second);
    First->AllocateDefaultPins();
    Second->AllocateDefaultPins();
    TestTrue(TEXT("First exit to shared body"), Graph->GetSchema()->TryCreateConnection(First->GetThenPinGivenIndex(0), Second->GetExecPin()));
    TestTrue(TEXT("Second exit to same shared body"), Graph->GetSchema()->TryCreateConnection(First->GetThenPinGivenIndex(1), Second->GetExecPin()));
    TestTrue(TEXT("Execution back edge"), Graph->GetSchema()->TryCreateConnection(Second->GetThenPinGivenIndex(0), First->GetExecPin()));
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            Pin->PinId.Invalidate();
        }
    }
    const TSharedPtr<FJsonObject> Snapshot = XBlueprintGraphExporterTests::BuildBlueprintJson(Blueprint);
    const TMap<FString, FString> ReadPack = XBlueprintReadPack::Build(Snapshot.ToSharedRef(), FString());
    const FString& Evidence = ReadPack.FindChecked(TEXT("20_Evidence/G0001.json"));
    const FString& Logic = ReadPack.FindChecked(TEXT("10_Logic/G0001.pseudo.md"));
    TSharedPtr<FJsonObject> EvidenceGraph;
    if (!TestTrue(TEXT("Control flow evidence parses"),
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Evidence), EvidenceGraph))) return false;
    TSet<FString> SourcePins;
    for (const TSharedPtr<FJsonValue>& Value : EvidenceGraph->GetArrayField(TEXT("edges")))
    {
        const TSharedPtr<FJsonObject> Edge = Value->AsObject();
        SourcePins.Add(Edge->GetObjectField(TEXT("from_node"))->GetStringField(TEXT("node_id")) + TEXT(":")
            + FString::FromInt(static_cast<int32>(Edge->GetNumberField(TEXT("from_pin_index")))));
    }
    int32 NodeCount = 0;
    TArray<FString> Lines;
    Logic.ParseIntoArrayLines(Lines);
    for (const FString& Line : Lines)
    {
        if (Line.StartsWith(TEXT("@")))
        {
            ++NodeCount;
        }
    }
    TestEqual(TEXT("Both shared exits and cycle retained without entry roots"), EvidenceGraph->GetArrayField(TEXT("edges")).Num(), 3);
    TestEqual(TEXT("Distinct exec pin identities even with invalid GUIDs"), SourcePins.Num(), 3);
    TestTrue(TEXT("Reflected node state included"), Evidence.Contains(TEXT("reflected_properties")));
    TestEqual(TEXT("Logic declares shared and cyclic nodes only once"), NodeCount, 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXBlueprintGraphExporterAIDependenciesTest,
    "XTools.AssetEditor.BlueprintGraphExporter.AIDependencies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXBlueprintGraphExporterAIDependenciesTest::RunTest(const FString& Parameters)
{
    UBlueprint* Blueprint = NewObject<UBlueprint>();
    Blueprint->ParentClass = AActor::StaticClass();
    UBlueprint* ExternalBlueprint = NewObject<UBlueprint>();
    ExternalBlueprint->ParentClass = AActor::StaticClass();
    UEdGraph* Graph = NewObject<UEdGraph>(Blueprint);
    UEdGraph* LocalMacro = NewObject<UEdGraph>(Blueprint);
    UEdGraph* ExternalMacro = NewObject<UEdGraph>(ExternalBlueprint);
    Graph->Schema = UEdGraphSchema_K2::StaticClass();
    LocalMacro->Schema = UEdGraphSchema_K2::StaticClass();
    ExternalMacro->Schema = UEdGraphSchema_K2::StaticClass();
    Blueprint->UbergraphPages.Add(Graph);
    Blueprint->MacroGraphs.Add(LocalMacro);
    ExternalBlueprint->MacroGraphs.Add(ExternalMacro);
    for (UEdGraph* Definition : {LocalMacro, ExternalMacro})
    {
        UK2Node_MacroInstance* Macro = NewObject<UK2Node_MacroInstance>(Graph);
        Graph->AddNode(Macro);
        Macro->SetMacroGraph(Definition);
    }
    UK2Node_CustomEvent* Event = NewObject<UK2Node_CustomEvent>(Graph);
    Event->CustomFunctionName = TEXT("ReflectedEventName");
    Graph->AddNode(Event);
    Event->AllocateDefaultPins();
    const TSharedPtr<FJsonObject> Snapshot = XBlueprintGraphExporterTests::BuildBlueprintJson(Blueprint);
    const TSharedPtr<FJsonObject> Coverage = Snapshot->GetObjectField(TEXT("coverage"));
    TestEqual(TEXT("External macro explicitly identified"), Coverage->GetArrayField(TEXT("external_macro_graphs")).Num(), 1);
    if (Coverage->GetArrayField(TEXT("external_macro_graphs")).Num() == 1)
    {
        TestEqual(TEXT("External graph path"), Coverage->GetArrayField(TEXT("external_macro_graphs"))[0]->AsString(), ExternalMacro->GetPathName());
    }
    int32 IncludedCount = 0;
    int32 ExternalCount = 0;
    bool bFoundReflectedName = false;
    for (const TSharedPtr<FJsonValue>& GraphValue : Snapshot->GetArrayField(TEXT("graphs")))
    {
        for (const TSharedPtr<FJsonValue>& NodeValue : GraphValue->AsObject()->GetArrayField(TEXT("nodes")))
        {
            const TSharedPtr<FJsonObject> Node = NodeValue->AsObject();
            const TSharedPtr<FJsonObject>* Semantic = nullptr;
            FString Status;
            if (Node->TryGetObjectField(TEXT("semantic"), Semantic) && (*Semantic)->TryGetStringField(TEXT("definition_status"), Status))
            {
                IncludedCount += Status == TEXT("included") ? 1 : 0;
                ExternalCount += Status == TEXT("external_not_included") ? 1 : 0;
            }
            for (const TSharedPtr<FJsonValue>& PropertyValue : Node->GetArrayField(TEXT("reflected_properties")))
            {
                const TSharedPtr<FJsonObject> Property = PropertyValue->AsObject();
                if (Property->GetStringField(TEXT("name")) == TEXT("CustomFunctionName"))
                {
                    bFoundReflectedName = Property->GetArrayField(TEXT("values"))[0]->AsString().Contains(TEXT("ReflectedEventName"));
                }
            }
        }
    }
    TestEqual(TEXT("Local definition included"), IncludedCount, 1);
    TestEqual(TEXT("External definition not fabricated"), ExternalCount, 1);
    TestTrue(TEXT("Generic reflection preserves subclass property"), bFoundReflectedName);
    TestFalse(TEXT("Classification is not exhaustive semantic analysis"), Coverage->GetBoolField(TEXT("semantic_analysis_exhaustive")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXBlueprintGraphExporterAIFileExportTest,
    "XTools.AssetEditor.BlueprintGraphExporter.AIFileExport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXBlueprintGraphExporterAIFileExportTest::RunTest(const FString& Parameters)
{
    const FString PackageName = TEXT("/Temp/XToolsAIExport_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UBlueprint* Blueprint = NewObject<UBlueprint>(CreatePackage(*PackageName), TEXT("Fixture"));
    Blueprint->ParentClass = AActor::StaticClass();
    FString Directory;
    FString Error;
    const bool bExported = XBlueprintGraphExporterTests::ExportBlueprintFiles(Blueprint, Directory, Error);
    TestTrue(*Error, bExported);
    if (!bExported)
    {
        return false;
    }
    const FString JsonPath = Directory / TEXT("90_Full/Fixture.json");
    const FString AIPath = Directory / TEXT("90_Full/Fixture.ai.md");
    const FString MarkdownPath = Directory / TEXT("90_Full/Fixture.md");
    const FString StartPath = Directory / TEXT("00_START_HERE.md");
    const FString AssetEvidencePath = Directory / TEXT("20_Evidence/00_Asset.json");
    TestTrue(TEXT("Output directory is under Saved/XTools/BlueprintExports"), Directory.StartsWith(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("XTools/BlueprintExports"))));
    const FString ExpectedDirectoryPrefix = TEXT("Fixture_");
    const FString DirectoryName = FPaths::GetCleanFilename(Directory);
    TestTrue(TEXT("Output directory uses asset name and stable digest"), DirectoryName.StartsWith(ExpectedDirectoryPrefix));
    TestFalse(TEXT("Output directory does not flatten package path"), DirectoryName.Contains(TEXT("_Temp_")));
    FString OriginalJson;
    TestTrue(TEXT("JSON file readable"), FFileHelper::LoadFileToString(OriginalJson, *JsonPath));
    TestFalse(TEXT("Full JSON uses LF independently of engine LINE_TERMINATOR"), OriginalJson.Contains(TEXT("\r")));
    TestFalse(TEXT("New export has no legacy AI Markdown"), IFileManager::Get().FileExists(*AIPath));
    TestFalse(TEXT("New export has no legacy human Markdown"), IFileManager::Get().FileExists(*MarkdownPath));
    FString StartContent;
    FString AssetEvidence;
    TestTrue(TEXT("Start file readable"), FFileHelper::LoadFileToString(StartContent, *StartPath));
    TestTrue(TEXT("Asset evidence readable"), FFileHelper::LoadFileToString(AssetEvidence, *AssetEvidencePath));
    // Simulate an earlier bundle. Retire only its known generated filenames, never user notes.
    const FString UserNotesPath = Directory / TEXT("90_Full/UserNotes.md");
    TestTrue(TEXT("Seed legacy AI Markdown"), FFileHelper::SaveStringToFile(TEXT("legacy AI"), *AIPath));
    TestTrue(TEXT("Seed legacy human Markdown"), FFileHelper::SaveStringToFile(TEXT("legacy Markdown"), *MarkdownPath));
    TestTrue(TEXT("Seed unrelated user notes"), FFileHelper::SaveStringToFile(TEXT("user notes"), *UserNotesPath));
    Blueprint->ParentClass = UObject::StaticClass();
    IFileManager& Files = IFileManager::Get();
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    const bool bProtected = PlatformFile.SetReadOnly(*StartPath, true);
    TestTrue(TEXT("Protect existing start output for failure test"), bProtected);
    if (bProtected)
    {
        TestFalse(TEXT("Unwritable bundle must fail"), XBlueprintGraphExporterTests::ExportBlueprintFiles(Blueprint, Directory, Error));
        FString CurrentJson;
        FString CurrentEvidence;
        FString CurrentLegacy;
        FString CurrentStart;
        FFileHelper::LoadFileToString(CurrentJson, *JsonPath);
        FFileHelper::LoadFileToString(CurrentEvidence, *AssetEvidencePath);
        FFileHelper::LoadFileToString(CurrentLegacy, *AIPath);
        FFileHelper::LoadFileToString(CurrentStart, *StartPath);
        TestEqual(TEXT("Failed export preserves old JSON"), CurrentJson, OriginalJson);
        TestEqual(TEXT("Failed export preserves old evidence"), CurrentEvidence, AssetEvidence);
        TestEqual(TEXT("Failed export preserves legacy views"), CurrentLegacy, FString(TEXT("legacy AI")));
        TestEqual(TEXT("Failed export preserves old entry"), CurrentStart, StartContent);
        TestTrue(TEXT("Restore output permissions"), PlatformFile.SetReadOnly(*StartPath, false));
    }
    const bool bLegacyProtected = PlatformFile.SetReadOnly(*AIPath, true);
    TestTrue(TEXT("Protect retired output"), bLegacyProtected);
    if (bLegacyProtected)
    {
        TestFalse(TEXT("Unwritable retired view must fail before replacing bundle"), XBlueprintGraphExporterTests::ExportBlueprintFiles(Blueprint, Directory, Error));
        FString CurrentJson;
        FFileHelper::LoadFileToString(CurrentJson, *JsonPath);
        TestEqual(TEXT("Retirement failure preserves old snapshot"), CurrentJson, OriginalJson);
        TestTrue(TEXT("Restore retired output permissions"), PlatformFile.SetReadOnly(*AIPath, false));
    }
    TestTrue(TEXT("Bundle can be replaced after failure"), XBlueprintGraphExporterTests::ExportBlueprintFiles(Blueprint, Directory, Error));
    FString UpdatedEvidence;
    FFileHelper::LoadFileToString(UpdatedEvidence, *AssetEvidencePath);
    TestTrue(TEXT("New evidence observes source change"), UpdatedEvidence != AssetEvidence);
    TestFalse(TEXT("Successful replacement retires old AI view"), Files.FileExists(*AIPath));
    TestFalse(TEXT("Successful replacement retires old human view"), Files.FileExists(*MarkdownPath));
    FString UserNotes;
    TestTrue(TEXT("Unrelated notes remain readable"), FFileHelper::LoadFileToString(UserNotes, *UserNotesPath));
    TestEqual(TEXT("Unrelated notes remain unchanged"), UserNotes, FString(TEXT("user notes")));
    TArray<FString> ExportedFiles;
    Files.FindFilesRecursive(ExportedFiles, *Directory, TEXT("*"), true, false);
    TestEqual(TEXT("Fixture has six generated files plus unrelated notes"), ExportedFiles.Num(), 7);
    const FString RootDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("XTools/BlueprintExports"));
    const FString RootPrefix = RootDirectory.EndsWith(TEXT("/")) || RootDirectory.EndsWith(TEXT("\\")) ? RootDirectory : RootDirectory + TEXT("/");
    TestTrue(TEXT("Remove test-owned export directory"), Directory.StartsWith(RootPrefix)
        && FPaths::GetCleanFilename(Directory) == DirectoryName && Files.DeleteDirectory(*Directory, true, true));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXBlueprintGraphExporterAssetIdentityTest,
    "XTools.AssetEditor.BlueprintGraphExporter.AssetIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXBlueprintGraphExporterAssetIdentityTest::RunTest(const FString& Parameters)
{
    IFileManager& Files = IFileManager::Get();
    const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Prefix = TEXT("/Temp/XToolsIdentity_") + Unique;
    UBlueprint* A = NewObject<UBlueprint>(CreatePackage(*(Prefix + TEXT("/A_B/C/D"))), TEXT("D"));
    UBlueprint* B = NewObject<UBlueprint>(CreatePackage(*(Prefix + TEXT("/A/B_C/D"))), TEXT("D"));
    A->ParentClass = B->ParentClass = AActor::StaticClass();
    FString ADir, BDir, Error;
    TestTrue(TEXT("Export collision fixture A"), XBlueprintGraphExporterTests::ExportBlueprintFiles(A, ADir, Error));
    TestTrue(TEXT("Export collision fixture B"), XBlueprintGraphExporterTests::ExportBlueprintFiles(B, BDir, Error));
    TestNotEqual(TEXT("Flattened package paths no longer collide"), ADir, BDir);
    auto Read = [](const FString& Path) { FString Text; FFileHelper::LoadFileToString(Text, *Path); return Text; };
    const FString BJson = Read(BDir / TEXT("90_Full/D.json"));
    const FString BEntry = Read(BDir / TEXT("00_START_HERE.md"));
    A->ParentClass = UObject::StaticClass();
    TestTrue(TEXT("Re-export A"), XBlueprintGraphExporterTests::ExportBlueprintFiles(A, ADir, Error));
    TestEqual(TEXT("A does not modify B full snapshot"), Read(BDir / TEXT("90_Full/D.json")), BJson);
    TestEqual(TEXT("A does not modify B entry"), Read(BDir / TEXT("00_START_HERE.md")), BEntry);
    const FString AEvidencePath = ADir / TEXT("20_Evidence/00_Asset.json");
    const FString AOriginalEvidence = Read(AEvidencePath);
    const FString AOriginalJson = Read(ADir / TEXT("90_Full/D.json"));
    TestTrue(TEXT("Write mismatched evidence fixture"), FFileHelper::SaveStringToFile(BJson, *AEvidencePath));
    TestFalse(TEXT("Different asset identity cannot overwrite"), XBlueprintGraphExporterTests::ExportBlueprintFiles(A, ADir, Error));
    TestEqual(TEXT("Rejected identity preserves full snapshot"), Read(ADir / TEXT("90_Full/D.json")), AOriginalJson);
    TestTrue(TEXT("Restore correct identity"), FFileHelper::SaveStringToFile(AOriginalEvidence, *AEvidencePath));
    TestTrue(TEXT("Correct identity can be exported again"), XBlueprintGraphExporterTests::ExportBlueprintFiles(A, ADir, Error));

    // Only this isolated index fixture is read/written; never replace the user's shared root index.
    const FString IndexRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("XTools/BlueprintIndexTest_") + Unique);
    auto Seed = [&](const FString& Name, const FString& Evidence)
    {
        const FString Dir = IndexRoot / Name;
        Files.MakeDirectory(*(Dir / TEXT("20_Evidence")), true);
        FFileHelper::SaveStringToFile(Evidence, *(Dir / TEXT("20_Evidence/00_Asset.json")));
        FFileHelper::SaveStringToFile(TEXT("fixture"), *(Dir / TEXT("00_START_HERE.md")));
    };
    const FString AName = FPaths::GetCleanFilename(ADir);
    const FString BName = FPaths::GetCleanFilename(BDir);
    Seed(TEXT("000_old_A"), AOriginalEvidence);
    Seed(AName, AOriginalEvidence);
    Seed(BName, Read(BDir / TEXT("20_Evidence/00_Asset.json")));
    const FString IndexAfterB = XBlueprintGraphExporterTests::BuildRootEntry(IndexRoot, B->GetPathName());
    const FString IndexAfterA = XBlueprintGraphExporterTests::BuildRootEntry(IndexRoot, A->GetPathName());
    TestEqual(TEXT("Root index independent of latest exported asset"), IndexAfterA, IndexAfterB);
    TestTrue(TEXT("Root routes to per-asset contracts and permits wider queries"), IndexAfterB.Contains(TEXT("相同契约摘要")) && IndexAfterB.Contains(TEXT("可扩大范围")));
    TestFalse(TEXT("Old asset duplicate excluded"), IndexAfterB.Contains(TEXT("000_old_A")));
    TestTrue(TEXT("Both current asset entries included"), IndexAfterB.Contains(AName) && IndexAfterB.Contains(BName));
    TArray<FString> Lines;
    IndexAfterB.ParseIntoArrayLines(Lines);
    int32 Links = 0;
    for (const FString& Line : Lines) { Links += Line.StartsWith(TEXT("- [")) ? 1 : 0; }
    TestEqual(TEXT("Exactly one root entry per asset"), Links, 2);

    auto Parse = [](const FString& Text)
    {
        TSharedPtr<FJsonObject> Result;
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Result);
        return Result;
    };
    const auto AManifest = Parse(Read(ADir / TEXT("01_Manifest.json")));
    const auto BManifest = Parse(Read(BDir / TEXT("01_Manifest.json")));
    TestTrue(TEXT("Seed older disk manifest A"), FFileHelper::SaveStringToFile(Read(ADir / TEXT("01_Manifest.json")), *(IndexRoot / AName / TEXT("01_Manifest.json"))));
    TestTrue(TEXT("Seed other asset disk manifest B"), FFileHelper::SaveStringToFile(Read(BDir / TEXT("01_Manifest.json")), *(IndexRoot / BName / TEXT("01_Manifest.json"))));
    AManifest->SetStringField(TEXT("snapshot_id"), TEXT("current-in-memory-snapshot"));
    const auto FirstIndex = Parse(XBlueprintGraphExporterTests::BuildRootIndex(IndexRoot / TEXT("not_created"), AManifest));
    TestEqual(TEXT("First export indexes in-memory package before any disk files exist"), FirstIndex->GetArrayField(TEXT("packages")).Num(), 1);
    const FString IndexText = XBlueprintGraphExporterTests::BuildRootIndex(IndexRoot, AManifest);
    TestFalse(TEXT("Root JSON index uses LF independently of engine LINE_TERMINATOR"), IndexText.Contains(TEXT("\r")));
    const auto Index = Parse(IndexText);
    TestEqual(TEXT("Machine index version"), Index->GetIntegerField(TEXT("format_version")), 1);
    TestEqual(TEXT("Machine index scope is explicitly local"), Index->GetStringField(TEXT("scope")), FString(TEXT("direct_child_export_packages")));
    TestEqual(TEXT("Machine index deduplicates old copies"), Index->GetArrayField(TEXT("packages")).Num(), 2);
    for (const auto& Value : Index->GetArrayField(TEXT("packages")))
    {
        const auto Package = Value->AsObject();
        const bool bA = Package->GetStringField(TEXT("asset_path")) == A->GetPathName();
        TestEqual(TEXT("Relative package directory"), Package->GetStringField(TEXT("directory")), bA ? AName : BName);
        TestEqual(TEXT("Current package uses staged manifest, sibling uses disk manifest"), Package->GetStringField(TEXT("snapshot_id")),
            bA ? FString(TEXT("current-in-memory-snapshot")) : BManifest->GetStringField(TEXT("snapshot_id")));
    }
    // An unusable selected package is disclosed, never silently replaced with an old duplicate.
    TestTrue(TEXT("Remove sibling manifest for legacy fixture"), Files.Delete(*(IndexRoot / BName / TEXT("01_Manifest.json"))));
    const auto LegacyIndex = Parse(XBlueprintGraphExporterTests::BuildRootIndex(IndexRoot, AManifest));
    TestEqual(TEXT("Legacy package omitted from machine targets"), LegacyIndex->GetArrayField(TEXT("packages")).Num(), 1);
    TestEqual(TEXT("Legacy omission is explicit"), LegacyIndex->GetArrayField(TEXT("unindexed_directories"))[0]->AsString(), BName);

    const FString ExportsRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("XTools/BlueprintExports"));
    for (const FString& Dir : {ADir, BDir})
    {
        TestTrue(TEXT("Remove uniquely owned asset fixture"), FPaths::GetPath(Dir) == ExportsRoot
            && Read(Dir / TEXT("20_Evidence/00_Asset.json")).Contains(Unique) && Files.DeleteDirectory(*Dir, true, true));
    }
    const FString SafeIndexRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("XTools"));
    TestTrue(TEXT("Remove uniquely owned index fixture"), FPaths::GetPath(IndexRoot) == SafeIndexRoot
        && FPaths::GetCleanFilename(IndexRoot) == TEXT("BlueprintIndexTest_") + Unique && Files.DeleteDirectory(*IndexRoot, true, true));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXBlueprintGraphExporterAIPromptTest,
    "XTools.AssetEditor.BlueprintGraphExporter.AIPrompt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXBlueprintGraphExporterAIPromptTest::RunTest(const FString& Parameters)
{
    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("提示词 测试"));
    const FString A = Root / TEXT("同名_A");
    const FString B = Root / TEXT("同名_B");
    const FString Single = XBlueprintGraphExporterTests::BuildAIPrompt(Root, {A});
    TestTrue(TEXT("Single result links exact absolute entry, including spaces and Unicode"), Single.Contains(TEXT("\"") + A / TEXT("00_START_HERE.md") + TEXT("\"")));
    TestFalse(TEXT("Single result does not route through historical root assets"), Single.Contains(Root / TEXT("00_START_HERE.md")));
    const FString Batch = XBlueprintGraphExporterTests::BuildAIPrompt(Root, {A, B});
    TestTrue(TEXT("Multiple successes link the root and identify this batch"), Batch.Contains(Root / TEXT("00_START_HERE.md")) && Batch.Contains(TEXT("同名_A")) && Batch.Contains(TEXT("同名_B")));
    TestTrue(TEXT("No successful exports cannot copy stale output"), XBlueprintGraphExporterTests::BuildAIPrompt(Root, {}).IsEmpty());
    TestTrue(TEXT("Reading scope remains adjustable"), Single.Contains(TEXT("必要时扩大范围")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXBlueprintGraphExporterLocalDefaultsTest,
    "XTools.AssetEditor.BlueprintGraphExporter.LocalDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXBlueprintGraphExporterLocalDefaultsTest::RunTest(const FString& Parameters)
{
    UBlueprint* Blueprint = NewObject<UBlueprint>(GetTransientPackage());
    Blueprint->ParentClass = UObject::StaticClass();
    UEdGraph* Graph = NewObject<UEdGraph>(Blueprint);
    UK2Node_FunctionEntry* Entry = NewObject<UK2Node_FunctionEntry>(Graph);
    auto AddLocal = [&](const TCHAR* Name, FName Category, const TCHAR* Default, EPinContainerType Container = EPinContainerType::None)
    {
        FBPVariableDescription Local;
        Local.VarName = Name;
        Local.VarType.PinCategory = Category;
        Local.VarType.ContainerType = Container;
        Local.DefaultValue = Default;
        Entry->LocalVariables.Add(Local);
    };
    AddLocal(TEXT("Accumulator"), UEdGraphSchema_K2::PC_Real, TEXT(""));
    AddLocal(TEXT("Flag"), UEdGraphSchema_K2::PC_Boolean, TEXT(""));
    AddLocal(TEXT("ExplicitZero"), UEdGraphSchema_K2::PC_Int, TEXT("0"));
    AddLocal(TEXT("ExplicitFalse"), UEdGraphSchema_K2::PC_Boolean, TEXT("false"));
    AddLocal(TEXT("Structure"), UEdGraphSchema_K2::PC_Struct, TEXT(""));
    AddLocal(TEXT("Array"), UEdGraphSchema_K2::PC_Int, TEXT(""), EPinContainerType::Array);
    const auto Semantic = XBlueprintGraphExporterTests::BuildNodeSemanticJson(Entry);
    if (!TestNotNull(TEXT("Function entry semantic exists"), Semantic.Get())) { return false; }
    const auto& Locals = Semantic->GetArrayField(TEXT("local_variables"));
    TestEqual(TEXT("All locals retained"), Locals.Num(), 6);
    TestEqual(TEXT("Raw empty accumulator default retained"), Locals[0]->AsObject()->GetStringField(TEXT("default")), FString());
    TestEqual(TEXT("Accumulator initialized by type"), Locals[0]->AsObject()->GetStringField(TEXT("default_source")), FString(TEXT("type_default")));
    TestEqual(TEXT("Scalar accumulator has effective zero"), Locals[0]->AsObject()->GetStringField(TEXT("effective_default")), FString(TEXT("0")));
    TestEqual(TEXT("Boolean has effective false"), Locals[1]->AsObject()->GetStringField(TEXT("effective_default")), FString(TEXT("false")));
    for (int32 Index : {2, 3})
    {
        TestEqual(TEXT("Explicit zero/false remain explicit"), Locals[Index]->AsObject()->GetStringField(TEXT("default_source")), FString(TEXT("explicit")));
        TestFalse(TEXT("No derived default replaces explicit value"), Locals[Index]->AsObject()->HasField(TEXT("effective_default")));
    }
    for (int32 Index : {4, 5})
    {
        TestFalse(TEXT("Complex defaults not fabricated as scalar zero"), Locals[Index]->AsObject()->HasField(TEXT("effective_default")));
    }
    TestTrue(TEXT("Source raw default not mutated"), Entry->LocalVariables[0].DefaultValue.IsEmpty());
    TestEqual(TEXT("Local declaration scope is the owning graph"), Semantic->GetStringField(TEXT("local_scope")), Graph->GetPathName());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXBlueprintGraphExporterComponentBindingsTest,
    "XTools.AssetEditor.BlueprintGraphExporter.ComponentBindings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXBlueprintGraphExporterComponentBindingsTest::RunTest(const FString& Parameters)
{
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("ComponentBindingFixture")), BPTYPE_Normal);
    USCS_Node* Component = BP->SimpleConstructionScript->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("Mesh"));
    BP->SimpleConstructionScript->AddNode(Component);
    FEdGraphPinType Type;
    Type.PinCategory = UEdGraphSchema_K2::PC_Object;
    Type.PinSubCategoryObject = UStaticMeshComponent::StaticClass();
    TestTrue(TEXT("Plain component pointer member fixture"), FBlueprintEditorUtils::AddMemberVariable(BP, TEXT("ComponentPointer"), Type));
    FKismetEditorUtilities::CompileBlueprint(BP);
    UEdGraph* Graph = NewObject<UEdGraph>(BP);
    UK2Node_VariableGet* Getter = NewObject<UK2Node_VariableGet>(Graph);
    Getter->VariableReference.SetSelfMember(TEXT("Mesh"), Component->VariableGuid);
    const auto SCS = XBlueprintGraphExporterTests::BuildNodeSemanticJson(Getter);
    TestEqual(TEXT("SCS declaration proven through property owner"), SCS->GetStringField(TEXT("component_binding")), FString(TEXT("scs_property")));
    TestEqual(TEXT("SCS identity retained"), SCS->GetStringField(TEXT("scs_node_path")), Component->GetPathName());
    Getter->VariableReference.SetSelfMember(TEXT("ComponentPointer"));
    const auto Plain = XBlueprintGraphExporterTests::BuildNodeSemanticJson(Getter);
    TestEqual(TEXT("Component pointer alone is not SCS"), Plain->GetStringField(TEXT("component_binding")), FString(TEXT("object_property")));
    TestFalse(TEXT("Plain pointer has no fabricated SCS path"), Plain->HasField(TEXT("scs_node_path")));
    Getter->VariableReference.SetExternalMember(TEXT("Mesh"), BP->GeneratedClass, Component->VariableGuid);
    TestEqual(TEXT("External receiver preserved"), XBlueprintGraphExporterTests::BuildNodeSemanticJson(Getter)->GetStringField(TEXT("binding_origin")), FString(TEXT("external_member")));
    Getter->VariableReference.SetLocalMember(TEXT("Mesh"), FString(TEXT("FunctionScope")), FGuid::NewGuid());
    const auto Local = XBlueprintGraphExporterTests::BuildNodeSemanticJson(Getter);
    TestEqual(TEXT("Local scope wins over same-named SCS member"), Local->GetStringField(TEXT("binding_origin")), FString(TEXT("local")));
    TestFalse(TEXT("Local is not relabeled as component property"), Local->HasField(TEXT("component_binding")));
    Getter->VariableReference.SetSelfMember(TEXT("DoesNotExist"));
    TestEqual(TEXT("Unresolved member stays explicit"), XBlueprintGraphExporterTests::BuildNodeSemanticJson(Getter)->GetStringField(TEXT("binding_origin")), FString(TEXT("unresolved")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXBlueprintGraphExporterStandardMacrosTest,
    "XTools.AssetEditor.BlueprintGraphExporter.StandardMacros",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXBlueprintGraphExporterStandardMacrosTest::RunTest(const FString& Parameters)
{
    UBlueprint* Standard = LoadObject<UBlueprint>(nullptr, TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
    if (!TestNotNull(TEXT("Current engine standard macro asset"), Standard)) { return false; }
    UBlueprint* BP = NewObject<UBlueprint>(GetTransientPackage());
    BP->ParentClass = UObject::StaticClass();
    UEdGraph* Graph = NewObject<UEdGraph>(BP);
    BP->FunctionGraphs.Add(Graph);
    const bool DirtyBefore = Standard->GetOutermost()->IsDirty();
    int32 References = 0;
    for (UEdGraph* Macro : Standard->MacroGraphs)
    {
        if (Macro->GetName() != TEXT("IsValid") && Macro->GetName() != TEXT("Gate")) { continue; }
        for (int32 Copy = 0; Copy < 2; ++Copy)
        {
            UK2Node_MacroInstance* Instance = NewObject<UK2Node_MacroInstance>(Graph);
            Instance->SetMacroGraph(Macro);
            Graph->Nodes.Add(Instance);
            ++References;
        }
    }
    TestEqual(TEXT("Two instances of two engine macros"), References, 4);
    const auto Snapshot = XBlueprintGraphExporterTests::BuildBlueprintJson(BP);
    const auto& Definitions = Snapshot->GetArrayField(TEXT("macro_definitions"));
    TestTrue(TEXT("Referenced definitions captured once including transitive macros"), Definitions.Num() >= 2);
    TSet<FString> Paths;
    int32 Assignments = 0;
    int32 Temporaries = 0;
    for (const auto& V : Definitions)
    {
        const auto Definition = V->AsObject();
        TestFalse(TEXT("Definition path deduplicated"), Paths.Contains(Definition->GetStringField(TEXT("path"))));
        Paths.Add(Definition->GetStringField(TEXT("path")));
        UEdGraph* Source = FindObject<UEdGraph>(nullptr, *Definition->GetStringField(TEXT("path")));
        TestNotNull(TEXT("Definition references a real engine graph"), Source);
        if (Source) { TestEqual(TEXT("Definition preserves every source node"), Definition->GetArrayField(TEXT("nodes")).Num(), Source->Nodes.Num()); }
        if (Source)
        {
            for (UEdGraphNode* SourceNode : Source->Nodes)
            {
                if (const auto* Temporary = Cast<UK2Node_TemporaryVariable>(SourceNode))
                {
                    const auto Semantic = XBlueprintGraphExporterTests::BuildNodeSemanticJson(Temporary);
                    TestEqual(TEXT("Macro temporary storage classified"), Semantic->GetStringField(TEXT("kind")), FString(TEXT("temporary_variable")));
                    TestEqual(TEXT("Actual persistent flag retained"), Semantic->GetBoolField(TEXT("is_persistent")), Temporary->bIsPersistent);
                    ++Temporaries;
                }
                if (const auto* Assignment = Cast<UK2Node_AssignmentStatement>(SourceNode))
                {
                    const auto Semantic = XBlueprintGraphExporterTests::BuildNodeSemanticJson(Assignment);
                    TestEqual(TEXT("Macro assignment classified"), Semantic->GetStringField(TEXT("kind")), FString(TEXT("assignment")));
                    TestEqual(TEXT("Write target pin retained"), Semantic->GetObjectField(TEXT("target_pin"))->GetStringField(TEXT("name")), Assignment->GetVariablePin()->PinName.ToString());
                    ++Assignments;
                }
            }
        }
    }
    TestTrue(TEXT("Stateful macro fixture covers assignments and temporaries"), Assignments > 0 && Temporaries > 0);
    TestEqual(TEXT("Own graph count not inflated with definitions"), Snapshot->GetArrayField(TEXT("graphs")).Num(), 1);
    TestEqual(TEXT("Separate call instances preserved"), Snapshot->GetArrayField(TEXT("graphs"))[0]->AsObject()->GetArrayField(TEXT("nodes")).Num(), 4);
    TestEqual(TEXT("Engine macro package not dirtied"), Standard->GetOutermost()->IsDirty(), DirtyBefore);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXBlueprintReadPackSpawnExposureTest,
    "XTools.AssetEditor.BlueprintGraphExporter.SpawnExposure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXBlueprintReadPackSpawnExposureTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FJsonObject> Snapshot;
    const FString Fixture = TEXT(R"JSON({
        "asset_path":"/Game/Test.Test",
        "variables":[{"name":"CoinRotate","guid":"FA33A0754A87952E34F4E0827334A86D",
            "property_flags":"5","metadata":{"ExposeOnSpawn":"true"}}],
        "graphs":[{"name":"EventGraph","path":"/Game/Test.Test:EventGraph","edges":[],"nodes":[
            {"id":"N20","is_enabled":true,"pins":[],"semantic":{"kind":"variable","access":"get",
                "binding_origin":"self_member","variable":{"name":"CoinRotate",
                    "guid":"FA33A0754A87952E34F4E0827334A86D","is_self_context":true,"is_local_scope":false}}}
        ]}]
    })JSON");
    if (!TestTrue(TEXT("ReadPack fixture parses"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Fixture), Snapshot))) { return false; }
    const auto Graph = Snapshot->GetArrayField(TEXT("graphs"))[0]->AsObject();
    const auto Semantic = Graph->GetArrayField(TEXT("nodes"))[0]->AsObject()->GetObjectField(TEXT("semantic"));
    const auto Reference = Semantic->GetObjectField(TEXT("variable"));
    const auto Declaration = Snapshot->GetArrayField(TEXT("variables"))[0]->AsObject();
    const FString Identity = Reference->GetStringField(TEXT("guid"));
    const FString Hint = TEXT("expose_on_spawn=true [spawn_argument_possible; default_not_constant]");
    const auto Render = [&Snapshot]() { return XBlueprintReadPack::Build(Snapshot.ToSharedRef(), TEXT("fixture contract")); };
    auto Pack = Render();
    TestTrue(TEXT("Metadata reaches the variable block even when raw declaration flags are only 5"), Pack.FindChecked(TEXT("10_Logic/G0001.pseudo.md")).Contains(Hint));
    TestTrue(TEXT("Manifest advertises additive hint support"), Pack.FindChecked(TEXT("01_Manifest.json")).Contains(TEXT("spawn_exposure_hints")));
    for (const FString& Origin : {FString(TEXT("local")), FString(TEXT("external_member")), FString(TEXT("unresolved"))})
    {
        Semantic->SetStringField(TEXT("binding_origin"), Origin);
        TestFalse(TEXT("Non-self bindings do not borrow this asset's metadata"), Render().FindChecked(TEXT("10_Logic/G0001.pseudo.md")).Contains(Hint));
    }
    Semantic->SetStringField(TEXT("binding_origin"), TEXT("self_member"));
    Reference->SetBoolField(TEXT("is_local_scope"), true);
    TestFalse(TEXT("Local identity takes precedence"), Render().FindChecked(TEXT("10_Logic/G0001.pseudo.md")).Contains(Hint));
    Reference->SetBoolField(TEXT("is_local_scope"), false);
    Reference->SetBoolField(TEXT("is_self_context"), false);
    TestFalse(TEXT("External receiver cannot reuse a local declaration"), Render().FindChecked(TEXT("10_Logic/G0001.pseudo.md")).Contains(Hint));
    Reference->SetBoolField(TEXT("is_self_context"), true);
    for (const FString& BadGuid : {FString(TEXT("00000000000000000000000000000000")), FGuid::NewGuid().ToString(), FString(TEXT("FA33A075"))})
    {
        Reference->SetStringField(TEXT("guid"), BadGuid);
        TestFalse(TEXT("Missing, invalid or unmatched identity cannot fall back to name"), Render().FindChecked(TEXT("10_Logic/G0001.pseudo.md")).Contains(Hint));
    }
    Reference->SetStringField(TEXT("guid"), Identity);
    Reference->SetStringField(TEXT("name"), TEXT("Other"));
    TestFalse(TEXT("GUID and declaration name must agree"), Render().FindChecked(TEXT("10_Logic/G0001.pseudo.md")).Contains(Hint));
    Reference->SetStringField(TEXT("name"), TEXT("CoinRotate"));
    Declaration->GetObjectField(TEXT("metadata"))->SetStringField(TEXT("ExposeOnSpawn"), TEXT("false"));
    TestFalse(TEXT("False metadata does not become a positive claim"), Render().FindChecked(TEXT("10_Logic/G0001.pseudo.md")).Contains(Hint));
    Declaration->GetObjectField(TEXT("metadata"))->SetStringField(TEXT("ExposeOnSpawn"), TEXT("true"));
    auto Variables = Snapshot->GetArrayField(TEXT("variables"));
    const auto DuplicateDeclaration = Variables[0];
    Variables.Add(DuplicateDeclaration);
    Snapshot->SetArrayField(TEXT("variables"), Variables);
    TestFalse(TEXT("Duplicate GUID is ambiguous"), Render().FindChecked(TEXT("10_Logic/G0001.pseudo.md")).Contains(Hint));
    Variables.SetNum(1);
    Snapshot->SetArrayField(TEXT("variables"), Variables);
    Snapshot->SetArrayField(TEXT("macro_definitions"), Snapshot->GetArrayField(TEXT("graphs")));
    Pack = Render();
    TestTrue(TEXT("Owned graph retains the hint"), Pack.FindChecked(TEXT("10_Logic/G0001.pseudo.md")).Contains(Hint));
    TestFalse(TEXT("External macro does not inherit caller variable declarations"), Pack.FindChecked(TEXT("30_Dependencies/M0001.pseudo.md")).Contains(Hint));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXBlueprintGraphExporterMacroIterationTest,
    "XTools.AssetEditor.BlueprintGraphExporter.MacroIteration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXBlueprintGraphExporterMacroIterationTest::RunTest(const FString& Parameters)
{
    UBlueprint* Standard = LoadObject<UBlueprint>(nullptr, TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
    if (!TestNotNull(TEXT("Current engine standard macro asset"), Standard)) { return false; }
    const bool DirtyBefore = Standard->GetOutermost()->IsDirty();
    UBlueprint* BP = NewObject<UBlueprint>(GetTransientPackage());
    BP->ParentClass = AActor::StaticClass();
    UEdGraph* Graph = NewObject<UEdGraph>(BP, TEXT("IterationCaller"));
    Graph->Schema = UEdGraphSchema_K2::StaticClass();
    Graph->GraphGuid = FGuid::NewGuid();
    BP->UbergraphPages.Add(Graph);
    UK2Node_CustomEvent* Entry = NewObject<UK2Node_CustomEvent>(Graph, TEXT("IterationEntry"));
    Entry->CustomFunctionName = TEXT("Start");
    Entry->NodeGuid = FGuid::NewGuid();
    Graph->AddNode(Entry);
    Entry->AllocateDefaultPins();
    int32 Supported = 0;
    for (UEdGraph* Macro : Standard->MacroGraphs)
    {
        const FString Name = Macro->GetName();
        if (Name != TEXT("ForEachLoop") && Name != TEXT("ForEachLoopWithBreak")
            && Name != TEXT("ForLoop") && Name != TEXT("ForLoopWithBreak") && Name != TEXT("WhileLoop")) { continue; }
        UK2Node_MacroInstance* Instance = NewObject<UK2Node_MacroInstance>(Graph);
        Instance->SetMacroGraph(Macro);
        Graph->AddNode(Instance);
        Instance->AllocateDefaultPins();
        const auto Semantic = XBlueprintGraphExporterTests::BuildNodeSemanticJson(Instance);
        const TSharedPtr<FJsonObject>* Iteration = nullptr;
        if (!TestTrue(*FString::Printf(TEXT("%s actual engine pins produce a hint"), *Name), Semantic->TryGetObjectField(TEXT("iteration"), Iteration))) { continue; }
        ++Supported;
        TestEqual(TEXT("Definition points to the real macro graph"), (*Iteration)->GetStringField(TEXT("definition_graph")), Macro->GetPathName());
        TestEqual(TEXT("Loop body is a caller continuation, not the macro definition"), (*Iteration)->GetStringField(TEXT("body_scope")), FString(TEXT("caller_continuation_from_loop_body_pin")));
        for (const auto& Field : (*Iteration)->Values)
        {
            if (!Field.Key.EndsWith(TEXT("_pin"))) { continue; }
            TestNotNull(TEXT("Every summarized pin exists on this instance"), Instance->FindPin(*Field.Value->AsString()));
        }
        UEdGraphPin* Body = Instance->FindPin(TEXT("LoopBody"), EGPD_Output);
        Body->Direction = EGPD_Input;
        TestFalse(TEXT("Malformed signature does not claim a loop hint"), XBlueprintGraphExporterTests::BuildNodeSemanticJson(Instance)->HasField(TEXT("iteration")));
        Body->Direction = EGPD_Output;
        UEdGraph* Impostor = NewObject<UEdGraph>(BP, *Name);
        Instance->SetMacroGraph(Impostor);
        TestFalse(TEXT("Same-named custom macro does not inherit engine semantics"), XBlueprintGraphExporterTests::BuildNodeSemanticJson(Instance)->HasField(TEXT("iteration")));
        Instance->SetMacroGraph(Macro);
    }
    TestEqual(TEXT("All five supported signatures exercised"), Supported, 5);
    const auto Snapshot = XBlueprintGraphExporterTests::BuildBlueprintJson(BP);
    const auto Pack = XBlueprintReadPack::Build(Snapshot.ToSharedRef(), TEXT("fixture contract"));
    const FString Logic = Pack.FindChecked(TEXT("10_Logic/G0001.pseudo.md"));
    TestTrue(TEXT("Compact loop hints reach pseudocode"), Logic.Contains(TEXT("\n  iteration: {")));
    TestTrue(TEXT("Definition and caller body scopes remain separate"), Logic.Contains(TEXT("caller_continuation_from_loop_body_pin")) && Logic.Contains(TEXT("definition_graph")));
    TSharedPtr<FJsonObject> Manifest;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Pack.FindChecked(TEXT("01_Manifest.json"))), Manifest);
    const auto Record = Manifest->GetArrayField(TEXT("graphs"))[0]->AsObject();
    TestEqual(TEXT("Manifest exposes full graph GUID"), Record->GetStringField(TEXT("graph_guid")), Graph->GraphGuid.ToString());
    TestEqual(TEXT("Manifest exposes entry node GUID"), Record->GetArrayField(TEXT("entries"))[0]->AsObject()->GetStringField(TEXT("node_guid")), Entry->NodeGuid.ToString());
    const FString OriginalEntryId = Record->GetArrayField(TEXT("entries"))[0]->AsObject()->GetStringField(TEXT("id"));
    Entry->NodePosY = 10000;
    const auto MovedPack = XBlueprintReadPack::Build(XBlueprintGraphExporterTests::BuildBlueprintJson(BP).ToSharedRef(), TEXT("fixture contract"));
    TSharedPtr<FJsonObject> MovedManifest;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(MovedPack.FindChecked(TEXT("01_Manifest.json"))), MovedManifest);
    const auto MovedRecord = MovedManifest->GetArrayField(TEXT("graphs"))[0]->AsObject();
    const auto MovedEntry = MovedRecord->GetArrayField(TEXT("entries"))[0]->AsObject();
    TestNotEqual(TEXT("Moving a node may change its snapshot-local id"), MovedEntry->GetStringField(TEXT("id")), OriginalEntryId);
    TestEqual(TEXT("Moving a node preserves its full identity"), MovedEntry->GetStringField(TEXT("node_guid")), Entry->NodeGuid.ToString());
    TestEqual(TEXT("Graph identity remains available across snapshots"), MovedRecord->GetStringField(TEXT("graph_guid")), Record->GetStringField(TEXT("graph_guid")));
    TestEqual(TEXT("Read-only hints do not dirty the engine macro asset"), Standard->GetOutermost()->IsDirty(), DirtyBefore);
    return true;
}

#endif
