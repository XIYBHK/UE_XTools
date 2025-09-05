// Copyright Epic Games, Inc. All Rights Reserved.

#include "CollisionTools/X_CoACDConfigManager.h"
#include "Misc/ConfigCacheIni.h"
#include "CollisionTools/X_CoACDIntegration.h" // 仅在cpp中包含以获得FX_CoACDArgs定义

// 质量预设集中实现
void FX_CoACDConfigManager::ApplyPresetQuality1(FX_CoACDArgs& A)
{
    A.Threshold = 0.10f; A.PreprocessMode = EX_CoACDPreprocessMode::Off; A.PreprocessResolution = 50; A.SampleResolution = 2000;
    A.MCTSNodes = 20; A.MCTSIteration = 100; A.MCTSMaxDepth = 2; A.bPCA = false; A.bMerge = true; A.MaxConvexHull = -1;
    A.Seed = 0; A.SourceLODIndex = 0; A.bRemoveExistingCollision = true;
}

void FX_CoACDConfigManager::ApplyPresetQuality2(FX_CoACDArgs& A)
{
    // 按用户提供截图更新的默认预设2
    A.Threshold = 0.07f; A.PreprocessMode = EX_CoACDPreprocessMode::On; A.PreprocessResolution = 40; A.SampleResolution = 1500;
    A.MCTSNodes = 16; A.MCTSIteration = 100; A.MCTSMaxDepth = 2; A.bPCA = false; A.bMerge = true; A.MaxConvexHull = 32;
    A.MaxConvexHullVertex = 256; A.bExtrude = false; A.ExtrudeMargin = 0.01f; A.ApproximateMode = 0;
    A.Seed = 0; A.SourceLODIndex = 0; A.bRemoveExistingCollision = true;
}

void FX_CoACDConfigManager::ApplyPresetQuality3(FX_CoACDArgs& A)
{
    A.Threshold = 0.05f; A.PreprocessMode = EX_CoACDPreprocessMode::On; A.PreprocessResolution = 50; A.SampleResolution = 2000;
    A.MCTSNodes = 20; A.MCTSIteration = 150; A.MCTSMaxDepth = 3; A.bPCA = false; A.bMerge = true; A.MaxConvexHull = -1;
    A.Seed = 0; A.SourceLODIndex = 0; A.bRemoveExistingCollision = true;
}

void FX_CoACDConfigManager::ApplyPresetQuality4(FX_CoACDArgs& A)
{
    A.Threshold = 0.03f; A.PreprocessMode = EX_CoACDPreprocessMode::On; A.PreprocessResolution = 50; A.SampleResolution = 2000;
    A.MCTSNodes = 20; A.MCTSIteration = 200; A.MCTSMaxDepth = 4; A.bPCA = false; A.bMerge = true; A.MaxConvexHull = -1;
    A.Seed = 0; A.SourceLODIndex = 0; A.bRemoveExistingCollision = true;
}

