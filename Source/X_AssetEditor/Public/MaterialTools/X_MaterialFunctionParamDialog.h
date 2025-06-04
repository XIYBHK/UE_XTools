// Copyright Epic Games, Inc. All Rights Reserved.

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

	SLATE_END_ARGS()

	/**
	 * 构造函数
	 * @param InArgs - Slate参数
	 * @param InParentWindow - 父窗口
	 * @param InStructOnScope - 结构体数据
	 * @param HiddenPropertyName - 需要隐藏的属性名称
	 */
	void Construct(const FArguments& InArgs, TWeakPtr<SWindow> InParentWindow, TSharedRef<FStructOnScope> InStructOnScope, FName HiddenPropertyName = NAME_None);

	/** 用户是否点击了确定按钮 */
	bool bOKPressed;

	/**
	 * 创建并显示参数对话框
	 * @param DialogTitle - 对话框标题
	 * @param InStructOnScope - 结构体数据
	 * @param HiddenPropertyName - 需要隐藏的属性名称
	 * @return 用户是否点击了确定按钮
	 */
	static bool ShowDialog(const FText& DialogTitle, TSharedRef<FStructOnScope> InStructOnScope, FName HiddenPropertyName = NAME_None);
};