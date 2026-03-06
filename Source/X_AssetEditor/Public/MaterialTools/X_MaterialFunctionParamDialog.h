/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "PropertyEditorModule.h"
#include "IStructureDetailsView.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

class UMaterialFunctionInterface;

/**
 * 材质函数参数对话框
 * 用于在添加材质函数到材质时配置参数
 */
class SX_MaterialFunctionParamDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SX_MaterialFunctionParamDialog) {}

	/** 确定按钮文本 */
	SLATE_ARGUMENT(FText, OkButtonText)

	/** 确定按钮提示文本 */
	SLATE_ARGUMENT(FText, OkButtonTooltipText)

	/** 对话框标题 */
	SLATE_ARGUMENT(FText, DialogTitle)

	/** 顶部摘要文本 */
	SLATE_ARGUMENT(FText, SummaryText)

	/** 所选函数输入数量 */
	SLATE_ARGUMENT(int32, FunctionInputCount)

	/** 所选函数输出数量 */
	SLATE_ARGUMENT(int32, FunctionOutputCount)

	/** 是否包含 Material Attributes 输出 */
	SLATE_ARGUMENT(bool, bHasMaterialAttributesOutput)

	/** 输出名称摘要 */
	SLATE_ARGUMENT(FText, OutputNamesText)

	/** 当前选中的材质函数 */
	SLATE_ARGUMENT(UMaterialFunctionInterface*, MaterialFunction)

	SLATE_END_ARGS()

	/**
	 * 构造函数
	 * @param InArgs - Slate参数
	 * @param InParentWindow - 父窗口
	 * @param InStructOnScope - 结构体数据
	 * @param HiddenPropertyName - 需要隐藏的属性名称
	 */
	void Construct(const FArguments& InArgs, TWeakPtr<SWindow> InParentWindow, TSharedRef<FStructOnScope> InStructOnScope, FName HiddenPropertyName = NAME_None);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** 用户是否点击了确定按钮 */
	bool bOKPressed;

	/**
	 * 创建并显示参数对话框
	 * @param DialogTitle - 对话框标题
	 * @param InStructOnScope - 结构体数据
	 * @param SummaryText - 顶部摘要文本
	 * @param HiddenPropertyName - 需要隐藏的属性名称
	 * @return 用户是否点击了确定按钮
	 */
	static bool ShowDialog(
		const FText& DialogTitle,
		TSharedRef<FStructOnScope> InStructOnScope,
		const FText& SummaryText = FText::GetEmpty(),
		int32 FunctionInputCount = 0,
		int32 FunctionOutputCount = 0,
		bool bHasMaterialAttributesOutput = false,
		const FText& OutputNamesText = FText::GetEmpty(),
		UMaterialFunctionInterface* MaterialFunction = nullptr,
		FName HiddenPropertyName = NAME_None);

private:
	bool CanConfirm() const;
	FText GetValidationText() const;
	FReply ConfirmAndClose();
	FReply CancelAndClose();

private:
	TWeakPtr<SWindow> ParentWindow;
	TSharedPtr<FStructOnScope> StructOnScope;
	int32 FunctionInputCount = 0;
	int32 FunctionOutputCount = 0;
	bool bHasMaterialAttributesOutput = false;
	FText OutputNamesText;
	TWeakObjectPtr<UMaterialFunctionInterface> MaterialFunction;

	FText GetExecutionPreviewText() const;
};
