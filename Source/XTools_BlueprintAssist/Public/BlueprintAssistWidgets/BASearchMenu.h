// Copyright fpwong. All Rights Reserved.

#pragma once

#include "BlueprintAssistGlobals.h"
#if BA_UE_VERSION_OR_LATER(5, 4)
#include "IContentBrowserSingleton.h"
#include "SearchQuery.h"
#include "Filters/SAssetFilterBar.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"

class SSearchBox;
class FAssetNode;
class FSearchNode;
class FAssetObjectNode;
class FAssetObjectPropertyNode;
class IAssetRegistry;

enum EBASearchMenuType
{
	File,
	Property
};

struct FBASearchMenuData : public TSharedFromThis<FBASearchMenuData>
{
	TSharedPtr<FAssetNode> ParentNode;

	TSharedPtr<FAssetObjectPropertyNode> PropNode;

	TSharedPtr<FAssetObjectNode> ObjNode;

	FBASearchMenuData(TSharedPtr<FAssetNode> InParentNode) : ParentNode(InParentNode)
	{
	};

	TSharedPtr<FSearchNode> GetNode();

	FString GetName();

	TOptional<FString> GetDetails();
};

class SBASearchMenuRow : public SMultiColumnTableRow<TSharedPtr<FBASearchMenuData>>
{
public:
	SLATE_BEGIN_ARGS(SBASearchMenuRow)
		{
		}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedPtr<FBASearchMenuData> InObject, const FText& InHighlightText);
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

	FReply HandleDrag(const FGeometry& Geometry, const FPointerEvent& PointerEvent);

	TSharedPtr<FBASearchMenuData> Object;
	FText HighlightText;
};

/**
 * Implements the undo history panel.
 */
class SBASearchMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBASearchMenu)
		{
		}

		SLATE_ARGUMENT(FString, SearchSpecifier)
		SLATE_ARGUMENT_DEFAULT(bool, AllowAssets) = true;
		SLATE_ARGUMENT_DEFAULT(bool, AllowObjects) = true;
		SLATE_ARGUMENT_DEFAULT(bool, AllowProperties) = true;
	SLATE_END_ARGS()

public:
	virtual ~SBASearchMenu();

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 */
	void Construct(const FArguments& InArgs, const FVector2D& WidgetSize, EBASearchMenuType MenuType);

public:
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	FText GetSearchBackgroundText() const;
	FText GetStatusText() const;
	FText GetAdvancedStatus() const;
	FText GetUnindexedAssetsText() const;

	void HandleForceIndexOfAssetsMissingIndex();

	EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const;
	void OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode);

	void RefreshList();

	void OnSearchTextCommited(const FText& InText, ETextCommit::Type InCommitType);
	void OnSearchTextChanged(const FText& InText);
	void TryRefreshingSearch(const FText& InText);

	TSharedRef<ITableRow> HandleListGenerateRow(TSharedPtr<FBASearchMenuData> TransactionInfo, const TSharedRef<STableViewBase>& OwnerTable);

	void HandleListItemClicked(TSharedPtr<FBASearchMenuData> Item);

	bool IsSearching() const;

	void OnPathPicked(const FString& String);

	void HandleFilterChanged();

	void SelectItem(TSharedPtr<FBASearchMenuData> Item);

	EActiveTimerReturnType SetFocusPostConstruct(double InCurrentTime, float InDeltaTime);

	void MarkSuggestion(int Index);

	void RestoreColumnWidths();
	void SaveColumnWidths();

	FString SearchSpecifier;

	FText FilterText;

	FSearchQueryWeakPtr ActiveSearchPtr;

	TArray<TSharedPtr<FBASearchMenuData>> SearchResults;

	TSharedPtr<SListView<TSharedPtr<FBASearchMenuData>>> SearchListView;

	IAssetRegistry* AssetRegistry = nullptr;

	TSharedPtr<SHeaderRow> HeaderColumns;

	FString PathFilter;
	FARCompiledFilter CompiledFilter;

	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<SAssetFilterBar<FText>> FilterBar;

	EBASearchMenuType MenuType = EBASearchMenuType::File;

	bool bAllowAssets = true;
	bool bAllowObjects = true;
	bool bAllowProperties = true;

	FString GetMenuTypeAsString() const;
	FString& GetSavedPathSetting();
	FString& GetSavedSearchSetting();

	// FName SortByColumn;
	// EColumnSortMode::Type SortMode;
};
#endif