// 参数记忆：使用 EditorPerProjectIni 持久化上次设置
FX_CoACDArgs FX_CoACDConfigManager::LoadSaved()
{
    FX_CoACDArgs Out; // 带默认值
    const TCHAR* Section = GetConfigSection();

    float FloatVal; int32 IntVal; bool BoolVal; FString StrVal;

    if (GConfig->GetFloat(Section, TEXT("Threshold"), FloatVal, GEditorPerProjectIni)) Out.Threshold = FloatVal;
    if (GConfig->GetInt(Section, TEXT("PreprocessMode"), IntVal, GEditorPerProjectIni)) Out.PreprocessMode = (EX_CoACDPreprocessMode)IntVal;
    if (GConfig->GetInt(Section, TEXT("PreprocessResolution"), IntVal, GEditorPerProjectIni)) Out.PreprocessResolution = IntVal;
    if (GConfig->GetInt(Section, TEXT("SampleResolution"), IntVal, GEditorPerProjectIni)) Out.SampleResolution = IntVal;
    if (GConfig->GetInt(Section, TEXT("MCTSNodes"), IntVal, GEditorPerProjectIni)) Out.MCTSNodes = IntVal;
    if (GConfig->GetInt(Section, TEXT("MCTSIteration"), IntVal, GEditorPerProjectIni)) Out.MCTSIteration = IntVal;
    if (GConfig->GetInt(Section, TEXT("MCTSMaxDepth"), IntVal, GEditorPerProjectIni)) Out.MCTSMaxDepth = IntVal;
    if (GConfig->GetBool(Section, TEXT("bPCA"), BoolVal, GEditorPerProjectIni)) Out.bPCA = BoolVal;
    if (GConfig->GetBool(Section, TEXT("bMerge"), BoolVal, GEditorPerProjectIni)) Out.bMerge = BoolVal;
    if (GConfig->GetInt(Section, TEXT("MaxConvexHull"), IntVal, GEditorPerProjectIni)) Out.MaxConvexHull = IntVal;
    if (GConfig->GetInt(Section, TEXT("Seed"), IntVal, GEditorPerProjectIni)) Out.Seed = IntVal;

    // v1.0.7 扩展字段（即便当前 1.0.0 DLL 未用，也保留记忆）
    if (GConfig->GetBool(Section, TEXT("bDecimate"), BoolVal, GEditorPerProjectIni)) Out.bDecimate = BoolVal;
    if (GConfig->GetInt(Section, TEXT("MaxConvexHullVertex"), IntVal, GEditorPerProjectIni)) Out.MaxConvexHullVertex = IntVal;
    if (GConfig->GetBool(Section, TEXT("bExtrude"), BoolVal, GEditorPerProjectIni)) Out.bExtrude = BoolVal;
    if (GConfig->GetFloat(Section, TEXT("ExtrudeMargin"), FloatVal, GEditorPerProjectIni)) Out.ExtrudeMargin = FloatVal;
    if (GConfig->GetInt(Section, TEXT("ApproximateMode"), IntVal, GEditorPerProjectIni)) Out.ApproximateMode = IntVal;

    // 其它控制项
    if (GConfig->GetInt(Section, TEXT("SourceLODIndex"), IntVal, GEditorPerProjectIni)) Out.SourceLODIndex = IntVal;
    if (GConfig->GetBool(Section, TEXT("bRemoveExistingCollision"), BoolVal, GEditorPerProjectIni)) Out.bRemoveExistingCollision = BoolVal;
    if (GConfig->GetBool(Section, TEXT("bEnableParallel"), BoolVal, GEditorPerProjectIni)) Out.bEnableParallel = BoolVal;
    if (GConfig->GetInt(Section, TEXT("MaxConcurrency"), IntVal, GEditorPerProjectIni)) Out.MaxConcurrency = IntVal;

    if (GConfig->GetString(Section, TEXT("MaterialKeywordsToExclude"), StrVal, GEditorPerProjectIni))
    {
        TArray<FString> Lines; StrVal.ParseIntoArrayLines(Lines);
        if (Lines.Num() > 0) Out.MaterialKeywordsToExclude = MoveTemp(Lines);
    }
    return Out;
}

void FX_CoACDConfigManager::Save(const FX_CoACDArgs& In)
{
    const TCHAR* Section = GetConfigSection();

    GConfig->SetFloat(Section, TEXT("Threshold"), In.Threshold, GEditorPerProjectIni);
    GConfig->SetInt(Section, TEXT("PreprocessMode"), (int32)In.PreprocessMode, GEditorPerProjectIni);
    GConfig->SetInt(Section, TEXT("PreprocessResolution"), In.PreprocessResolution, GEditorPerProjectIni);
    GConfig->SetInt(Section, TEXT("SampleResolution"), In.SampleResolution, GEditorPerProjectIni);
    GConfig->SetInt(Section, TEXT("MCTSNodes"), In.MCTSNodes, GEditorPerProjectIni);
    GConfig->SetInt(Section, TEXT("MCTSIteration"), In.MCTSIteration, GEditorPerProjectIni);
    GConfig->SetInt(Section, TEXT("MCTSMaxDepth"), In.MCTSMaxDepth, GEditorPerProjectIni);
    GConfig->SetBool(Section, TEXT("bPCA"), In.bPCA, GEditorPerProjectIni);
    GConfig->SetBool(Section, TEXT("bMerge"), In.bMerge, GEditorPerProjectIni);
    GConfig->SetInt(Section, TEXT("MaxConvexHull"), In.MaxConvexHull, GEditorPerProjectIni);
    GConfig->SetInt(Section, TEXT("Seed"), In.Seed, GEditorPerProjectIni);

    // v1.0.7 扩展字段
    GConfig->SetBool(Section, TEXT("bDecimate"), In.bDecimate, GEditorPerProjectIni);
    GConfig->SetInt(Section, TEXT("MaxConvexHullVertex"), In.MaxConvexHullVertex, GEditorPerProjectIni);
    GConfig->SetBool(Section, TEXT("bExtrude"), In.bExtrude, GEditorPerProjectIni);
    GConfig->SetFloat(Section, TEXT("ExtrudeMargin"), In.ExtrudeMargin, GEditorPerProjectIni);
    GConfig->SetInt(Section, TEXT("ApproximateMode"), In.ApproximateMode, GEditorPerProjectIni);

    // 其它控制项
    GConfig->SetInt(Section, TEXT("SourceLODIndex"), In.SourceLODIndex, GEditorPerProjectIni);
    GConfig->SetBool(Section, TEXT("bRemoveExistingCollision"), In.bRemoveExistingCollision, GEditorPerProjectIni);
    GConfig->SetBool(Section, TEXT("bEnableParallel"), In.bEnableParallel, GEditorPerProjectIni);
    GConfig->SetInt(Section, TEXT("MaxConcurrency"), In.MaxConcurrency, GEditorPerProjectIni);

    const FString Joined = FString::Join(In.MaterialKeywordsToExclude, TEXT("\n"));
    GConfig->SetString(Section, TEXT("MaterialKeywordsToExclude"), *Joined, GEditorPerProjectIni);

    // 延迟Flush：由调用方在合适的时机统一Flush
}

