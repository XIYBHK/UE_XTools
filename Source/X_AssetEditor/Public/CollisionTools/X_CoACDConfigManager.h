// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
// 前向声明，避免在Public头中引入较重的Integration头
struct FX_CoACDArgs;

/**
 * CoACD 配置管理器
 * 负责参数持久化、预设管理和配置验证
 */
class FX_CoACDConfigManager
{
public:
    // 参数持久化
    static FX_CoACDArgs LoadSaved();
    static void Save(const FX_CoACDArgs& Args);
    static void Flush();

    // 质量预设
    static void ApplyPresetQuality1(FX_CoACDArgs& Args);
    static void ApplyPresetQuality2(FX_CoACDArgs& Args);
    static void ApplyPresetQuality3(FX_CoACDArgs& Args);
    static void ApplyPresetQuality4(FX_CoACDArgs& Args);

    // 参数校验
    static bool ValidateArgs(const FX_CoACDArgs& Args, FString& OutErrorMessage);

    // 获取默认值（结构默认构造）
    static FX_CoACDArgs GetDefaultArgs();

private:
    static const TCHAR* GetConfigSection();
    static bool ValidateThreshold(float Threshold, FString& OutErrorMessage);
    static bool ValidatePreprocessResolution(int32 Resolution, FString& OutErrorMessage);
    static bool ValidateSampleResolution(int32 Resolution, FString& OutErrorMessage);
    static bool ValidateMCTSNodes(int32 Nodes, FString& OutErrorMessage);
    static bool ValidateMCTSIteration(int32 Iteration, FString& OutErrorMessage);
    static bool ValidateMCTSMaxDepth(int32 Depth, FString& OutErrorMessage);
    static bool ValidateMaxConvexHullVertex(int32 VertexCount, FString& OutErrorMessage);
    static bool ValidateExtrudeMargin(float Margin, FString& OutErrorMessage);
};


