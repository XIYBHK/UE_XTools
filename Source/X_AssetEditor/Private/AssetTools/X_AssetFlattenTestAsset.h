/* Copyright (c) 2025 XIYBHK. Licensed under UE_XTools License. */
#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "X_AssetFlattenTestAsset.generated.h"

/** Serialized reference fixture, confined to the editor module. */
UCLASS(NotBlueprintable, Hidden)
class UX_AssetFlattenTestAsset : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY()
    TObjectPtr<UX_AssetFlattenTestAsset> HardReference;
    UPROPERTY()
    TSoftObjectPtr<UX_AssetFlattenTestAsset> SoftReference;
};
