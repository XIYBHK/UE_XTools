// Copyright Epic Games, Inc. All Rights Reserved.

#include "CollisionTools/SX_CoACDDialog.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

// 质量预设集中实现（在类定义之后实现静态方法）
void SX_CoACDDialog::ApplyPresetQuality1(FX_CoACDArgs& A)
{
    A.Threshold = 0.10f; A.PreprocessMode = EX_CoACDPreprocessMode::Off; A.PreprocessResolution = 50; A.SampleResolution = 2000;
    A.MCTSNodes = 20; A.MCTSIteration = 100; A.MCTSMaxDepth = 2; A.bPCA = false; A.bMerge = true; A.MaxConvexHull = -1;
    A.Seed = 0; A.SourceLODIndex = 0; A.bRemoveExistingCollision = true;
}
void SX_CoACDDialog::ApplyPresetQuality2(FX_CoACDArgs& A)
{
    A.Threshold = 0.07f; A.PreprocessMode = EX_CoACDPreprocessMode::On; A.PreprocessResolution = 50; A.SampleResolution = 2000;
    A.MCTSNodes = 20; A.MCTSIteration = 120; A.MCTSMaxDepth = 3; A.bPCA = false; A.bMerge = true; A.MaxConvexHull = -1;
    A.Seed = 0; A.SourceLODIndex = 0; A.bRemoveExistingCollision = true;
}
void SX_CoACDDialog::ApplyPresetQuality3(FX_CoACDArgs& A)
{
    A.Threshold = 0.05f; A.PreprocessMode = EX_CoACDPreprocessMode::On; A.PreprocessResolution = 50; A.SampleResolution = 2000;
    A.MCTSNodes = 20; A.MCTSIteration = 150; A.MCTSMaxDepth = 3; A.bPCA = false; A.bMerge = true; A.MaxConvexHull = -1;
    A.Seed = 0; A.SourceLODIndex = 0; A.bRemoveExistingCollision = true;
}
void SX_CoACDDialog::ApplyPresetQuality4(FX_CoACDArgs& A)
{
    A.Threshold = 0.03f; A.PreprocessMode = EX_CoACDPreprocessMode::On; A.PreprocessResolution = 50; A.SampleResolution = 2000;
    A.MCTSNodes = 20; A.MCTSIteration = 200; A.MCTSMaxDepth = 4; A.bPCA = false; A.bMerge = true; A.MaxConvexHull = -1;
    A.Seed = 0; A.SourceLODIndex = 0; A.bRemoveExistingCollision = true;
}

