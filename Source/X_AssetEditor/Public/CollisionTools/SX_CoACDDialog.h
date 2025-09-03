// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/SlateDelegates.h"
#include "CollisionTools/X_CoACDIntegration.h"
#include "Widgets/Views/SListView.h"

class SWindow;

/** CoACD 参数弹窗（中文标签，记忆上次使用） */
class SX_CoACDDialog : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SX_CoACDDialog){}
        SLATE_ARGUMENT(FX_CoACDArgs, Defaults)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    static bool ShowDialog(FX_CoACDArgs& InOutArgs);

    // 质量预设（集中定义，便于统一调整）
    static void ApplyPresetQuality1(FX_CoACDArgs& Args); // 偏向速度
    static void ApplyPresetQuality2(FX_CoACDArgs& Args); // 平衡1
    static void ApplyPresetQuality3(FX_CoACDArgs& Args); // 平衡2
    static void ApplyPresetQuality4(FX_CoACDArgs& Args); // 偏向质量

private:
    FX_CoACDArgs Args;
    TSharedPtr<SWindow> DialogWindow;
    bool bConfirmed = false;
    TArray<TSharedPtr<int32>> PreprocessModeOptions;
    TArray<TSharedPtr<int32>> ApproximateModeOptions;

    // 材质关键字条目式编辑支持
    TArray<TSharedPtr<FString>> MaterialKeywordItems;
    TSharedPtr<SListView<TSharedPtr<FString>>> MaterialKeywordListView;
    int32 PendingFocusIndex = -1;

    void RefreshMaterialKeywordItems();

    // 记忆保存/读取
    static FX_CoACDArgs LoadSaved();
    static void Save(const FX_CoACDArgs& ToSave);

    FReply OnOK();
    FReply OnCancel();
};


