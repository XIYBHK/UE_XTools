// Copyright Epic Games, Inc. All Rights Reserved.

#include "CollisionTools/X_CollisionSettingsDialog.h"
#include "CollisionTools/X_CollisionManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorStyleSet.h"
#include "Styling/SlateStyle.h"

#define LOCTEXT_NAMESPACE "X_CollisionSettingsDialog"

void SX_CollisionSettingsDialog::Construct(const FArguments& InArgs)
{
    SelectedAssets = InArgs._SelectedAssets;
    SelectedComplexity = EX_CollisionComplexity::UseDefault;
    bConfirmed = false;

    CreateComplexityOptions();

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(16.0f)
        [
            SNew(SBox)
            .MinDesiredWidth(400.0f)
            .MinDesiredHeight(250.0f)
            [
                SNew(SVerticalBox)

                // 标题
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 16)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("DialogTitle", "批量设置碰撞复杂度"))
                    .Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
                    .Justification(ETextJustify::Center)
                ]

                // 预览信息
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 16)
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush("DetailsView.CategoryMiddle"))
                    .Padding(8.0f)
                    [
                        SNew(STextBlock)
                        .Text(this, &SX_CollisionSettingsDialog::GetPreviewText)
                        .AutoWrapText(true)
                    ]
                ]

                // 碰撞复杂度选择
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 8)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ComplexityLabel", "碰撞复杂度设置:"))
                    .Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 16)
                [
                    SAssignNew(ComplexityComboBox, SComboBox<TSharedPtr<EX_CollisionComplexity>>)
                    .OptionsSource(&ComplexityOptions)
                    .OnGenerateWidget(this, &SX_CollisionSettingsDialog::GenerateComplexityComboBoxItem)
                    .OnSelectionChanged(this, &SX_CollisionSettingsDialog::OnComplexitySelectionChanged)
                    .InitiallySelectedItem(ComplexityOptions[0])
                    [
                        SNew(STextBlock)
                        .Text(this, &SX_CollisionSettingsDialog::GetSelectedComplexityText)
                    ]
                ]

                // 说明文本
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                .Padding(0, 0, 0, 16)
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush("DetailsView.CategoryBottom"))
                    .Padding(8.0f)
                    [
                        SNew(SScrollBox)
                        + SScrollBox::Slot()
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("ComplexityDescription", 
                                "碰撞复杂度说明:\n\n"
                                "• 项目默认: 使用项目设置中的默认碰撞设置\n"
                                "• 简单与复杂: 同时使用简单碰撞和复杂碰撞\n"
                                "• 将简单碰撞用作复杂碰撞: 复杂查询使用简单碰撞形状\n"
                                "• 将复杂碰撞用作简单碰撞: 简单查询使用复杂碰撞形状\n\n"
                                "注意: 此操作将修改所有选中的静态网格体资产"))
                            .AutoWrapText(true)
                            .Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
                        ]
                    ]
                ]

                // 按钮区域
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SUniformGridPanel)
                    .SlotPadding(8.0f)

                    + SUniformGridPanel::Slot(0, 0)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("ConfirmButton", "应用设置"))
                        .HAlign(HAlign_Center)
                        .OnClicked(this, &SX_CollisionSettingsDialog::OnConfirmClicked)
                        .IsEnabled_Lambda([this]() { return GetStaticMeshCount() > 0; })
                    ]

                    + SUniformGridPanel::Slot(1, 0)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("CancelButton", "取消"))
                        .HAlign(HAlign_Center)
                        .OnClicked(this, &SX_CollisionSettingsDialog::OnCancelClicked)
                    ]
                ]
            ]
        ]
    ];
}

bool SX_CollisionSettingsDialog::ShowDialog(const TArray<FAssetData>& SelectedAssets)
{
    TSharedRef<SX_CollisionSettingsDialog> DialogWidget = SNew(SX_CollisionSettingsDialog)
        .SelectedAssets(SelectedAssets);

    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(LOCTEXT("WindowTitle", "碰撞复杂度设置"))
        .SizingRule(ESizingRule::UserSized)
        .ClientSize(FVector2D(450, 350))
        .SupportsMaximize(false)
        .SupportsMinimize(false)
        [
            DialogWidget
        ];

    DialogWidget->DialogWindow = Window;

    FSlateApplication::Get().AddModalWindow(Window, FSlateApplication::Get().GetActiveTopLevelWindow());

    return DialogWidget->bConfirmed;
}

