// Copyright fpwong. All Rights Reserved.

#include "ClassViewerFilter.h"
#include "PropertyCustomizationHelpers.h"
#include "BlueprintAssistWidgets/BAConfigViewer.h"
#include "BlueprintAssistGlobals.h"
#if BA_UE_VERSION_OR_LATER(5, 4)
#include "BlueprintAssistSettings_Search.h"
#include "BlueprintAssistMisc/BAMiscUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/Docking/SDockTab.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ConfigContext.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"

namespace ConfigFileColumns
{
	static const FName Name = "Name";
	static const FName State = "State";
	static const FName Actions = "Actions";
}

void SBAConfigFileRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
{
	Item = InArgs._Item;
	ConfigViewer = InArgs._ConfigViewer;

	SMultiColumnTableRow<TSharedPtr<FBAConfigFileItem>>::Construct(FSuperRowType::FArguments(), InOwnerTable);
}

TSharedRef<SWidget> SBAConfigFileRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == ConfigFileColumns::Name)
	{
		return SNew(SBox).Padding(2).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(Item->FullPath)).Font(FAppStyle::GetFontStyle("SmallFont"))
			];
	}
	else if (ColumnName == ConfigFileColumns::State)
	{
		auto [String, Color] = Item->GetStateValues();
		return SNew(SBox).Padding(2).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(String))
				.ColorAndOpacity(Color)
			];
	}
	else if (ColumnName == ConfigFileColumns::Actions)
	{
		return SNew(SBox).Padding(2).VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(INVTEXT("打开"))
					.Visibility(Item->bExists ? EVisibility::Visible : EVisibility::Collapsed)
					.OnClicked(this, &SBAConfigFileRow::OnOpenFile)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(INVTEXT("保存"))
					.Visibility(Item->bExists ? EVisibility::Visible : EVisibility::Collapsed)
					.OnClicked(this, &SBAConfigFileRow::OnSaveSettings)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(INVTEXT("重新加载"))
					.IsEnabled(!Item->bIsFinalIni)
					.Visibility(Item->bExists ? EVisibility::Visible : EVisibility::Collapsed)
					.OnClicked(this, &SBAConfigFileRow::OnReloadSettings)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(INVTEXT("创建"))
					.Visibility(Item->bExists ? EVisibility::Collapsed : EVisibility::Visible)
					.OnClicked(this, &SBAConfigFileRow::OnCreateFile)
				]
			];
	}
	return SNullWidget::NullWidget;
}

FReply SBAConfigFileRow::OnOpenFile()
{
	FPlatformProcess::ExploreFolder(*Item->FullPath);
	return FReply::Handled();
}

FReply SBAConfigFileRow::OnSaveSettings()
{
	if (Item->SettingClass)
	{
		if (UObject* CDO = Item->SettingClass->GetDefaultObject<UObject>())
		{
			if (FBAMiscUtils::MakeFileWritable(Item->FullPath))
			{
				CDO->TryUpdateDefaultConfigFile(FConfigCacheIni::NormalizeConfigIniPath(Item->FullPath));
			}
			else
			{
				UE_LOG(LogBlueprintAssist, Warning, TEXT("[%hs] Unable to make file writable %s"), __FUNCTION__, *Item->FullPath);
				FBAMiscUtils::ShowSimpleSlateNotification(INVTEXT("无法将 ini 文件设为可写"), SNotificationItem::CS_Fail);
			}
		}
	}

	return FReply::Handled();
}

FReply SBAConfigFileRow::OnCreateFile()
{
	FFileHelper::SaveStringToFile(TEXT(""), *Item->FullPath);
	ConfigViewer->RefreshSourceFiles();
	return FReply::Handled();
}

FReply SBAConfigFileRow::OnReloadSettings()
{
	if (Item->SettingClass)
	{
		if (auto ObjectToReload = Item->SettingClass->GetDefaultObject<UObject>())
		{
			// refer to the console command "RELOADCONFIG"
			{
				// unload the branch so next access will load the static and dynamic layers
				GConfig->SafeUnloadBranch(*ObjectToReload->GetClass()->GetConfigName());

				// now updates all the class properties now that the config was reloaded from disk
				ObjectToReload->ReloadConfig();
			}

			ConfigViewer->RefreshPropertyTree();
		}
	}

	return FReply::Handled();
}

