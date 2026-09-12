/* Copyright (c) 2025 XIYBHK; Licensed under UE_XTools License */
#pragma once

#include "CoreMinimal.h"

class FJsonObject;

namespace XBlueprintReadPack
{
    /** Deterministic, read-only presentation of a snapshot. Keys are controlled relative paths. */
    TMap<FString, FString> Build(const TSharedRef<FJsonObject>& Snapshot);
}
