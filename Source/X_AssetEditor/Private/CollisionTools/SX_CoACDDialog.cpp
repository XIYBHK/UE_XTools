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
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/ConfigCacheIni.h"

// 本版本不做记忆，保留默认值即可
FX_CoACDArgs SX_CoACDDialog::LoadSaved(){ return FX_CoACDArgs(); }
void SX_CoACDDialog::Save(const FX_CoACDArgs&){ }

void SX_CoACDDialog::Construct(const FArguments& InArgs)
{
    Args = InArgs._Defaults;

    PreprocessModeOptions = { MakeShared<int32>(0), MakeShared<int32>(1), MakeShared<int32>(2) };
    ApproximateModeOptions = { MakeShared<int32>(0), MakeShared<int32>(1) };

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(12)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","Title","CoACD 算法参数 (SIGGRAPH 2022)"))
              .ToolTipText(NSLOCTEXT("CoACD","TitleTip","基于论文 'Approximate Convex Decomposition for 3D Meshes with Collision-Aware Concavity and Tree Search'\nWei et al., ACM TOG 2022\n项目：https://github.com/SarahWeiii/CoACD"))
              .Justification(ETextJustify::Center) ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(STextBlock)
                .Text(NSLOCTEXT("CoACD","Threshold","凹度阈值 (0.01~1)"))
                .ToolTipText(NSLOCTEXT("CoACD","ThresholdTip","精细程度，越大越粗，越小越细；默认 0.1"))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<float>)
                .MinValue(0.01f).MaxValue(1.f)
                .ToolTipText(NSLOCTEXT("CoACD","ThresholdBoxTip","0.01~1，常用 0.03~0.1"))
                .Value_Lambda([this]{return Args.Threshold;})
                .OnValueChanged_Lambda([this](float v){Args.Threshold=v;})
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
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
                .OnSelectionChanged_Lambda([this](TSharedPtr<int32> V, ESelectInfo::Type){ if (V.IsValid()) Args.PreprocessMode=(EX_CoACDPreprocessMode)*V; })
                [ SNew(STextBlock)
                    .Text_Lambda([this]{ return FText::FromString(Args.PreprocessMode==EX_CoACDPreprocessMode::Auto?TEXT("自动"):Args.PreprocessMode==EX_CoACDPreprocessMode::On?TEXT("开启"):TEXT("关闭")); })
                    .ToolTipText(NSLOCTEXT("CoACD","PrepModeBoxTip","推荐“自动”。模型很干净想提速可选“关闭”；出问题请选“开启”。"))
                ]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(STextBlock)
                .Text(NSLOCTEXT("CoACD","PrepRes","预处理分辨率 (20~100)"))
                .ToolTipText(NSLOCTEXT("CoACD","PrepResTip","仅在开启时生效；越大越贴近原模型，越慢；默认 50"))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>)
                .MinValue(20).MaxValue(100)
                .ToolTipText(NSLOCTEXT("CoACD","PrepResBoxTip","20~100，建议 40~60"))
                .Value_Lambda([this]{return Args.PreprocessResolution;})
                .OnValueChanged_Lambda([this](int32 v){Args.PreprocessResolution=v;})
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(STextBlock)
                .Text(NSLOCTEXT("CoACD","SampleRes","采样分辨率 (1000~10000)"))
                .ToolTipText(NSLOCTEXT("CoACD","SampleResTip","越大采样越准，越慢；默认 2000"))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>)
                .MinValue(1000).MaxValue(10000).Delta(100)
                .ToolTipText(NSLOCTEXT("CoACD","SampleResBoxTip","1000~10000，常用 1500~3000"))
                .Value_Lambda([this]{return Args.SampleResolution;})
                .OnValueChanged_Lambda([this](int32 v){Args.SampleResolution=v;})
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","MCTSNodes","MCTS 节点 (10~40)"))
              .ToolTipText(NSLOCTEXT("CoACD","MCTSNodesTip","分支数；配合迭代/深度影响效果与耗时")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(10).MaxValue(40)
              .ToolTipText(NSLOCTEXT("CoACD","MCTSNodesBoxTip","建议 15~25"))
              .Value_Lambda([this]{return Args.MCTSNodes;}).OnValueChanged_Lambda([this](int32 v){Args.MCTSNodes=v;}) ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","MCTSIt","MCTS 迭代 (60~2000)"))
              .ToolTipText(NSLOCTEXT("CoACD","MCTSItTip","迭代次数，越大越好但更慢；默认 100（速度优化）")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(60).MaxValue(2000).Delta(10)
              .ToolTipText(NSLOCTEXT("CoACD","MCTSItBoxTip","速度优先：60~100；质量优先：150~300"))
              .Value_Lambda([this]{return Args.MCTSIteration;}).OnValueChanged_Lambda([this](int32 v){Args.MCTSIteration=v;}) ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","MCTSDepth","MCTS 深度 (2~7)"))
              .ToolTipText(NSLOCTEXT("CoACD","MCTSDepthTip","搜索树最大深度；默认 2（速度优化）")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(2).MaxValue(7)
              .ToolTipText(NSLOCTEXT("CoACD","MCTSDepthBoxTip","速度优先：2；平衡：3；质量优先：4~5"))
              .Value_Lambda([this]{return Args.MCTSMaxDepth;}).OnValueChanged_Lambda([this](int32 v){Args.MCTSMaxDepth=v;}) ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(SCheckBox)
                .ToolTipText(NSLOCTEXT("CoACD","PCATip","将切割方向与主轴对齐，某些形状更稳定"))
                .IsChecked_Lambda([this]{return Args.bPCA?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this](ECheckBoxState s){Args.bPCA=(s==ECheckBoxState::Checked);})
                .Content()[SNew(STextBlock).Text(NSLOCTEXT("CoACD","PCA","启用 PCA 预处理"))]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(SCheckBox)
                .ToolTipText(NSLOCTEXT("CoACD","MergeTip","合并相邻小凸包，减少数量"))
                .IsChecked_Lambda([this]{return Args.bMerge?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this](ECheckBoxState s){Args.bMerge=(s==ECheckBoxState::Checked);})
                .Content()[SNew(STextBlock).Text(NSLOCTEXT("CoACD","Merge","启用合并后处理"))]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","MaxHull","最大凸包数量 (-1为不限制)"))
              .ToolTipText(NSLOCTEXT("CoACD","MaxHullTip","仅在合并开启时生效；限制过小会牺牲精度")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(-1).MaxValue(1000)
              .ToolTipText(NSLOCTEXT("CoACD","MaxHullBoxTip","常用 -1 或 32~128"))
              .Value_Lambda([this]{return Args.MaxConvexHull;}).OnValueChanged_Lambda([this](int32 v){Args.MaxConvexHull=v;}) ]

            // === v1.0.7 新增核心参数 ===
            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","MaxVertex","每个凸包最大顶点数 (8~512)"))
              .ToolTipText(NSLOCTEXT("CoACD","MaxVertexTip","限制单个凸包的顶点数量，影响碰撞精度与性能；默认 256")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(8).MaxValue(512).Delta(8)
              .ToolTipText(NSLOCTEXT("CoACD","MaxVertexBoxTip","建议 64~256，UE碰撞推荐不超过 256"))
              .Value_Lambda([this]{return Args.MaxConvexHullVertex;}).OnValueChanged_Lambda([this](int32 v){Args.MaxConvexHullVertex=v;}) ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(SCheckBox)
                .ToolTipText(NSLOCTEXT("CoACD","DecimateTip","开启顶点约束以控制凸包复杂度，可能略微降低精度"))
                .IsChecked_Lambda([this]{return Args.bDecimate?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this](ECheckBoxState s){Args.bDecimate=(s==ECheckBoxState::Checked);})
                .Content()[SNew(STextBlock).Text(NSLOCTEXT("CoACD","Decimate","启用顶点约束"))]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","ApproxMode","近似模式"))
              .ToolTipText(NSLOCTEXT("CoACD","ApproxModeTip","凸包（推荐）：精确分解\n包围盒：快速但粗糙，适合预览")) ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SComboBox<TSharedPtr<int32>>)
                .OptionsSource(&ApproximateModeOptions)
                .OnGenerateWidget_Lambda([](TSharedPtr<int32> V){
                    const TCHAR* T = *V==0?TEXT("凸包（精确）"):TEXT("包围盒（快速）");
                    return SNew(STextBlock).Text(FText::FromString(T));
                })
                .OnSelectionChanged_Lambda([this](TSharedPtr<int32> V, ESelectInfo::Type){ if (V.IsValid()) Args.ApproximateMode=*V; })
                [ SNew(STextBlock)
                    .Text_Lambda([this]{ return FText::FromString(Args.ApproximateMode==0?TEXT("凸包（精确）"):TEXT("包围盒（快速）")); })
                    .ToolTipText(NSLOCTEXT("CoACD","ApproxModeBoxTip","凸包模式提供最佳精度；包围盒模式快速但粗糙"))
                ]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(SCheckBox)
                .ToolTipText(NSLOCTEXT("CoACD","ExtrudeTip","对凸包进行轻微挤出，可改善某些碰撞检测边界情况"))
                .IsChecked_Lambda([this]{return Args.bExtrude?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this](ECheckBoxState s){Args.bExtrude=(s==ECheckBoxState::Checked);})
                .Content()[SNew(STextBlock).Text(NSLOCTEXT("CoACD","Extrude","启用凸包挤出"))]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","ExtrudeMargin","挤出边距 (0.001~0.1)"))
              .ToolTipText(NSLOCTEXT("CoACD","ExtrudeMarginTip","仅在启用挤出时生效；值过大可能导致碰撞体积明显增大；默认 0.01")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<float>).MinValue(0.001f).MaxValue(0.1f).Delta(0.001f)
              .ToolTipText(NSLOCTEXT("CoACD","ExtrudeMarginBoxTip","建议 0.005~0.02"))
              .Value_Lambda([this]{return Args.ExtrudeMargin;}).OnValueChanged_Lambda([this](float v){Args.ExtrudeMargin=v;}) ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","Seed","随机种子"))
              .ToolTipText(NSLOCTEXT("CoACD","SeedTip","相同参数+种子可复现实验结果")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(0).MaxValue(999999)
              .ToolTipText(NSLOCTEXT("CoACD","SeedBoxTip","0=随机"))
              .Value_Lambda([this]{return Args.Seed;}).OnValueChanged_Lambda([this](int32 v){Args.Seed=v;}) ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","LOD","源 LOD 索引"))
              .ToolTipText(NSLOCTEXT("CoACD","LODTip","通常选择 0；较高LOD可加速但精度降低")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(0).MaxValue(8)
              .ToolTipText(NSLOCTEXT("CoACD","LODBoxTip","0~8"))
              .Value_Lambda([this]{return Args.SourceLODIndex;}).OnValueChanged_Lambda([this](int32 v){Args.SourceLODIndex=v;}) ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(SCheckBox)
                .ToolTipText(NSLOCTEXT("CoACD","RemoveTip","执行前清空旧的简单碰撞"))
                .IsChecked_Lambda([this]{return Args.bRemoveExistingCollision?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this](ECheckBoxState s){Args.bRemoveExistingCollision=(s==ECheckBoxState::Checked);})
                .Content()[SNew(STextBlock).Text(NSLOCTEXT("CoACD","RemoveOld","移除现有碰撞"))]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,6)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","MatExclude","材质槽排除关键词（包含匹配，分行输入）"))
              .ToolTipText(NSLOCTEXT("CoACD","MatExcludeTip","在这些关键字的材质槽上的三角将被忽略，例如 Outline、Backface 等")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SEditableTextBox)
                .MinDesiredWidth(400)
                .ToolTipText(NSLOCTEXT("CoACD","MatExcludeBoxTip","每行一个关键字；大小写不敏感；示例：Outline"))
                .Text_Lambda([this]{ return FText::FromString(FString::Join(Args.MaterialKeywordsToExclude, TEXT("\n"))); })
                .OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type){
                    TArray<FString> Lines; T.ToString().ParseIntoArrayLines(Lines);
                    Args.MaterialKeywordsToExclude = MoveTemp(Lines);
                })
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,10)
            [
                SNew(SUniformGridPanel).SlotPadding(8)
                + SUniformGridPanel::Slot(0,0)[ SNew(SButton).Text(NSLOCTEXT("CoACD","OK","开始") ).OnClicked(this,&SX_CoACDDialog::OnOK) ]
                + SUniformGridPanel::Slot(1,0)[ SNew(SButton).Text(NSLOCTEXT("CoACD","Cancel","取消") ).OnClicked(this,&SX_CoACDDialog::OnCancel) ]
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
    if (DialogWindow.IsValid()) DialogWindow->RequestDestroyWindow();
    return FReply::Handled();
}

FReply SX_CoACDDialog::OnCancel()
{
    bConfirmed = false;
    if (DialogWindow.IsValid()) DialogWindow->RequestDestroyWindow();
    return FReply::Handled();
}


