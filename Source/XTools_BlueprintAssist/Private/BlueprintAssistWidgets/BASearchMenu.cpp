// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistWidgets/BASearchMenu.h"

#if BA_UE_VERSION_OR_LATER(5, 4)
#include "BlueprintAssistGlobals.h"
#include "IContentBrowserDataModule.h"

#include "BlueprintAssistSettings.h"
#include "BlueprintAssistStyle.h"
#include "ContentBrowserDataDragDropOp.h"
#include "ContentBrowserDelegates.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "IAssetSearchModule.h"
#include "IContentBrowserSingleton.h"
#include "IMaterialEditor.h"
#include "ISinglePropertyView.h"
#include "SAssetSearchBox.h"
#include "SlateOptMacros.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor/ContentBrowser/Private/SFilterList.h"
#include "Filters/SAssetFilterBar.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Misc/ConfigCacheIni.h"
#include "Private/Widgets/SearchModel.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "SObjectBrowser"

static FName NAME_BASearchName(TEXT("Name"));
static FName NAME_BASearchType(TEXT("Type"));
static FName NAME_BASearchPath(TEXT("Path"));

TSharedPtr<FSearchNode> FBASearchMenuData::GetNode()
{
	if (PropNode)
	{
		return PropNode;
	}

	if (ObjNode)
	{
		return ObjNode;
	}

	return ParentNode;
}

FString FBASearchMenuData::GetName()
{
	FString OutName = GetNode()->GetText();

	if (PropNode)
	{
		// OutName = FString::Printf(TEXT("%s (%s)"), *OutName, *ObjNode->GetText());
		// OutName = FString::Printf(TEXT("PROP %s:%s:%s:%s (%s:%s:%s)"),
		// 	*PropNode->Record.property_name, *PropNode->Record.property_field, *PropNode->Record.property_class, *PropNode->Record.value_text,
		// 	*PropNode->Record.object_name, *PropNode->Record.object_native_class, *PropNode->Record.object_path);

		if (PropNode->Record.object_name == PropNode->Record.value_text)
		{
			OutName = FString::Printf(TEXT("%s"),
				*PropNode->Record.value_text
			);
		}
		else
		{
			OutName = FString::Printf(TEXT("%s"),
				*PropNode->Record.value_text
			);
		}
			// *PropNode->Record.property_name, *PropNode->Record.property_field, *PropNode->Record.property_class, ,
			// *, *PropNode->Record.object_native_class, *PropNode->Record.object_path);
	}
	else if (PropNode)
	{
		OutName = FString::Printf(TEXT("%s:%s:%s:%s"),
			*PropNode->Record.property_name, *PropNode->Record.property_field, *PropNode->Record.property_class, *PropNode->Record.value_text);
	}
	else if (ObjNode)
	{
		OutName = FString::Printf(TEXT("OBJ %s:%s:%s"), *ObjNode->object_name, *ObjNode->object_native_class, *ObjNode->object_path);
	}

	return OutName;
}

TOptional<FString> FBASearchMenuData::GetDetails()
{
	TOptional<FString> Out;

	if (PropNode)
	{
		// OutName = FString::Printf(TEXT("%s (%s)"), *OutName, *ObjNode->GetText());
		// OutName = FString::Printf(TEXT("PROP %s:%s:%s:%s (%s:%s:%s)"),
		// 	*PropNode->Record.property_name, *PropNode->Record.property_field, *PropNode->Record.property_class, *PropNode->Record.value_text,
		// 	*PropNode->Record.object_name, *PropNode->Record.object_native_class, *PropNode->Record.object_path);

		if (PropNode->Record.object_name == PropNode->Record.value_text)
		{
			return PropNode->Record.property_name;
		}
		else
		{
			FString PropName = PropNode->Record.property_name;
			PropName.RemoveFromStart(FString("[Pin] "));
			return FString::Printf(TEXT("%s | %s"), *PropNode->Record.object_name, *PropName);
		}
	}

	return Out;
}

void SBASearchMenuRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedPtr<FBASearchMenuData> InObject, const FText& InHighlightText)
{
	Object = InObject;
	HighlightText = InHighlightText;

	FSuperRowType::Construct(
		FSuperRowType::FArguments().OnDragDetected(this, &SBASearchMenuRow::HandleDrag),
		InOwnerTableView
	);
}