void SX_CollisionSettingsDialog::CreateComplexityOptions()
{
    ComplexityOptions.Empty();
    ComplexityOptions.Add(MakeShareable(new EX_CollisionComplexity(EX_CollisionComplexity::UseDefault)));
    ComplexityOptions.Add(MakeShareable(new EX_CollisionComplexity(EX_CollisionComplexity::UseSimpleAndComplex)));
    ComplexityOptions.Add(MakeShareable(new EX_CollisionComplexity(EX_CollisionComplexity::UseSimpleAsComplex)));
    ComplexityOptions.Add(MakeShareable(new EX_CollisionComplexity(EX_CollisionComplexity::UseComplexAsSimple)));
}

FText SX_CollisionSettingsDialog::GetComplexityDisplayText(EX_CollisionComplexity ComplexityType) const
{
    switch (ComplexityType)
    {
        case EX_CollisionComplexity::UseDefault:
            return LOCTEXT("UseDefault", "项目默认");
        case EX_CollisionComplexity::UseSimpleAndComplex:
            return LOCTEXT("UseSimpleAndComplex", "简单与复杂");
        case EX_CollisionComplexity::UseSimpleAsComplex:
            return LOCTEXT("UseSimpleAsComplex", "将简单碰撞用作复杂碰撞");
        case EX_CollisionComplexity::UseComplexAsSimple:
            return LOCTEXT("UseComplexAsSimple", "将复杂碰撞用作简单碰撞");
        default:
            return LOCTEXT("Unknown", "未知");
    }
}

FText SX_CollisionSettingsDialog::GetComplexityDescriptionText(EX_CollisionComplexity ComplexityType) const
{
    switch (ComplexityType)
    {
        case EX_CollisionComplexity::UseDefault:
            return LOCTEXT("UseDefaultDesc", "使用项目设置中的默认碰撞设置");
        case EX_CollisionComplexity::UseSimpleAndComplex:
            return LOCTEXT("UseSimpleAndComplexDesc", "同时使用简单碰撞和复杂碰撞，提供最佳精度但性能开销较大");
        case EX_CollisionComplexity::UseSimpleAsComplex:
            return LOCTEXT("UseSimpleAsComplexDesc", "复杂查询使用简单碰撞形状，提高性能但可能降低精度");
        case EX_CollisionComplexity::UseComplexAsSimple:
            return LOCTEXT("UseComplexAsSimpleDesc", "简单查询使用复杂碰撞形状，提高精度但可能影响性能");
        default:
            return LOCTEXT("UnknownDesc", "未知设置");
    }
}

TSharedRef<SWidget> SX_CollisionSettingsDialog::GenerateComplexityComboBoxItem(TSharedPtr<EX_CollisionComplexity> InOption)
{
    return SNew(STextBlock)
        .Text(GetComplexityDisplayText(*InOption))
        .ToolTipText(GetComplexityDescriptionText(*InOption));
}

FText SX_CollisionSettingsDialog::GetSelectedComplexityText() const
{
    return GetComplexityDisplayText(SelectedComplexity);
}

void SX_CollisionSettingsDialog::OnComplexitySelectionChanged(TSharedPtr<EX_CollisionComplexity> SelectedItem, ESelectInfo::Type SelectInfo)
{
    if (SelectedItem.IsValid())
    {
        SelectedComplexity = *SelectedItem;
    }
}

FReply SX_CollisionSettingsDialog::OnConfirmClicked()
{
    bConfirmed = true;
    
    // 执行碰撞复杂度设置
    FX_CollisionManager::SetCollisionComplexity(SelectedAssets, SelectedComplexity);
    
    if (DialogWindow.IsValid())
    {
        DialogWindow->RequestDestroyWindow();
    }
    
    return FReply::Handled();
}

FReply SX_CollisionSettingsDialog::OnCancelClicked()
{
    bConfirmed = false;
    
    if (DialogWindow.IsValid())
    {
        DialogWindow->RequestDestroyWindow();
    }
    
    return FReply::Handled();
}

int32 SX_CollisionSettingsDialog::GetStaticMeshCount() const
{
    int32 Count = 0;
    for (const FAssetData& Asset : SelectedAssets)
    {
        if (FX_CollisionManager::IsStaticMeshAsset(Asset))
        {
            Count++;
        }
    }
    return Count;
}

FText SX_CollisionSettingsDialog::GetPreviewText() const
{
    int32 StaticMeshCount = GetStaticMeshCount();
    int32 TotalCount = SelectedAssets.Num();
    int32 SkippedCount = TotalCount - StaticMeshCount;
    
    FString PreviewText = FString::Printf(TEXT("选中 %d 个资产，其中 %d 个静态网格体将被处理"), TotalCount, StaticMeshCount);
    
    if (SkippedCount > 0)
    {
        PreviewText += FString::Printf(TEXT("，%d 个非静态网格体将被跳过"), SkippedCount);
    }
    
    return FText::FromString(PreviewText);
}

#undef LOCTEXT_NAMESPACE
