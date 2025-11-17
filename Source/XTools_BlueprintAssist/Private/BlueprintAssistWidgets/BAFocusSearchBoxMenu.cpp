// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistWidgets/BAFocusSearchBoxMenu.h"

#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistUtils.h"
#include "BlueprintEditor.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"

void SBAFocusSearchBoxMenu::Construct(
	const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBAFilteredList<TSharedPtr<FSearchBoxStruct>>)
		.InitListItems(this, &SBAFocusSearchBoxMenu::InitListItems)
		.OnGenerateRow(this, &SBAFocusSearchBoxMenu::CreateItemWidget)
		.OnSelectItem(this, &SBAFocusSearchBoxMenu::SelectItem)
		.WidgetSize(GetWidgetSize())
		.MenuTitle(FString("Focus Search Box"))
	];
}

void SBAFocusSearchBoxMenu::InitListItems(TArray<TSharedPtr<FSearchBoxStruct>>& Items)
{
	TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow();

	TArray<TSharedPtr<SWidget>> DockTabs;
	FBAUtils::GetChildWidgets(Window, "SDockTab", DockTabs);

	for (TSharedPtr<SWidget> Widget : DockTabs)
	{
		TSharedPtr<SDockTab> DockTab = StaticCastSharedPtr<SDockTab>(Widget);

		if (DockTab.IsValid())
		{
			if (DockTab->GetTabRole() == ETabRole::MajorTab)
			{
				continue;
			}

			if (!DockTab->IsForeground())
			{
				continue;
			}

			TSet<TSharedPtr<SWidget>> SearchBoxes;
			FBAUtils::GetChildWidgetsByTypes(DockTab->GetContent(), FBAUtils::GetSearchBoxNames(), SearchBoxes);

			for (TSharedPtr<SWidget> SearchBoxWidget : SearchBoxes)
			{
				if (SearchBoxWidget->GetVisibility().IsVisible() &&
					SearchBoxWidget->IsEnabled() &&
					SearchBoxWidget->GetDesiredSize().SizeSquared() > 0 &&
					SearchBoxWidget->GetTickSpaceGeometry().GetAbsoluteSize().SizeSquared() > 0)
				{
					Items.Add(MakeShareable(new FSearchBoxStruct(SearchBoxWidget, DockTab)));
				}
			}
		}
	}
}

TSharedRef<ITableRow> SBAFocusSearchBoxMenu::CreateItemWidget(TSharedPtr<FSearchBoxStruct> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	static const FSlateBrush* SearchIcon = BA_STYLE_CLASS::Get().GetBrush("Symbols.SearchGlass");

	return
		SNew(STableRow<TSharedPtr<FString>>, OwnerTable).Padding(FMargin(2.0f, 4.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 2)
			[
				SNew(SImage).Image(SearchIcon)
			]
			+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center).FillWidth(1)
			[
				SNew(STextBlock).Text(FText::FromString(Item->GetTabLabel()))
			]
			+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center).FillWidth(1)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->ToString()))
				.Font(BA_STYLE_CLASS::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];
}

void SBAFocusSearchBoxMenu::SelectItem(TSharedPtr<FSearchBoxStruct> Item)
{
	if (Item->Widget.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(Item->Widget, EFocusCause::Navigation);
		Item->DockTab->FlashTab();
	}
}

/********************/
/* FSearchBoxStruct */
/********************/

FString FSearchBoxStruct::ToString() const
{
	TSharedPtr<SWidget> FoundWidget = FBAUtils::GetChildWidget(Widget, "SEditableText");
	TSharedPtr<SEditableText> EditableTextBox = StaticCastSharedPtr<SEditableText>(FoundWidget);
	if (EditableTextBox.IsValid())
	{
		return EditableTextBox->GetHintText().ToString();
	}

	return Widget->ToString();
}

FString FSearchBoxStruct::GetSearchText() const
{
	return ToString() + " " + GetKeySearchText();
}

FString FSearchBoxStruct::GetKeySearchText() const
{
	return GetTabLabel();
}

FString FSearchBoxStruct::GetTabLabel() const
{
	return DockTab->GetTabLabel().ToString();
}