TSharedRef<SWidget> SBASearchMenuRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	TSharedRef<SHorizontalBox> HorizBox = SNew(SHorizontalBox);

	if (ColumnName == NAME_BASearchName)
	{
		// FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

		// TSharedPtr<SWidget> IconWidget;
		// if (Object->ObjNode)
		// {
		// 	Object->PropNode->GetObjectPath();
		// 	
		// 	if (UClass* ObjectClass = UClass::TryFindTypeSlow<UClass>(*Object->ObjNode->object_native_class, EFindFirstObjectOptions::ExactClass))
		// 	{
		// 		FSlateIcon ClassIcon = FSlateIconFinder::FindIconForClass(ObjectClass);
		// 		if (ClassIcon.IsSet())
		// 		{
		// 			IconWidget = SNew(SImage)
		// 				.Image(ClassIcon.GetIcon());
		// 		}
		// 	}
		// }
		// else if (Object->ParentNode)
		// {
		// 	if (UClass* ObjectClass = UClass::TryFindTypeSlow<UClass>(Object->ParentNode->AssetClass, EFindFirstObjectOptions::ExactClass))
		// 	{
		// 		TSharedPtr<IAssetTypeActions> AssetActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(ObjectClass).Pin();
		//
		// 		FSlateIcon ClassIcon = FSlateIconFinder::FindIconForClass(ObjectClass);
		// 		if (ClassIcon.IsSet())
		// 		{
		// 			IconWidget = SNew(SImage)
		// 				.Image(ClassIcon.GetIcon())
		// 				.ColorAndOpacity(AssetActions.IsValid() ? AssetActions->GetTypeColor() : FColor::White);
		// 		}
		// 	}
		// }
		//
		// if (IconWidget.IsValid())
		// {
		// 	HorizBox->AddSlot()
		// 	        .VAlign(VAlign_Center)
		// 	        .AutoWidth()
		// 	        .Padding(0, 0, 4, 0)
		// 	[
		// 		IconWidget.ToSharedRef()
		// 	];
		// }
		

		HorizBox->AddSlot()
		        .Padding(4)
		        .AutoWidth()
		        .VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Object->GetName()))
			.ToolTipText(FText::FromString(Object->GetName()))
			.HighlightText(HighlightText)
		];

		if (TOptional<FString> Details = Object->GetDetails())
		{
			HorizBox->AddSlot()
				.Padding(4)
				.AutoWidth()
				.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Details.GetValue()))
				.ToolTipText(FText::FromString(Details.GetValue()))
				.Font(BA_STYLE_CLASS::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			];
		}

		// if (Object->PropNode)
		// {
		// 	// if (UClass* ObjectClass = UClass::TryFindTypeSlow<UClass>(*Object->ObjNode->object_native_class, EFindFirstObjectOptions::ExactClass))
		// 	// {
		// 	// 	FSlateIcon ClassIcon = FSlateIconFinder::FindIconForClass(ObjectClass);
		// 	// 	if (ClassIcon.IsSet())
		// 	// 	{
		// 	// 		IconWidget = SNew(SImage)
		// 	// 			.Image(ClassIcon.GetIcon());
		// 	// 	}
		// 	// }
		//
		// 	FSinglePropertyParams Params;
		// 	Params.NamePlacement = EPropertyNamePlacement::Type::Hidden;
		// 	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		//
		// 	HorizBox->AddSlot()
		// 	.AutoWidth()
		// 	.VAlign(VAlign_Fill)
		// 	[
		// 		EditModule.CreateSingleProperty(nullptr, FName(Object->PropNode->Record.object_name), Params));
		// 	]
		// }
	}
	else if (ColumnName == NAME_BASearchType)
	{
		HorizBox->AddSlot()
		        .Padding(4)
		        .AutoWidth()
		        .VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Object->ParentNode->AssetClass))
			.ToolTipText(FText::FromString(Object->ParentNode->AssetClass))
		];
	}
	else if (ColumnName == NAME_BASearchPath)
	{
		HorizBox->AddSlot()
		        .Padding(4)
		        .AutoWidth()
		        .VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Object->ParentNode->GetObjectPath()))
			.ToolTipText(FText::FromString(Object->ParentNode->GetObjectPath()))
		];
	}

	return HorizBox;
}

