/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#include "MaterialTools/X_MaterialFunctionParamDialog.h"
#include "MaterialTools/X_MaterialFunctionOperation.h"
#include "MaterialTools/X_MaterialFunctionParams.h"
#include "X_AssetEditor.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorStyleSet.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Editor/EditorEngine.h"
#include "Editor.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "X_MaterialFunctionParamDialog"

void SX_MaterialFunctionParamDialog::Construct(const FArguments& InArgs, TWeakPtr<SWindow> InParentWindow, TSharedRef<FStructOnScope> InStructOnScope, FName HiddenPropertyName)
{
	bOKPressed = false;
	ParentWindow = InParentWindow;
	StructOnScope = InStructOnScope;
	FunctionInputCount = InArgs._FunctionInputCount;
	FunctionOutputCount = InArgs._FunctionOutputCount;
	bHasMaterialAttributesOutput = InArgs._bHasMaterialAttributesOutput;
	OutputNamesText = InArgs._OutputNamesText;
	MaterialFunction = InArgs._MaterialFunction;

	// 初始化详情视图
	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bSearchInitialKeyFocus = true;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bShowOptions = false;
		DetailsViewArgs.bShowModifiedPropertiesOption = false;
		DetailsViewArgs.bShowObjectLabel = false;
		DetailsViewArgs.bForceHiddenPropertyVisibility = true;
		DetailsViewArgs.bShowScrollBar = false;
	}

	FStructureDetailsViewArgs StructureViewArgs;
	{
		StructureViewArgs.bShowObjects = true;
		StructureViewArgs.bShowAssets = true;
		StructureViewArgs.bShowClasses = true;
		StructureViewArgs.bShowInterfaces = true;
	}

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	TSharedRef<IStructureDetailsView> StructureDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, InStructOnScope);

	// 隐藏指定的属性
	if (HiddenPropertyName != NAME_None)
	{
		StructureDetailsView->GetDetailsView()->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([HiddenPropertyName](const FPropertyAndParent& InPropertyAndParent)
		{
			return InPropertyAndParent.Property.GetFName() != HiddenPropertyName;
		}));
	}

	StructureDetailsView->GetDetailsView()->ForceRefresh();

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 8.0f, 8.0f, 0.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			.Visibility(InArgs._SummaryText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
			[
				SNew(STextBlock)
				.Text(InArgs._SummaryText)
				.AutoWrapText(true)
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 6.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			.Visibility(TAttribute<EVisibility>::CreateLambda([this]()
			{
				return GetValidationText().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
			}))
			[
				SNew(STextBlock)
				.Text(TAttribute<FText>::CreateLambda([this]()
				{
					return GetValidationText();
				}))
				.ColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.4f, 1.0f))
				.AutoWrapText(true)
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 6.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			[
				SNew(STextBlock)
				.Text(TAttribute<FText>::CreateLambda([this]()
				{
					return GetExecutionPreviewText();
				}))
				.AutoWrapText(true)
			]
		]
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(SScrollBox)
			+SScrollBox::Slot()
			[
				StructureDetailsView->GetWidget().ToSharedRef()
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(2.0f)
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
					.ForegroundColor(FLinearColor::White)
					.ContentPadding(FMargin(6, 2))
					.IsEnabled(TAttribute<bool>::CreateLambda([this]()
					{
						return CanConfirm();
					}))
					.OnClicked_Lambda([this]()
					{
						return ConfirmAndClose();
					})
					.ToolTipText(InArgs._OkButtonTooltipText)
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "ContentBrowser.TopBar.Font")
						.Text(InArgs._OkButtonText.IsEmpty() ? LOCTEXT("OK", "确定") : InArgs._OkButtonText)
					]
				]
				+SHorizontalBox::Slot()
				.Padding(2.0f)
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "FlatButton")
					.ForegroundColor(FLinearColor::White)
					.ContentPadding(FMargin(6, 2))
					.OnClicked_Lambda([this]()
					{
						return CancelAndClose();
					})
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "ContentBrowser.TopBar.Font")
						.Text(LOCTEXT("Cancel", "取消"))
					]
				]
			]
		]
	];
}

FReply SX_MaterialFunctionParamDialog::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Enter || InKeyEvent.GetKey() == EKeys::Virtual_Accept)
	{
		return CanConfirm() ? ConfirmAndClose() : FReply::Handled();
	}

	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		return CancelAndClose();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

bool SX_MaterialFunctionParamDialog::CanConfirm() const
{
	return GetValidationText().IsEmpty();
}

