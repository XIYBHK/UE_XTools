// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** 供 CoACD DLL 使用的轻量数据结构，与ABI一致 */
struct FCoACD_Mesh
{
    double* vertices_ptr = nullptr;
    uint64 vertices_count = 0;      // uint64_t
    int* triangles_ptr = nullptr;
    uint64 triangles_count = 0;     // uint64_t (number of triangles)
};

struct FCoACD_MeshArray
{
    FCoACD_Mesh* meshes_ptr = nullptr;
    uint64 meshes_count = 0;        // uint64_t
};

/** CoACD v1.0.7 运行/释放函数指针类型 */
using TCoACD_Run = FCoACD_MeshArray(*) (
    const FCoACD_Mesh&,    // input mesh
    double,                // threshold
    int,                   // max_convex_hull
    int,                   // preprocess_mode
    int,                   // prep_resolution
    int,                   // sample_resolution
    int,                   // mcts_nodes
    int,                   // mcts_iteration
    int,                   // mcts_max_depth
    bool,                  // pca
    bool,                  // merge
    bool,                  // decimate
    int,                   // max_ch_vertex
    bool,                  // extrude
    double,                // extrude_margin
    int,                   // approximate_mode
    unsigned int           // seed
);

using TCoACD_Free = void(*)(FCoACD_MeshArray);

/** 适配层 API：负责加载/卸载 DLL 与符号解析 */
namespace CoACD
{
    /** 初始化并解析符号 */
    bool Initialize();
    /** 释放 DLL 资源 */
    void Shutdown();
    /** 是否可用 */
    bool IsAvailable();

    /** 获取运行/释放函数指针（可能为nullptr） */
    TCoACD_Run GetRun();
    TCoACD_Free GetFree();
}


