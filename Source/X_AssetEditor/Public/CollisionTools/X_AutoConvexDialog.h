// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SWindow;

/**
 * 自动凸包参数对话框（简易）
 */
class X_ASSETEDITOR_API SX_AutoConvexDialog : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SX_AutoConvexDialog)
        : _DefaultHullCount(1)
        , _DefaultMaxHullVerts(16)
        , _DefaultHullPrecision(100000)
    {}
        SLATE_ARGUMENT(int32, DefaultHullCount)
        SLATE_ARGUMENT(int32, DefaultMaxHullVerts)
        SLATE_ARGUMENT(int32, DefaultHullPrecision)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** 显示对话框，返回是否确认；输出参数返回配置值 */
    static bool ShowDialog(int32& OutHullCount, int32& OutMaxHullVerts, int32& OutHullPrecision,
                           int32 DefaultHullCount = 1, int32 DefaultMaxHullVerts = 16, int32 DefaultHullPrecision = 100000);

private:
    int32 HullCount;
    int32 MaxHullVerts;
    int32 HullPrecision;

    TSharedPtr<SWindow> DialogWindow;
    bool bConfirmed = false;

private:
    FReply OnConfirm();
    FReply OnCancel();
};


