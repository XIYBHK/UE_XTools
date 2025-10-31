/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#include "CollisionTools/X_AutoConvexDialog.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"

void SX_AutoConvexDialog::Construct(const FArguments& InArgs)
{
    HullCount = InArgs._DefaultHullCount;
    MaxHullVerts = InArgs._DefaultMaxHullVerts;
    HullPrecision = InArgs._DefaultHullPrecision;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(16.0f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 12)
            [
                SNew(STextBlock)
                .Text(NSLOCTEXT("X_AutoConvexDialog", "Title", "自动凸包碰撞参数"))
                .Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 4)
            [
                SNew(STextBlock)
                .Text(NSLOCTEXT("X_AutoConvexDialog", "HullCount", "HullCount (最大凸包数)"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SSpinBox<int32>)
                .MinValue(1)
                .MaxValue(64)
                .Value_Lambda([this]() { return HullCount; })
                .OnValueChanged_Lambda([this](int32 V) { HullCount = V; })
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 8, 0, 0)
            [
                SNew(STextBlock)
                .Text(NSLOCTEXT("X_AutoConvexDialog", "MaxHullVerts", "MaxHullVerts (每个凸包最大点数)"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SSpinBox<int32>)
                .MinValue(4)
                .MaxValue(64)
                .Value_Lambda([this]() { return MaxHullVerts; })
                .OnValueChanged_Lambda([this](int32 V) { MaxHullVerts = V; })
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 8, 0, 0)
            [
                SNew(STextBlock)
                .Text(NSLOCTEXT("X_AutoConvexDialog", "HullPrecision", "HullPrecision (体素精度)"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SSpinBox<int32>)
                .MinValue(1000)
                .MaxValue(1000000)
                .Delta(1000)
                .Value_Lambda([this]() { return HullPrecision; })
                .OnValueChanged_Lambda([this](int32 V) { HullPrecision = V; })
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 12, 0, 0)
            [
                SNew(SUniformGridPanel)
                .SlotPadding(8.0f)
                + SUniformGridPanel::Slot(0,0)
                [
                    SNew(SButton)
                    .Text(NSLOCTEXT("X_AutoConvexDialog", "OK", "开始"))
                    .OnClicked(this, &SX_AutoConvexDialog::OnConfirm)
                ]
                + SUniformGridPanel::Slot(1,0)
                [
                    SNew(SButton)
                    .Text(NSLOCTEXT("X_AutoConvexDialog", "Cancel", "取消"))
                    .OnClicked(this, &SX_AutoConvexDialog::OnCancel)
                ]
            ]
        ]
    ];
}

bool SX_AutoConvexDialog::ShowDialog(int32& OutHullCount, int32& OutMaxHullVerts, int32& OutHullPrecision,
                                     int32 DefaultHullCount, int32 DefaultMaxHullVerts, int32 DefaultHullPrecision)
{
    TSharedRef<SX_AutoConvexDialog> DialogWidget = SNew(SX_AutoConvexDialog)
        .DefaultHullCount(DefaultHullCount)
        .DefaultMaxHullVerts(DefaultMaxHullVerts)
        .DefaultHullPrecision(DefaultHullPrecision);

    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(NSLOCTEXT("X_AutoConvexDialog", "WindowTitle", "自动凸包碰撞"))
        .SizingRule(ESizingRule::Autosized)
        [ DialogWidget ];

    DialogWidget->DialogWindow = Window;
    FSlateApplication::Get().AddModalWindow(Window, FSlateApplication::Get().GetActiveTopLevelWindow());

    if (DialogWidget->bConfirmed)
    {
        OutHullCount = DialogWidget->HullCount;
        OutMaxHullVerts = DialogWidget->MaxHullVerts;
        OutHullPrecision = DialogWidget->HullPrecision;
        return true;
    }
    return false;
}

FReply SX_AutoConvexDialog::OnConfirm()
{
    bConfirmed = true;
    if (DialogWindow.IsValid())
    {
        DialogWindow->RequestDestroyWindow();
    }
    return FReply::Handled();
}

FReply SX_AutoConvexDialog::OnCancel()
{
    bConfirmed = false;
    if (DialogWindow.IsValid())
    {
        DialogWindow->RequestDestroyWindow();
    }
    return FReply::Handled();
}


