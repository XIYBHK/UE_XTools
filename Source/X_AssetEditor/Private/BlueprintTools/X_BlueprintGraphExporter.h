/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

/**
 * Blueprint graph readback exporter.
 * Exports selected Blueprint assets to JSON and Markdown under Saved/XTools.
 */
class FX_BlueprintGraphExporter
{
public:
    static void ExportBlueprints(const TArray<FAssetData>& SelectedAssets);
};

#if WITH_DEV_AUTOMATION_TESTS
class FJsonObject;
class UEdGraphNode;

namespace XBlueprintGraphExporterTests
{
    TSharedPtr<FJsonObject> BuildNodeSemanticJson(const UEdGraphNode* Node);
}
#endif
