/* Copyright (c) 2025 XIYBHK; Licensed under UE_XTools License */
#include "BlueprintTools/X_BlueprintReadPack.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Containers/StringConv.h"
#include "Misc/AutomationTest.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXBlueprintGraphExporterReadPackTest,
    "XTools.AssetEditor.BlueprintGraphExporter.ReadPack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXBlueprintGraphExporterReadPackTest::RunTest(const FString& Parameters)
{
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("XTools"));
    FString ContractText;
    if (!TestTrue(TEXT("Plugin resource available"), Plugin.IsValid())
        || !TestTrue(TEXT("Read contract resource"), FFileHelper::LoadFileToString(ContractText, *(Plugin->GetBaseDir() / TEXT("Resources/BlueprintExport/02_ReadingContract.md"))))) { return false; }
    // Non-monotonic pin indices and duplicate GUIDs intentionally prevent name/order-based joins.
    const FString Input = TEXT(R"JSON({"asset_path":"/Game/Fixture.Fixture","graphs":[
      {"name":"EventGraph","path":"/Game/Fixture.Fixture:EventGraph","nodes":[
        {"id":"N0","class":"K2Node_CustomEvent","is_enabled":true,"semantic":{"kind":"custom_event","custom_function_name":"Start"},"pins":[{"index":0,"name":"then","direction":"output","is_exec":true,"connected":true}]},
        {"id":"N1","class":"K2Node_AsyncAction","is_enabled":true,"semantic":{"kind":"async_action","proxy_factory_class":"/Script/Test.Proxy","proxy_factory_function":"Run"},"pins":[{"index":0,"name":"execute","direction":"input","is_exec":true},{"index":1,"name":"then","direction":"output","is_exec":true,"connected":false},{"index":2,"name":"OnTick","direction":"output","is_exec":true,"connected":true},{"index":3,"name":"OnFinished","direction":"output","is_exec":true,"connected":true}]},
        {"id":"N2","class":"CustomNode","class_path":"/Script/Test.Custom","is_enabled":true,"enabled_state":"DevelopmentOnly","pins":[{"index":0,"name":"execute","direction":"input","is_exec":true},{"index":7,"id":"0000","name":"Value_5","direction":"input","connected":true,"default":"WRONG_DEFAULT"},{"index":6,"id":"0000","name":"Value_6","direction":"input","connected":true,"default":""},{"index":9,"name":"ignored","direction":"input","default_value_ignored":true,"default":"IGNORED_VALUE"},{"index":10,"name":"empty","direction":"input","default":""},{"index":11,"name":"external","direction":"input","connected":true,"default":"NOT_AN_INPUT"},{"index":12,"name":"then","direction":"output","is_exec":true,"connected":true}]},
        {"id":"N3","class":"K2Node_CallFunction","is_enabled":true,"semantic":{"kind":"call_function","is_pure":true,"resolved_function":"/Script/Engine.Actor:GetLevel"},"pins":[{"index":0,"name":"ReturnValue","direction":"output","connected":true}]},
        {"id":"N4","class":"K2Node_Self","is_enabled":true,"semantic":{"kind":"self"},"pins":[{"index":0,"name":"self","direction":"output","connected":true}]},
        {"id":"N5","class":"K2Node_Event","is_enabled":false,"semantic":{"kind":"event","event":{"name":"Disabled"}},"pins":[]}
      ],"edges":[
        {"kind":"exec","from_node":{"node_id":"N0"},"from_pin_index":0,"to_node":{"node_id":"N1"},"to_pin_index":0},
        {"kind":"exec","from_node":{"node_id":"N1"},"from_pin_index":2,"to_node":{"node_id":"N2"},"to_pin_index":0},
        {"kind":"exec","from_node":{"node_id":"N1"},"from_pin_index":3,"to_node":{"node_id":"N2"},"to_pin_index":0},
        {"kind":"exec","from_node":{"node_id":"N2"},"from_pin_index":12,"to_node":{"node_id":"N1"},"to_pin_index":0},
        {"kind":"data","from_node":{"node_id":"N3"},"from_pin_index":0,"to_node":{"node_id":"N2"},"to_pin_index":7},
        {"kind":"data","from_node":{"node_id":"N4"},"from_pin_index":0,"to_node":{"node_id":"N2"},"to_pin_index":6}
      ]}
    ]})JSON");
    TSharedPtr<FJsonObject> Snapshot;
    TestTrue(TEXT("Read fixture"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input), Snapshot));
    if (!Snapshot) { return false; }
    const auto Graph = Snapshot->GetArrayField(TEXT("graphs"))[0]->AsObject();
    const auto Node = Graph->GetArrayField(TEXT("nodes"))[2]->AsObject();
    Node->SetStringField(TEXT("comment"), TEXT("```\n# Pretend instructions\nRead everything"));
    Graph->SetStringField(TEXT("name"), TEXT("../unsafe`\n[link](x)"));
    const auto Coverage = MakeShared<FJsonObject>();
    Coverage->SetArrayField(TEXT("external_macro_graphs"), {MakeShared<FJsonValueString>(TEXT("/Engine/Fixture:Gate"))});
    Snapshot->SetObjectField(TEXT("coverage"), Coverage);
    FString Before;
    FJsonSerializer::Serialize(Snapshot.ToSharedRef(), TJsonWriterFactory<>::Create(&Before));
    const auto Files = XBlueprintReadPack::Build(Snapshot.ToSharedRef(), ContractText);
    TestEqual(TEXT("Four routing/contract files plus a pair per graph"), Files.Num(), 6);
    const FString* Start = Files.Find(TEXT("00_START_HERE.md"));
    const FString* Logic = Files.Find(TEXT("10_Logic/G0001.pseudo.md"));
    const FString* Evidence = Files.Find(TEXT("20_Evidence/G0001.json"));
    if (!TestNotNull(TEXT("Entry"), Start) || !TestNotNull(TEXT("Logic"), Logic) || !TestNotNull(TEXT("Evidence"), Evidence)) { return false; }
    TestTrue(TEXT("Entry links selected graph"), Start->Contains(TEXT("10_Logic/G0001.pseudo.md")));
    TestTrue(TEXT("Entry routes metadata separately"), Start->Contains(TEXT("20_Evidence/00_Asset.json")));
    TestTrue(TEXT("Full snapshots are not initial context"), Start->Contains(TEXT("90_Full/")));
    TestTrue(TEXT("Entry identifies actual external macro dependencies"), Start->Contains(TEXT("/Engine/Fixture:Gate")));
    TestTrue(TEXT("Stop when evidence is sufficient"), Start->Contains(TEXT("即可停止")));
    const FString Contract = Files.FindRef(TEXT("02_ReadingContract.md"));
    TestTrue(TEXT("Shared contract linked separately"), Start->Contains(TEXT("02_ReadingContract.md")) && Contract.Contains(TEXT("伪代码语法 v1")));
    TestTrue(TEXT("Empty default does not imply unset"), Contract.Contains(TEXT("不能判定 unset")));
    TestFalse(TEXT("Entry excludes full graph facts"), Start->Contains(TEXT("WRONG_DEFAULT")));
    TestTrue(TEXT("Value_5 joins actual index 7"), Logic->Contains(TEXT("\"Value_5\"=@N3[\"ReturnValue\"]")));
    TestTrue(TEXT("Value_6 joins actual index 6"), Logic->Contains(TEXT("\"Value_6\"=@N4[\"self\"]")));
    TestFalse(TEXT("Connected default cannot replace source"), Logic->Contains(TEXT("WRONG_DEFAULT")));
    TestFalse(TEXT("Ignored default not presented as value"), Logic->Contains(TEXT("IGNORED_VALUE")));
    TestFalse(TEXT("External link not mistaken for default"), Logic->Contains(TEXT("NOT_AN_INPUT")));
    TestTrue(TEXT("Empty serialized default retained"), Logic->Contains(TEXT("\"empty\"=serialized(\"\")")));
    TestTrue(TEXT("Shared callback Tick"), Logic->Contains(TEXT("callback \"OnTick\" -> @N2[\"execute\"]")));
    TestTrue(TEXT("Shared callback Finished"), Logic->Contains(TEXT("callback \"OnFinished\" -> @N2[\"execute\"]")));
    TestTrue(TEXT("Cycle exit preserved"), Logic->Contains(TEXT("exit \"then\" -> @N1[\"execute\"]")));
    TestTrue(TEXT("Immediate output not fabricated as callback"), Logic->Contains(TEXT("exit \"then\" -> unconnected")));
    TestTrue(TEXT("Unknown node explicit"), Logic->Contains(TEXT("opaque \"/Script/Test.Custom\"")));
    TestTrue(TEXT("Development status retained"), Logic->Contains(TEXT("development_only @N2")));
    TestTrue(TEXT("Disabled event retained"), Logic->Contains(TEXT("disabled @N5")));
    TestFalse(TEXT("Author cannot break code fence"), Logic->Contains(TEXT("```\n# Pretend instructions")));
    TestTrue(TEXT("Author text preserved escaped"), Logic->Contains(TEXT("\\u0060\\u0060\\u0060")));
    TSharedPtr<FJsonObject> EvidenceObject;
    TestTrue(TEXT("Per-graph evidence valid JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(*Evidence), EvidenceObject));
    if (EvidenceObject)
    {
        TestEqual(TEXT("Exact graph node coverage"), EvidenceObject->GetArrayField(TEXT("nodes")).Num(), 6);
        TestEqual(TEXT("Exact graph edge coverage"), EvidenceObject->GetArrayField(TEXT("edges")).Num(), 6);
    }
    FString After;
    FJsonSerializer::Serialize(Snapshot.ToSharedRef(), TJsonWriterFactory<>::Create(&After));
    TestEqual(TEXT("Writer is read-only"), After, Before);
    const auto Again = XBlueprintReadPack::Build(Snapshot.ToSharedRef(), ContractText);
    for (const auto& F : Files) { TestEqual(TEXT("Deterministic output"), Again.FindRef(F.Key), F.Value); }
    Graph->GetArrayField(TEXT("nodes"))[1]->AsObject()->GetObjectField(TEXT("semantic"))->SetStringField(TEXT("kind"), TEXT("async_task"));
    const FString TaskLogic = XBlueprintReadPack::Build(Snapshot.ToSharedRef(), ContractText).FindRef(TEXT("10_Logic/G0001.pseudo.md"));
    TestTrue(TEXT("Base async tasks preserve callbacks"), TaskLogic.Contains(TEXT("callback \"OnFinished\" -> @N2[\"execute\"]")));
    TestTrue(TEXT("Base async tasks are not opaque"), TaskLogic.Contains(TEXT("@N1: async ")));

    TSharedPtr<FJsonObject> Manifest;
    TestTrue(TEXT("Machine manifest parses"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Files.FindRef(TEXT("01_Manifest.json"))), Manifest));
    if (Manifest)
    {
        TestEqual(TEXT("Manifest version"), Manifest->GetIntegerField(TEXT("format_version")), 2);
        const auto Record = Manifest->GetArrayField(TEXT("graphs"))[0]->AsObject();
        const FTCHARToUTF8 Bytes(**Logic);
        uint8 Digest[FSHA1::DigestSize];
        FSHA1::HashBuffer(Bytes.Get(), Bytes.Length(), Digest);
        TestEqual(TEXT("Manifest binds exact UTF8 logic bytes"), Record->GetStringField(TEXT("logic_sha1")), BytesToHex(Digest, FSHA1::DigestSize).ToLower());
        TestEqual(TEXT("Disabled event remains an explicit entry candidate"), Record->GetArrayField(TEXT("entries")).Num(), 2);
        TestEqual(TEXT("Manifest graph identity"), Record->GetStringField(TEXT("path")), Graph->GetStringField(TEXT("path")));
        TestEqual(TEXT("Pseudo grammar is versioned independently"), Manifest->GetIntegerField(TEXT("pseudo_format_version")), 1);
        TestEqual(TEXT("Logic budget uses exact UTF8 bytes"), Record->GetIntegerField(TEXT("logic_bytes")), Bytes.Length());
        TestEqual(TEXT("Evidence budget uses exact UTF8 bytes"), Record->GetIntegerField(TEXT("evidence_bytes")), FTCHARToUTF8(**Evidence).Length());
        uint8 ContractDigest[FSHA1::DigestSize];
        const FTCHARToUTF8 ContractBytes(*Contract);
        FSHA1::HashBuffer(ContractBytes.Get(), ContractBytes.Length(), ContractDigest);
        TestEqual(TEXT("Contract content identity is reproducible"), Manifest->GetObjectField(TEXT("reading_contract"))->GetStringField(TEXT("sha1")), BytesToHex(ContractDigest, FSHA1::DigestSize).ToLower());
    }

    // Pin names are not unique identities: duplicate declarations and references use the same actual index.
    auto Pins = Node->GetArrayField(TEXT("pins"));
    for (int32 Index = 20; Index < 24; ++Index)
    {
        const auto Pin = MakeShared<FJsonObject>();
        Pin->SetNumberField(TEXT("index"), Index);
        Pin->SetStringField(TEXT("name"), Index < 22 ? TEXT("duplicate") : TEXT("result"));
        Pin->SetStringField(TEXT("direction"), Index < 22 ? TEXT("input") : TEXT("output"));
        Pin->SetStringField(TEXT("default"), Index == 20 ? TEXT("0") : TEXT("false"));
        const auto Type = MakeShared<FJsonObject>();
        Type->SetStringField(TEXT("category"), Index == 20 ? TEXT("int") : TEXT("bool"));
        Pin->SetObjectField(TEXT("type"), Type);
        Pins.Add(MakeShared<FJsonValueObject>(Pin));
    }
    Node->SetArrayField(TEXT("pins"), Pins);
    const FString DuplicateLogic = XBlueprintReadPack::Build(Snapshot.ToSharedRef(), ContractText).FindRef(TEXT("10_Logic/G0001.pseudo.md"));
    TestTrue(TEXT("Duplicate input zero retained with identity"), DuplicateLogic.Contains(TEXT("\"duplicate\"#20=0")));
    TestTrue(TEXT("Duplicate input false retained with identity"), DuplicateLogic.Contains(TEXT("\"duplicate\"#21=false")));
    TestTrue(TEXT("Duplicate output identity retained"), DuplicateLogic.Contains(TEXT("\"result\"#22 [unconnected], \"result\"#23 [unconnected]")));
    TSharedPtr<FJsonObject> EntrySemantic;
    const FString LocalFixture = TEXT(R"JSON({"kind":"function_entry","function":{"name":"Sum"},"local_variables":[
      {"name":"Val","type":{"display":"real:double"},"default":"","default_source":"type_default","effective_default":"0"},
      {"name":"Flag","type":{"display":"bool"},"default":"false","default_source":"explicit"},
      {"name":"Struct","type":{"display":"struct"},"default":"","default_source":"type_default"}
    ]})JSON");
    TestTrue(TEXT("Local declaration fixture parses"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LocalFixture), EntrySemantic));
    EntrySemantic->SetStringField(TEXT("local_scope"), TEXT("/Game/Fixture:Sum"));
    Node->SetObjectField(TEXT("semantic"), EntrySemantic);
    const FString LocalLogic = XBlueprintReadPack::Build(Snapshot.ToSharedRef(), ContractText).FindRef(TEXT("10_Logic/G0001.pseudo.md"));
    TestTrue(TEXT("Accumulator zero and raw empty both visible"), LocalLogic.Contains(TEXT("local \"Val\": \"real:double\" = type_default(0) [raw_default=\"\"]")));
    TestTrue(TEXT("Explicit false visible"), LocalLogic.Contains(TEXT("local \"Flag\": \"bool\" = serialized(\"false\") [explicit]")));
    TestTrue(TEXT("Complex type requires evidence"), LocalLogic.Contains(TEXT("local \"Struct\": \"struct\" = type_default [see_evidence] [raw_default=\"\"]")));
    TestTrue(TEXT("Cross-asset implementation lookup explained"), LocalLogic.Contains(TEXT("05_Query.py deps")));
    TestTrue(TEXT("Function scope appears next to declarations"), LocalLogic.Contains(TEXT("local_scope: \"/Game/Fixture:Sum\"")));
    // Two output links into one consumer are not two consumer nodes or two calls.
    auto Edges = Graph->GetArrayField(TEXT("edges"));
    auto ExtraEdge = MakeShared<FJsonObject>();
    ExtraEdge->Values = Edges[4]->AsObject()->Values;
    ExtraEdge->SetNumberField(TEXT("to_pin_index"), 6);
    Edges.Add(MakeShared<FJsonValueObject>(ExtraEdge));
    Graph->SetArrayField(TEXT("edges"), Edges);
    FString Hints = XBlueprintReadPack::Build(Snapshot.ToSharedRef(), ContractText).FindRef(TEXT("10_Logic/G0001.pseudo.md"));
    TestTrue(TEXT("Data uses and consumers are separate static facts"), Hints.Contains(TEXT("demand: data_edges=2 consumers=1 [static_direct; not_call_count]")));
    auto ComponentSemantic = MakeShared<FJsonObject>();
    ComponentSemantic->SetStringField(TEXT("kind"), TEXT("variable"));
    ComponentSemantic->SetStringField(TEXT("access"), TEXT("get"));
    auto Member = MakeShared<FJsonObject>();
    Member->SetStringField(TEXT("name"), TEXT("Mesh"));
    ComponentSemantic->SetObjectField(TEXT("variable"), Member);
    ComponentSemantic->SetStringField(TEXT("binding_origin"), TEXT("self_member"));
    ComponentSemantic->SetStringField(TEXT("component_binding"), TEXT("scs_property"));
    ComponentSemantic->SetStringField(TEXT("scs_node_path"), TEXT("/Game/Fixture:MeshNode"));
    Graph->GetArrayField(TEXT("nodes"))[4]->AsObject()->SetObjectField(TEXT("semantic"), ComponentSemantic);
    Hints = XBlueprintReadPack::Build(Snapshot.ToSharedRef(), ContractText).FindRef(TEXT("10_Logic/G0001.pseudo.md"));
    TestTrue(TEXT("Component declaration readable without asset metadata"), Hints.Contains(TEXT("component_read \"Mesh\"")));
    TestTrue(TEXT("SCS identity stays explicit"), Hints.Contains(TEXT("component=\"scs_property\" scs=\"/Game/Fixture:MeshNode\"")));
    auto Macro = MakeShared<FJsonObject>();
    Macro->Values = Graph->Values;
    Macro->SetStringField(TEXT("path"), TEXT("/Engine/Fixture.Fixture:Gate"));
    Snapshot->SetArrayField(TEXT("macro_definitions"), {MakeShared<FJsonValueObject>(Macro)});
    const auto DependencyFiles = XBlueprintReadPack::Build(Snapshot.ToSharedRef(), ContractText);
    TestTrue(TEXT("Macro definition has separate pseudo"), DependencyFiles.Contains(TEXT("30_Dependencies/M0001.pseudo.md")));
    TestTrue(TEXT("Macro definition has separate evidence"), DependencyFiles.Contains(TEXT("30_Dependencies/M0001.json")));
    TSharedPtr<FJsonObject> AssetMetadata;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(DependencyFiles.FindRef(TEXT("20_Evidence/00_Asset.json"))), AssetMetadata);
    TestFalse(TEXT("Macro bodies do not inflate initial asset metadata"), AssetMetadata->HasField(TEXT("macro_definitions")));
    auto Temporary = MakeShared<FJsonObject>();
    Temporary->SetStringField(TEXT("kind"), TEXT("temporary_variable"));
    Temporary->SetBoolField(TEXT("is_persistent"), true);
    Graph->GetArrayField(TEXT("nodes"))[4]->AsObject()->SetObjectField(TEXT("semantic"), Temporary);
    auto Assignment = MakeShared<FJsonObject>();
    Assignment->SetStringField(TEXT("kind"), TEXT("assignment"));
    Node->SetObjectField(TEXT("semantic"), Assignment);
    Hints = XBlueprintReadPack::Build(Snapshot.ToSharedRef(), ContractText).FindRef(TEXT("10_Logic/G0001.pseudo.md"));
    TestTrue(TEXT("Assignment preserves connected input arguments"), Hints.Contains(TEXT("assign [Variable_is_write_target](")) && Hints.Contains(TEXT("\"Value_5\"=@N3[\"ReturnValue\"]")));
    TestTrue(TEXT("Temporary flag does not assert per-call lifetime"), Hints.Contains(TEXT("[compiler_local; persistent_savegame; no_lifetime_inference]")));
    const FString QueryText = TEXT("# portable query fixture\n");
    const auto WithQuery = XBlueprintReadPack::Build(Snapshot.ToSharedRef(), ContractText, QueryText);
    TestEqual(TEXT("Standalone query text preserved"), WithQuery.FindRef(TEXT("05_Query.py")), QueryText);
    TSharedPtr<FJsonObject> QueryManifest;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(WithQuery.FindRef(TEXT("01_Manifest.json"))), QueryManifest);
    TestEqual(TEXT("Query compatibility protocol recorded"), QueryManifest->GetObjectField(TEXT("query"))->GetIntegerField(TEXT("protocol_version")), 1);
    Snapshot->SetStringField(TEXT("asset_path"), TEXT("/Game/Other.Other"));
    TestEqual(TEXT("Contract shared by content across assets"), XBlueprintReadPack::Build(Snapshot.ToSharedRef(), ContractText).FindRef(TEXT("02_ReadingContract.md")), Contract);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXBlueprintGraphExporterKindCoverageTest,
    "XTools.AssetEditor.BlueprintGraphExporter.KindCoverage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXBlueprintGraphExporterKindCoverageTest::RunTest(const FString& Parameters)
{
    // Renderer coverage, not a claim that these fixtures exercise every native K2 expansion.
    const FString KindsText = TEXT("actor_bound_event add_component_by_class add_delegate array_get assign_delegate assignment async_action async_task bitmask_literal branch break_struct call_delegate call_function cast_byte_to_enum clear_delegate comment component_bound_event composite construct_object create_delegate custom_event delegate_set do_once_multi_input dynamic_cast enum_equality enum_inequality enum_literal event format_text function_entry function_result get_class_defaults get_data_table_row get_input_axis_key_value get_input_axis_value get_input_vector_axis_value get_subsystem input_action input_action_event input_axis_event input_axis_key_event input_key input_key_event input_touch input_touch_event input_vector_axis_event macro_instance make_array make_map make_set make_struct math_expression multi_gate multicast_delegate remove_delegate reroute select self sequence set_fields_in_struct spawn_actor struct_member_get struct_member_set struct_operation switch temporary_variable timeline tunnel_entry tunnel_exit variable future_classified_kind");
    TArray<FString> Kinds;
    KindsText.ParseIntoArray(Kinds, TEXT(" "), true);
    const auto Snapshot = MakeShared<FJsonObject>();
    const auto Graph = MakeShared<FJsonObject>();
    Graph->SetStringField(TEXT("path"), TEXT("/Game/Test.Test:Kinds"));
    TArray<TSharedPtr<FJsonValue>> Nodes;
    for (int32 Index = 0; Index < Kinds.Num(); ++Index)
    {
        const auto Node = MakeShared<FJsonObject>();
        Node->SetStringField(TEXT("id"), FString::Printf(TEXT("N%d"), Index));
        Node->SetStringField(TEXT("title"), TEXT("带换行\n与反引号`的标题"));
        Node->SetStringField(TEXT("class_path"), TEXT("/Script/Test.Node"));
        const auto Semantic = MakeShared<FJsonObject>();
        Semantic->SetStringField(TEXT("kind"), Kinds[Index]);
        Semantic->SetNumberField(TEXT("observed_value"), 42);
        Semantic->SetArrayField(TEXT("cases"), {MakeShared<FJsonValueString>(TEXT("测试1")), MakeShared<FJsonValueString>(TEXT("测试2"))});
        Node->SetObjectField(TEXT("semantic"), Semantic);
        Node->SetArrayField(TEXT("pins"), {});
        Nodes.Add(MakeShared<FJsonValueObject>(Node));
    }
    Graph->SetArrayField(TEXT("nodes"), Nodes);
    Graph->SetArrayField(TEXT("edges"), {});
    Snapshot->SetArrayField(TEXT("graphs"), {MakeShared<FJsonValueObject>(Graph)});
    const auto Files = XBlueprintReadPack::Build(Snapshot, TEXT("fixture contract"));
    const FString Logic = Files.FindRef(TEXT("10_Logic/G0001.pseudo.md"));
    TestEqual(TEXT("All current kinds plus a future classifier kind"), Kinds.Num(), 71);
    TestFalse(TEXT("Classified kinds never fall back to opaque"), Logic.Contains(TEXT(": opaque ")));
    TestTrue(TEXT("Switch classification and case values remain visible"), Logic.Contains(TEXT("classified \"switch\"")) && Logic.Contains(TEXT("\"cases\":[\"测试1\",\"测试2\"]")));
    TestTrue(TEXT("Future kinds retain their collected facts without a template"), Logic.Contains(TEXT("classified \"future_classified_kind\"")) && Logic.Contains(TEXT("\"observed_value\":42")));
    TestTrue(TEXT("Generic titles cannot create statements or code fences"), Logic.Contains(TEXT("带换行\\n与反引号\\u0060的标题")));
    TestTrue(TEXT("New capability is discoverable"), Files.FindRef(TEXT("01_Manifest.json")).Contains(TEXT("classified_fallback")));
    Nodes[0]->AsObject()->RemoveField(TEXT("semantic"));
    const FString Unknown = XBlueprintReadPack::Build(Snapshot, TEXT("fixture contract")).FindRef(TEXT("10_Logic/G0001.pseudo.md"));
    TestTrue(TEXT("Unclassified node retains opaque warning"), Unknown.Contains(TEXT("@N0: opaque \"/Script/Test.Node\" [see_evidence; do_not_assume_noop]")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXBlueprintGraphExporterRerouteBoundTest,
    "XTools.AssetEditor.BlueprintGraphExporter.RerouteBound",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXBlueprintGraphExporterRerouteBoundTest::RunTest(const FString& Parameters)
{
    const auto Snapshot = MakeShared<FJsonObject>();
    const auto Graph = MakeShared<FJsonObject>();
    Graph->SetStringField(TEXT("path"), TEXT("/Game/Test.Test:Reroutes"));
    TArray<TSharedPtr<FJsonValue>> Nodes, Edges;
    for (int32 Index = 0; Index <= 66; ++Index)
    {
        const FString Json = FString::Printf(TEXT("{\"id\":\"N%d\",\"semantic\":{\"kind\":\"%s\"},\"pins\":[{\"index\":0,\"name\":\"In\",\"direction\":\"input\"},{\"index\":1,\"name\":\"Out\",\"direction\":\"output\"}]}"), Index, Index == 0 ? TEXT("self") : TEXT("reroute"));
        TSharedPtr<FJsonObject> Node;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Node)) { return false; }
        Nodes.Add(MakeShared<FJsonValueObject>(Node));
        if (Index > 0)
        {
            const FString EdgeJson = FString::Printf(TEXT("{\"kind\":\"data\",\"from_node\":{\"node_id\":\"N%d\"},\"from_pin_index\":1,\"to_node\":{\"node_id\":\"N%d\"},\"to_pin_index\":0}"), Index - 1, Index);
            TSharedPtr<FJsonObject> Edge;
            if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(EdgeJson), Edge)) { return false; }
            Edges.Add(MakeShared<FJsonValueObject>(Edge));
        }
    }
    Graph->SetArrayField(TEXT("nodes"), Nodes);
    Graph->SetArrayField(TEXT("edges"), Edges);
    Snapshot->SetArrayField(TEXT("graphs"), {MakeShared<FJsonValueObject>(Graph)});
    const FString Logic = XBlueprintReadPack::Build(Snapshot, TEXT("fixture contract")).FindRef(TEXT("10_Logic/G0001.pseudo.md"));
    TestTrue(TEXT("63 reroutes reach original source"), Logic.Contains(TEXT("@N64: reroute(\"In\"=@N0[\"Out\"])")));
    TestTrue(TEXT("64 reroutes retain source reference at limit"), Logic.Contains(TEXT("@N65: reroute(\"In\"=@N0[\"Out\"] [continue_in_evidence])")));
    TestTrue(TEXT("65 reroutes retain next unresolved node at limit"), Logic.Contains(TEXT("@N66: reroute(\"In\"=@N1[\"Out\"] [continue_in_evidence])")));
    const auto SourceRef = Edges[0]->AsObject()->GetObjectField(TEXT("from_node"));
    SourceRef->SetStringField(TEXT("node_id"), TEXT("N2"));
    const FString Cycle = XBlueprintReadPack::Build(Snapshot, TEXT("fixture contract")).FindRef(TEXT("10_Logic/G0001.pseudo.md"));
    TestTrue(TEXT("Data cycle is explicit without recursive expansion"), Cycle.Contains(TEXT("@N2: reroute(\"In\"=@N1[\"Out\"] [data_cycle])")));
    return true;
}
#endif
