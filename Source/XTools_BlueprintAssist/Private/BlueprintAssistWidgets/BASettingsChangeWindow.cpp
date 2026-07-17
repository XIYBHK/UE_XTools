// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistWidgets/BASettingsChangeWindow.h"

#include "BlueprintAssistCommands.h"
#include "BlueprintAssistSettings.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistSettings_EditorFeatures.h"
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
				FPlatformApplicationMisc::ClipboardCopy(*Change.GetJsonValueAsString(Change.PropertyName, Change.OldValue, true));
				FNotificationInfo Notification(INVTEXT("已复制到剪贴板"));
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
				FPlatformApplicationMisc::ClipboardCopy(*Change.GetJsonValueAsString(Change.PropertyName, Change.NewValue, true));
				FNotificationInfo Notification(INVTEXT("已复制到剪贴板"));
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
				SNew(SButton).Text(INVTEXT("记录")).OnClicked_Lambda([&]()
				{
					Data->Change.LogChange();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(INVTEXT("重置")).OnClicked_Lambda([&]()
				{
					Data->Change.ResetToDefault(Data->SettingsObj);
					return FReply::Handled();
				})
			];
	}

	if (!InnerContent)
	{
		InnerContent = SNew(STextBlock).Text(INVTEXT("SBASettingsTableRow 错误"));
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

void SBASettingsListView::Refresh(UBASettingsBase* NewSettings)
{
	SettingsObj = NewSettings;

	HeaderRowWidget->ClearColumns();
	HeaderRowWidget->AddColumn(SHeaderRow::Column(PropertyHeaderName)
	.DefaultLabel(INVTEXT("属性"))
	.FillWidth(100.f));

	HeaderRowWidget->AddColumn(SHeaderRow::Column(DefaultHeaderName)
	.DefaultLabel(INVTEXT("默认值"))
	.FillWidth(100.f));

	HeaderRowWidget->AddColumn(SHeaderRow::Column(CurrentHeaderName)
	.DefaultLabel(INVTEXT("当前值"))
	.FillWidth(100.f));

	HeaderRowWidget->AddColumn(SHeaderRow::Column(ButtonHeaderName)
	.DefaultLabel(INVTEXT("恢复默认值"))
	.FillWidth(100.f));

	Rows.Empty();

	if (!Handle.IsValid())
	{
		Handle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &SBASettingsListView::CheckSettingsObjectChanged);
	}

	TArray<FBASettingsChange> Changes = SettingsObj->GetChanges();
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

	SettingsObjects =
	{
		&UBASettings::GetMutable(),
		&UBASettings_EditorFeatures::GetMutable(),
		&UBASettings_Advanced::GetMutable(),
	};

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

	SidePanelBox->AddSlot().AutoHeight().AttachWidget(MakeSaveButton(EBASettingsLocation::PROJECT));
	SidePanelBox->AddSlot().AutoHeight().AttachWidget(MakeSaveButton(EBASettingsLocation::ENGINE));
	SidePanelBox->AddSlot().AutoHeight().AttachWidget(MakeSaveButton(EBASettingsLocation::GLOBAL));

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

TSharedRef<SWidget> SBASettingsChangeWindow::MakeSettingMenuButton(UBASettingsBase* SettingsObj)
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

void SBASettingsChangeWindow::SetActiveSettings(UBASettingsBase* Settings)
{
	ActiveSetting = Settings;
	SettingsList->Refresh(ActiveSetting);
}

void SBASettingsChangeWindow::SaveSettings(EBASettingsLocation Location)
{
	TArray<UObject*> Settings = {
		&UBASettings::GetMutable(),
		&UBASettings_Advanced::GetMutable(),
		&UBASettings_EditorFeatures::GetMutable(),
	};

	FString IniPath = GetIniPath(Location);

	if (!FBAMiscUtils::MakeFileWritable(IniPath))
	{
		UE_LOG(LogBlueprintAssist, Warning, TEXT("[%hs] Unable to make file writable %s"), __FUNCTION__, *IniPath);
		FBAMiscUtils::ShowSimpleSlateNotification(INVTEXT("无法将 ini 文件设为可写"), SNotificationItem::CS_Fail);
		return;
	}

	bool bSuccess = true;
	for (auto Set : Settings)
	{
		bSuccess &= Set->TryUpdateDefaultConfigFile(FConfigCacheIni::NormalizeConfigIniPath(IniPath));
		if (!bSuccess)
		{
			UE_LOG(LogBlueprintAssist, Warning, TEXT("[%hs] Failed to save setting %s to %s"), __FUNCTION__, *GetNameSafe(Set), *IniPath);
			FString Msg = FString::Printf(TEXT("Failed to save settings %s to %s"), *GetNameSafe(Set), *IniPath);
			FBAMiscUtils::ShowSimpleSlateNotification(FText::FromString(Msg), SNotificationItem::CS_Fail);
		}
	}

	if (bSuccess)
	{
		UE_LOG(LogBlueprintAssist, Log, TEXT("Saved settings to %s"), *IniPath);
		FString Msg = FString::Printf(TEXT("Saved settings to %s"), *IniPath);
		FBAMiscUtils::ShowSimpleSlateNotification(FText::FromString(Msg), SNotificationItem::CS_Success);
	}
}

FString SBASettingsChangeWindow::GetIniPath(EBASettingsLocation Location)
{
	FString IniPath;

	// all 3 setting classes should have the same ini location, so just select UBASettings
	switch (Location)
	{
	case EBASettingsLocation::PROJECT:
		{
			IniPath = UBASettings::GetMutable().GetDefaultConfigFilename();
			break;
		}
	case EBASettingsLocation::ENGINE:
		{
			IniPath = FPaths::ConvertRelativePathToFull(FPaths::EngineConfigDir()) + "BaseEditorPerProjectUserSettings.ini";
			break;
		}
	case EBASettingsLocation::GLOBAL:
		{
			IniPath = UBASettings::GetMutable().GetGlobalUserConfigFilename();
			break;
		}
	default: ;
	}

	return IniPath;
}

TSharedRef<SWidget> SBASettingsChangeWindow::MakeSaveButton(EBASettingsLocation Location)
{
	FText SettingLocationText = StaticEnum<EBASettingsLocation>()->GetDisplayNameTextByValue(static_cast<int64>(Location));

	FString Info;
	switch (Location)
	{
	case EBASettingsLocation::PROJECT:
			Info = "local to project";
			break;
	case EBASettingsLocation::ENGINE:
		{
			Info = "shared between projects for this engine version";
			break;
		}
	case EBASettingsLocation::GLOBAL:
		{
			Info = "shared between projects and engine versions";
			break;
		}
	default: ;
	}

	FText TooltipText = FText::FromString(FString::Printf(TEXT("Save settings %s (%s)"), *Info, *GetIniPath(Location)));

	return SNew(SButton)
		.ButtonStyle(BA_STYLE_CLASS::Get(), "FlatButton")
		.Text(FText::Format(INVTEXT("保存到 {Loc}"), {SettingLocationText}))
		.ToolTipText(TooltipText)
		.OnClicked_Lambda([&, Location]
	{
		SaveSettings(Location);
		return FReply::Handled();
	});
}
