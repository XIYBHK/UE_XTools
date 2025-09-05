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
#include "RawMesh.h"

// UE平台抽象层
#include "HAL/PlatformProcess.h" // may be needed by progress helpers sleep; keep FPlatformProcess
#include "Misc/ScopedSlowTask.h"

// UE插件和工具系统
#include "CollisionTools/X_CoACDMeshOps.h"
#include "CollisionTools/X_CoACDResultApply.h"

// UE异步和并发系统
// 为了将耗时任务放到线程池并在主线程刷新进度
#include "Async/Async.h"


// 定义日志类别
DEFINE_LOG_CATEGORY_STATIC(LogX_AssetEditor, Log, All);

// 适配层：仅引用接口，不再在此维护 DLL 句柄/符号
#include "CollisionTools/X_CoACDAdapter.h"

#if WITH_EDITOR
namespace
{
    FORCEINLINE void CoACD_AdvanceProgress(FScopedSlowTask& Inner, float& Shown, float Delta, const FText& Phase)
    {
        Shown += Delta;
        Inner.EnterProgressFrame(Delta, Phase);
    }

    FORCEINLINE void CoACD_BriefPaint(FScopedSlowTask& Inner)
    {
        const double Start = FPlatformTime::Seconds();
        while ((FPlatformTime::Seconds() - Start) < 0.2)
        {
            FPlatformProcess::Sleep(0.02f);
            Inner.EnterProgressFrame(0.f);
        }
    }
}
#endif

// Mesh操作已移至 X_CoACDMeshOps

bool FX_CoACDIntegration::Initialize()
{
    if (!CoACD::IsAvailable())
    {
        if (!CoACD::Initialize())
        {
            UE_LOG(LogX_AssetEditor, Warning, TEXT("[CoACD] 初始化失败"));
        return false;
    }
    }
    UE_LOG(LogX_AssetEditor, Log, TEXT("[CoACD] 初始化成功"));
    return true;
}

void FX_CoACDIntegration::Shutdown()
{
    CoACD::Shutdown();
}

bool FX_CoACDIntegration::IsAvailable()
{
    return CoACD::IsAvailable();
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

// 旧的本地实现已移除：使用 X_CoACDMeshOps / X_CoACDResultApply 提供的实现

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

    // 材质过滤统一由 FilterRawMeshByKeywords 处理，避免重复过滤

    // 过滤 + 压缩
    FilterRawMeshByKeywords(StaticMesh, Raw, Args.MaterialKeywordsToExclude);
    CompactUnusedVertices(Raw);

    FCoACDInputBuffers InputBuf;
    BuildInputFromRawMesh(Raw, InputBuf);

    FCoACD_MeshArray Result{ nullptr, 0 };
    check(CoACD::GetRun() && CoACD::GetFree());
    Result = CoACD::GetRun()(InputBuf.MeshView,
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
        if (CoACD::GetFree()) CoACD::GetFree()(Result);
        return false;
    }

    // 注：材质槽过滤已在进入 DLL 之前完成

    if (CoACD::GetFree()) CoACD::GetFree()(Result);
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
            CoACD_AdvanceProgress(Inner, Shown, 10.f, NSLOCTEXT("CoACD","PhaseLoad","加载与校验网格..."));
            CoACD_BriefPaint(Inner);

            // 读取 RawMesh（游戏线程）
            FRawMesh Raw;
            if (!LoadRawMeshLOD(StaticMesh, Args.SourceLODIndex, Raw))
            {
                return;
            }

            // 材质过滤（统一调用工具函数）
            FilterRawMeshByKeywords(StaticMesh, Raw, Args.MaterialKeywordsToExclude);
            CoACD_AdvanceProgress(Inner, Shown, 10.f, NSLOCTEXT("CoACD","PhaseFilter","材质过滤..."));
            CoACD_BriefPaint(Inner);

            // 压缩未使用顶点（游戏线程）
            CompactUnusedVertices(Raw);

            // 构建 DLL 输入（使用 TArray 承载，避免手动 new[]/delete[]）
            FCoACDInputBuffers InputBuf;
            BuildInputFromRawMesh(Raw, InputBuf);

            CoACD_AdvanceProgress(Inner, Shown, 20.f, NSLOCTEXT("CoACD","PhaseBuild","构建输入数据..."));
            CoACD_BriefPaint(Inner);

            // 后台计算（仅 DLL 调用）
            FThreadSafeBool bDone(false);
            FCoACD_MeshArray Result{ nullptr, 0 };
            Async(EAsyncExecution::ThreadPool, [&Args,&InputBuf,&Result,&bDone]()
            {
                Result = CoACD::GetRun()(InputBuf.MeshView,
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
                    (unsigned int)Args.Seed);
                bDone = true;
            });

            // 进度循环（游戏线程） - 求解阶段 50%
            const float SolveStart = Shown;
            const float SolveWeight = 50.f;
            const double SolvePhaseStartTime = FPlatformTime::Seconds();
            const double ExpectedMinSolveSeconds = 3.5; // 自适应最短可视时长（避免强制等待）
            while (!bDone)
            {
                if (Task.ShouldCancel()) break;
                const double Elapsed = FPlatformTime::Seconds() - SolvePhaseStartTime;
                const float Frac = FMath::Clamp((float)(Elapsed / ExpectedMinSolveSeconds), 0.f, 0.98f);
                const float Target = SolveStart + Frac * SolveWeight;
                if (Target > Shown)
                {
                    const int32 Percent = FMath::Clamp((int32)FMath::RoundToInt((Target / 100.f) * 100.f), 0, 99);
                    CoACD_AdvanceProgress(Inner, Shown, Target - Shown,
                        FText::Format(NSLOCTEXT("CoACD","PhaseSolveFmt","CoACD 求解中 {0}%"), FText::AsNumber(Percent)));
                }
                else
                {
                    Inner.EnterProgressFrame(0.f, NSLOCTEXT("CoACD","PhaseSolve","CoACD 求解中"));
                }
                FPlatformProcess::Sleep(0.05f);
            }
            if (Shown < SolveStart + SolveWeight)
            {
                CoACD_AdvanceProgress(Inner, Shown, (SolveStart + SolveWeight) - Shown, NSLOCTEXT("CoACD","PhaseSolveDone","CoACD 求解完成"));
            }
            
            // 回写结果（游戏线程）
            if (!ApplyResultToBodySetup(StaticMesh, Result, Args.bRemoveExistingCollision))
            {
                if (CoACD::GetFree()) CoACD::GetFree()(Result);
                return;
            }

            // 释放资源
            if (CoACD::GetFree()) CoACD::GetFree()(Result);

            // 写回阶段 余量（大约 10%）
            if (Shown < 100.f)
            {
                CoACD_AdvanceProgress(Inner, Shown, 100.f - Shown, NSLOCTEXT("CoACD","PhaseWrite","写回结果..."));
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