FReply SBASearchMenuRow::HandleDrag(const FGeometry& Geometry, const FPointerEvent& PointerEvent)
{
	// don't do anything on drag for prop nodes
	if (Object->PropNode.IsValid())
	{
		return FReply::Unhandled();
	}

	UContentBrowserDataSubsystem* ContentBrowserDataSubsystem = IContentBrowserDataModule::Get().GetSubsystem();

	FName VirtualPath;
	ContentBrowserDataSubsystem->ConvertInternalPathToVirtual(Object->GetNode()->GetObjectPath(), VirtualPath);

	FContentBrowserItem Item = ContentBrowserDataSubsystem->GetItemAtPath(VirtualPath, EContentBrowserItemTypeFilter::IncludeFiles);
	if (Item.IsValid())
	{
		return FReply::Handled().BeginDragDrop(FContentBrowserDataDragDropOp::New({ Item }));
	}

	return FReply::Unhandled();
}

SBASearchMenu::~SBASearchMenu()
{
	// GetMutableDefault<USearchUserSettings>()->SearchInForeground--;
	FilterBar->SaveSettings();
	SaveColumnWidths();
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SBASearchMenu::Construct(const FArguments& InArgs, const FVector2D& WidgetSize, EBASearchMenuType InMenuType)
{
	SearchSpecifier = InArgs._SearchSpecifier;
	bAllowAssets = InArgs._AllowAssets;
	bAllowObjects = InArgs._AllowAssets;
	bAllowProperties = InArgs._AllowProperties;
	MenuType = InMenuType;

	// TODO: Will things screw up if we don't increment this?
	// UserSettings->SearchInForeground++;

	// SortByColumn = SSearchTreeRow::NAME_ColumnName;
	// SortMode = EColumnSortMode::Ascending;

	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.bFocusSearchBoxWhenOpened = false;
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SBASearchMenu::OnPathPicked);

	const FString& SavedPath = GetSavedPathSetting();
	const FString& LastSearch = GetSavedSearchSetting();

	FString InternalPath;
	IContentBrowserDataModule::Get().GetSubsystem()->TryConvertVirtualPath(SavedPath, InternalPath);
	PathPickerConfig.DefaultPath = InternalPath;
	PathPickerConfig.bAllowReadOnlyFolders = true;
	PathPickerConfig.bOnPathSelectedPassesVirtualPaths = true;
	PathPickerConfig.bAllowContextMenu = false;

	PathFilter = SavedPath;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SBASearchMenu::SetFocusPostConstruct));

	AssetRegistry = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FilterText = FText::FromString(LastSearch);

	FName FilterBarId = (MenuType == EBASearchMenuType::File) ? FName("BASearchFilter_File") : FName("BASearchFilter_Property"); 

	TSharedRef<SBorder> Content = SNew(SBorder)
		.BorderImage(FBAStyle::GetBrush("BlueprintAssist.PanelBorder"))
		.Padding(5)
		[
			SNew(SBox)
			.WidthOverride(WidgetSize.X)
			.HeightOverride(WidgetSize.Y)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Bottom)
					[
						SNew(SBox)
						.WidthOverride(200)
						.HeightOverride(300)
						[
							ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
						]
					]
					+ SHorizontalBox::Slot()
					[
						SNew(SBorder)
						[
							SAssignNew(FilterBar, SAssetFilterBar<FText>)
							.UseDefaultAssetFilters(true)
							.FilterBarIdentifier(FilterBarId)
							.FilterBarLayout(EFilterBarLayout::Horizontal)
							.CanChangeOrientation(false)
							.OnFilterChanged(this, &SBASearchMenu::HandleFilterChanged)
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Top)
					[
						SBasicFilterBar<FText>::MakeAddFilterButton(FilterBar.ToSharedRef())
					]
				]
				+ SVerticalBox::Slot()
				.Padding(0, 8, 0, 0)
				.FillHeight(1.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(SearchBox, SSearchBox)
						.InitialText(FText::FromString(LastSearch))
						.HintText(LOCTEXT("SearchHint", "Search"))
						.OnTextCommitted(this, &SBASearchMenu::OnSearchTextCommited)
						.OnTextChanged(this, &SBASearchMenu::OnSearchTextChanged)
						.IsSearching(this, &SBASearchMenu::IsSearching)
						.DelayChangeNotificationsWhileTyping(true)
						.DelayChangeNotificationsWhileTypingSeconds(0.2f)
						.OnKeyDownHandler(this, &SBASearchMenu::OnKeyDown)
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.Padding(FMargin(0.0f, 4.0f))
						[
							SNew(SOverlay)

							+ SOverlay::Slot()
							[
								SAssignNew(SearchListView, SListView<TSharedPtr<FBASearchMenuData>>)
								.SelectionMode(ESelectionMode::Single)
								.ListItemsSource(&SearchResults)
								.OnGenerateRow(this, &SBASearchMenu::HandleListGenerateRow)
								.OnMouseButtonClick(this, &SBASearchMenu::HandleListItemClicked)
								.IsFocusable(false)
								.HeaderRow
								(
									SAssignNew(HeaderColumns, SHeaderRow)
									+ SHeaderRow::Column(NAME_BASearchName)
									.DefaultLabel(INVTEXT("Name"))
									.FillWidth(70)
									+ SHeaderRow::Column(NAME_BASearchPath)
									.DefaultLabel(INVTEXT("Path"))
									.FillWidth(70)
									+ SHeaderRow::Column(NAME_BASearchType)
									.HAlignHeader(HAlign_Right)
									.HAlignCell(HAlign_Right)
									.DefaultLabel(INVTEXT("Type"))
									.FillWidth(70)

								)
							]

							+ SOverlay::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Visibility(EVisibility::HitTestInvisible)
								.Clipping(EWidgetClipping::Inherit)
								.Text(this, &SBASearchMenu::GetSearchBackgroundText)
								.Justification(ETextJustify::Center)
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 30))
								.ColorAndOpacity(FLinearColor(1, 1, 1, 0.25))
								.RenderTransformPivot(FVector2D(0.5, 0.5))
								.RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(-30.0f))))
							]
						]
					]
				]


				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 1)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(8, 0)
					[
						SNew(SImage)
						// .Image(FSearchStyle::Get().GetBrush("Stats"))
						// .Image(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer")
						            .Image(BA_STYLE_CLASS::Get().GetBrush("LevelEditor.Tabs.StatsViewer"))
						            .ToolTip(
							            SNew(SToolTip)
							            [
								            SNew(SVerticalBox)

								            + SVerticalBox::Slot()
								            .AutoHeight()
								            [
									            SNew(STextBlock)
									            .Text(this, &SBASearchMenu::GetAdvancedStatus)
								            ]
							            ]
						            )
					]

					// Asset Stats 
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					.Padding(2, 0)
					[
						SNew(STextBlock)
						.Text(this, &SBASearchMenu::GetStatusText)
					]

					// Index unindexed items
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SHyperlink)
						.Text(this, &SBASearchMenu::GetUnindexedAssetsText)
						.ToolTipText(LOCTEXT("AssetsNeedIndexingTooltip", "Click this to open and index the assets that are don't have any index data or their index data was found to be out of date."))
						.Visibility(EVisibility::Visible)
						.OnNavigate(this, &SBASearchMenu::HandleForceIndexOfAssetsMissingIndex)
					]
				]
			]
		];


	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(FBAStyle::GetPluginBrush("BlueprintAssist.PlainBorder"))
			.ColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f))

		]
		+ SOverlay::Slot()
		.Padding(1)
		[
			Content
		]
	];

	RestoreColumnWidths();
	FilterBar->LoadSettings();

	RefreshList();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FReply SBASearchMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey() == EKeys::F && FSlateApplication::Get().GetModifierKeys().IsControlDown())
	{
		FWidgetPath WidgetToFocusPath;
		FSlateApplication::Get().GeneratePathToWidgetUnchecked(SearchBox.ToSharedRef(), WidgetToFocusPath);
		FSlateApplication::Get().SetKeyboardFocus(WidgetToFocusPath, EFocusCause::SetDirectly);
		WidgetToFocusPath.GetWindow()->SetWidgetToFocusOnActivate(SearchBox);
		return FReply::Handled();
	}

	if (KeyEvent.GetKey() == EKeys::Escape)
	{
		FSlateApplication::Get().DismissMenuByWidget(SharedThis(this));
		return FReply::Handled();
	}

	if (KeyEvent.GetKey() == EKeys::Enter)
	{
		TArray<TSharedPtr<FBASearchMenuData>> SelectedItems;
		SearchListView->GetSelectedItems(SelectedItems);

		if (SelectedItems.Num() > 0)
		{
			SelectItem(SelectedItems[0]);
			return FReply::Handled();
		}

		if (SearchResults.Num())
		{
			SelectItem(SearchResults[0]);
			return FReply::Handled();
		}
	}

	// CTRL+B display in content browser
	if (KeyEvent.GetKey() == EKeys::B && FSlateApplication::Get().GetModifierKeys().IsControlDown())
	{
		TArray<TSharedPtr<FBASearchMenuData>> SelectedItems;
		SearchListView->GetSelectedItems(SelectedItems);

		if (SelectedItems.Num() > 0 && SelectedItems[0].IsValid())
		{
			TSharedPtr<FBASearchMenuData> Item = SelectedItems[0];

			TArray<UObject*> Assets;
			if (Item->ObjNode || Item->PropNode)
			{
				FSoftObjectPath ReferencePath(Item->GetNode()->GetObjectPath());
				if (UObject* Object = ReferencePath.TryLoad())
				{
					Assets.Add(Object);
				}
			}
			else if (Item->ParentNode)
			{
				if (UObject* Object = FSoftObjectPath(Item->ParentNode->AssetPath).TryLoad())
				{
					Assets.Add(Object);
				}
			}

			// Highlight the asset in content browser
			if (Assets.Num() > 0)
			{
				const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
				ContentBrowserModule.Get().SyncBrowserToAssets(Assets);
				return FReply::Handled();
			}
		}
	}

	const int NumItems = SearchResults.Num();
	if (NumItems > 0)
	{
		int32 SelectionDelta = 0;

		// move up and down through the filtered node list
		if (KeyEvent.GetKey() == EKeys::Up)
		{
			SelectionDelta = -1;
		}
		else if (KeyEvent.GetKey() == EKeys::Down)
		{
			SelectionDelta = 1;
		}

		int SuggestionIndex = INDEX_NONE;
		
		TArray<TSharedPtr<FBASearchMenuData>> SelectedItems = SearchListView->GetSelectedItems();
		if (SelectedItems.Num() == 1)
		{
			SuggestionIndex = SearchResults.IndexOfByKey(SelectedItems[0]);
		}

		if (SelectionDelta != 0)
		{
			// If we have no selected suggestion then we need to use the items in the root to set the selection and set the focus
			if (SuggestionIndex == INDEX_NONE)
			{
				SuggestionIndex = SelectionDelta == 1 ? 0 : NumItems - 1;
				MarkSuggestion(SuggestionIndex);
				return FReply::Handled();
			}

			SuggestionIndex = (SuggestionIndex + SelectionDelta + NumItems) % NumItems;
			MarkSuggestion(SuggestionIndex);

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FText SBASearchMenu::GetSearchBackgroundText() const
{
	if (FilterText.IsEmpty())
	{
		const IAssetSearchModule& SearchModule = IAssetSearchModule::Get();
		const FSearchStats SearchStats = SearchModule.GetStats();

		if (SearchStats.IsUpdating() && SearchStats.TotalRecords > 0)
		{
			return FText::Format(LOCTEXT("SearchNumberOfThings", "Search\n{0} Things!"), SearchStats.TotalRecords);
		}
		else
		{
			const FString SearchDescription = MenuType == EBASearchMenuType::File ? "Files" : "Properties"; 
			return FText::FromString(FString::Printf(TEXT("Search\nAll The %s"), *SearchDescription));
		}
	}

	if (SearchResults.Num() == 0 && !IsSearching())
	{
		return LOCTEXT("FoundNoResults", "¯\\_(ツ)_/¯");
	}

	return FText::GetEmpty();
}

FText SBASearchMenu::GetStatusText() const
{
	const IAssetSearchModule& SearchModule = IAssetSearchModule::Get();
	const FSearchStats SearchStats = SearchModule.GetStats();

	if (SearchStats.IsUpdating())
	{
		return LOCTEXT("Updating", "Updating...  (You can search any time)");
	}
	else
	{
		return LOCTEXT("Ready", "Ready");
	}
}

FText SBASearchMenu::GetAdvancedStatus() const
{
	const IAssetSearchModule& SearchModule = IAssetSearchModule::Get();
	const FSearchStats SearchStats = SearchModule.GetStats();
	return FText::Format(LOCTEXT("AdvancedSearchStatusTextFmt", "Scanning {0}\nProcessing {1}\nUpdating {2}\n\nTotal Records {3}"), SearchStats.Scanning, SearchStats.Processing, SearchStats.Updating, SearchStats.TotalRecords);
}

FText SBASearchMenu::GetUnindexedAssetsText() const
{
	const IAssetSearchModule& SearchModule = IAssetSearchModule::Get();
	const FSearchStats SearchStats = SearchModule.GetStats();

	if (SearchStats.AssetsMissingIndex > 0)
	{
		return FText::Format(LOCTEXT("UnindexedAssetsLinkFormat", "{0} Need Indexing"), SearchStats.AssetsMissingIndex);
	}

	return FText::GetEmpty();
}

void SBASearchMenu::HandleForceIndexOfAssetsMissingIndex()
{
	IAssetSearchModule& SearchModule = IAssetSearchModule::Get();
	SearchModule.ForceIndexOnAssetsMissingIndex();
}

EColumnSortMode::Type SBASearchMenu::GetColumnSortMode(const FName ColumnId) const
{
	// if (SortByColumn == ColumnId)
	// {
	// 	return SortMode;
	// }

	return EColumnSortMode::None;
}

void SBASearchMenu::OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode)
{
	// SortByColumn = ColumnId;
	// SortMode = InSortMode;
	// RefreshList();
}

void SBASearchMenu::RefreshList()
{
	SearchResults.Reset();

	SearchListView->RequestListRefresh();

	if (!FilterText.IsEmpty())
	{
		if (FSearchQueryPtr ActiveSearch = ActiveSearchPtr.Pin())
		{
			ActiveSearch->ClearResultsCallback();
			ActiveSearchPtr.Reset();
		}

		TWeakPtr<SBASearchMenu> WeakSelf = SharedThis(this);

		FString SearchText = FilterText.ToString();
		if (!SearchSpecifier.IsEmpty())
		{
			SearchText = FString::Printf(TEXT("%s=%s"), *SearchSpecifier, *SearchText);
		}

		FSearchQueryPtr NewQuery = MakeShared<FSearchQuery, ESPMode::ThreadSafe>(SearchText);
		NewQuery->SetResultsCallback([this, WeakSelf](TArray<FSearchRecord>&& InResults)
		{
			check(IsInGameThread());

			if (WeakSelf.IsValid())
			{
				// SearchResults.Reset();
				for (FSearchRecord& SearchRecord : InResults)
				{
					TSharedRef<FBASearchMenuData> Data = MakeShared<FBASearchMenuData>(MakeShared<FAssetNode>(SearchRecord));

					TSharedPtr<FAssetNode> AssetNode = Data->ParentNode;

					// Apply the path filter
					FString VirtualPath = IContentBrowserDataModule::Get().GetSubsystem()->ConvertInternalPathToVirtual(*AssetNode->AssetPath).ToString();
					if (!VirtualPath.Contains(PathFilter))
					{
						continue;
					}

					// Apply the class filter
					FTopLevelAssetPath AssetClassPath(AssetNode->AssetClass);
					if (CompiledFilter.ClassPaths.Num() > 0 && !CompiledFilter.ClassPaths.Contains(AssetClassPath))
					{
						continue;
					}

					bool bAddParent = true;
					if (bAllowProperties)
					{
						for (TSharedPtr<FAssetObjectPropertyNode> Prop : AssetNode->Properties)
						{
							// skip function name cause it dupes with node title
							if (Prop->Record.property_name == "FunctionName" || Prop->Record.object_name == "Set " + Prop->Record.property_name)
							{
								continue;
							}

							if (Prop->Record.object_name == "$self" && Prop->Record.property_name != "Var Name")
							{
								// UE_LOG(LogTemp, Warning, TEXT("Skip %s | %s"), *Prop->Record.object_name, *Prop->Record.property_name);
								continue;
							}

							// UE_LOG(LogTemp, Warning, TEXT("Make prop"))
							TSharedRef<FBASearchMenuData> PropData = MakeShared<FBASearchMenuData>(AssetNode);
							PropData->PropNode = Prop;
							SearchResults.Add(PropData);
							bAddParent = false;
						}
					}

					for (TSharedPtr<FAssetObjectNode> Obj : AssetNode->SortedObjectArray)
					{
						if (Obj->Properties.Num() > 0 && bAllowProperties)
						{
							// UE_LOG(LogTemp, Warning, TEXT("Make prop 2"))
							for (TSharedPtr<FAssetObjectPropertyNode> Prop : Obj->Properties)
							{
								// skip function name cause it dupes with node title
								if (Prop->Record.property_name == "FunctionName"|| Prop->Record.object_name == "Set " + Prop->Record.value_text)
								{
									continue;
								}

								TSharedRef<FBASearchMenuData> PropData = MakeShared<FBASearchMenuData>(AssetNode);
								PropData->PropNode = Prop;
								PropData->ObjNode = Obj;
								SearchResults.Add(PropData);
								bAddParent = false;
							}
						}

						if (bAllowObjects)
						{
							// UE_LOG(LogTemp, Warning, TEXT("Make obj"))
							TSharedRef<FBASearchMenuData> ObjData = MakeShared<FBASearchMenuData>(AssetNode);
							ObjData->ObjNode = Obj;
							SearchResults.Add(ObjData);
							bAddParent = false;
						}
					}

					if (bAddParent && bAllowAssets)
					{
						SearchResults.Add(Data);
					}
				}

				SearchResults.Sort([](const TSharedPtr<FBASearchMenuData>& A, const TSharedPtr<FBASearchMenuData>& B)
				{
					return A->GetNode()->GetMaxScore() < B->GetNode()->GetMaxScore();
				});

				SearchListView->RequestListRefresh();
			}
		});

		IAssetSearchModule& SearchModule = IAssetSearchModule::Get();
		SearchModule.Search(NewQuery);

		ActiveSearchPtr = NewQuery;
	}
}

void SBASearchMenu::OnSearchTextCommited(const FText& InText, ETextCommit::Type InCommitType)
{
	TryRefreshingSearch(InText);
}

void SBASearchMenu::OnSearchTextChanged(const FText& InText)
{
	const int32 Length = InText.ToString().Len();

	if (Length > 0)
	{
		TryRefreshingSearch(InText);
		GetSavedSearchSetting() = InText.ToString();
		UBlueprintAssistSettings_Search::Get().SaveConfig();
	}
	else // if (Length == 0)
	{
		TryRefreshingSearch(InText);
		GetSavedSearchSetting().Empty();
		UBlueprintAssistSettings_Search::Get().SaveConfig();
	}
}

void SBASearchMenu::TryRefreshingSearch(const FText& InText)
{
	if (FilterText.ToString() != InText.ToString())
	{
		FilterText = InText;
		RefreshList();
	}
}

TSharedRef<ITableRow> SBASearchMenu::HandleListGenerateRow(TSharedPtr<FBASearchMenuData> ObjectPtr, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SBASearchMenuRow, OwnerTable, ObjectPtr, SearchBox->GetText());
}

