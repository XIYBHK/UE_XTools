/* Copyright (c) 2025 XIYBHK; Licensed under UE_XTools License */
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "BlueprintTools/X_BlueprintGraphExporter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_FunctionEntry.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    // Independent source inventory: do not reuse the exporter's JSON/edge helpers.
    TArray<TSharedPtr<FJsonValue>> InventoryGraphs(UBlueprint* Blueprint)
    {
        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        TArray<TSharedPtr<FJsonValue>> Result;
        for (UEdGraph* Graph : Graphs)
        {
            if (!Graph) continue;
            TSharedPtr<FJsonObject> GraphJson = MakeShared<FJsonObject>();
            GraphJson->SetStringField(TEXT("path"), Graph->GetPathName());
            TArray<TSharedPtr<FJsonValue>> Nodes;
            TArray<TSharedPtr<FJsonValue>> Edges;
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (!Node) continue;
                TSharedPtr<FJsonObject> NodeJson = MakeShared<FJsonObject>();
                NodeJson->SetStringField(TEXT("name"), Node->GetName());
                NodeJson->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString());
                NodeJson->SetStringField(TEXT("class_path"), Node->GetClass()->GetPathName());
                NodeJson->SetStringField(TEXT("comment"), Node->NodeComment);
                NodeJson->SetBoolField(TEXT("is_enabled"), Node->IsNodeEnabled());
                if (const UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
                {
                    TArray<TSharedPtr<FJsonValue>> Locals;
                    for (const FBPVariableDescription& Variable : Entry->LocalVariables)
                    {
                        TSharedPtr<FJsonObject> Local = MakeShared<FJsonObject>();
                        Local->SetStringField(TEXT("name"), Variable.VarName.ToString());
                        Local->SetStringField(TEXT("default"), Variable.DefaultValue);
                        Locals.Add(MakeShared<FJsonValueObject>(Local));
                    }
                    NodeJson->SetArrayField(TEXT("local_variables"), Locals);
                }
                TArray<TSharedPtr<FJsonValue>> Pins;
                for (int32 Index = 0; Index < Node->Pins.Num(); ++Index)
                {
                    UEdGraphPin* Pin = Node->Pins[Index];
                    if (!Pin) continue;
                    TSharedPtr<FJsonObject> PinJson = MakeShared<FJsonObject>();
                    PinJson->SetNumberField(TEXT("index"), Index);
                    PinJson->SetStringField(TEXT("id"), Pin->PinId.ToString());
                    PinJson->SetStringField(TEXT("name"), Pin->PinName.ToString());
                    PinJson->SetStringField(TEXT("default"), Pin->GetDefaultAsString());
                    PinJson->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
                    PinJson->SetBoolField(TEXT("is_reference"), Pin->PinType.bIsReference);
                    PinJson->SetBoolField(TEXT("is_const"), Pin->PinType.bIsConst);
                    PinJson->SetBoolField(TEXT("orphaned"), Pin->bOrphanedPin);
                    PinJson->SetNumberField(TEXT("parent_pin_index"), Node->Pins.IndexOfByKey(Pin->ParentPin));
                    TArray<TSharedPtr<FJsonValue>> SubPins;
                    for (UEdGraphPin* SubPin : Pin->SubPins)
                    {
                        if (SubPin) SubPins.Add(MakeShared<FJsonValueNumber>(Node->Pins.IndexOfByKey(SubPin)));
                    }
                    PinJson->SetArrayField(TEXT("sub_pin_indices"), SubPins);
                    Pins.Add(MakeShared<FJsonValueObject>(PinJson));
                    if (Pin->Direction != EGPD_Output) continue;
                    for (UEdGraphPin* TargetPin : Pin->LinkedTo)
                    {
                        UEdGraphNode* TargetNode = TargetPin ? TargetPin->GetOwningNode() : nullptr;
                        if (!TargetNode) continue;
                        TSharedPtr<FJsonObject> Edge = MakeShared<FJsonObject>();
                        Edge->SetStringField(TEXT("from_name"), Node->GetName());
                        Edge->SetNumberField(TEXT("from_index"), Index);
                        Edge->SetStringField(TEXT("to_name"), TargetNode->GetName());
                        Edge->SetNumberField(TEXT("to_index"), TargetNode->Pins.IndexOfByKey(TargetPin));
                        Edge->SetStringField(TEXT("kind"), Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec ? TEXT("exec") : TEXT("data"));
                        Edges.Add(MakeShared<FJsonValueObject>(Edge));
                    }
                }
                NodeJson->SetArrayField(TEXT("pins"), Pins);
                Nodes.Add(MakeShared<FJsonValueObject>(NodeJson));
            }
            GraphJson->SetArrayField(TEXT("nodes"), Nodes);
            GraphJson->SetArrayField(TEXT("edges"), Edges);
            Result.Add(MakeShared<FJsonValueObject>(GraphJson));
        }
        return Result;
    }

    void ValidateProjectAssets(const TArray<FString>& Paths)
    {
        TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
        Report->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
        TArray<TSharedPtr<FJsonValue>> Assets;
        for (const FString& Path : Paths)
        {
            TSharedPtr<FJsonObject> Asset = MakeShared<FJsonObject>();
            Asset->SetStringField(TEXT("requested_path"), Path);
            Assets.Add(MakeShared<FJsonValueObject>(Asset));
            if (!Path.StartsWith(TEXT("/Game/")))
            {
                Asset->SetStringField(TEXT("error"), TEXT("Only /Game assets are accepted"));
                continue;
            }
            const FString ObjectPath = Path.Contains(TEXT(".")) ? Path : Path + TEXT(".") + FPackageName::GetShortName(Path);
            UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
            if (!Blueprint)
            {
                Asset->SetStringField(TEXT("error"), TEXT("Blueprint load failed"));
                continue;
            }
            Asset->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
            Asset->SetBoolField(TEXT("dirty_before"), Blueprint->GetOutermost()->IsDirty());
            Asset->SetArrayField(TEXT("source_graphs"), InventoryGraphs(Blueprint));
            FString Directory;
            FString Error;
            Asset->SetBoolField(TEXT("export_succeeded"), XBlueprintGraphExporterTests::ExportBlueprintFiles(Blueprint, Directory, Error));
            Asset->SetStringField(TEXT("export_directory"), Directory);
            Asset->SetStringField(TEXT("error"), Error);
            Asset->SetBoolField(TEXT("dirty_after"), Blueprint->GetOutermost()->IsDirty());
            Asset->SetArrayField(TEXT("source_graphs_after"), InventoryGraphs(Blueprint));
        }
        Report->SetArrayField(TEXT("assets"), Assets);
        FString Text;
        FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Text));
        const FString Directory = FPaths::ProjectSavedDir() / TEXT("XTools/BlueprintExportValidation");
        IFileManager::Get().MakeDirectory(*Directory, true);
        FFileHelper::SaveStringToFile(Text, *(Directory / TEXT("source-inventory.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        if (FParse::Param(FCommandLine::Get(), TEXT("XToolsBlueprintValidationExit")))
        {
            FPlatformMisc::RequestExit(false);
        }
    }

    FAutoConsoleCommand ValidateProjectAssetsCommand(
        TEXT("XTools.BlueprintExport.ValidateAssets"),
        TEXT("Development validation: export specified /Game Blueprint paths and record independent source graph inventory under Saved/XTools/BlueprintExportValidation. Does not save source assets."),
        FConsoleCommandWithArgsDelegate::CreateStatic(&ValidateProjectAssets));
}
#endif
