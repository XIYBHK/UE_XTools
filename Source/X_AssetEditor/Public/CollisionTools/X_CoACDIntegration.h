// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "AssetRegistry/AssetData.h"

// 前向声明 - 遵循IWYU最佳实践
class UStaticMesh;

/** 与参考实现一致的预处理模式 */
UENUM()
enum class EX_CoACDPreprocessMode : uint8
{
    Auto = 0,
    On = 1,
    Off = 2,
};

/** CoACD 参数（完整v1.0.7 API支持） */
struct FX_CoACDArgs
{
    float Threshold = 0.1f;                  // 凹度阈值 (0.01~1)
    EX_CoACDPreprocessMode PreprocessMode = EX_CoACDPreprocessMode::Off; // 默认关闭预处理（推荐）
    int32 PreprocessResolution = 50;         // 20~100
    int32 SampleResolution = 2000;           // 1000~10000
    int32 MCTSNodes = 20;                    // 10~40
    int32 MCTSIteration = 100;               // 60~2000 (基于GitHub社区经验优化)
    int32 MCTSMaxDepth = 2;                  // 2~7 (降低深度提升速度)
    bool bPCA = false;                       // PCA预处理，默认关闭
    bool bMerge = true;                      // 合并后处理，默认开启
    int32 MaxConvexHull = -1;                // -1 无限制
    int32 Seed = 0;                          // 随机种子

    // === v1.0.7 新增参数（基于官方推荐） ===
    bool bDecimate = false;                  // 顶点约束开关，默认关闭
    int32 MaxConvexHullVertex = 256;         // 每个凸包最大顶点数，默认256
    bool bExtrude = false;                   // 凸包挤出开关，默认关闭
    float ExtrudeMargin = 0.01f;             // 挤出边距，默认0.01
    int32 ApproximateMode = 0;               // 近似模式：0=凸包 1=包围盒，默认凸包

    // 扩展
    int32 SourceLODIndex = 0;                // LOD 索引
    bool bRemoveExistingCollision = true;    // 移除现有碰撞

    // 需要排除的材质关键词（槽名/材质名/路径/索引，忽略大小写）
    TArray<FString> MaterialKeywordsToExclude; // 默认不排除

    // 批量控制
    bool bEnableParallel = false;            // 是否并行（默认串行更安全）
    int32 MaxConcurrency = 2;                // 最大并发（并行时有效）
};

/**
 * CoACD 算法集成：Collision-Aware Convex Decomposition
 * 
 * 基于 SIGGRAPH2022 论文实现的高质量凸包分解算法
 * 论文: "Approximate Convex Decomposition for 3D Meshes with Collision-Aware Concavity and Tree Search"
 * 作者: Wei, Xinyue; Liu, Minghua; Ling, Zhan; Su, Hao
 * 发表: ACM Transactions on Graphics (TOG), Vol.41, No.4, 2022
 * 项目: https://github.com/SarahWeiii/CoACD (MIT License)
 * 
 * 功能：动态加载 lib_coacd.dll 并执行分解，写回 FKConvexElem
 */
class FX_CoACDIntegration
{
public:
    static bool Initialize();
    static void Shutdown();
    static bool IsAvailable();

    static bool GenerateForMesh(UStaticMesh* StaticMesh, const FX_CoACDArgs& Args);
    static void GenerateForAssets(const TArray<FAssetData>& SelectedAssets, const FX_CoACDArgs& Args);
};


