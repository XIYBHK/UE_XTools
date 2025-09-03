// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/SlateDelegates.h"
#include "CollisionTools/X_CoACDIntegration.h"

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

private:
    FX_CoACDArgs Args;
    TSharedPtr<SWindow> DialogWindow;
    bool bConfirmed = false;
    TArray<TSharedPtr<int32>> PreprocessModeOptions;
    TArray<TSharedPtr<int32>> ApproximateModeOptions;

    // 记忆保存/读取
    static FX_CoACDArgs LoadSaved();
    static void Save(const FX_CoACDArgs& ToSave);

    FReply OnOK();
    FReply OnCancel();
};


