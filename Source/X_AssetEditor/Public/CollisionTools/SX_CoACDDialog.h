// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/SlateDelegates.h"
#include "CollisionTools/X_CoACDIntegration.h" // 需要完整类型：在SLATE_ARGUMENT与成员中按值使用
#include "Widgets/Views/SListView.h"

// 前向声明
class SWindow;

/**
 * CoACD 参数对话框
 * 负责对话框的显示和用户交互，委托具体功能给专门的组件
 */
class SX_CoACDDialog : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SX_CoACDDialog)
        : _Defaults()
    {}
        SLATE_ARGUMENT(FX_CoACDArgs, Defaults)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** 显示对话框 */
    static bool ShowDialog(FX_CoACDArgs& InOutArgs);

    /** 确认按钮回调 */
    FReply OnOK();
    
    /** 取消按钮回调 */
    FReply OnCancel();

private:
    /** 当前参数 */
    FX_CoACDArgs Args;
    
    /** 对话框窗口 */
    TSharedPtr<SWindow> DialogWindow;
    
    /** 是否确认 */
    bool bConfirmed = false;
    
    /** 下拉选项 */
    TArray<TSharedPtr<int32>> PreprocessModeOptions;
    TArray<TSharedPtr<int32>> ApproximateModeOptions;
    
    /** 材质关键词列表 */
    TSharedPtr<SListView<TSharedPtr<FString>>> MaterialKeywordListView;
    TArray<TSharedPtr<FString>> MaterialKeywordItems;
};