void SBASearchMenu::HandleListItemClicked(TSharedPtr<FBASearchMenuData> Item)
{
	SelectItem(Item);
}

bool SBASearchMenu::IsSearching() const
{
	return ActiveSearchPtr.IsValid();
}

void SBASearchMenu::OnPathPicked(const FString& String)
{
	PathFilter = String;
	RefreshList();

	GetSavedPathSetting() = String;
	UBlueprintAssistSettings_Search::Get().SaveConfig();
}

void SBASearchMenu::HandleFilterChanged()
{
	AssetRegistry = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetRegistry->CompileFilter(FilterBar->GetCombinedBackendFilter(), CompiledFilter);
	RefreshList();
}

void SBASearchMenu::SelectItem(TSharedPtr<FBASearchMenuData> Item)
{
	if (Item->ObjNode || Item->PropNode)
	{
		FSoftObjectPath ReferencePath(Item->GetNode()->GetObjectPath()); 
		UObject* Object = ReferencePath.TryLoad();

		if (Object && Object->GetTypedOuter<UBlueprint>())  
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Object, false);
			return;
		}

		FString AssetPathName = ReferencePath.GetAssetPathString().Replace(TEXT("Default__"), TEXT(""));
		AssetPathName.RemoveFromEnd(TEXT("_C"));

		if (Object && (Object->GetTypedOuter<UMaterial>() || Object->GetTypedOuter<UMaterialFunction>()))
		{
			if (UPackage* Package = LoadPackage(nullptr, *AssetPathName, LOAD_NoRedirects))
			{
				Package->FullyLoad();

				FString AssetName = FPaths::GetBaseFilename(AssetPathName);

				if (UObject* MaterialObject = FindObject<UObject>(Package, *AssetName))
				{
					UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
					AssetEditorSubsystem->OpenEditorForAsset(MaterialObject);

					if (IMaterialEditor* MaterialEditor = static_cast<IMaterialEditor*>(AssetEditorSubsystem->FindEditorForAsset(MaterialObject, true)))
					{
						MaterialEditor->JumpToExpression(Cast<UMaterialExpression>(Object));
						return;
					}
				}
			}
		}
		else
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AssetPathName);
			return;
		}
	}

	if (Item->ParentNode)
	{
		TSharedPtr<FAssetNode> ObjectNode = StaticCastSharedPtr<FAssetNode>(Item->ParentNode);
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(ObjectNode->AssetPath);
	}
}

