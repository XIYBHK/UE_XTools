// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistWidgets/BASettingsChangeWindow.h"

#include "BlueprintAssistGlobals.h"
#include "BlueprintAssistMisc/BASettingsBase.h"
#include "BlueprintAssistSettings.h"
#include "BlueprintAssistStyle.h"
#include "BlueprintAssistTypes.h"
#include "BlueprintAssistMisc/BAMiscUtils.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SWindow.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"

static const FName PropertyHeaderName(TEXT("Property"));
static const FName DefaultHeaderName(TEXT("Default"));
static const FName CurrentHeaderName(TEXT("Current"));
static const FName ButtonHeaderName(TEXT("Button"));

void SBASettingTableRow::Construct(const FArguments& InArgs, TSharedPtr<FBASettingChangeData> InData, const TSharedRef<STableViewBase>& OwnerTable)
{
	Data = InData;
	SMultiColumnTableRow::Construct(SMultiColumnTableRow::FArguments(), OwnerTable);
}

TSharedRef<SWidget> SBASettingTableRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	FText TooltipText;

	TSharedPtr<SWidget> InnerContent;
	if (ColumnName == PropertyHeaderName)
	{
		FText PropertyText = FBASettingsChange::GetJsonPropertyDisplayName(Data->Change.PropertyName, Data->SettingsObj);
		InnerContent = SNew(STextBlock).Text(PropertyText).Font(BA_GET_FONT_STYLE(TEXT("PropertyWindow.BoldFont")));
		TooltipText = PropertyText;
	}

	if (ColumnName == DefaultHeaderName)
	{
		FString ValueAsStr = FBASettingsChange::GetJsonValueAsString(Data->Change.PropertyName, Data->Change.OldValue);
		InnerContent = SNew(STextBlock)
			.Text(FText::FromString(ValueAsStr))
			.OnDoubleClicked(FPointerEventHandler::CreateLambda([&](const FGeometry&, const FPointerEvent&)
			{
				FBASettingsChange& Change = Data->Change;
				FPlatformApplicationMisc::ClipboardCopy(*FBASettingsChange::GetJsonValueAsString(Change.PropertyName, Change.OldValue, true));
				FNotificationInfo Notification(INVTEXT("Copied to clipboard"));
				Notification.ExpireDuration = 1.0f;
				FSlateNotificationManager::Get().AddNotification(Notification);
				return FReply::Handled();
			}));

		TooltipText = FText::FromString(ValueAsStr);
	}

	if (ColumnName == CurrentHeaderName)
	{
		FString ValueAsStr = FBASettingsChange::GetJsonValueAsString(Data->Change.PropertyName, Data->Change.NewValue);
		InnerContent = SNew(STextBlock)
			.Text(FText::FromString(ValueAsStr))
			.OnDoubleClicked(FPointerEventHandler::CreateLambda([&](const FGeometry&, const FPointerEvent&)
			{
				FBASettingsChange& Change = Data->Change;
				FPlatformApplicationMisc::ClipboardCopy(*FBASettingsChange::GetJsonValueAsString(Change.PropertyName, Change.NewValue, true));
				FNotificationInfo Notification(INVTEXT("Copied to clipboard"));
				Notification.ExpireDuration = 1.0f;
				FSlateNotificationManager::Get().AddNotification(Notification);
				return FReply::Handled();
			}));

		TooltipText = FText::FromString(ValueAsStr);
	}

	if (ColumnName == ButtonHeaderName)
	{
		InnerContent = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(INVTEXT("Log")).OnClicked_Lambda([&]()
				{
					Data->Change.LogChange();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(INVTEXT("Reset")).OnClicked_Lambda([&]()
				{
					Data->Change.ResetToDefault(Data->SettingsObj);
					return FReply::Handled();
				})
			];
	}

	if (!InnerContent)
	{
		InnerContent = SNew(STextBlock).Text(INVTEXT("ERROR SBASettingsTableRow"));
	}

	return SNew(SBox).Padding(FMargin(4, 2)).ToolTipText(TooltipText)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				InnerContent.ToSharedRef()
			]
		];
}

SBASettingsListView::~SBASettingsListView()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
}

void SBASettingsListView::Construct(const FArguments& InArgs)
{
	SAssignNew(HeaderRowWidget, SHeaderRow);

	SListView::Construct
	(
		SListView::FArguments()
		.ListItemsSource(&Rows)
		.OnGenerateRow(this, &SBASettingsListView::OnGenerateRow)
		.HeaderRow(HeaderRowWidget)
	);
}

