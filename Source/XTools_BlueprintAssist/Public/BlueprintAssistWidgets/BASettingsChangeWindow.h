// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistTypes.h"
#include "BlueprintAssistMisc/BASettingsBase.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"


class FUICommandInfo;
class SCheckBox;
class SDockTab;

class FBASettingChangeData : public TSharedFromThis<FBASettingChangeData>
{
public:
	FBASettingsChange Change;
	UObject* SettingsObj;
};

class SBASettingTableRow : public SMultiColumnTableRow<TSharedPtr<FBASettingChangeData>>
{
public:
	SLATE_BEGIN_ARGS(SBASettingTableRow)
	{
	}

	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, TSharedPtr<FBASettingChangeData> InData, const TSharedRef<STableViewBase>& OwnerTable);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	TSharedPtr<FBASettingChangeData> Data;
};

class SBASettingsListView : public SListView<TSharedPtr<FBASettingChangeData>>
{
	SLATE_BEGIN_ARGS(SBASettingsListView)
	{
	}

	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);
	virtual ~SBASettingsListView() override;


	void Refresh(UObject* NewSettings);

	FOnSelectionChanged& MyOnSelectionChanged() { return OnSelectionChanged; }

protected:
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FBASettingChangeData> InDisplayNode, const TSharedRef<STableViewBase>& OwnerTable);

private:
	FOnSelectionChanged OnSelectionChanged;

	TSharedPtr<SHeaderRow> HeaderRowWidget;
	TArray<TSharedPtr<FBASettingChangeData>> Rows;

	UObject* SettingsObj = nullptr;
	FDelegateHandle Handle;

	void CheckSettingsObjectChanged(UObject* Obj, struct FPropertyChangedEvent& Event);
};


class XTOOLS_BLUEPRINTASSIST_API SBASettingsChangeWindow : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SBASettingsChangeWindow)
		{
		}
	SLATE_END_ARGS()

	static FName GetTabId() { return FName("BASettingChanges"); }

	void Construct(const FArguments& InArgs);

	static TSharedRef<SDockTab> CreateTab(const FSpawnTabArgs& Args);

	TSharedRef<SWidget> MakeSettingMenuButton(UObject* SettingsObj);

	TSharedPtr<SWidgetSwitcher> WidgetSwitcher;
	TSharedPtr<SBASettingsListView> SettingsList;

	TArray<UObject*> SettingsObjects;
	UObject* ActiveSetting;

	void SetActiveSettings(UObject* Settings);
};