FText SX_MaterialFunctionParamDialog::GetValidationText() const
{
	if (!StructOnScope.IsValid() || !StructOnScope->GetStructMemory())
	{
		return LOCTEXT("InvalidStructMemory", "无法读取材质函数参数，请关闭后重试。");
	}

	const FX_MaterialFunctionParams* Params = reinterpret_cast<const FX_MaterialFunctionParams*>(StructOnScope->GetStructMemory());
	if (!Params)
	{
		return LOCTEXT("InvalidStructParams", "材质函数参数无效。");
	}

	if (Params->NodeName.TrimStartAndEnd().IsEmpty())
	{
		return LOCTEXT("EmptyNodeName", "节点名称不能为空。");
	}

	if (Params->bSetupConnections && FunctionOutputCount <= 0)
	{
		return LOCTEXT("NoFunctionOutputs", "当前材质函数没有可用输出，无法执行自动连接。请关闭“自动连接到材质属性”后仅添加节点。");
	}

	if (Params->bUseMaterialAttributes && !bHasMaterialAttributesOutput)
	{
		return LOCTEXT("MissingMaterialAttributesOutput", "当前函数未检测到 Material Attributes 输出，不能使用该连接模式。");
	}

	if (Params->bSetupConnections
		&& !Params->bEnableSmartConnect
		&& !Params->bUseMaterialAttributes
		&& !Params->bConnectToBaseColor
		&& !Params->bConnectToMetallic
		&& !Params->bConnectToRoughness
		&& !Params->bConnectToNormal
		&& !Params->bConnectToEmissive
		&& !Params->bConnectToAO)
	{
		return LOCTEXT("NoManualTargets", "手动连接模式下至少需要选择一个最终材质属性，或改用智能连接 / Material Attributes。");
	}

	return FText::GetEmpty();
}

FText SX_MaterialFunctionParamDialog::GetExecutionPreviewText() const
{
	if (!StructOnScope.IsValid() || !StructOnScope->GetStructMemory())
	{
		return LOCTEXT("ExecutionPreviewUnavailable", "预计执行结果：当前无法生成预览。");
	}

	const FX_MaterialFunctionParams* Params = reinterpret_cast<const FX_MaterialFunctionParams*>(StructOnScope->GetStructMemory());
	if (!Params)
	{
		return LOCTEXT("ExecutionPreviewInvalidParams", "预计执行结果：当前无法读取参数。");
	}

	if (MaterialFunction.IsValid())
	{
		return FX_MaterialFunctionOperation::BuildConnectionPreviewText(MaterialFunction.Get(), *Params);
	}

	return FText::Format(
		LOCTEXT("ExecutionPreviewFallback", "预计执行结果：将基于当前参数尝试连接。函数输出：{0}"),
		OutputNamesText.IsEmpty() ? LOCTEXT("ExecutionPreviewNoOutputsFallback", "无") : OutputNamesText);
}

FReply SX_MaterialFunctionParamDialog::ConfirmAndClose()
{
	if (!CanConfirm())
	{
		return FReply::Handled();
	}

	bOKPressed = true;
	if (TSharedPtr<SWindow> PinnedWindow = ParentWindow.Pin())
	{
		PinnedWindow->RequestDestroyWindow();
	}
	return FReply::Handled();
}

FReply SX_MaterialFunctionParamDialog::CancelAndClose()
{
	if (TSharedPtr<SWindow> PinnedWindow = ParentWindow.Pin())
	{
		PinnedWindow->RequestDestroyWindow();
	}
	return FReply::Handled();
}

bool SX_MaterialFunctionParamDialog::ShowDialog(
	const FText& DialogTitle,
	TSharedRef<FStructOnScope> InStructOnScope,
	const FText& SummaryText,
	int32 InFunctionInputCount,
	int32 InFunctionOutputCount,
	bool bInHasMaterialAttributesOutput,
	const FText& InOutputNamesText,
	UMaterialFunctionInterface* InMaterialFunction,
	FName HiddenPropertyName)
{
	// 创建窗口
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(DialogTitle)
		.ClientSize(FVector2D(440, 420))
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	// 创建对话框
	TSharedPtr<SX_MaterialFunctionParamDialog> Dialog;
	Window->SetContent(
		SAssignNew(Dialog, SX_MaterialFunctionParamDialog, Window, InStructOnScope, HiddenPropertyName)
		.OkButtonText(LOCTEXT("OKButton", "确定"))
		.OkButtonTooltipText(LOCTEXT("OKButtonTooltip", "应用参数并添加材质函数"))
		.DialogTitle(DialogTitle)
		.SummaryText(SummaryText)
		.FunctionInputCount(InFunctionInputCount)
		.FunctionOutputCount(InFunctionOutputCount)
		.bHasMaterialAttributesOutput(bInHasMaterialAttributesOutput)
		.OutputNamesText(InOutputNamesText)
		.MaterialFunction(InMaterialFunction)
	);

	// 显示模态窗口
	GEditor->EditorAddModalWindow(Window);

	// 返回用户是否点击了确定按钮
	return Dialog->bOKPressed;
}

#undef LOCTEXT_NAMESPACE
