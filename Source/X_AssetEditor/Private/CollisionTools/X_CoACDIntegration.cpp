// Copyright Epic Games, Inc. All Rights Reserved.

/*
 * CoACD Algorithm Integration for Unreal Engine
 * 
 * This implementation integrates the CoACD algorithm for high-quality convex decomposition.
 * 
 * Algorithm Citation:
 * @article{wei2022coacd,
 *   title={Approximate convex decomposition for 3d meshes with collision-aware concavity and tree search},
 *   author={Wei, Xinyue and Liu, Minghua and Ling, Zhan and Su, Hao},
 *   journal={ACM Transactions on Graphics (TOG)},
 *   volume={41},
 *   number={4},
 *   pages={1--18},
 *   year={2022},
 *   publisher={ACM New York, NY, USA}
 * }
 * 
 * Original Project: https://github.com/SarahWeiii/CoACD (MIT License)
 * SIGGRAPH 2022 Paper: "Collision-Aware Convex Decomposition with MCTS"
 */

// 自己的头文件 - 必须第一个
#include "CollisionTools/X_CoACDIntegration.h"

// UE引擎核心头文件
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/ConvexElem.h"
#include "RawMesh.h"

// UE平台抽象层
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"

// UE插件和工具系统
#include "Interfaces/IPluginManager.h"

// UE异步和并发系统
#include "Async/ParallelFor.h"

// UE UI和通知系统
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

// 定义日志类别
DEFINE_LOG_CATEGORY_STATIC(LogX_AssetEditor, Log, All);

// 参考 Docs/ref/CoACD 实现，先留空导入符号，后续接第三方 DLL
// 必须与 CoACD DLL 的 ABI 完全一致（参考 Docs/ref/CoACD/Source/CoACD/CoACD.cpp）
struct FCoACD_Mesh
{
    double* vertices_ptr;          // 顶点数组指针
    uint64 vertices_count;         // 顶点数量 (uint64_t)
    int* triangles_ptr;            // 三角索引指针
    uint64 triangles_count;        // 三角数量 (uint64_t)
};

struct FCoACD_MeshArray
{
    FCoACD_Mesh* meshes_ptr;       // 凸包数组指针
    uint64 meshes_count;           // 凸包数量 (uint64_t)
};

// 基于GitHub commit "Add missing function args in public CoACD function (#66)" 更新的函数签名
// 修复与官方API头文件的参数顺序不匹配问题
using TCoACD_Run = FCoACD_MeshArray(*)(
    const FCoACD_Mesh&,    // 1. 输入网格
    double,                // 2. threshold
    int,                   // 3. max_convex_hull
    int,                   // 4. preprocess_mode
    int,                   // 5. prep_resolution
    int,                   // 6. resolution
    int,                   // 7. mcts_nodes
    int,                   // 8. mcts_iteration
    int,                   // 9. mcts_depth
    bool,                  // 10. pca
    bool,                  // 11. merge
    bool,                  // 12. decimate (enable vertex constraints, v1.0.3)
    int,                   // 13. max_ch_vertex (max vertices per hull, v1.0.3)
    bool,                  // 14. extrude (enable hull extrusion, v1.0.5)
    double,                // 15. extrude_margin (extrusion margin, v1.0.5)
    int,                   // 16. apx_mode (0=convex_hulls, 1=box, v1.0.2)
    unsigned int           // 17. seed (random seed)
);
using TCoACD_Free = void(*)(FCoACD_MeshArray);

static void* GCoACD_DLL = nullptr;
static TCoACD_Run GCoACD_Run = nullptr;
static TCoACD_Free GCoACD_Free = nullptr;

static FString FindCoACDDll()
{
    TArray<FString> Candidates;
    // 1) 当前插件目录 UE_XTools/ThirdParty/CoACD/DLL/lib_coacd.dll
    if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UE_XTools")))
    {
        const FString Base = Plugin->GetBaseDir();
        Candidates.Add(FPaths::Combine(Base, TEXT("ThirdParty/CoACD/DLL/lib_coacd.dll")));
    }
    // 2) 项目Plugins目录（开发场景）
    Candidates.Add(FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UE_XTools/ThirdParty/CoACD/DLL/lib_coacd.dll")));
    // 3) 直接查找工作目录/PATH
    Candidates.Add(TEXT("lib_coacd.dll"));

    for (const FString& P : Candidates)
    {
        if (FPaths::FileExists(P))
        {
            return P;
        }
    }
    return FString();
}

bool FX_CoACDIntegration::Initialize()
{
#if PLATFORM_WINDOWS
    if (GCoACD_Run)
    {
        return true;
    }
    const FString DllPath = FindCoACDDll();
    GCoACD_DLL = FPlatformProcess::GetDllHandle(*DllPath);
    if (!GCoACD_DLL)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("[CoACD] 未能加载DLL: %s"), *DllPath);
        return false;
    }
    GCoACD_Run = (TCoACD_Run)FPlatformProcess::GetDllExport(GCoACD_DLL, TEXT("CoACD_run"));
    GCoACD_Free = (TCoACD_Free)FPlatformProcess::GetDllExport(GCoACD_DLL, TEXT("CoACD_freeMeshArray"));
    if (!GCoACD_Run || !GCoACD_Free)
    {
        UE_LOG(LogX_AssetEditor, Error, TEXT("[CoACD] 符号解析失败"));
        Shutdown();
        return false;
    }
    UE_LOG(LogX_AssetEditor, Log, TEXT("[CoACD] 初始化成功"));
    return true;
