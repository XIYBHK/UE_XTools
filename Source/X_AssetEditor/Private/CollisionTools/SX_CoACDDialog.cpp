// Copyright Epic Games, Inc. All Rights Reserved.

#include "CollisionTools/SX_CoACDDialog.h"
#include "CollisionTools/X_CoACDConfigManager.h"
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
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Layout/SExpandableArea.h"

void SX_CoACDDialog::Construct(const FArguments& InArgs)
{
    Args = InArgs._Defaults;

    // 改为延迟刷盘：控件改动仅 Set；Flush 在 OK/Cancel/析构
    auto SaveNow = [this]() { FX_CoACDConfigManager::Save(this->Args); };
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

    // 选项数据
    PreprocessModeOptions = {
        MakeShared<int32>((int32)EX_CoACDPreprocessMode::Off),
        MakeShared<int32>((int32)EX_CoACDPreprocessMode::On),
        MakeShared<int32>((int32)EX_CoACDPreprocessMode::Auto)
    };
    ApproximateModeOptions = {
        MakeShared<int32>(0), // 凸包
        MakeShared<int32>(1)  // 包围盒
    };

    auto ModeToText = [](EX_CoACDPreprocessMode M)
    {
        switch (M)
        {
        case EX_CoACDPreprocessMode::Off: return NSLOCTEXT("CoACD","PreOff","预处理：关闭");
        case EX_CoACDPreprocessMode::On:  return NSLOCTEXT("CoACD","PreOn","预处理：开启");
        default:                           return NSLOCTEXT("CoACD","PreAuto","预处理：自动");
        }
    };

    auto ApproxText = [](int32 V)
    {
        return V == 0 ? NSLOCTEXT("CoACD","ApproxHull","近似：凸包") : NSLOCTEXT("CoACD","ApproxBox","近似：包围盒");
    };

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(8)
        [
            SNew(SVerticalBox)
            // 参数面板内容：主区（简洁）
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

            // 主面板：展示会被“质量预设”修改的关键参数
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock)
                .Text(NSLOCTEXT("CoACD","PrepModeLabel","预处理模式"))
                .ToolTipText(NSLOCTEXT("CoACD","PrepModeTip","自动（推荐）：自动判断是否修复网格\n开启：总是修复，最稳但更慢\n关闭：不修复，最快；模型不干净可能失败"))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SComboBox<TSharedPtr<int32>>)
                .OptionsSource(&PreprocessModeOptions)
                .OnSelectionChanged_Lambda([this,SaveNow](TSharedPtr<int32> V, ESelectInfo::Type){ if (V.IsValid()){ Args.PreprocessMode=(EX_CoACDPreprocessMode)(*V); SaveNow(); } })
                .OnGenerateWidget_Lambda([ModeToText](TSharedPtr<int32> V){ return SNew(STextBlock).Text(ModeToText((EX_CoACDPreprocessMode)*V)); })
                .Content()[ SNew(STextBlock).Text_Lambda([this,ModeToText]{ return ModeToText(Args.PreprocessMode); }).ToolTipText(NSLOCTEXT("CoACD","PrepModeBoxTip","推荐“自动”。模型很干净想提速可选“关闭”；出问题请选“开启”。")) ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock)
                .Text(NSLOCTEXT("CoACD","PreResLabel","预处理分辨率 (20~100)"))
                .ToolTipText(NSLOCTEXT("CoACD","PrepResTip","仅在开启时生效；越大越贴近原模型，越慢；默认 50"))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(20).MaxValue(100)
                .ToolTipText(NSLOCTEXT("CoACD","PrepResBoxTip","20~100，建议 40~60"))
                .Value_Lambda([this]{return Args.PreprocessResolution;})
                .OnValueChanged_Lambda([this,SaveNow](int32 v){Args.PreprocessResolution=v; SaveNow();})
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock)
                .Text(NSLOCTEXT("CoACD","SampleResLabel","采样分辨率 (1000~10000)"))
                .ToolTipText(NSLOCTEXT("CoACD","SampleResTip","越大采样越准，越慢；默认 2000"))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(1000).MaxValue(10000)
                .ToolTipText(NSLOCTEXT("CoACD","SampleResBoxTip","1000~10000，常用 1500~3000"))
                .Value_Lambda([this]{return Args.SampleResolution;})
                .OnValueChanged_Lambda([this,SaveNow](int32 v){Args.SampleResolution=v; SaveNow();})
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","NodesLabel","MCTS 节点 (10~40)"))
              .ToolTipText(NSLOCTEXT("CoACD","MCTSNodesTip","分支数；配合迭代/深度影响效果与耗时")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(10).MaxValue(40)
              .ToolTipText(NSLOCTEXT("CoACD","MCTSNodesBoxTip","建议 15~25"))
              .Value_Lambda([this]{return Args.MCTSNodes;})
              .OnValueChanged_Lambda([this,SaveNow](int32 v){Args.MCTSNodes=v; SaveNow();})
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","IterLabel","MCTS 迭代 (60~2000)"))
              .ToolTipText(NSLOCTEXT("CoACD","MCTSItTip","迭代次数，越大越好但更慢；默认 100（速度优化）")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(60).MaxValue(2000)
              .ToolTipText(NSLOCTEXT("CoACD","MCTSItBoxTip","速度优先：60~100；质量优先：150~300"))
              .Value_Lambda([this]{return Args.MCTSIteration;})
              .OnValueChanged_Lambda([this,SaveNow](int32 v){Args.MCTSIteration=v; SaveNow();})
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","DepthLabel","MCTS 深度 (2~7)"))
              .ToolTipText(NSLOCTEXT("CoACD","MCTSDepthTip","搜索树最大深度；默认 2（速度优化）")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(2).MaxValue(7)
              .ToolTipText(NSLOCTEXT("CoACD","MCTSDepthBoxTip","速度优先：2；平衡：3；质量优先：4~5"))
              .Value_Lambda([this]{return Args.MCTSMaxDepth;})
              .OnValueChanged_Lambda([this,SaveNow](int32 v){Args.MCTSMaxDepth=v; SaveNow();})
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(SCheckBox)
                .IsChecked_Lambda([this]{return Args.bPCA?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this,SaveNow](ECheckBoxState st){Args.bPCA=(st==ECheckBoxState::Checked); SaveNow();})
                .ToolTipText(NSLOCTEXT("CoACD","PCATip","将切割方向与主轴对齐，某些形状更稳定"))
                [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","PCALabel","启用 PCA")) ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(SCheckBox)
                .IsChecked_Lambda([this]{return Args.bMerge?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this,SaveNow](ECheckBoxState st){Args.bMerge=(st==ECheckBoxState::Checked); SaveNow();})
                .ToolTipText(NSLOCTEXT("CoACD","MergeTip","合并相邻小凸包，减少数量"))
                [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","MergeLabel","合并后处理")) ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","MaxHullLabel","最大凸包数量 (-1 不限)"))
              .ToolTipText(NSLOCTEXT("CoACD","MaxHullTip2","仅在合并开启时生效；限制过小会牺牲精度")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(-1).MaxValue(4096)
              .ToolTipText(NSLOCTEXT("CoACD","MaxHullBoxTip","常用 -1 或 32~128"))
              .Value_Lambda([this]{return Args.MaxConvexHull;})
              .OnValueChanged_Lambda([this,SaveNow](int32 v){Args.MaxConvexHull=v; SaveNow();})
            ]
            // v1.0.7 关键参数
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","MaxVertexLabel","每个凸包最大顶点数 (8~512)"))
              .ToolTipText(NSLOCTEXT("CoACD","MaxVertexTip","限制单个凸包的顶点数量，影响碰撞精度与性能")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(8).MaxValue(512).Delta(8)
              .ToolTipText(NSLOCTEXT("CoACD","MaxVertexBoxTip","建议 64~256，UE碰撞推荐不超过 256"))
              .Value_Lambda([this]{return Args.MaxConvexHullVertex;})
              .OnValueChanged_Lambda([this,SaveNow](int32 v){Args.MaxConvexHullVertex=v; SaveNow();})
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(SCheckBox)
                .IsChecked_Lambda([this]{return Args.bDecimate?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this,SaveNow](ECheckBoxState st){Args.bDecimate=(st==ECheckBoxState::Checked); SaveNow();})
                .ToolTipText(NSLOCTEXT("CoACD","DecimateTip","开启顶点约束以控制凸包复杂度，可能略微降低精度"))
                [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","Decimate","启用顶点约束")) ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(SCheckBox)
                .IsChecked_Lambda([this]{return Args.bExtrude?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this,SaveNow](ECheckBoxState st){Args.bExtrude=(st==ECheckBoxState::Checked); SaveNow();})
                .ToolTipText(NSLOCTEXT("CoACD","ExtrudeTip","对凸包进行轻微挤出，可改善某些碰撞检测边界情况"))
                [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","ExtrudeLabel","启用挤出")) ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","ExtrudeMarginLabel","挤出边距 (0.001~0.1)"))
              .ToolTipText(NSLOCTEXT("CoACD","ExtrudeMarginTip","仅在启用挤出时有效；值过大可能导致碰撞体积增大")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<float>).MinValue(0.001f).MaxValue(0.1f).Delta(0.001f)
              .ToolTipText(NSLOCTEXT("CoACD","ExtrudeMarginBoxTip","建议 0.005~0.02"))
              .Value_Lambda([this]{return Args.ExtrudeMargin;})
              .OnValueChanged_Lambda([this,SaveNow](float v){Args.ExtrudeMargin=v; SaveNow();})
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","ApproxLabel","近似模式"))
              .ToolTipText(NSLOCTEXT("CoACD","ApproxModeTip","凸包（推荐）：精确分解；包围盒：快速但粗糙")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SComboBox<TSharedPtr<int32>>)
                .OptionsSource(&ApproximateModeOptions)
                .OnSelectionChanged_Lambda([this,SaveNow](TSharedPtr<int32> V, ESelectInfo::Type){ if (V.IsValid()){ Args.ApproximateMode=*V; SaveNow(); } })
                .OnGenerateWidget_Lambda([ApproxText](TSharedPtr<int32> V){ return SNew(STextBlock).Text(ApproxText(*V)); })
                .Content()[ SNew(STextBlock).Text_Lambda([this,ApproxText]{ return ApproxText(Args.ApproximateMode); }).ToolTipText(NSLOCTEXT("CoACD","ApproxModeBoxTip","凸包模式提供最佳精度；包围盒模式快速但粗糙")) ]
            ]

            // 其它控制项提示
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","Seed","随机种子")).ToolTipText(NSLOCTEXT("CoACD","SeedTip","相同参数+种子可复现实验结果")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(0).MaxValue(999999)
              .ToolTipText(NSLOCTEXT("CoACD","SeedBoxTip","0=随机"))
              .Value_Lambda([this]{return Args.Seed;}).OnValueChanged_Lambda([this,SaveNow](int32 v){Args.Seed=v; SaveNow();}) ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock).Text(NSLOCTEXT("CoACD","LOD","源 LOD 索引")).ToolTipText(NSLOCTEXT("CoACD","LODTip","通常选择 0；较高LOD可加速但精度降低")) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSpinBox<int32>).MinValue(0).MaxValue(8)
              .ToolTipText(NSLOCTEXT("CoACD","LODBoxTip","0~8"))
              .Value_Lambda([this]{return Args.SourceLODIndex;}).OnValueChanged_Lambda([this,SaveNow](int32 v){Args.SourceLODIndex=v; SaveNow();}) ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(SCheckBox)
              .IsChecked_Lambda([this]{return Args.bRemoveExistingCollision?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
              .OnCheckStateChanged_Lambda([this,SaveNow](ECheckBoxState s){Args.bRemoveExistingCollision=(s==ECheckBoxState::Checked); SaveNow();})
              .ToolTipText(NSLOCTEXT("CoACD","RemoveTip","执行前清空旧的简单碰撞"))
              .Content()[ SNew(STextBlock).Text(NSLOCTEXT("CoACD","RemoveOld","移除现有碰撞")) ]
            ]

            // 材质排除关键词（槽名/材质名/路径/索引）
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [ SNew(STextBlock)
                .Text(NSLOCTEXT("CoACD","MatExclude","材质排除关键词（槽名/材质名/路径/索引）"))
                .ToolTipText(NSLOCTEXT("CoACD","MatExcludeTip","支持中文与英文；可按槽名、材质名、材质路径或槽位索引匹配（大小写不敏感）。示例：玻璃、边框、Element 2、元素3、2、/Game/.../MI_玻璃"))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SVerticalBox)
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

            // 预设按钮
            + SVerticalBox::Slot().AutoHeight().Padding(0,10)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)
                [
                    SNew(SUniformGridPanel).SlotPadding(6)
                    + SUniformGridPanel::Slot(0,0)
                    [ SNew(SButton)
                        .Text(NSLOCTEXT("CoACD","PresetQ1","速度优先")) 
                        .OnClicked_Lambda([this,SaveNow](){ FX_CoACDConfigManager::ApplyPresetQuality1(Args); SaveNow(); return FReply::Handled(); }) ]
                    + SUniformGridPanel::Slot(1,0)
                    [ SNew(SButton)
                        .Text(NSLOCTEXT("CoACD","PresetQ2","均衡预设"))
                        .OnClicked_Lambda([this,SaveNow](){ FX_CoACDConfigManager::ApplyPresetQuality2(Args); SaveNow(); return FReply::Handled(); }) ]
                    + SUniformGridPanel::Slot(2,0)
                    [ SNew(SButton)
                        .Text(NSLOCTEXT("CoACD","PresetQ3","质量优先"))
                        .OnClicked_Lambda([this,SaveNow](){ FX_CoACDConfigManager::ApplyPresetQuality3(Args); SaveNow(); return FReply::Handled(); }) ]
                    + SUniformGridPanel::Slot(3,0)
                    [ SNew(SButton)
                        .Text(NSLOCTEXT("CoACD","PresetQ4","最高质量"))
                        .OnClicked_Lambda([this,SaveNow](){ FX_CoACDConfigManager::ApplyPresetQuality4(Args); SaveNow(); return FReply::Handled(); }) ]
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
    // 使用配置管理器加载默认值
    FX_CoACDArgs Defaults = FX_CoACDConfigManager::LoadSaved();
    // 以保存的为默认，用传入的覆盖（若调用方想自定义初始值）
    // 只有当传入的参数与默认构造值不同时，才认为是调用方想要覆盖
    FX_CoACDArgs DefaultConstructed;
    if (InOutArgs.Threshold != DefaultConstructed.Threshold) Defaults.Threshold = InOutArgs.Threshold;
    TSharedRef<SX_CoACDDialog> Widget = SNew(SX_CoACDDialog).Defaults(Defaults);
    TSharedRef<SWindow> Win = SNew(SWindow).Title(NSLOCTEXT("CoACD","Win","CoACD 算法参数 (SIGGRAPH 2022)"))
        .SizingRule(ESizingRule::Autosized)[Widget];
    Widget->DialogWindow = Win;
    FSlateApplication::Get().AddModalWindow(Win, FSlateApplication::Get().GetActiveTopLevelWindow());
    if (Widget->bConfirmed)
    {
        InOutArgs = Widget->Args;
        FX_CoACDConfigManager::Save(InOutArgs);
        FX_CoACDConfigManager::Flush();
        return true;
    }
    return false;
}

FReply SX_CoACDDialog::OnOK()
{
    bConfirmed = true;
    // 统一在确认时刷盘一次
    FX_CoACDConfigManager::Save(this->Args);
    FX_CoACDConfigManager::Flush();
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