// 参数记忆：使用 EditorPerProjectIni 持久化上次设置
FX_CoACDArgs SX_CoACDDialog::LoadSaved()
{
    FX_CoACDArgs Out; // 带默认值
    const TCHAR* Section = TEXT("UE_XTools.CoACD");

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

void SX_CoACDDialog::Save(const FX_CoACDArgs& In)
{
    const TCHAR* Section = TEXT("UE_XTools.CoACD");

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

    GConfig->Flush(false, GEditorPerProjectIni);
}

void SX_CoACDDialog::Construct(const FArguments& InArgs)
{
    Args = InArgs._Defaults;

    PreprocessModeOptions = { MakeShared<int32>(0), MakeShared<int32>(1), MakeShared<int32>(2) };
    ApproximateModeOptions = { MakeShared<int32>(0), MakeShared<int32>(1) };

    // 改为延迟刷盘：控件改动仅 Set；Flush 在 OK/Cancel/析构
    auto SaveNow = [this]() { SX_CoACDDialog::Save(this->Args); };
    const FMargin RowPad(0.f, 2.f, 0.f, 0.f);

    auto RebuildKeywordItems = [this]()
    {
        MaterialKeywordItems.Reset();
        for (const FString& S : Args.MaterialKeywordsToExclude)
        {
            MaterialKeywordItems.Add(MakeShared<FString>(S));
        }
        if (MaterialKeywordListView.IsValid())
        {
            MaterialKeywordListView->RequestListRefresh();
        }
    };

    // 初始化一次列表数据，确保已保存的条目能在首次打开时直接显示
    RebuildKeywordItems();

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(8)
        [
            SNew(SVerticalBox)
            // 顶部标题与窗口标题重复，移除内部标题以避免冗余

            // 预设按钮将放到窗口底部（靠近操作按钮）

            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock)
                .Text(NSLOCTEXT("CoACD","Threshold","凹度阈值 (0.01~1)"))
                .ToolTipText(NSLOCTEXT("CoACD","ThresholdTip","精细程度，越大越粗，越小越细；默认 0.1"))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<float>)
                .MinValue(0.01f).MaxValue(1.f)
                .ToolTipText(NSLOCTEXT("CoACD","ThresholdBoxTip","0.01~1，常用 0.03~0.1"))
                .Value_Lambda([this]{return Args.Threshold;})
                .OnValueChanged_Lambda([this,SaveNow](float v){Args.Threshold=v; SaveNow();})
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock)
                .Text(NSLOCTEXT("CoACD","PrepMode","预处理模式"))
                .ToolTipText(NSLOCTEXT("CoACD","PrepModeTip","自动（推荐）：自动判断是否修复网格\n开启：总是修复，最稳但更慢\n关闭：不修复，最快；模型不干净可能失败"))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SComboBox<TSharedPtr<int32>>)
                .OptionsSource(&PreprocessModeOptions)
                .OnGenerateWidget_Lambda([](TSharedPtr<int32> V){
                    const TCHAR* T = *V==0?TEXT("自动"):(*V==1?TEXT("开启"):TEXT("关闭"));
                    return SNew(STextBlock).Text(FText::FromString(T));
                })
                .OnSelectionChanged_Lambda([this,SaveNow](TSharedPtr<int32> V, ESelectInfo::Type){ if (V.IsValid()) { Args.PreprocessMode=(EX_CoACDPreprocessMode)*V; SaveNow(); } })
                [ SNew(STextBlock)
                    .Text_Lambda([this]{ return FText::FromString(Args.PreprocessMode==EX_CoACDPreprocessMode::Auto?TEXT("自动"):Args.PreprocessMode==EX_CoACDPreprocessMode::On?TEXT("开启"):TEXT("关闭")); })
                    .ToolTipText(NSLOCTEXT("CoACD","PrepModeBoxTip","推荐“自动”。模型很干净想提速可选“关闭”；出问题请选“开启”。"))
                ]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock)
                .Text(NSLOCTEXT("CoACD","PrepRes","预处理分辨率 (20~100)"))
                .ToolTipText(NSLOCTEXT("CoACD","PrepResTip","仅在开启时生效；越大越贴近原模型，越慢；默认 50"))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>)
                .MinValue(20).MaxValue(100)
                .ToolTipText(NSLOCTEXT("CoACD","PrepResBoxTip","20~100，建议 40~60"))
                .Value_Lambda([this]{return Args.PreprocessResolution;})
                .OnValueChanged_Lambda([this,SaveNow](int32 v){Args.PreprocessResolution=v; SaveNow();})
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock)
                .Text(NSLOCTEXT("CoACD","SampleRes","采样分辨率 (1000~10000)"))
                .ToolTipText(NSLOCTEXT("CoACD","SampleResTip","越大采样越准，越慢；默认 2000"))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>)
                .MinValue(1000).MaxValue(10000).Delta(100)
                .ToolTipText(NSLOCTEXT("CoACD","SampleResBoxTip","1000~10000，常用 1500~3000"))
                .Value_Lambda([this]{return Args.SampleResolution;})
                .OnValueChanged_Lambda([this,SaveNow](int32 v){Args.SampleResolution=v; SaveNow();})
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","MCTSNodes","MCTS 节点 (10~40)"))
              .ToolTipText(NSLOCTEXT("CoACD","MCTSNodesTip","分支数；配合迭代/深度影响效果与耗时")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(10).MaxValue(40)
              .ToolTipText(NSLOCTEXT("CoACD","MCTSNodesBoxTip","建议 15~25"))
              .Value_Lambda([this]{return Args.MCTSNodes;}).OnValueChanged_Lambda([this,SaveNow](int32 v){Args.MCTSNodes=v; SaveNow();}) ]

            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","MCTSIt","MCTS 迭代 (60~2000)"))
              .ToolTipText(NSLOCTEXT("CoACD","MCTSItTip","迭代次数，越大越好但更慢；默认 100（速度优化）")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(60).MaxValue(2000).Delta(10)
              .ToolTipText(NSLOCTEXT("CoACD","MCTSItBoxTip","速度优先：60~100；质量优先：150~300"))
              .Value_Lambda([this]{return Args.MCTSIteration;}).OnValueChanged_Lambda([this,SaveNow](int32 v){Args.MCTSIteration=v; SaveNow();}) ]

            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","MCTSDepth","MCTS 深度 (2~7)"))
              .ToolTipText(NSLOCTEXT("CoACD","MCTSDepthTip","搜索树最大深度；默认 2（速度优化）")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(2).MaxValue(7)
              .ToolTipText(NSLOCTEXT("CoACD","MCTSDepthBoxTip","速度优先：2；平衡：3；质量优先：4~5"))
              .Value_Lambda([this]{return Args.MCTSMaxDepth;}).OnValueChanged_Lambda([this,SaveNow](int32 v){Args.MCTSMaxDepth=v; SaveNow();}) ]

            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(SCheckBox)
                .ToolTipText(NSLOCTEXT("CoACD","PCATip","将切割方向与主轴对齐，某些形状更稳定"))
                .IsChecked_Lambda([this]{return Args.bPCA?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this,SaveNow](ECheckBoxState s){Args.bPCA=(s==ECheckBoxState::Checked); SaveNow();})
                .Content()[SNew(STextBlock).Text(NSLOCTEXT("CoACD","PCA","启用 PCA 预处理"))]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(SCheckBox)
                .ToolTipText(NSLOCTEXT("CoACD","MergeTip","合并相邻小凸包，减少数量"))
                .IsChecked_Lambda([this]{return Args.bMerge?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this,SaveNow](ECheckBoxState s){Args.bMerge=(s==ECheckBoxState::Checked); SaveNow();})
                .Content()[SNew(STextBlock).Text(NSLOCTEXT("CoACD","Merge","启用合并后处理"))]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","MaxHull","最大凸包数量 (-1为不限制)"))
              .ToolTipText(NSLOCTEXT("CoACD","MaxHullTip","仅在合并开启时生效；限制过小会牺牲精度")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(-1).MaxValue(1000)
              .ToolTipText(NSLOCTEXT("CoACD","MaxHullBoxTip","常用 -1 或 32~128"))
              .Value_Lambda([this]{return Args.MaxConvexHull;}).OnValueChanged_Lambda([this,SaveNow](int32 v){Args.MaxConvexHull=v; SaveNow();}) ]

            // 高级参数标题移除，直接展示条目
            // 原: SNew(STextBlock).Text(NSLOCTEXT("CoACD","Adv107Header","高级参数 (1.0.7)"))
            // 现: 不显示标题，保持紧凑
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(SSpacer).Size(FVector2D(1,1)) ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","MaxVertex","每个凸包最大顶点数 (8~512)"))
              .ToolTipText(NSLOCTEXT("CoACD","MaxVertexTip","限制单个凸包的顶点数量，影响碰撞精度与性能")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(8).MaxValue(512).Delta(8)
              .ToolTipText(NSLOCTEXT("CoACD","MaxVertexBoxTip","建议 64~256，UE碰撞推荐不超过 256"))
              .Value_Lambda([this]{return Args.MaxConvexHullVertex;}).OnValueChanged_Lambda([this,SaveNow](int32 v){Args.MaxConvexHullVertex=v; SaveNow();}) ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(SCheckBox)
                .ToolTipText(NSLOCTEXT("CoACD","DecimateTip","开启顶点约束以控制凸包复杂度，可能略微降低精度"))
                .IsChecked_Lambda([this]{return Args.bDecimate?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this,SaveNow](ECheckBoxState s){Args.bDecimate=(s==ECheckBoxState::Checked); SaveNow();})
                .Content()[SNew(STextBlock).Text(NSLOCTEXT("CoACD","Decimate","启用顶点约束"))]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","ApproxMode","近似模式"))
              .ToolTipText(NSLOCTEXT("CoACD","ApproxModeTip","凸包（推荐）：精确分解；包围盒：快速但粗糙")) ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SComboBox<TSharedPtr<int32>>)
                .OptionsSource(&ApproximateModeOptions)
                .OnGenerateWidget_Lambda([](TSharedPtr<int32> V){
                    const TCHAR* T = *V==0?TEXT("凸包（精确）"):TEXT("包围盒（快速）");
                    return SNew(STextBlock).Text(FText::FromString(T));
                })
                .OnSelectionChanged_Lambda([this,SaveNow](TSharedPtr<int32> V, ESelectInfo::Type){ if (V.IsValid()) { Args.ApproximateMode=*V; SaveNow(); } })
                [ SNew(STextBlock)
                    .Text_Lambda([this]{ return FText::FromString(Args.ApproximateMode==0?TEXT("凸包（精确）"):TEXT("包围盒（快速）")); })
                    .ToolTipText(NSLOCTEXT("CoACD","ApproxModeBoxTip","凸包模式提供最佳精度；包围盒模式快速但粗糙"))
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(SCheckBox)
                .ToolTipText(NSLOCTEXT("CoACD","ExtrudeTip","对凸包进行轻微挤出，可改善某些碰撞检测边界情况"))
                .IsChecked_Lambda([this]{return Args.bExtrude?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this,SaveNow](ECheckBoxState s){Args.bExtrude=(s==ECheckBoxState::Checked); SaveNow();})
                .Content()[SNew(STextBlock).Text(NSLOCTEXT("CoACD","Extrude","启用凸包挤出"))]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","ExtrudeMargin","挤出边距 (0.001~0.1)"))
              .ToolTipText(NSLOCTEXT("CoACD","ExtrudeMarginTip","仅在启用挤出时有效；值过大可能导致碰撞体积增大")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<float>).MinValue(0.001f).MaxValue(0.1f).Delta(0.001f)
              .ToolTipText(NSLOCTEXT("CoACD","ExtrudeMarginBoxTip","建议 0.005~0.02"))
              .Value_Lambda([this]{return Args.ExtrudeMargin;}).OnValueChanged_Lambda([this,SaveNow](float v){Args.ExtrudeMargin=v; SaveNow();}) ]

            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","Seed","随机种子"))
              .ToolTipText(NSLOCTEXT("CoACD","SeedTip","相同参数+种子可复现实验结果")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(0).MaxValue(999999)
              .ToolTipText(NSLOCTEXT("CoACD","SeedBoxTip","0=随机"))
              .Value_Lambda([this]{return Args.Seed;}).OnValueChanged_Lambda([this,SaveNow](int32 v){Args.Seed=v; SaveNow();}) ]

            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","LOD","源 LOD 索引"))
              .ToolTipText(NSLOCTEXT("CoACD","LODTip","通常选择 0；较高LOD可加速但精度降低")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(0).MaxValue(8)
              .ToolTipText(NSLOCTEXT("CoACD","LODBoxTip","0~8"))
              .Value_Lambda([this]{return Args.SourceLODIndex;}).OnValueChanged_Lambda([this,SaveNow](int32 v){Args.SourceLODIndex=v; SaveNow();}) ]

            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(SCheckBox)
                .ToolTipText(NSLOCTEXT("CoACD","RemoveTip","执行前清空旧的简单碰撞"))
                .IsChecked_Lambda([this]{return Args.bRemoveExistingCollision?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this,SaveNow](ECheckBoxState s){Args.bRemoveExistingCollision=(s==ECheckBoxState::Checked); SaveNow();})
                .Content()[SNew(STextBlock).Text(NSLOCTEXT("CoACD","RemoveOld","移除现有碰撞"))]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","MatExclude","材质排除关键词（槽名/材质名/路径/索引）"))
              .ToolTipText(NSLOCTEXT("CoACD","MatExcludeTip","支持中文与英文；可按槽名、材质名、材质路径或槽位索引匹配（大小写不敏感）。示例：玻璃、边框、Element 2、元素3、2、/Game/.../MI_玻璃")) ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SVerticalBox)
                // 列表
                + SVerticalBox::Slot().AutoHeight()
                [
                    SAssignNew(MaterialKeywordListView, SListView<TSharedPtr<FString>>)
                    .ListItemsSource(&MaterialKeywordItems)
                    .OnGenerateRow_Lambda([this,SaveNow,RebuildKeywordItems](TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
                    {
                        return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1.f)
                            [
                                SNew(SEditableTextBox)
                                .Text(FText::FromString(Item.IsValid()?*Item:TEXT("")))
                                .OnTextCommitted_Lambda([this,Item,SaveNow,RebuildKeywordItems](const FText& NewText, ETextCommit::Type){
                                    const FString OldStr = Item.IsValid()?*Item:TEXT("");
                                    const int32 Idx = Args.MaterialKeywordsToExclude.IndexOfByKey(OldStr);
                                    const FString NewStr = NewText.ToString().TrimStartAndEnd();
                                    if (Idx != INDEX_NONE)
                                    {
                                        if (NewStr.IsEmpty())
                                        {
                                            Args.MaterialKeywordsToExclude.RemoveAt(Idx);
                                        }
                                        else
                                        {
                                            Args.MaterialKeywordsToExclude[Idx] = NewStr;
                                        }
                                        SaveNow();
                                        RebuildKeywordItems();
                                    }
                                })
                            ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(6,0)
                            [
                                SNew(SButton)
                                .Text(NSLOCTEXT("CoACD","MatExcludeDel","×"))
                                .OnClicked_Lambda([this,Item,SaveNow,RebuildKeywordItems]()
                                {
                                    const FString OldStr = Item.IsValid()?*Item:TEXT("");
                                    const int32 Idx = Args.MaterialKeywordsToExclude.IndexOfByKey(OldStr);
                                    if (Idx != INDEX_NONE)
                                    {
                                        Args.MaterialKeywordsToExclude.RemoveAt(Idx);
                                        SaveNow();
                                        RebuildKeywordItems();
                                    }
                                    return FReply::Handled();
                                })
                            ]
                        ];
                    })
                    .SelectionMode(ESelectionMode::None)
                    .ItemHeight(22.f)
                ]
                // 新增条目输入
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.f)
                    [ SNew(SSpacer) ]
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SButton)
                        .Text(NSLOCTEXT("CoACD","MatExcludeAddBtn","+ 添加条目"))
                        .OnClicked_Lambda([this,SaveNow,RebuildKeywordItems]()
                        {
                            Args.MaterialKeywordsToExclude.Add(TEXT(""));
                            SaveNow();
                            RebuildKeywordItems();
                            return FReply::Handled();
                        })
                    ]
                ]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,10)
            [
                SNew(SVerticalBox)
                // 预设按钮：质量预设1/质量预设2/质量预设3/质量预设4
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)
                [
                    SNew(SUniformGridPanel).SlotPadding(6)
                    + SUniformGridPanel::Slot(0,0)
                    [ SNew(SButton)
                        .Text(NSLOCTEXT("CoACD","PresetQ1","质量预设1")) 
                        .OnClicked_Lambda([this](){ ApplyPresetQuality1(Args); return FReply::Handled(); }) ]
                    + SUniformGridPanel::Slot(1,0)
                    [ SNew(SButton)
                        .Text(NSLOCTEXT("CoACD","PresetQ2","质量预设2"))
                        .OnClicked_Lambda([this](){ ApplyPresetQuality2(Args); return FReply::Handled(); }) ]
                    + SUniformGridPanel::Slot(2,0)
                    [ SNew(SButton)
                        .Text(NSLOCTEXT("CoACD","PresetQ3","质量预设3"))
                        .OnClicked_Lambda([this](){ ApplyPresetQuality3(Args); return FReply::Handled(); }) ]
                    + SUniformGridPanel::Slot(3,0)
                    [ SNew(SButton)
                        .Text(NSLOCTEXT("CoACD","PresetQ4","质量预设4"))
                        .OnClicked_Lambda([this](){ ApplyPresetQuality4(Args); return FReply::Handled(); }) ]
                ]
                // 操作按钮：开始 / 取消
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SUniformGridPanel).SlotPadding(8)
                    + SUniformGridPanel::Slot(0,0)[ SNew(SButton).Text(NSLOCTEXT("CoACD","OK","开始") ).OnClicked(this,&SX_CoACDDialog::OnOK) ]
                    + SUniformGridPanel::Slot(1,0)[ SNew(SButton).Text(NSLOCTEXT("CoACD","Cancel","取消") ).OnClicked(this,&SX_CoACDDialog::OnCancel) ]
                ]
            ]
        ]
    ];
}