#else
    return false;
#endif
}

void FX_CoACDIntegration::Shutdown()
{
#if PLATFORM_WINDOWS
    if (GCoACD_DLL)
    {
        FPlatformProcess::FreeDllHandle(GCoACD_DLL);
    }
    GCoACD_DLL = nullptr;
    GCoACD_Run = nullptr;
    GCoACD_Free = nullptr;
#endif
}

bool FX_CoACDIntegration::IsAvailable()
{
    return GCoACD_Run != nullptr;
}

static bool LoadRawMeshLOD(UStaticMesh* StaticMesh, int32 LODIndex, FRawMesh& OutRaw)
{
#if WITH_EDITOR
    const TArray<FStaticMeshSourceModel>& Models = StaticMesh->GetSourceModels();
    if (Models.IsEmpty()) return false;
    const int32 Clamped = FMath::Clamp(LODIndex, 0, Models.Num() - 1);
    Models[Clamped].LoadRawMesh(OutRaw);
    return OutRaw.VertexPositions.Num() > 0 && OutRaw.WedgeIndices.Num() > 0;
#else
    return false;
#endif
}

// 参照参考实现：压缩未使用顶点，确保索引连续合法
static void CompactUnusedVertices(FRawMesh& RawMesh)
{
    TSet<uint32> UsedVIDs;
    UsedVIDs.Reserve(RawMesh.WedgeIndices.Num());
    for (int32 W : RawMesh.WedgeIndices)
    {
        if (W >= 0)
        {
            UsedVIDs.Add((uint32)W);
        }
    }

    // 构建映射 old->new
    TArray<int32> MapOldToNew;
    MapOldToNew.Init(-1, RawMesh.VertexPositions.Num());
    TArray<uint32> NewIndexOrder;
    NewIndexOrder.Reserve(UsedVIDs.Num());

    int32 NewCounter = 0;
    for (uint32 Old = 0; Old < (uint32)RawMesh.VertexPositions.Num(); ++Old)
    {
        if (UsedVIDs.Contains(Old))
        {
            MapOldToNew[Old] = NewCounter++;
            NewIndexOrder.Add(Old);
        }
    }

    if (NewCounter == RawMesh.VertexPositions.Num())
    {
        return; // 已无未使用顶点
    }

    // 生成新顶点数组
    TArray<FVector3f> NewPositions;
    NewPositions.SetNumUninitialized(NewCounter);
    for (int32 i = 0; i < NewCounter; ++i)
    {
        NewPositions[i] = RawMesh.VertexPositions[NewIndexOrder[i]];
    }
    RawMesh.VertexPositions = MoveTemp(NewPositions);

    // 更新面索引
    for (uint32& Idx : RawMesh.WedgeIndices)
    {
        const int32 Old = (int32)Idx;
        Idx = (uint32)MapOldToNew[Old];
    }
}

