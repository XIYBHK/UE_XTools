// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STreeView.h"

struct FBAConfigFileItem
{
	const UClass* SettingClass;
	FString Name;
	FString FullPath;
	bool bExists;
	bool bHasSection;
	bool bIsFinalIni;

	TPair<FString, FLinearColor> GetStateValues()
	{
		if (bHasSection)
		{
			return {"MODIFY", FLinearColor::Green};
		}
		else if (bExists)
		{
			return {"EXIST", FLinearColor::Yellow};
		}
		else
		{
			return {"MISS", FLinearColor::White};
		}
	}
};

struct FBAConfigPropertyItem
{
	FString DisplayText;
	FString Value;
	FString DefaultValue;
	TArray<TSharedPtr<FBAConfigPropertyItem>> Children;
	FString CurrValue;

	TSharedPtr<FBAConfigFileItem> FileItem;
};

class SBAConfigFileRow : public SMultiColumnTableRow<TSharedPtr<FBAConfigFileItem>>
{
public:
	SLATE_BEGIN_ARGS(SBAConfigFileRow)
		{
		}

		SLATE_ARGUMENT(TSharedPtr<FBAConfigFileItem>, Item)
		SLATE_ARGUMENT(TSharedPtr<class SBAConfigViewer>, ConfigViewer)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable);
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	TSharedPtr<class SBAConfigViewer> ConfigViewer;
	TSharedPtr<FBAConfigFileItem> Item;

	FReply OnOpenFile();
	FReply OnSaveSettings();
	FReply OnCreateFile();
	FReply OnReloadSettings();
};

class SBAConfigViewer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBAConfigViewer)
		{
		}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	SBAConfigViewer();
	virtual ~SBAConfigViewer() override;
	static TSharedRef<SDockTab> CreateTab(const FSpawnTabArgs& Args);
	static FName GetTabId() { return FName("BASettingWindow"); }

	void HandleNewEntryClassSelected(const UClass* Class);
	void RefreshSourceFiles();
	void RefreshPropertyTree();

	TSharedRef<ITableRow> OnGenerateFileRow(TSharedPtr<FBAConfigFileItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> OnGeneratePropertyRow(TSharedPtr<FBAConfigPropertyItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	FReply OnPropertyTreeItemDoubleClicked(const FGeometry&, const FPointerEvent&, TSharedPtr<FBAConfigPropertyItem> Item);

	void GetPropertyChildren(TSharedPtr<FBAConfigPropertyItem> Item, TArray<TSharedPtr<FBAConfigPropertyItem>>& OutChildren);

	/* File related */
	bool bIncludeFinalIni = true;
	bool bHideMissingFiles = true;
	bool bShowModifyingFiles = true;
	TSharedPtr<SSearchBox> FileSearchBox;
	FString FileSearchFilter;

	TArray<TSharedPtr<FBAConfigFileItem>> FullSourceFiles;
	TArray<TSharedPtr<FBAConfigFileItem>> FilteredSourceFiles;
	TSharedPtr<SListView<TSharedPtr<FBAConfigFileItem>>> FileListView;

	void OnFileSearchTextChanged(const FText& InFilterText);
	void OnFinalIniChanged(ECheckBoxState NewState);
	void OnRemoveMissingChanged(ECheckBoxState NewState);
	void OnShowModifyingChanged(ECheckBoxState NewState);

	void ApplyFileFilter();

	/* Property Related */
	bool bShowModifiedProperties = true;
	TSharedPtr<SSearchBox> PropertySearchBox;
	FString PropertySearchFilter;

	bool bDirtyPropertyTree = false;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	TArray<TSharedPtr<FBAConfigPropertyItem>> FullPropertyTreeRoot;
	TArray<TSharedPtr<FBAConfigPropertyItem>> FilteredPropertyTreeRoot;
	TSharedPtr<STreeView<TSharedPtr<FBAConfigPropertyItem>>> PropertyTreeView;

	void OnPropertySearchTextChanged(const FText& InFilterText);
	void OnReadConstructorDefaultsChanged(ECheckBoxState NewState);
	void OnShowChangedPropertiesChanged(ECheckBoxState NewState);

	void ApplyPropertyFilter();

	const UClass* SelectedClass = nullptr;
	const UClass* GetSelectedClass() const { return SelectedClass; }

	void CheckSettingsObjectChanged(UObject* Obj, struct FPropertyChangedEvent&);

	static bool& ShouldReadConstructorDefaults();
};
