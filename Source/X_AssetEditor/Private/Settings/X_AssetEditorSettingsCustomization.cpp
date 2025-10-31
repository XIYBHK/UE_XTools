/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "Settings/X_AssetEditorSettingsCustomization.h"
#include "Settings/X_AssetEditorSettings.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "X_AssetEditorSettingsCustomization"

TSharedRef<IDetailCustomization> FX_AssetEditorSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FX_AssetEditorSettingsCustomization);
}

void FX_AssetEditorSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// 缓存DetailBuilder
	CachedDetailBuilder = &DetailBuilder;

	// 获取所有分类名称
	TArray<FName> CategoryNames;
	DetailBuilder.GetCategoryNames(CategoryNames);

	// 定义分类的显示顺序（使用字符串比较，避免 FName 编码问题）
	TArray<FString> DesiredCategoryOrder;
	DesiredCategoryOrder.Add(TEXT("资产命名通用"));
	DesiredCategoryOrder.Add(TEXT("资产命名排除规则"));
	DesiredCategoryOrder.Add(TEXT("资产命名前缀规则"));
	DesiredCategoryOrder.Add(TEXT("子系统开关"));
	DesiredCategoryOrder.Add(TEXT("工具功能"));

	// 按顺序展开分类
	for (const FString& DesiredName : DesiredCategoryOrder)
	{
		// 查找匹配的 FName
		for (const FName& CategoryName : CategoryNames)
		{
			if (CategoryName.ToString() == DesiredName)
			{
				IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(CategoryName);
				Category.InitiallyCollapsed(false);
				break;
			}
		}
	}

	// 在前缀规则分类中添加使用提示和重置按钮
	FName PrefixRulesCategoryName;
	for (const FName& CategoryName : CategoryNames)
	{
		if (CategoryName.ToString() == TEXT("资产命名前缀规则"))
		{
			PrefixRulesCategoryName = CategoryName;
			break;
		}
	}
	
	IDetailCategoryBuilder& PrefixRulesCategory = DetailBuilder.EditCategory(PrefixRulesCategoryName);

	// 添加父类前缀映射的使用提示
	PrefixRulesCategory.AddCustomRow(LOCTEXT("ParentClassMappingTipsRow", "父类前缀映射使用提示"))
		.WholeRowContent()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 4)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ParentClassMappingTipsTitle", "父类前缀映射 - 使用提示"))
					.Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
					.ColorAndOpacity(FLinearColor(0.8f, 0.9f, 1.0f, 1.0f))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 2)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ParentClassMappingTipsDesc",
						"使用父类前缀映射为蓝图、数据资产子类添加自定义前缀。\n"
						"系统会自动检测父C++类并应用对应的前缀。"))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 8, 0, 2)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ParentClassMappingExampleTitle", "示例:"))
					.Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 2)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ParentClassMappingExample1", "• \"MyGameDataAsset\" → \"GD_\"  (自定义游戏数据)"))
					.ColorAndOpacity(FLinearColor(0.7f, 0.9f, 0.7f, 1.0f))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 2)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ParentClassMappingExample2", "• \"MyWeaponData\" → \"WD_\"  (武器数据)"))
					.ColorAndOpacity(FLinearColor(0.7f, 0.9f, 0.7f, 1.0f))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 2)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ParentClassMappingExample3", "• \"MyCharacterData\" → \"CD_\"  (角色数据)"))
					.ColorAndOpacity(FLinearColor(0.7f, 0.9f, 0.7f, 1.0f))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 8, 0, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ParentClassMappingNote",
						"注意: 更具体的类名优先级更高 (例如, \"SceneComponent\" 优先于 \"ActorComponent\")。"))
					.AutoWrapText(true)
					.ColorAndOpacity(FLinearColor(1.0f, 0.9f, 0.6f, 1.0f))
					.Font(FAppStyle::GetFontStyle("PropertyWindow.ItalicFont"))
				]
			]
		];

	// 添加"重置为默认值"按钮
	PrefixRulesCategory.AddCustomRow(LOCTEXT("ResetButtonRow", "重置前缀映射"))
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 5, 0, 5)
			[
				SNew(SButton)
				.Text(LOCTEXT("ResetPrefixMappingsButton", "重置所有前缀映射为默认值"))
				.ToolTipText(LOCTEXT("ResetPrefixMappingsTooltip", "重置所有前缀映射（86种资产类型 + 30+父类映射）为默认值。这将清除您的自定义修改。"))
				.OnClicked(this, &FX_AssetEditorSettingsCustomization::OnResetPrefixMappingsClicked)
			]
		];
}

FReply FX_AssetEditorSettingsCustomization::OnResetPrefixMappingsClicked()
{
	if (UX_AssetEditorSettings* Settings = GetMutableDefault<UX_AssetEditorSettings>())
	{
		// 清除现有映射
		Settings->AssetPrefixMappings.Empty();
		Settings->ParentClassPrefixMappings.Empty();

		// 重新初始化为默认值
		Settings->InitializeDefaultPrefixMappings();

		// 标记为已修改并保存
		Settings->SaveConfig();

		// 刷新详情面板
		if (CachedDetailBuilder)
		{
			CachedDetailBuilder->ForceRefreshDetails();
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