EActiveTimerReturnType SBASearchMenu::SetFocusPostConstruct(double InCurrentTime, float InDeltaTime)
{
	if (SearchBox.IsValid())
	{
		FWidgetPath WidgetToFocusPath;
		FSlateApplication::Get().GeneratePathToWidgetUnchecked(SearchBox.ToSharedRef(), WidgetToFocusPath);
		FSlateApplication::Get().SetKeyboardFocus(WidgetToFocusPath, EFocusCause::SetDirectly);
		WidgetToFocusPath.GetWindow()->SetWidgetToFocusOnActivate(SearchBox);

		return EActiveTimerReturnType::Stop;
	}

	return EActiveTimerReturnType::Continue;
}

void SBASearchMenu::MarkSuggestion(int Index)
{
	if (SearchResults.IsValidIndex(Index))
	{
		auto& ItemToSelect = SearchResults[Index];
		SearchListView->SetSelection(ItemToSelect);
		SearchListView->RequestScrollIntoView(ItemToSelect);
	}
	else
	{
		SearchListView->ClearSelection();
	}
}


void SBASearchMenu::SaveColumnWidths()
{
	const TIndirectArray<SHeaderRow::FColumn>& Columns = HeaderColumns->GetColumns();
	for (int32 Idx = 0; Idx < Columns.Num(); ++Idx)
	{
		const SHeaderRow::FColumn& Column = Columns[Idx];
		const float Width = Column.GetWidth();
		const FString Id = FString::Printf(TEXT("BAColumnWidths_%s"), *GetMenuTypeAsString());
		GConfig->SetFloat(*Id, *Column.ColumnId.ToString(), Width, GEditorPerProjectIni);
	}
}