FX_CoACDArgs FX_CoACDConfigManager::GetDefaultArgs()
{
    return FX_CoACDArgs();
}

bool FX_CoACDConfigManager::ValidateArgs(const FX_CoACDArgs& In, FString& OutError)
{
    if (!ValidateThreshold(In.Threshold, OutError)) return false;
    if (!ValidatePreprocessResolution(In.PreprocessResolution, OutError)) return false;
    if (!ValidateSampleResolution(In.SampleResolution, OutError)) return false;
    if (!ValidateMCTSNodes(In.MCTSNodes, OutError)) return false;
    if (!ValidateMCTSIteration(In.MCTSIteration, OutError)) return false;
    if (!ValidateMCTSMaxDepth(In.MCTSMaxDepth, OutError)) return false;
    if (!ValidateMaxConvexHullVertex(In.MaxConvexHullVertex, OutError)) return false;
    if (!ValidateExtrudeMargin(In.ExtrudeMargin, OutError)) return false;
    return true;
}

const TCHAR* FX_CoACDConfigManager::GetConfigSection()
{
    return TEXT("UE_XTools.CoACD");
}

bool FX_CoACDConfigManager::ValidateThreshold(float V, FString& OutError)
{
    if (V < 0.01f || V > 1.0f) { OutError = TEXT("凹度阈值必须在 0.01~1.0 范围内"); return false; }
    return true;
}
bool FX_CoACDConfigManager::ValidatePreprocessResolution(int32 V, FString& OutError)
{
    if (V < 20 || V > 100) { OutError = TEXT("预处理分辨率必须在 20~100 范围内"); return false; }
    return true;
}
bool FX_CoACDConfigManager::ValidateSampleResolution(int32 V, FString& OutError)
{
    if (V < 1000 || V > 10000) { OutError = TEXT("采样分辨率必须在 1000~10000 范围内"); return false; }
    return true;
}
bool FX_CoACDConfigManager::ValidateMCTSNodes(int32 V, FString& OutError)
{
    if (V < 10 || V > 40) { OutError = TEXT("MCTS 节点数必须在 10~40 范围内"); return false; }
    return true;
}
bool FX_CoACDConfigManager::ValidateMCTSIteration(int32 V, FString& OutError)
{
    if (V < 60 || V > 2000) { OutError = TEXT("MCTS 迭代数必须在 60~2000 范围内"); return false; }
    return true;
}
bool FX_CoACDConfigManager::ValidateMCTSMaxDepth(int32 V, FString& OutError)
{
    if (V < 2 || V > 7) { OutError = TEXT("MCTS 深度必须在 2~7 范围内"); return false; }
    return true;
}
bool FX_CoACDConfigManager::ValidateMaxConvexHullVertex(int32 V, FString& OutError)
{
    if (V < 8 || V > 512) { OutError = TEXT("每个凸包最大顶点数必须在 8~512 范围内"); return false; }
    return true;
}
bool FX_CoACDConfigManager::ValidateExtrudeMargin(float V, FString& OutError)
{
    if (V < 0.001f || V > 0.1f) { OutError = TEXT("挤出边距必须在 0.001~0.1 范围内"); return false; }
    return true;
}

void FX_CoACDConfigManager::Flush()
{
    GConfig->Flush(false, GEditorPerProjectIni);
}