class FConfigClassFilter : public IClassViewerFilter
{
public:
	virtual bool IsClassAllowed(const FClassViewerInitializationOptions&, const UClass* InClass, TSharedRef<FClassViewerFilterFuncs>) override
	{
		return InClass->HasAnyClassFlags(CLASS_Config);
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions&, const TSharedRef<const IUnloadedBlueprintData>, TSharedRef<FClassViewerFilterFuncs>) override { return false; }
};

void SBAConfigViewer::Construct(const FArguments& InArgs)
{
	TSharedRef<FConfigClassFilter> ClassFilter = MakeShared<FConfigClassFilter>();

	ChildSlot
	[
		SNew(SSplitter)
		+ SSplitter::Slot().Value(0.7f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().VAlign(VAlign_Center).AutoHeight().Padding(5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.MinWidth(100.0f)
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SClassPropertyEntryBox)
					.ToolTipText(INVTEXT("选择要检查的设置配置类"))
					.AllowNone(true)
					.ShowDisplayNames(true)
					.SelectedClass(this, &SBAConfigViewer::GetSelectedClass)
					.OnSetClass(this, &SBAConfigViewer::HandleNewEntryClassSelected)
					.ClassViewerFilters({ClassFilter})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(10, 0)
				[
					SNew(SCheckBox)
					.ToolTipText(INVTEXT("仅显示实际存在的文件"))
					.IsChecked_Lambda([&]() { return bHideMissingFiles ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged(this, &SBAConfigViewer::OnRemoveMissingChanged)
					[
						SNew(STextBlock).Text(INVTEXT("仅现有文件"))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(10, 0)
				[
					SNew(SCheckBox)
					.ToolTipText(INVTEXT("仅显示包含所选类配置值的文件"))
					.IsChecked_Lambda([&]() { return bShowModifyingFiles ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged(this, &SBAConfigViewer::OnShowModifyingChanged)
					[
						SNew(STextBlock).Text(INVTEXT("仅有修改"))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(10, 0)
				[
					SNew(SCheckBox)
					.ToolTipText(INVTEXT("包含最终合并后的 ini"))
					.IsChecked_Lambda([&]() { return bIncludeFinalIni ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged(this, &SBAConfigViewer::OnFinalIniChanged)
					[
						SNew(STextBlock).Text(INVTEXT("最终 Ini"))
					]
				]
			]
			+ SVerticalBox::Slot().VAlign(VAlign_Center).AutoHeight().Padding(5)
			[
				SAssignNew(FileSearchBox, SSearchBox)
				.HintText(INVTEXT("搜索文件名..."))
				.OnTextChanged(this, &SBAConfigViewer::OnFileSearchTextChanged)
			]
			+ SVerticalBox::Slot().FillHeight(0.3f).Padding(5)
			[
				SAssignNew(FileListView, SListView<TSharedPtr<FBAConfigFileItem>>)
				.ListItemsSource(&FilteredSourceFiles)
				.OnGenerateRow(this, &SBAConfigViewer::OnGenerateFileRow)
				.HeaderRow(
					SNew(SHeaderRow)
					+ SHeaderRow::Column(ConfigFileColumns::Name).DefaultLabel(INVTEXT("文件路径")).FillWidth(1.0f)
					+ SHeaderRow::Column(ConfigFileColumns::State).DefaultLabel(INVTEXT("状态")).FixedWidth(60)
					+ SHeaderRow::Column(ConfigFileColumns::Actions).DefaultLabel(INVTEXT("操作")).ManualWidth(240)
				)
			]
		]
		+ SSplitter::Slot().Value(0.6f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(5)
			+ SVerticalBox::Slot().AutoHeight().Padding(5, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().MinWidth(100.0f).FillWidth(1.0f)
				[
					SAssignNew(PropertySearchBox, SSearchBox)
					.HintText(INVTEXT("搜索属性..."))
					.OnTextChanged(this, &SBAConfigViewer::OnPropertySearchTextChanged)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([&]() { return ShouldReadConstructorDefaults() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.ToolTipText(INVTEXT("读取类构造函数中设置的默认值"))
					.OnCheckStateChanged(this, &SBAConfigViewer::OnReadConstructorDefaultsChanged)
					[
						SNew(STextBlock).Text(INVTEXT("读取默认值"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([&]() { return bShowModifiedProperties ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.IsEnabled_Lambda([&]() { return ShouldReadConstructorDefaults(); })
					.OnCheckStateChanged(this, &SBAConfigViewer::OnShowChangedPropertiesChanged)
					.ToolTipText(INVTEXT("仅显示与默认值不同的属性"))
					[
						SNew(STextBlock).Text(INVTEXT("仅已修改"))
					]
				]
			]
			+ SVerticalBox::Slot().FillHeight(0.7f).Padding(5)
			[
				SAssignNew(PropertyTreeView, STreeView<TSharedPtr<FBAConfigPropertyItem>>)
				.TreeItemsSource(&FilteredPropertyTreeRoot)
				.OnGenerateRow(this, &SBAConfigViewer::OnGeneratePropertyRow)
				.OnGetChildren(this, &SBAConfigViewer::GetPropertyChildren)
			]
		]
	];
}

SBAConfigViewer::SBAConfigViewer()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &SBAConfigViewer::CheckSettingsObjectChanged);
}

SBAConfigViewer::~SBAConfigViewer()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
}

void SBAConfigViewer::HandleNewEntryClassSelected(const UClass* InClass)
{
	SelectedClass = InClass;
	PropertySearchFilter.Empty();
	RefreshSourceFiles();
	RefreshPropertyTree();
}

static FString GetClassPathName(const UClass* Class)
{
#if BA_UE_VERSION_OR_LATER(5, 7)
	return Class->GetClassPathName().ToString();
#else
	return Class->GetPathName();
#endif
}

void SBAConfigViewer::RefreshSourceFiles()
{
	FullSourceFiles.Empty();
	if (!SelectedClass)
	{
		return;
	}

	// FName Platform;
	// if (const TCHAR* OverridePlatform = SelectedClass->GetDefaultObject<UObject>()->GetConfigOverridePlatform())
	// {
	// 	Platform = OverridePlatform;
	// }
	// FConfigCacheIni* PlatformConfigCache = FConfigCacheIni::ForPlatform(Platform);

	FConfigCacheIni* PlatformConfigCache = GConfig;
	if (!PlatformConfigCache)
	{
		FBAMiscUtils::ShowSimpleSlateNotification(INVTEXT("未找到该类的配置缓存"), SNotificationItem::CS_Fail);
		return;
	}

	FString ClassConfigName = SelectedClass->ClassConfigName.ToString();
	if (FConfigBranch* Branch = PlatformConfigCache->FindBranch(*ClassConfigName, ClassConfigName))
	{
		TArray<FString> PotentialPaths;

#if BA_UE_VERSION_OR_LATER(5, 7)
		TArray<FUtf8String> Utf8Paths;
		Branch->Hierarchy.GenerateValueArray(Utf8Paths);
		for (const FUtf8String& Utf8 : Utf8Paths)
		{
			PotentialPaths.Add(FString(Utf8));
		}
#else
		Branch->Hierarchy.GenerateValueArray(PotentialPaths);
#endif

		if (bIncludeFinalIni)
		{
			PotentialPaths.Add(Branch->IniPath);
		}

#if BA_UE_VERSION_OR_LATER(5, 7)
		FString SectionName = GetClassPathName(SelectedClass);
#else
		FString SectionName = SelectedClass->GetPathName();
#endif

		for (const FString& Path : PotentialPaths)
		{
			TSharedPtr<FBAConfigFileItem> Item = MakeShared<FBAConfigFileItem>();
			Item->SettingClass = SelectedClass;
			Item->FullPath = FPaths::ConvertRelativePathToFull(Path);
			Item->Name = FPaths::GetCleanFilename(Path);
			Item->bExists = FPaths::FileExists(Item->FullPath);
			Item->bHasSection = Item->bExists && PlatformConfigCache->DoesSectionExist(*SectionName, FConfigCacheIni::NormalizeConfigIniPath(Item->FullPath));
			Item->bIsFinalIni = Path == Branch->IniPath;
			FullSourceFiles.Add(Item);
		}
	}
	else
	{
		auto Msg = FText::FromString(FString::Printf(TEXT("未找到配置分支：%s"), *ClassConfigName));
		FBAMiscUtils::ShowSimpleSlateNotification(Msg, SNotificationItem::CS_Fail);
	}

	ApplyFileFilter();
}

void SBAConfigViewer::RefreshPropertyTree()
{
	FullPropertyTreeRoot.Empty();
	if (!SelectedClass)
	{
		ApplyPropertyFilter();
		return;
	}

	UObject* DummyObj = nullptr;
	if (ShouldReadConstructorDefaults())
	{
		// create a dummy object to check default values before config is loaded
		DummyObj = StaticAllocateObject(SelectedClass, GetTransientPackage(), NAME_None, RF_NoFlags, EInternalObjectFlags::None, false);
		(*SelectedClass->ClassConstructor)(FObjectInitializer(DummyObj, nullptr, EObjectInitializerOptions::None));
	}

	for (TFieldIterator<FProperty> PropIt(SelectedClass); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;
		if (!Prop->HasAnyPropertyFlags(CPF_Config))
		{
			continue;
		}

		FString DefaultValue;
		if (DummyObj)
		{
			Prop->ExportTextItem_Direct(DefaultValue, Prop->ContainerPtrToValuePtr<void>(DummyObj), nullptr, nullptr, 0);
		}

		FString CurrValue;
		Prop->ExportTextItem_Direct(CurrValue, Prop->ContainerPtrToValuePtr<void>(SelectedClass->GetDefaultObject<UObject>()), nullptr, nullptr, 0);
		// UE_LOG(LogTemp, Warning, TEXT("Default Value for %s: %s Curr: %s"), *GetNameSafe(Prop), *DefaultValue, *CurrValue);

		TSharedPtr<FBAConfigPropertyItem> PropNode = MakeShared<FBAConfigPropertyItem>();
		PropNode->DisplayText = Prop->GetName();
		PropNode->DefaultValue = DefaultValue;
		PropNode->CurrValue = CurrValue;
		PropNode->Value = CurrValue;

		FString SectionName = GetClassPathName(SelectedClass);

		for (const auto& FileItem : FullSourceFiles)
		{
			if (!FileItem->bExists)
			{
				continue;
			}
			// Now TempObj has the values set in the C++ Constructor,
			// because LoadConfig() hasn't been called on this specific instance.

			FString Value;
			if (GConfig->GetString(*SectionName, *Prop->GetName(), Value, FConfigCacheIni::NormalizeConfigIniPath(FileItem->FullPath)))
			{
				TSharedPtr<FBAConfigPropertyItem> ValNode = MakeShared<FBAConfigPropertyItem>();
				ValNode->DisplayText = FileItem->FullPath;
				ValNode->Value = Value;
				ValNode->FileItem = FileItem;
				PropNode->Children.Add(ValNode);
				// ValNode->DefaultValue = CPPDefaultValue;
				// ValNode->CurrValue = CurrValue;
			}
		}

		if (PropNode->Children.Num() > 0)
		{
			FullPropertyTreeRoot.Add(PropNode);
		}
	}

	ApplyPropertyFilter();
}

TSharedRef<ITableRow> SBAConfigViewer::OnGenerateFileRow(TSharedPtr<FBAConfigFileItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SBAConfigFileRow, OwnerTable).Item(Item).ConfigViewer(SharedThis(this));
}

TSharedRef<ITableRow> SBAConfigViewer::OnGeneratePropertyRow(TSharedPtr<FBAConfigPropertyItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	bool bIsHeader = Item->Children.Num() > 0;

	FString DefaultValueStr = (bIsHeader && !Item->DefaultValue.IsEmpty()) ? FString::Printf(TEXT("Default: %s"), *Item->DefaultValue) : "";

	return SNew(STableRow<TSharedPtr<FBAConfigPropertyItem>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->DisplayText))
				.Font(FAppStyle::GetFontStyle(bIsHeader ? "NormalFontBold" : "NormalFont"))
				.OnDoubleClicked(this, &SBAConfigViewer::OnPropertyTreeItemDoubleClicked, Item)
			]
			+ SHorizontalBox::Slot().Padding(10, 0).AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(Item->Value)).ColorAndOpacity(FLinearColor::Green)
			]
			+ SHorizontalBox::Slot().Padding(4, 0).AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(DefaultValueStr)).ColorAndOpacity(FLinearColor::Yellow).Font(FAppStyle::GetFontStyle("SmallFont"))
			]
		];
}

FReply SBAConfigViewer::OnPropertyTreeItemDoubleClicked(const FGeometry&, const FPointerEvent&, TSharedPtr<FBAConfigPropertyItem> Item)
{
	if (Item->FileItem)
	{
		FPlatformProcess::ExploreFolder(*Item->FileItem->FullPath);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SBAConfigViewer::GetPropertyChildren(TSharedPtr<FBAConfigPropertyItem> Item, TArray<TSharedPtr<FBAConfigPropertyItem>>& OutChildren)
{
	OutChildren = Item->Children;
}

void SBAConfigViewer::OnFileSearchTextChanged(const FText& InFilterText)
{
	FileSearchFilter = InFilterText.ToString();
	ApplyFileFilter();
}

void SBAConfigViewer::OnFinalIniChanged(ECheckBoxState NewState)
{
	bIncludeFinalIni = (NewState == ECheckBoxState::Checked);
	RefreshSourceFiles();
	RefreshPropertyTree();
}

void SBAConfigViewer::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bDirtyPropertyTree)
	{
		bDirtyPropertyTree = false;
		RefreshPropertyTree();
	}
}

void SBAConfigViewer::OnPropertySearchTextChanged(const FText& InFilterText)
{
	PropertySearchFilter = InFilterText.ToString();
	ApplyPropertyFilter();
}

void SBAConfigViewer::OnRemoveMissingChanged(ECheckBoxState NewState)
{
	bHideMissingFiles = (NewState == ECheckBoxState::Checked);
	ApplyFileFilter();
}

void SBAConfigViewer::OnShowModifyingChanged(ECheckBoxState NewState)
{
	bShowModifyingFiles = (NewState == ECheckBoxState::Checked);
	ApplyFileFilter();
}

void SBAConfigViewer::OnReadConstructorDefaultsChanged(ECheckBoxState NewState)
{
	const FText Msg = INVTEXT("读取类构造函数默认值可能产生意外副作用。是否继续？");

	bool bEnabled = (NewState == ECheckBoxState::Checked);
	if (!bEnabled || FMessageDialog::Open(EAppMsgType::YesNo, Msg) == EAppReturnType::Yes)
	{
		ShouldReadConstructorDefaults() = bEnabled;
		UBlueprintAssistSettings_Search::Get().SaveConfig();

		RefreshPropertyTree();
	}
}

void SBAConfigViewer::OnShowChangedPropertiesChanged(ECheckBoxState NewState)
{
	bShowModifiedProperties = (NewState == ECheckBoxState::Checked);
	ApplyPropertyFilter();
}

void SBAConfigViewer::ApplyFileFilter()
{
	FilteredSourceFiles.Empty();

	for (const auto& Item : FullSourceFiles)
	{
		if (!FileSearchFilter.IsEmpty() && !Item->FullPath.Contains(PropertySearchFilter))
		{
			continue;
		}

		// If checkbox is off, show everything. If checkbox is on, only show if bExists is true.
		if (bHideMissingFiles && !Item->bExists)
		{
			continue;
		}

		if (bShowModifyingFiles && !Item->bHasSection)
		{
			continue;
		}

		FilteredSourceFiles.Add(Item);
	}

	FileListView->RequestListRefresh();
}

void SBAConfigViewer::ApplyPropertyFilter()
{
	FilteredPropertyTreeRoot.Empty();

	for (const auto& Item : FullPropertyTreeRoot)
	{
		if (!PropertySearchFilter.IsEmpty() && !Item->DisplayText.Contains(PropertySearchFilter))
		{
			continue;
		}

		if (ShouldReadConstructorDefaults() && bShowModifiedProperties)
		{
			// UE_LOG(LogTemp, Warning, TEXT("%s Checking %s, %s"), *Item->DisplayText, *Item->DefaultValue, *Item->CurrValue);
			if (Item->DefaultValue == Item->CurrValue)
			{
				/*
				 * TODO: This should be running the check below to compare each ini file to the default
				 * HOWEVER the value loaded from the ini file right now is incorrect for arrays
				 * we likely need to load it into a FProperty to read it correctly instead of reading directly from the ini
				 */

				// bool bAllDefault = true;
				// for (TSharedPtr<FBAConfigPropertyItem> Child : Item->Children)
				// {
				// 	if (Child->Value != Item->DefaultValue)
				// 	{
				// 		bAllDefault = false;
				// 	}
				// }
				//
				// if (bAllDefault)
				{
					continue;
				}
			}
		}

		FilteredPropertyTreeRoot.Add(Item);

		if (!PropertySearchFilter.IsEmpty())
		{
			PropertyTreeView->SetItemExpansion(Item, true);
		}
	}

	PropertyTreeView->RequestTreeRefresh();
}

void SBAConfigViewer::CheckSettingsObjectChanged(UObject* Obj, struct FPropertyChangedEvent&)
{
	if (Obj && Obj->GetClass() == SelectedClass)
	{
		bDirtyPropertyTree = true;
	}
}

bool& SBAConfigViewer::ShouldReadConstructorDefaults()
{
	return UBlueprintAssistSettings_Search::Get().bReadConstructorDefaults;
}

TSharedRef<SDockTab> SBAConfigViewer::CreateTab(const FSpawnTabArgs& Args)
{
	const TSharedRef<SDockTab> MajorTab = SNew(SDockTab).TabRole(ETabRole::NomadTab);
	MajorTab->SetContent(SNew(SBAConfigViewer));
	return MajorTab;
}
#endif
