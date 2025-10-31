/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "CollisionTools/X_CollisionManager.h"
#include "AssetRegistry/AssetData.h"

class SWindow;
template<typename OptionType> class SComboBox;

/**
 * 碰撞设置对话框
 * 用于批量设置静态网格体的碰撞复杂度
 */
class X_ASSETEDITOR_API SX_CollisionSettingsDialog : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SX_CollisionSettingsDialog)
        : _SelectedAssets()
    {}
        SLATE_ARGUMENT(TArray<FAssetData>, SelectedAssets)
    SLATE_END_ARGS()

    /** 构造函数 */
    void Construct(const FArguments& InArgs);

    /**
     * 显示碰撞设置对话框
     * @param SelectedAssets 选中的资产列表
     * @return 是否确认应用设置
     */
    static bool ShowDialog(const TArray<FAssetData>& SelectedAssets);

private:
    /** 选中的资产列表 */
    TArray<FAssetData> SelectedAssets;

    /** 当前选择的碰撞复杂度 */
    EX_CollisionComplexity SelectedComplexity;

    /** 碰撞复杂度选项列表 */
    TArray<TSharedPtr<EX_CollisionComplexity>> ComplexityOptions;

    /** 碰撞复杂度下拉框 */
    TSharedPtr<SComboBox<TSharedPtr<EX_CollisionComplexity>>> ComplexityComboBox;

    /** 对话框窗口 */
    TSharedPtr<SWindow> DialogWindow;

    /** 是否确认应用 */
    bool bConfirmed;

    /**
     * 创建碰撞复杂度选项
     */
    void CreateComplexityOptions();

    /**
     * 获取碰撞复杂度显示文本
     * @param ComplexityType 碰撞复杂度类型
     * @return 显示文本
     */
    FText GetComplexityDisplayText(EX_CollisionComplexity ComplexityType) const;

    /**
     * 获取碰撞复杂度描述文本
     * @param ComplexityType 碰撞复杂度类型
     * @return 描述文本
     */
    FText GetComplexityDescriptionText(EX_CollisionComplexity ComplexityType) const;

    /**
     * 生成碰撞复杂度下拉框项目
     * @param InOption 选项
     * @return 生成的Widget
     */
    TSharedRef<SWidget> GenerateComplexityComboBoxItem(TSharedPtr<EX_CollisionComplexity> InOption);

    /**
     * 获取当前选择的碰撞复杂度文本
     * @return 当前选择的文本
     */
    FText GetSelectedComplexityText() const;

    /**
     * 碰撞复杂度选择变更回调
     * @param SelectedItem 选中的项目
     * @param SelectInfo 选择信息
     */
    void OnComplexitySelectionChanged(TSharedPtr<EX_CollisionComplexity> SelectedItem, ESelectInfo::Type SelectInfo);

    /**
     * 确认按钮点击回调
     * @return 回复结果
     */
    FReply OnConfirmClicked();

    /**
     * 取消按钮点击回调
     * @return 回复结果
     */
    FReply OnCancelClicked();

    /**
     * 获取静态网格体资产数量
     * @return 静态网格体数量
     */
    int32 GetStaticMeshCount() const;

    /**
     * 获取预览信息文本
     * @return 预览信息
     */
    FText GetPreviewText() const;
};
