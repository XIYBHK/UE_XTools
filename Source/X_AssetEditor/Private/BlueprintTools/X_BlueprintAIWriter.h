/* Copyright (c) 2025 XIYBHK; Licensed under UE_XTools License */
#pragma once

#include "CoreMinimal.h"

class FJsonObject;

namespace XBlueprintAIWriter
{
    /** Converts an exporter snapshot into a self-contained, loss-minimizing AI Markdown document. */
    FString Write(const TSharedRef<FJsonObject>& Snapshot);
}
