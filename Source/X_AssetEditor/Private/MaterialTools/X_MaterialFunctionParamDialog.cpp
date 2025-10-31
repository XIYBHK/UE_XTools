/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#include "MaterialTools/X_MaterialFunctionParamDialog.h"
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

#define LOCTEXT_NAMESPACE "X_MaterialFunctionParamDialog"

void SX_MaterialFunctionParamDialog::Construct(const FArguments& InArgs, TWeakPtr<SWindow> InParentWindow, TSharedRef<FStructOnScope> InStructOnScope, FName HiddenPropertyName)
{
	bOKPressed = false;

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
			if (InPropertyAndParent.Property.GetFName() == HiddenPropertyName)
			{
				return false;
			}

			if (InPropertyAndParent.Property.HasAnyPropertyFlags(CPF_Parm))
			{
				return true;
			}

			for (const FProperty* Parent : InPropertyAndParent.ParentProperties)
			{
				if (Parent->HasAnyPropertyFlags(CPF_Parm))
				{
					return true;
				}
			}

			return false;
		}));
	}

	StructureDetailsView->GetDetailsView()->ForceRefresh();

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
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
					.OnClicked_Lambda([this, InParentWindow]()
					{
						if(InParentWindow.IsValid())
						{
							InParentWindow.Pin()->RequestDestroyWindow();
						}
						bOKPressed = true;
						return FReply::Handled(); 
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
					.OnClicked_Lambda([InParentWindow]()
					{ 
						if(InParentWindow.IsValid())
						{
							InParentWindow.Pin()->RequestDestroyWindow();
						}
						return FReply::Handled(); 
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

bool SX_MaterialFunctionParamDialog::ShowDialog(const FText& DialogTitle, TSharedRef<FStructOnScope> InStructOnScope, FName HiddenPropertyName)
{
	// 创建窗口
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(DialogTitle)
		.ClientSize(FVector2D(400, 300))
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	// 创建对话框
	TSharedPtr<SX_MaterialFunctionParamDialog> Dialog;
	Window->SetContent(
		SAssignNew(Dialog, SX_MaterialFunctionParamDialog, Window, InStructOnScope, HiddenPropertyName)
		.OkButtonText(LOCTEXT("OKButton", "确定"))
		.OkButtonTooltipText(LOCTEXT("OKButtonTooltip", "应用参数并添加材质函数"))
		.DialogTitle(DialogTitle)
	);

	// 显示模态窗口
	GEditor->EditorAddModalWindow(Window);

	// 返回用户是否点击了确定按钮
	return Dialog->bOKPressed;
}

#undef LOCTEXT_NAMESPACE