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
// 为了将耗时任务放到线程池并在主线程刷新进度
#include "Async/Async.h"

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

// 适配 CoACD v1.0.7 DLL 的公开 API 签名（17 参数版本，包含 decimate/extrude/approx 等）
using TCoACD_Run = FCoACD_MeshArray(*) (
    const FCoACD_Mesh&,    // 1. 输入网格
    double,                // 2. threshold
    int,                   // 3. max_convex_hull
    int,                   // 4. preprocess_mode
    int,                   // 5. prep_resolution
    int,                   // 6. sample_resolution
    int,                   // 7. mcts_nodes
    int,                   // 8. mcts_iteration
    int,                   // 9. mcts_max_depth
    bool,                  // 10. pca
    bool,                  // 11. merge
    bool,                  // 12. decimate (v1.0.3+)
    int,                   // 13. max_ch_vertex (v1.0.3+)
    bool,                  // 14. extrude (v1.0.5+)
    double,                // 15. extrude_margin (v1.0.5+)
    int,                   // 16. approximate_mode (v1.0.2+)
    unsigned int           // 17. seed
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

// 轻量输入承载：TArray 承载真实数据，MeshView 仅指向其内存
struct FCoACDInputBuffers
{
    TArray<double> Vertices;   // size = NumVerts * 3
    TArray<int> Indices;       // size = NumWedges
    FCoACD_Mesh MeshView{};    // 指向上面两块内存
};

static void BuildInputFromRawMesh(const FRawMesh& Raw, FCoACDInputBuffers& Out)
{
    Out.Vertices.SetNumUninitialized(Raw.VertexPositions.Num() * 3);
    for (int32 i = 0; i < Raw.VertexPositions.Num(); ++i)
    {
        const FVector3f& P = Raw.VertexPositions[i];
        Out.Vertices[i*3+0] = (double)P.X;
        Out.Vertices[i*3+1] = (double)P.Y;
        Out.Vertices[i*3+2] = (double)P.Z;
    }
    Out.Indices.SetNumUninitialized(Raw.WedgeIndices.Num());
    for (int32 i = 0; i < Raw.WedgeIndices.Num(); ++i)
    {
        Out.Indices[i] = Raw.WedgeIndices[i];
    }
    Out.MeshView.vertices_count = (uint64)Raw.VertexPositions.Num();
    Out.MeshView.vertices_ptr = Out.Vertices.GetData();
    Out.MeshView.triangles_count = (uint64)(Raw.WedgeIndices.Num() / 3);
    Out.MeshView.triangles_ptr = Out.Indices.GetData();
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

// 根据材质槽ID移除对应三角面（以及相关的楔数据），参考 Docs/ref/CoACD 实现
static bool DeleteWedgesByMaterialIDs(FRawMesh& RawMesh, const TArray<int32>& MaterialIDs, bool bCleanUpVertexPositions)
{
    if (!MaterialIDs.ContainsByPredicate([](int32 i){ return i >= 0; }))
    {
        return false;
    }

    bool bModified = false;
    for (int32 FaceIdx = RawMesh.FaceMaterialIndices.Num() - 1; FaceIdx >= 0; --FaceIdx)
    {
        if (MaterialIDs.Contains(RawMesh.FaceMaterialIndices[FaceIdx]))
        {
            RawMesh.FaceMaterialIndices.RemoveAt(FaceIdx);
            RawMesh.FaceSmoothingMasks.RemoveAt(FaceIdx);

            for (int32 j = 2; j >= 0; --j)
            {
                const int32 WedgeK = 3 * FaceIdx + j;
                if (RawMesh.WedgeColors.IsValidIndex(WedgeK)) RawMesh.WedgeColors.RemoveAt(WedgeK);
                if (RawMesh.WedgeIndices.IsValidIndex(WedgeK)) RawMesh.WedgeIndices.RemoveAt(WedgeK);
                if (RawMesh.WedgeTangentX.IsValidIndex(WedgeK)) RawMesh.WedgeTangentX.RemoveAt(WedgeK);
                if (RawMesh.WedgeTangentY.IsValidIndex(WedgeK)) RawMesh.WedgeTangentY.RemoveAt(WedgeK);
                if (RawMesh.WedgeTangentZ.IsValidIndex(WedgeK)) RawMesh.WedgeTangentZ.RemoveAt(WedgeK);
                for (int32 m = 0; m < MAX_MESH_TEXTURE_COORDS; ++m)
                {
                    if (RawMesh.WedgeTexCoords[m].IsValidIndex(WedgeK)) RawMesh.WedgeTexCoords[m].RemoveAt(WedgeK);
                }
            }
            bModified = true;
        }
    }

    if (bModified && bCleanUpVertexPositions)
    {
        TSet<uint32> UsedVIDs(RawMesh.WedgeIndices);
        TArray<uint32> UnusedVIDs;
        UnusedVIDs.Reserve(RawMesh.VertexPositions.Num());
        for (int32 i = RawMesh.VertexPositions.Num() - 1; i >= 0; --i)
        {
            if (!UsedVIDs.Contains((uint32)i)) UnusedVIDs.Add((uint32)i);
        }
        for (uint32 ID : UnusedVIDs)
        {
            RawMesh.VertexPositions.RemoveAt((int32)ID);
            for (uint32& WID : RawMesh.WedgeIndices)
            {
                ensure(WID != ID);
                if (WID > ID) WID -= 1;
            }
        }
    }
    return bModified;
}

static void FilterRawMeshByKeywords(UStaticMesh* StaticMesh, FRawMesh& Raw, const TArray<FString>& Keywords)
{
    if (Keywords.Num() <= 0) return;
    TArray<int32> BlacklistMaterialIDs;
    const TArray<FStaticMaterial>& StaticMats = StaticMesh->GetStaticMaterials();
    for (int32 Idx = 0; Idx < StaticMats.Num(); ++Idx)
    {
        const FString SlotNameStr = StaticMats[Idx].MaterialSlotName.ToString();
        const UMaterialInterface* Mat = StaticMats[Idx].MaterialInterface;
        const FString MatNameStr = Mat ? Mat->GetName() : FString();
        const FString MatPathStr = Mat ? Mat->GetPathName() : FString();
        const FString ElementStrEn = FString::Printf(TEXT("Element %d"), Idx);
        const FString ElementStrZh = FString::Printf(TEXT("元素%d"), Idx);
        const FString IndexStr = FString::FromInt(Idx);

        for (const FString& KWRaw : Keywords)
        {
            const FString KW = KWRaw.TrimStartAndEnd();
            if (KW.IsEmpty()) continue;

            bool bMatch = false;
            bMatch |= SlotNameStr.Contains(KW, ESearchCase::IgnoreCase);
            bMatch |= MatNameStr.Contains(KW, ESearchCase::IgnoreCase);
            bMatch |= MatPathStr.Contains(KW, ESearchCase::IgnoreCase);
            bMatch |= ElementStrEn.Contains(KW, ESearchCase::IgnoreCase);
            bMatch |= ElementStrZh.Contains(KW, ESearchCase::IgnoreCase);
            bMatch |= (KW.Equals(IndexStr, ESearchCase::IgnoreCase));
            if (bMatch)
            {
                BlacklistMaterialIDs.Add(Idx);
                break;
            }
        }
    }
    if (BlacklistMaterialIDs.Num() > 0)
    {
        DeleteWedgesByMaterialIDs(Raw, BlacklistMaterialIDs, /*bCleanUpVertexPositions*/ true);
    }
}

static bool ApplyResultToBodySetup(UStaticMesh* StaticMesh, const FCoACD_MeshArray& Result, bool bRemoveExistingCollision)
{
    StaticMesh->Modify();
    StaticMesh->CreateBodySetup();
    UBodySetup* BodySetup = StaticMesh->GetBodySetup();
    if (!BodySetup) return false;
    if (bRemoveExistingCollision)
    {
        BodySetup->RemoveSimpleCollision();
    }
    if (Result.meshes_count == 0 || !Result.meshes_ptr) return false;

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
    return true;
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

    // 根据材质槽关键词过滤三角面（可选）
    if (Args.MaterialKeywordsToExclude.Num() > 0)
    {
        TArray<int32> BlacklistMaterialIDs;
        const TArray<FStaticMaterial>& StaticMats = StaticMesh->GetStaticMaterials();
        for (int32 Idx = 0; Idx < StaticMats.Num(); ++Idx)
        {
            const FString SlotNameStr = StaticMats[Idx].MaterialSlotName.ToString();
            const UMaterialInterface* Mat = StaticMats[Idx].MaterialInterface;
            const FString MatNameStr = Mat ? Mat->GetName() : FString();
            const FString MatPathStr = Mat ? Mat->GetPathName() : FString();
            const FString ElementStrEn = FString::Printf(TEXT("Element %d"), Idx);
            const FString ElementStrZh = FString::Printf(TEXT("元素%d"), Idx);
            const FString IndexStr = FString::FromInt(Idx);

            for (const FString& KWRaw : Args.MaterialKeywordsToExclude)
            {
                const FString KW = KWRaw.TrimStartAndEnd();
                if (KW.IsEmpty()) continue;

                bool bMatch = false;
                // 支持按槽名、材质资产名、资产路径匹配
                bMatch |= SlotNameStr.Contains(KW, ESearchCase::IgnoreCase);
                bMatch |= MatNameStr.Contains(KW, ESearchCase::IgnoreCase);
                bMatch |= MatPathStr.Contains(KW, ESearchCase::IgnoreCase);
                // 支持按“Element 0 / 元素0 / 索引数字”匹配
                bMatch |= ElementStrEn.Contains(KW, ESearchCase::IgnoreCase);
                bMatch |= ElementStrZh.Contains(KW, ESearchCase::IgnoreCase);
                bMatch |= (KW.Equals(IndexStr, ESearchCase::IgnoreCase));

                if (bMatch)
                {
                    BlacklistMaterialIDs.Add(Idx);
                    break;
                }
            }
        }
        if (BlacklistMaterialIDs.Num() > 0)
        {
            DeleteWedgesByMaterialIDs(Raw, BlacklistMaterialIDs, /*bCleanUpVertexPositions*/ true);
        }
    }

    // 过滤 + 压缩
    FilterRawMeshByKeywords(StaticMesh, Raw, Args.MaterialKeywordsToExclude);
    CompactUnusedVertices(Raw);

    FCoACDInputBuffers InputBuf;
    BuildInputFromRawMesh(Raw, InputBuf);

    FCoACD_MeshArray Result{ nullptr, 0 };
    check(GCoACD_Run && GCoACD_Free);
    Result = GCoACD_Run(
        InputBuf.MeshView,
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
        Args.bDecimate,
        Args.MaxConvexHullVertex,
        Args.bExtrude,
        (double)Args.ExtrudeMargin,
        Args.ApproximateMode,
        (unsigned int)Args.Seed
    );

    if (!ApplyResultToBodySetup(StaticMesh, Result, Args.bRemoveExistingCollision))
    {
        if (GCoACD_Free) GCoACD_Free(Result);
        return false;
    }

    // 注：材质槽过滤已在进入 DLL 之前完成

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

    auto WorkMainThread = [&SelectedAssets,&Args,&Task](int32 Index)
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

    // 单资产：提供动态进度（在游戏线程准备数据与回写；仅将 DLL 计算放到线程池）
    if (SelectedAssets.Num() == 1)
    {
        const FAssetData& AD = SelectedAssets[0];
        if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(AD.GetAsset()))
        {
            if (!IsAvailable() && !Initialize()) return;

            // 进度框（分阶段：加载10% + 过滤10% + 构建输入20% + 求解50% + 写回10%）
            FScopedSlowTask Inner(100.f, NSLOCTEXT("CoACD","Single","CoACD 处理中..."));
            Inner.MakeDialog(true);

            float Shown = 0.f;
            auto Advance = [&Inner,&Shown](float Delta, const FText& Phase){ Shown += Delta; Inner.EnterProgressFrame(Delta, Phase); };
            auto BriefPaint = [&Inner](){
                const double Start = FPlatformTime::Seconds();
                while ((FPlatformTime::Seconds() - Start) < 0.2)
                {
                    FPlatformProcess::Sleep(0.02f);
                    Inner.EnterProgressFrame(0.f);
                }
            };
            Advance(10.f, NSLOCTEXT("CoACD","PhaseLoad","加载与校验网格..."));
            BriefPaint();

            // 读取 RawMesh（游戏线程）
            FRawMesh Raw;
            if (!LoadRawMeshLOD(StaticMesh, Args.SourceLODIndex, Raw))
            {
                return;
            }

            // 材质过滤（游戏线程）
            if (Args.MaterialKeywordsToExclude.Num() > 0)
            {
                TArray<int32> BlacklistMaterialIDs;
                const TArray<FStaticMaterial>& StaticMats = StaticMesh->GetStaticMaterials();
                for (int32 Idx = 0; Idx < StaticMats.Num(); ++Idx)
                {
                    const FString SlotNameStr = StaticMats[Idx].MaterialSlotName.ToString();
                    const UMaterialInterface* Mat = StaticMats[Idx].MaterialInterface;
                    const FString MatNameStr = Mat ? Mat->GetName() : FString();
                    const FString MatPathStr = Mat ? Mat->GetPathName() : FString();
                    const FString ElementStrEn = FString::Printf(TEXT("Element %d"), Idx);
                    const FString ElementStrZh = FString::Printf(TEXT("元素%d"), Idx);
                    const FString IndexStr = FString::FromInt(Idx);

                    for (const FString& KWRaw : Args.MaterialKeywordsToExclude)
                    {
                        const FString KW = KWRaw.TrimStartAndEnd();
                        if (KW.IsEmpty()) continue;

                        bool bMatch = false;
                        bMatch |= SlotNameStr.Contains(KW, ESearchCase::IgnoreCase);
                        bMatch |= MatNameStr.Contains(KW, ESearchCase::IgnoreCase);
                        bMatch |= MatPathStr.Contains(KW, ESearchCase::IgnoreCase);
                        bMatch |= ElementStrEn.Contains(KW, ESearchCase::IgnoreCase);
                        bMatch |= ElementStrZh.Contains(KW, ESearchCase::IgnoreCase);
                        bMatch |= (KW.Equals(IndexStr, ESearchCase::IgnoreCase));

                        if (bMatch)
                        {
                            BlacklistMaterialIDs.Add(Idx);
                            break;
                        }
                    }
                }
                if (BlacklistMaterialIDs.Num() > 0)
                {
                    DeleteWedgesByMaterialIDs(Raw, BlacklistMaterialIDs, /*bCleanUpVertexPositions*/ true);
                }
            }
            Advance(10.f, NSLOCTEXT("CoACD","PhaseFilter","材质过滤..."));
            BriefPaint();

            // 压缩未使用顶点（游戏线程）
            CompactUnusedVertices(Raw);

            // 构建 DLL 输入（Plain 数据，可跨线程使用）
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

            Advance(20.f, NSLOCTEXT("CoACD","PhaseBuild","构建输入数据..."));
            BriefPaint();

            // 后台计算（仅 DLL 调用）
            FThreadSafeBool bDone(false);
            FCoACD_MeshArray Result{ nullptr, 0 };
            Async(EAsyncExecution::ThreadPool, [&Args,&Input,&Result,&bDone]()
            {
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
                    Args.bDecimate,
                    Args.MaxConvexHullVertex,
                    Args.bExtrude,
                    (double)Args.ExtrudeMargin,
                    Args.ApproximateMode,
                    (unsigned int)Args.Seed
                );
                bDone = true;
            });

            // 进度循环（游戏线程） - 求解阶段 50%
            const float SolveTarget = Shown + 50.f;
            while (!bDone)
            {
                if (Task.ShouldCancel()) break;
                const float Step = 0.5f;
                if (Shown + Step <= SolveTarget)
                {
                    Advance(Step, NSLOCTEXT("CoACD","PhaseSolve","CoACD 求解中..."));
    }
    else
    {
                    Inner.EnterProgressFrame(0.f, NSLOCTEXT("CoACD","PhaseSolve","CoACD 求解中..."));
                }
                FPlatformProcess::Sleep(0.03f);
            }

            // 回写结果（游戏线程）
            StaticMesh->Modify();
            StaticMesh->CreateBodySetup();
            UBodySetup* BodySetup = StaticMesh->GetBodySetup();
            if (BodySetup)
            {
                if (Args.bRemoveExistingCollision)
                {
                    BodySetup->RemoveSimpleCollision();
                }

                if (Result.meshes_count > 0 && Result.meshes_ptr)
                {
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
                }
            }

            // 释放资源
            if (GCoACD_Free) GCoACD_Free(Result);
            if (GCoACD_Free) GCoACD_Free(Result);

            // 写回阶段 余量（大约 10%）
            if (Shown < 100.f)
            {
                Advance(100.f - Shown, NSLOCTEXT("CoACD","PhaseWrite","写回结果..."));
            }
        }
        else
        {
            Task.EnterProgressFrame(1.f);
        }
        return;
    }

    // 多资产：顺序处理（游戏线程），避免 UObject 跨线程
    for (int32 i=0;i<SelectedAssets.Num();++i)
    {
        WorkMainThread(i);
        if (Task.ShouldCancel()) break;
    }
}


