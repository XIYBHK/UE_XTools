/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

/**
 * Blueprint graph readback exporter.
 * Exports selected Blueprint assets to JSON, AI context and Markdown under Saved/XTools.
 */
class FX_BlueprintGraphExporter
{
public:
    static void ExportBlueprints(const TArray<FAssetData>& SelectedAssets);
};

#if WITH_DEV_AUTOMATION_TESTS
class FJsonObject;
class UEdGraphNode;
class UEdGraph;
class UBlueprint;

namespace XBlueprintGraphExporterTests
{
    TSharedPtr<FJsonObject> BuildNodeSemanticJson(const UEdGraphNode* Node);
    TSharedPtr<FJsonObject> BuildGraphJson(UEdGraph* Graph);
    TSharedPtr<FJsonObject> BuildBlueprintJson(UBlueprint* Blueprint);
    bool ExportBlueprintFiles(UBlueprint* Blueprint, FString& OutDirectory, FString& OutError);
    FString BuildRootEntry(const FString& RootDirectory, const FString& AssetPath);
    FString BuildGraphMarkdown(UEdGraph* Graph);
}
#endif