void SBASearchMenu::RestoreColumnWidths()
{
	const TIndirectArray<SHeaderRow::FColumn>& Columns = HeaderColumns->GetColumns();
	for (int32 Idx = 0; Idx < Columns.Num(); ++Idx)
	{
		const SHeaderRow::FColumn& Column = Columns[Idx];
		float Width = 1.0f;
		const FString Id = FString::Printf(TEXT("BAColumnWidths_%s"), *GetMenuTypeAsString());
		if (GConfig->GetFloat(*Id, *Column.ColumnId.ToString(), Width, GEditorPerProjectIni))
		{
			HeaderColumns->SetColumnWidth(Column.ColumnId, Width);
		}
	}
}

FString SBASearchMenu::GetMenuTypeAsString() const
{
	return MenuType == EBASearchMenuType::File ? FString("File") : FString("Property");
}

FString& SBASearchMenu::GetSavedPathSetting()
{
	return MenuType == EBASearchMenuType::File ? UBlueprintAssistSettings_Search::Get().File_SavedPath : UBlueprintAssistSettings_Search::Get().Properties_SavedPath;
}

FString& SBASearchMenu::GetSavedSearchSetting()
{
	return MenuType == EBASearchMenuType::File ? UBlueprintAssistSettings_Search::Get().File_LastSearch : UBlueprintAssistSettings_Search::Get().Properties_LastSearch;
}

#undef LOCTEXT_NAMESPACE
#endif