bool SX_CoACDDialog::ShowDialog(FX_CoACDArgs& InOutArgs)
{
    FX_CoACDArgs Defaults = LoadSaved();
    // 以保存的为默认，用传入的覆盖（若调用方想自定义初始值）
    if (InOutArgs.Threshold != 0.f) Defaults.Threshold = InOutArgs.Threshold;
    TSharedRef<SX_CoACDDialog> Widget = SNew(SX_CoACDDialog).Defaults(Defaults);
    TSharedRef<SWindow> Win = SNew(SWindow).Title(NSLOCTEXT("CoACD","Win","CoACD 算法参数 (SIGGRAPH 2022)"))
        .SizingRule(ESizingRule::Autosized)[Widget];
    Widget->DialogWindow = Win;
    FSlateApplication::Get().AddModalWindow(Win, FSlateApplication::Get().GetActiveTopLevelWindow());
    if (Widget->bConfirmed)
    {
        InOutArgs = Widget->Args;
        Save(InOutArgs);
        return true;
    }
    return false;
}

FReply SX_CoACDDialog::OnOK()
{
    bConfirmed = true;
    // 统一在确认时刷盘一次
    SX_CoACDDialog::Save(this->Args);
    if (DialogWindow.IsValid()) DialogWindow->RequestDestroyWindow();
    return FReply::Handled();
}

FReply SX_CoACDDialog::OnCancel()
{
    bConfirmed = false;
    // 取消时不覆盖配置（保持上次确认值）
    if (DialogWindow.IsValid()) DialogWindow->RequestDestroyWindow();
    return FReply::Handled();
}


