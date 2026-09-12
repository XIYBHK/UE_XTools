/*
 * 蓝图图表导出器自动化测试
 */

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "BlueprintTools/X_BlueprintGraphExporter.h"
#include "BlueprintTools/X_BlueprintAIWriter.h"

#include "Dom/JsonObject.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
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
#include "K2Node_ExecutionSequence.h"
#include "K2Node_InputKeyEvent.h"
#include "K2Node_MathExpression.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_SetFieldsInStruct.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

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
    const FString AI = XBlueprintAIWriter::Write(Snapshot.ToSharedRef());
    FString After;
    FJsonSerializer::Serialize(Snapshot.ToSharedRef(), TJsonWriterFactory<>::Create(&After));
    TestEqual(TEXT("Writer does not mutate snapshot"), After, Before);
    TestEqual(TEXT("Repeated AI export stable"), XBlueprintAIWriter::Write(Snapshot.ToSharedRef()), AI);
    TArray<FString> Lines;
    AI.ParseIntoArrayLines(Lines);
    int32 EdgeCount = 0;
    int32 NodeCount = 0;
    for (const FString& Line : Lines)
    {
        if (!Line.StartsWith(TEXT("{"))) continue;
        TSharedPtr<FJsonObject> Record;
        if (!TestTrue(TEXT("Every JSON record parseable with escaped comments"),
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line), Record))) return false;
        if (Record->HasField(TEXT("from")))
        {
            ++EdgeCount;
            TestEqual(TEXT("Fanout source pin identity"), Record->GetObjectField(TEXT("from"))->GetStringField(TEXT("pin_id")), Output->PinId.ToString());
        }
        if (Record->HasField(TEXT("pins")))
        {
            ++NodeCount;
            if (Record->GetStringField(TEXT("node_guid")) == Source->NodeGuid.ToString())
            {
                TestEqual(TEXT("Author comment preserved"), Record->GetStringField(TEXT("comment")), Source->NodeComment);
                TestFalse(TEXT("Disabled state preserved"), Record->GetBoolField(TEXT("is_enabled")));
            }
            for (const TSharedPtr<FJsonValue>& PinValue : Record->GetArrayField(TEXT("pins")))
            {
                TestFalse(TEXT("Internal links represented only in edge table"), PinValue->AsObject()->HasField(TEXT("linked_to")));
            }
        }
    }
    TestEqual(TEXT("AI export does not truncate at 100 edges"), EdgeCount, 105);
    TestEqual(TEXT("AI exports classified and unknown nodes"), NodeCount, 3);
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
    const FString AI = XBlueprintAIWriter::Write(Snapshot.ToSharedRef());
    int32 EdgeCount = 0;
    TSet<FString> SourcePins;
    TArray<FString> Lines;
    AI.ParseIntoArrayLines(Lines);
    for (const FString& Line : Lines)
    {
        if (!Line.StartsWith(TEXT("{"))) continue;
        TSharedPtr<FJsonObject> Record;
        if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line), Record) && Record->HasField(TEXT("from")))
        {
            ++EdgeCount;
            const TSharedPtr<FJsonObject> From = Record->GetObjectField(TEXT("from"));
            SourcePins.Add(From->GetStringField(TEXT("node_id")) + TEXT(":") + FString::FromInt(static_cast<int32>(From->GetNumberField(TEXT("pin_index")))));
        }
    }
    TestEqual(TEXT("Both shared exits and cycle retained without entry roots"), EdgeCount, 3);
    TestEqual(TEXT("Distinct exec pin identities even with invalid GUIDs"), SourcePins.Num(), 3);
    TestTrue(TEXT("Reflected node state included"), AI.Contains(TEXT("reflected_properties")));
    TestFalse(TEXT("AI avoids repeated reachability expansion"), AI.Contains(TEXT("\"exec_chain\":")));
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
    FString OriginalAI;
    TestTrue(TEXT("JSON file readable"), FFileHelper::LoadFileToString(OriginalJson, *JsonPath));
    TestTrue(TEXT("AI file readable"), FFileHelper::LoadFileToString(OriginalAI, *AIPath));
    FString StartContent;
    FString AssetEvidence;
    TestTrue(TEXT("Start file readable"), FFileHelper::LoadFileToString(StartContent, *StartPath));
    TestTrue(TEXT("Asset evidence readable"), FFileHelper::LoadFileToString(AssetEvidence, *AssetEvidencePath));
    Blueprint->ParentClass = UObject::StaticClass();
    IFileManager& Files = IFileManager::Get();
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    const bool bProtected = PlatformFile.SetReadOnly(*StartPath, true);
    TestTrue(TEXT("Protect existing start output for failure test"), bProtected);
    if (bProtected)
    {
        TestFalse(TEXT("Unwritable bundle must fail"), XBlueprintGraphExporterTests::ExportBlueprintFiles(Blueprint, Directory, Error));
        FString CurrentJson;
        FString CurrentAI;
        FString CurrentStart;
        FFileHelper::LoadFileToString(CurrentJson, *JsonPath);
        FFileHelper::LoadFileToString(CurrentAI, *AIPath);
        FFileHelper::LoadFileToString(CurrentStart, *StartPath);
        TestEqual(TEXT("Failed export preserves old JSON"), CurrentJson, OriginalJson);
        TestEqual(TEXT("Failed export preserves old AI context"), CurrentAI, OriginalAI);
        TestEqual(TEXT("Failed export preserves old entry"), CurrentStart, StartContent);
        TestTrue(TEXT("Restore output permissions"), PlatformFile.SetReadOnly(*StartPath, false));
    }
    TestTrue(TEXT("Bundle can be replaced after failure"), XBlueprintGraphExporterTests::ExportBlueprintFiles(Blueprint, Directory, Error));
    FString UpdatedAI;
    FFileHelper::LoadFileToString(UpdatedAI, *AIPath);
    TestTrue(TEXT("New AI file observes source change"), UpdatedAI != OriginalAI);
    TArray<FString> ExportedFiles;
    Files.FindFilesRecursive(ExportedFiles, *Directory, TEXT("*"), true, false);
    TestEqual(TEXT("Fixture has seven files without graph evidence"), ExportedFiles.Num(), 7);
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
    TestFalse(TEXT("Old asset duplicate excluded"), IndexAfterB.Contains(TEXT("000_old_A")));
    TestTrue(TEXT("Both current asset entries included"), IndexAfterB.Contains(AName) && IndexAfterB.Contains(BName));
    TArray<FString> Lines;
    IndexAfterB.ParseIntoArrayLines(Lines);
    int32 Links = 0;
    for (const FString& Line : Lines) { Links += Line.StartsWith(TEXT("- [")) ? 1 : 0; }
    TestEqual(TEXT("Exactly one root entry per asset"), Links, 2);

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
    return true;
}

#endif
