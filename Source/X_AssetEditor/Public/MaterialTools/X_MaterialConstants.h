/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"

/**
 * 材质常量定义
 * 集中管理材质属性名称字符串，避免硬编码和拼写错误
 */
namespace X_MaterialConstants
{
    // 基础属性
    const FString BaseColor = TEXT("BaseColor");
    const FString Metallic = TEXT("Metallic");
    const FString Specular = TEXT("Specular");
    const FString Roughness = TEXT("Roughness");
    const FString EmissiveColor = TEXT("EmissiveColor");
    const FString Opacity = TEXT("Opacity");
    const FString OpacityMask = TEXT("OpacityMask");
    const FString Normal = TEXT("Normal");
    const FString WorldPositionOffset = TEXT("WorldPositionOffset");
    const FString SubsurfaceColor = TEXT("SubsurfaceColor");
    const FString AmbientOcclusion = TEXT("AmbientOcclusion");
    const FString Refraction = TEXT("Refraction");
    const FString MaterialAttributes = TEXT("MaterialAttributes");

    // 常见别名（用于模糊匹配）
    const FString Alias_Albedo = TEXT("Albedo");
    const FString Alias_Diffuse = TEXT("Diffuse");
    const FString Alias_Metalness = TEXT("Metalness");
    const FString Alias_Rough = TEXT("Rough");
    const FString Alias_Emission = TEXT("Emission");
    const FString Alias_Emissive = TEXT("Emissive");
    const FString Alias_AO = TEXT("AO");
    const FString Alias_Ambient = TEXT("Ambient");

    // 排除词缀（黑名单）
    const FString Prefix_Not = TEXT("Not");
    const FString Prefix_Ignore = TEXT("Ignore");
    const FString Suffix_Map = TEXT("Map"); // 例如 BaseColorMap 可能不应该直接连到 BaseColor，视情况而定
}