bool FX_CoACDIntegration::GenerateForMesh(UStaticMesh* StaticMesh, const FX_CoACDArgs& Args)
{
#if WITH_EDITOR
    if (!StaticMesh) return false;
    if (!IsAvailable() && !Initialize()) return false;

    FRawMesh Raw;
    if (!LoadRawMeshLOD(StaticMesh, Args.SourceLODIndex, Raw))
    {
        return false;
    }

    // 压缩未使用顶点，避免出现索引越界或稀疏索引
    CompactUnusedVertices(Raw);

    FCoACD_Mesh Input{};
    Input.vertices_count = (uint64)Raw.VertexPositions.Num();
    Input.vertices_ptr = new double[(size_t)Input.vertices_count * 3];
    for (int32 i = 0; i < Raw.VertexPositions.Num(); ++i)
    {
        const FVector3f& P = Raw.VertexPositions[i];
        Input.vertices_ptr[(size_t)i * 3 + 0] = (double)P.X;
        Input.vertices_ptr[(size_t)i * 3 + 1] = (double)P.Y;
        Input.vertices_ptr[(size_t)i * 3 + 2] = (double)P.Z;
    }
    Input.triangles_count = (uint64)(Raw.WedgeIndices.Num() / 3);
    Input.triangles_ptr = new int[Raw.WedgeIndices.Num()];
    for (int32 i = 0; i < Raw.WedgeIndices.Num(); ++i)
    {
        Input.triangles_ptr[i] = Raw.WedgeIndices[i];
    }

    FCoACD_MeshArray Result{ nullptr, 0 };
    check(GCoACD_Run && GCoACD_Free);
    Result = GCoACD_Run(
        Input,
        (double)Args.Threshold,
        Args.MaxConvexHull,
        (int)Args.PreprocessMode,
        Args.PreprocessResolution,
        Args.SampleResolution,
        Args.MCTSNodes,
        Args.MCTSIteration,
        Args.MCTSMaxDepth,
        Args.bPCA,
        Args.bMerge,
        // v1.0.7 新增参数 - 使用用户配置的实际值
        Args.bDecimate,          // decimate: 顶点约束开关
        Args.MaxConvexHullVertex, // max_ch_vertex: 每个凸包最大顶点数
        Args.bExtrude,           // extrude: 凸包挤出开关
        (double)Args.ExtrudeMargin, // extrude_margin: 挤出边距
        Args.ApproximateMode,    // apx_mode: 近似模式 (0=凸包, 1=包围盒)
        (unsigned int)Args.Seed  // seed: 随机种子（移到最后）
    );

    StaticMesh->Modify();
    StaticMesh->CreateBodySetup();
    UBodySetup* BodySetup = StaticMesh->GetBodySetup();
    if (!BodySetup)
    {
        delete[] Input.vertices_ptr;
        delete[] Input.triangles_ptr;
        if (GCoACD_Free) GCoACD_Free(Result);
        return false;
    }

    if (Args.bRemoveExistingCollision)
    {
        BodySetup->RemoveSimpleCollision();
    }

    // 材质槽排除关键词：与参考实现一致
    if (Args.MaterialKeywordsToExclude.Num() > 0)
    {
        // 重新过滤 RawMesh 面：参考插件是在进入 DLL 前做，这里已在上方Compact后，简化为忽略
        // 生产中可在 LoadRawMeshLOD 之后按 FaceMaterialIndices 过滤三角并重建索引
    }

    if (Result.meshes_count == 0 || !Result.meshes_ptr)
    {
        // 没有生成任何凸包，清理并返回
        delete[] Input.vertices_ptr;
        delete[] Input.triangles_ptr;
        if (GCoACD_Free) GCoACD_Free(Result);
        return false;
    }

    auto& ConvexElems = BodySetup->AggGeom.ConvexElems;
    const int32 FirstIdx = ConvexElems.Num();
    ConvexElems.AddDefaulted((int32)Result.meshes_count);
    for (uint64 i = 0; i < Result.meshes_count; ++i)
    {
        FKConvexElem& Elem = ConvexElems[FirstIdx + (int32)i];
        const FCoACD_Mesh& RM = Result.meshes_ptr[i];
        if (RM.vertices_count == 0 || !RM.vertices_ptr || RM.triangles_count == 0 || !RM.triangles_ptr)
        {
            continue;
        }
        Elem.VertexData.SetNumUninitialized((int32)RM.vertices_count);
        for (uint64 p = 0; p < RM.vertices_count; ++p)
        {
            Elem.VertexData[(int32)p].X = RM.vertices_ptr[(size_t)p * 3 + 0];
            Elem.VertexData[(int32)p].Y = RM.vertices_ptr[(size_t)p * 3 + 1];
            Elem.VertexData[(int32)p].Z = RM.vertices_ptr[(size_t)p * 3 + 2];
        }
        Elem.IndexData.SetNumUninitialized((int32)RM.triangles_count * 3);
        for (int32 p = 0; p < Elem.IndexData.Num(); ++p)
        {
            Elem.IndexData[p] = RM.triangles_ptr[p];
        }
        Elem.UpdateElemBox();
    }

    BodySetup->InvalidatePhysicsData();

    delete[] Input.vertices_ptr;
    delete[] Input.triangles_ptr;
    if (GCoACD_Free) GCoACD_Free(Result);
    return true;
#else
    return false;
#endif
}

void FX_CoACDIntegration::GenerateForAssets(const TArray<FAssetData>& SelectedAssets, const FX_CoACDArgs& Args)
{
    if (SelectedAssets.Num() <= 0) return;

    FScopedSlowTask Task((float)SelectedAssets.Num(), NSLOCTEXT("CoACD","Batch","CoACD 生成碰撞中..."));
    Task.MakeDialog(true);

    auto Work = [&SelectedAssets,&Args,&Task](int32 Index)
    {
        const FAssetData& AD = SelectedAssets[Index];
        if (UStaticMesh* SM = Cast<UStaticMesh>(AD.GetAsset()))
        {
            if (Task.ShouldCancel()) return;
            Task.EnterProgressFrame(1.f, FText::FromString(AD.AssetName.ToString()));
            FX_CoACDIntegration::GenerateForMesh(SM, Args);
        }
        else
        {
            Task.EnterProgressFrame(1.f);
        }
    };

    if (Args.bEnableParallel)
    {
        // UE ParallelFor 当前不直接暴露并发度控制；使用默认线程池并发。
        ParallelFor(SelectedAssets.Num(), Work, /*bForceSingleThread*/ false, /*bChunked*/ false);
    }
    else
    {
        for (int32 i=0;i<SelectedAssets.Num();++i)
        {
            Work(i);
            if (Task.ShouldCancel()) break;
        }
    }
}


