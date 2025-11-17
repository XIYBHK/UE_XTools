// Copyright fpwong. All Rights Reserved.


#include "BlueprintAssistMisc/BACrashReportDialog.h"

#include "BlueprintAssistGlobals.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistStyle.h"
#include "BlueprintAssistMisc/BACrashReporter.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SRichTextHyperlink.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"

void SBACrashReportDialog::Construct(const FArguments& InArgs)
{
	FText OverviewText = INVTEXT(
		"The crash log includes technical details like the callstack, Unreal version and system information. "
		"It would be extremely helpful to include the optional crash related nodes, as this gives me the best chance of replicating the crash. "
		"For more details about the process, please see the links below."
	);

	auto OnLinkClicked = [](const FSlateHyperlinkRun::FMetadata& Metadata)
	{
		if (const FString* Url = Metadata.Find(TEXT("href")))
		{
			FPlatformProcess::LaunchURL(**Url, nullptr, nullptr);
		}
	};

	SWindow::Construct(SWindow::FArguments()
						.Title(FText::FromString("Blueprint Assist Crash Report"))
						.IsPopupWindow(true)
						.IsTopmostWindow(true)
						.ClientSize(FVector2D(500, 300))
	);

	// TSharedRef<SVerticalBox> ReportList = SNew(SVerticalBox);
	// for (const FBACrashReport& Report : InReports)
	// {
	// 	ReportList->AddSlot().AttachWidget(SNew(STextBlock).Text(FText::FromString(Report.ReportId)));
	// }

	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);

	auto Inner = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 0.f, 0.f, 0.f))
			.AutoWidth()
			[
				SNew(STextBlock)
#if ENGINE_MINOR_VERSION >= 26 || ENGINE_MAJOR_VERSION >= 5
				.TransformPolicy(ETextTransformPolicy::ToUpper)
#endif
				.Text(INVTEXT("Blueprint Assist Crash Reporter"))
				.Font(TitleFont)
				.TextStyle(BA_STYLE_CLASS::Get(), TEXT("DetailsView.CategoryTextStyle"))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4)
		[
			SNew(SRichTextBlock)
			.AutoWrapText(true)
			.TextStyle(BA_STYLE_CLASS::Get(), TEXT("LargeText"))
			.WrappingPolicy(ETextWrappingPolicy::DefaultWrapping)
			.DecoratorStyleSet(&BA_STYLE_CLASS::Get())
			.Text(OverviewText)
			+ SRichTextBlock::HyperlinkDecorator(TEXT("browser"), FSlateHyperlinkRun::FOnClick::CreateLambda(OnLinkClicked))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SHyperlink)
				.Text(INVTEXT("Blueprint Assist Privacy Policy"))
				.OnNavigate_Lambda([=]() { FPlatformProcess::LaunchURL(TEXT("https://blueprintassist.github.io/miscellaneous/privacy-policy/"), nullptr, nullptr); })
				.Style(BA_STYLE_CLASS::Get(), "EULA.Hyperlink")
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SSpacer).Size(FVector2D(24.0f, 0.0f))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SHyperlink)
				.Text(INVTEXT("BugSplat Security and Compliance"))
				.OnNavigate_Lambda([=]() { FPlatformProcess::LaunchURL(TEXT("https://docs.bugsplat.com/introduction/production/security-privacy-and-compliance"), nullptr, nullptr); })
				.Style(BA_STYLE_CLASS::Get(), "EULA.Hyperlink")
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0)
		// TODO list of crash reports
		// + SVerticalBox::Slot()
		// .Padding(0, 24)
		// .FillHeight(1.0)
		// [
		// 	SNew(SVerticalBox)
		// 	+ SVerticalBox::Slot()
		// 	[
		// 		SNew(STextBlock).Text(INVTEXT("Crash Report List")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
		// 	]
		// 	+ SVerticalBox::Slot()
		// 	[
		// 		SNew(SScrollBox)
		// 		.ScrollBarAlwaysVisible(true)
		// 		+ SScrollBox::Slot()
		// 		[
		// 			ReportList
		// 		]
		// 	]
		// ]
		+ SVerticalBox::Slot() // CHECKBOX - crash Related nodes
		.AutoHeight()
		[
			SNew(SCheckBox)
			.ToolTipText(INVTEXT("Send the nodes that triggered the crash. Only for formatting related crashes."))
			.Content()
			[
				SNew(STextBlock).Text(INVTEXT("Include Crash Related Nodes"))
			]
			.IsChecked_Lambda([]()
			{
				return UBASettings_Advanced::Get().bIncludeNodesInCrashReport ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
			{
				UBASettings_Advanced& BASettings = UBASettings_Advanced::GetMutable();
				BASettings.bIncludeNodesInCrashReport = (NewState == ECheckBoxState::Checked);
				BASettings.PostEditChange();
				BASettings.SaveConfig();
			})
		]
		+ SVerticalBox::Slot() // CHECKBOX - Blueprint Assist settings
		.AutoHeight()
		[
			SNew(SCheckBox)
			.ToolTipText(INVTEXT("Send your Blueprint Assist settings"))
			.Content()
			[
				SNew(STextBlock).Text(INVTEXT("Include Blueprint Assist Settings"))
			]
			.IsChecked_Lambda([]()
			{
				return UBASettings_Advanced::Get().bIncludeSettingsInCrashReport ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
			{
				UBASettings_Advanced& BASettings = UBASettings_Advanced::GetMutable();
				BASettings.bIncludeSettingsInCrashReport = (NewState == ECheckBoxState::Checked);
				BASettings.PostEditChange();
				BASettings.SaveConfig();
			})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(FString::Printf(TEXT("Send %d Report(s)"), FBACrashReporter::Get().GetPendingReports().Num())))
				.OnClicked_Lambda([&]()
				{
					FBACrashReporter::Get().SendReports();
					RequestDestroyWindow();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString("Cancel"))
				.OnClicked_Lambda([&]()
				{
					RequestDestroyWindow();
					return FReply::Handled();
				})
			]
		];

	TSharedRef<SBorder> Styling = SNew(SBorder)
		.BorderImage(FBAStyle::GetBrush("BlueprintAssist.PanelBorder"))
		.Padding(16)
		[
			Inner
		];

	SetContent(Styling);
}
