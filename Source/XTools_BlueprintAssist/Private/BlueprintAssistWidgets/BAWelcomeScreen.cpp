// Copyright fpwong. All Rights Reserved.


#include "BlueprintAssistWidgets/BAWelcomeScreen.h"

#include "BlueprintAssistCommands.h"
#include "BlueprintAssistSettings.h"
#include "BlueprintAssistStyle.h"
#include "BlueprintAssistTypes.h"
#include "ISinglePropertyView.h"
#include "BlueprintAssistMisc/BAMiscUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SWindow.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"

void SBAWelcomeScreen::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

#if BA_UE_VERSION_OR_LATER(5, 0)
	FName ButtonStyle("FVerticalToolBar.ToggleButton");
#else
	FName ButtonStyle("Menu.ToggleButton");
#endif

	// Use the tool bar style for this check box
	auto IntroMenuEntry = SNew(SCheckBox)
		.Style(BA_STYLE_CLASS::Get(), ButtonStyle)
		.Padding(8.0f)
		.IsChecked_Lambda([&]()
		{
			return WidgetSwitcher->GetActiveWidgetIndex() == 0 ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([&](ECheckBoxState State)
		{
			WidgetSwitcher->SetActiveWidgetIndex(0);
		})
		[
			SNew(STextBlock).Text(INVTEXT("Introduction"))
		];

	auto SettingsMenuEntry = SNew(SCheckBox)
		.Style(BA_STYLE_CLASS::Get(), ButtonStyle)
		.Padding(8.0f)
		.IsChecked_Lambda([&]()
		{
			return WidgetSwitcher->GetActiveWidgetIndex() == 1 ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([&](ECheckBoxState State)
		{
			WidgetSwitcher->SetActiveWidgetIndex(1);
		})
		[
			SNew(STextBlock).Text(INVTEXT("Settings"))
		];

	// XTools 适配：使用统一的设置对象
	FSinglePropertyParams SinglePropertyParams;
	SinglePropertyParams.NamePlacement = EPropertyNamePlacement::Hidden;
	TSharedPtr<ISinglePropertyView> SinglePropertyView = EditModule.CreateSingleProperty(&UBASettings::GetMutable(), GET_MEMBER_NAME_CHECKED(UBASettings, bShowWelcomeScreen), SinglePropertyParams);

	auto SidePanelBox = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			IntroMenuEntry
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SettingsMenuEntry
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SSpacer)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SinglePropertyView.ToSharedRef()
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(STextBlock).Text(INVTEXT("Show on startup"))
			]
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
			SAssignNew(WidgetSwitcher, SWidgetSwitcher)
			+ SWidgetSwitcher::Slot()
			[
				CreateIntroductionWidget()
			]
			+ SWidgetSwitcher::Slot()
			[
				CreateSettingsWidget()
			]
		]
	];
}

TSharedRef<SWidget> SBAWelcomeScreen::CreateIntroductionWidget()
{
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(16.0f)
			[
				SNew(STextBlock)
				.Text(INVTEXT("Welcome to Blueprint Assist!"))
				.Font(BA_STYLE_CLASS::Get().GetFontStyle(TEXT("PropertyWindow.BoldFont")))
				.Justification(ETextJustify::Center)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(16.0f)
			[
				SNew(SRichTextBlock)
				.Text(INVTEXT("Blueprint Assist is a plugin for Unreal Engine that helps you format and organize your blueprint graphs.\n\nKey features:\n• Auto-formatting of blueprint nodes\n• Smart node positioning\n• Hotkey shortcuts\n• Customizable settings"))
				.AutoWrapText(true)
			]
		];
}

TSharedRef<SWidget> SBAWelcomeScreen::CreateSettingsWidget()
{
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bAllowMultipleTopLevelObjects = false;

	TSharedRef<IDetailsView> DetailsView = EditModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(&UBASettings::GetMutable());

	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			DetailsView
		];
}

TSharedRef<SDockTab> SBAWelcomeScreen::CreateTab(const FSpawnTabArgs& Args)
{
	const TSharedRef<SDockTab> MajorTab = SNew(SDockTab).TabRole(ETabRole::NomadTab);
	MajorTab->SetContent(SNew(SBAWelcomeScreen));
	return MajorTab;
}
