// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BAFilteredList.h"
#include "BlueprintAssistTypes.h"

class SBlueprintContextTargetMenu;
class SWidget;
class FUICommandInfo;

struct FBAHotkeyItem : IBAFilteredListItem
{
	FName CommandName;
	FText CommandChord;
	FText CommandDesc;
	FText CommandLabel;
	FString SearchText;
	FName BindingContext;
	TSharedPtr<const FUICommandInfo> CommandInfo;
	TSharedPtr<FUICommandList> CommandList;

	FBAHotkeyItem(TSharedPtr<FUICommandInfo> Command, TSharedPtr<FUICommandList> InCommandList);
	FBAHotkeyItem(UObject* SettingObj, FStructProperty* ChordProp, const FInputChord& Chord);

	virtual FString ToString() const override { return CommandLabel.ToString(); }

	virtual FString GetSearchText() const override;
	bool CanExecute() const;
	bool Execute();
};

class XTOOLS_BLUEPRINTASSIST_API SBAHotkeyMenu : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SBAHotkeyMenu) {}
	SLATE_END_ARGS()

	static FVector2D GetWidgetSize() { return FVector2D(700, 600); }

	void Construct(const FArguments& InArgs);

	void InitListItems(TArray<TSharedPtr<FBAHotkeyItem>>& Items);

	void AddInputChords(TArray<TSharedPtr<FBAHotkeyItem>>& Items);

	TSharedPtr<FUICommandList> FindCommandListForCommand(TSharedPtr<FUICommandInfo> Command, TArray<TSharedPtr<FUICommandList>>& AvailableLists);

	TSharedRef<ITableRow> CreateItemWidget(TSharedPtr<FBAHotkeyItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const;

	void SelectItem(TSharedPtr<FBAHotkeyItem> Item);

	TSharedPtr<SBAFilteredList<TSharedPtr<FBAHotkeyItem>>> FilteredList;

	void RefreshList();

	FBASettingsPropertyHook SettingsPropertyHook;
};