void SBASettingsListView::Refresh(UObject* NewSettings)
{
	SettingsObj = NewSettings;

	HeaderRowWidget->ClearColumns();
	HeaderRowWidget->AddColumn(SHeaderRow::Column(PropertyHeaderName)
	.DefaultLabel(INVTEXT("Property"))
	.FillWidth(100.f));

	HeaderRowWidget->AddColumn(SHeaderRow::Column(DefaultHeaderName)
	.DefaultLabel(INVTEXT("Default"))
	.FillWidth(100.f));

	HeaderRowWidget->AddColumn(SHeaderRow::Column(CurrentHeaderName)
	.DefaultLabel(INVTEXT("Current"))
	.FillWidth(100.f));

	HeaderRowWidget->AddColumn(SHeaderRow::Column(ButtonHeaderName)
	.DefaultLabel(INVTEXT("ResetToDefault"))
	.FillWidth(100.f));

	Rows.Empty();

	if (!Handle.IsValid())
	{
		Handle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &SBASettingsListView::CheckSettingsObjectChanged);
	}

	TArray<FBASettingsChange> Changes;
	if (UBASettings* BASettings = Cast<UBASettings>(SettingsObj))
	{
		Changes = BASettings->GetChanges();
	}
	for (FBASettingsChange& Change : Changes)
	{
		TSharedPtr<FBASettingChangeData> Data = MakeShared<FBASettingChangeData>();
		Data->Change = Change;
		Data->SettingsObj = SettingsObj;
		Rows.Add(Data);
	}

	RequestListRefresh();
}

TSharedRef<ITableRow> SBASettingsListView::OnGenerateRow(TSharedPtr<FBASettingChangeData> InDisplayNode, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SBASettingTableRow, InDisplayNode, OwnerTable);
}

void SBASettingsListView::CheckSettingsObjectChanged(UObject* Obj, struct FPropertyChangedEvent& Event)
{
	if (Obj == SettingsObj)
	{
		Refresh(SettingsObj);
	}
}

void SBASettingsChangeWindow::Construct(const FArguments& InArgs)
{
#if BA_UE_VERSION_OR_LATER(5, 0)
	FName ButtonStyle("FVerticalToolBar.ToggleButton");
#else
	FName ButtonStyle("Menu.ToggleButton");
#endif

	// XTools 适配：只使用统一的 UBASettings
	SettingsObjects.Add(&UBASettings::GetMutable());

	auto SidePanelBox = SNew(SVerticalBox);

	for (auto Obj : SettingsObjects)
	{
		auto Widget = MakeSettingMenuButton(Obj);
		SidePanelBox->AddSlot().AutoHeight().AttachWidget(Widget);
	}

	SidePanelBox->AddSlot().FillHeight(1.0f)
	[
		SNew(SSpacer)
	];

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBorder).BorderImage(FBAStyle::GetBrush("BlueprintAssist.PanelBorder")).Padding(24.0f)
			[
				SidePanelBox
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			SAssignNew(SettingsList, SBASettingsListView)
		]
	];

	SetActiveSettings(SettingsObjects[0]);
}

TSharedRef<SDockTab> SBASettingsChangeWindow::CreateTab(const FSpawnTabArgs& Args)
{
	const TSharedRef<SDockTab> MajorTab = SNew(SDockTab).TabRole(ETabRole::NomadTab);
	MajorTab->SetContent(SNew(SBASettingsChangeWindow));
	return MajorTab;
}

TSharedRef<SWidget> SBASettingsChangeWindow::MakeSettingMenuButton(UObject* SettingsObj)
{
#if BA_UE_VERSION_OR_LATER(5, 0)
	FName ButtonStyle("FVerticalToolBar.ToggleButton");
#else
	FName ButtonStyle("Menu.ToggleButton");
#endif

	const FText DisplayText = SettingsObj->GetClass()->GetDisplayNameText();

	return SNew(SCheckBox)
			.Style(BA_STYLE_CLASS::Get(), ButtonStyle)
			.Padding(8.0f)
			.IsChecked_Lambda([&, SettingsObj]()
				{
					return ActiveSetting == SettingsObj ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
			.OnCheckStateChanged_Lambda([&, SettingsObj](ECheckBoxState State)
				{
					SetActiveSettings(SettingsObj);
				})
				[
					SNew(STextBlock).Text(DisplayText)
				];
}

void SBASettingsChangeWindow::SetActiveSettings(UObject* Settings)
{
	ActiveSetting = Settings;
	SettingsList->Refresh(ActiveSetting);
}
