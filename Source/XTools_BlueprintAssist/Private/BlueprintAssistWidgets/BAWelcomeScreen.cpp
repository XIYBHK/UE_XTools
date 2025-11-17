// Copyright fpwong. All Rights Reserved.


#include "BlueprintAssistWidgets/BAWelcomeScreen.h"

#include "BlueprintAssistCommands.h"
#include "BlueprintAssistSettings.h"
#include "BlueprintAssistSettings_EditorFeatures.h"
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
			SNew(STextBlock).Text(INVTEXT("介绍"))
		];

	auto CustomizeMenuEntry = SNew(SCheckBox)
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
			SNew(STextBlock).Text(INVTEXT("自定义"))
		];

	WidgetSwitcher = SNew(SWidgetSwitcher)
		+ SWidgetSwitcher::Slot().Padding(24.0f)
		[
			MakeIntroPage()
		]
		+ SWidgetSwitcher::Slot().Padding(24.0f)
		[
			MakeCustomizePage()
		];

	FSinglePropertyParams ShowWelcomeScreenParams;
	ShowWelcomeScreenParams.NotifyHook = &SettingsPropertyHook;
	ShowWelcomeScreenParams.NamePlacement = EPropertyNamePlacement::Type::Inside;
	ShowWelcomeScreenParams.Font = BA_STYLE_CLASS::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont"));

	auto SideButtons =
		SNew(SBorder).BorderImage(FBAStyle::GetBrush("BlueprintAssist.PanelBorder")).Padding(24.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				IntroMenuEntry
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				CustomizeMenuEntry
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SSpacer)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().HAlign(HAlign_Right)
				[
					EditModule.CreateSingleProperty(GetMutableDefault<UBASettings_EditorFeatures>(), GET_MEMBER_NAME_CHECKED(UBASettings_EditorFeatures, bShowWelcomeScreenOnLaunch), ShowWelcomeScreenParams).ToSharedRef()
				]
			]
		];

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SideButtons
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			WidgetSwitcher.ToSharedRef()
		]
	];
}

TSharedRef<SDockTab> SBAWelcomeScreen::CreateWelcomeScreenTab(const FSpawnTabArgs& Args)
{
	const TSharedRef<SDockTab> MajorTab = SNew(SDockTab).TabRole(ETabRole::NomadTab);
	MajorTab->SetContent(SNew(SBAWelcomeScreen));
	return MajorTab;
}

TSharedRef<SWidget> SBAWelcomeScreen::MakeCommandWidget(TSharedPtr<FUICommandInfo> Command)
{
	FString Text = Command->GetLabel().ToString() + " " + FBAMiscUtils::GetInputChordName(Command->GetFirstValidChord().Get());
	return SNew(STextBlock).Text(FText::FromString(Text));
}

FText SBAWelcomeScreen::GetCommandText(TSharedPtr<FUICommandInfo> Command)
{
	return FText::FromString(FString::Printf(TEXT("<NormalText.Important>%s (%s)</>"),
		*Command->GetLabel().ToString(),
		*FBAMiscUtils::GetInputChordName(Command->GetFirstValidChord().Get())));
}

TSharedRef<SWidget> SBAWelcomeScreen::MakeIntroPage()
{
	auto OnLinkClicked = [](const FSlateHyperlinkRun::FMetadata& Metadata)
	{
		if (const FString* Url = Metadata.Find(TEXT("href")))
		{
			FPlatformProcess::LaunchURL(**Url, nullptr, nullptr);
		}
	};

	const FText IntroText = INVTEXT(
		"<LargeText>欢迎使用 Blueprint Assist 插件!</>"
		"\n要了解插件功能概述,请从查看 <a id=\"browser\" href=\"https://blueprintassist.github.io/features/command-list\" style=\"Hyperlink\">wiki中的示例</> 和 "
		"<a id=\"browser\" href=\"https://blueprintassist.github.io/features/editor-features/#auto-enable-instance-editable\" style=\"Hyperlink\">新编辑器功能概述</> 开始"
		"\n打开蓝图或支持的图表时,你可以找到一个新的工具栏图标,这将允许快速访问一些有用的设置和菜单。"
	);

	const FText FeaturesText = FText::FormatOrdered(INVTEXT(
			"<LargeText>插件的主要功能</>"
			"\n\t- 使用 <NormalText.Important>箭头键</> 在节点上导航引脚"
			"\n\t- 选中节点后,按 {0} 来布局节点"
			"\n\t- 使用 {1} 调出节点创建菜单"
			"\n\t- 使用 {2} 打开编辑器中所有选项卡和设置的菜单"
			"\n\t- 使用 {3} 显示插件和编辑器中所有快捷键的菜单"
			"\n\t- 使用 {4} 尝试通过距离连接选中节点上的任何未链接引脚")
		, GetCommandText(FBACommands::Get().FormatNodes)
		, GetCommandText(FBACommands::Get().OpenContextMenu)
		, GetCommandText(FBACommands::Get().OpenWindow)
		, GetCommandText(FBACommands::Get().OpenBlueprintAssistHotkeySheet)
		, GetCommandText(FBACommands::Get().ConnectUnlinkedPins)
	);

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SRichTextBlock)
			.AutoWrapText(true)
			.WrappingPolicy(ETextWrappingPolicy::DefaultWrapping)
			.Text(IntroText)
			.DecoratorStyleSet(&BA_STYLE_CLASS::Get())
			+ SRichTextBlock::HyperlinkDecorator(TEXT("browser"), FSlateHyperlinkRun::FOnClick::CreateLambda(OnLinkClicked))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SSpacer).Size(FVector2D(0.0f, 24.0f))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SRichTextBlock)
			.AutoWrapText(true)
			.WrappingPolicy(ETextWrappingPolicy::DefaultWrapping)
			.Text(FeaturesText)
			.DecoratorStyleSet(&BA_STYLE_CLASS::Get())
			+ SRichTextBlock::HyperlinkDecorator(TEXT("browser"), FSlateHyperlinkRun::FOnClick::CreateLambda(OnLinkClicked))
		];
}

TSharedRef<SWidget> SBAWelcomeScreen::MakeCustomizePage()
{
	TMap<UObject*, TArray<FName>> FormattingProps;
	FormattingProps.Add(GetMutableDefault<UBASettings>(), {
			GET_MEMBER_NAME_CHECKED(UBASettings, bGloballyDisableAutoFormatting),
			GET_MEMBER_NAME_CHECKED(UBASettings, ParameterStyle)
	});

	TMap<UObject*, TArray<FName>> AppearanceProps;
	AppearanceProps.Add(GetMutableDefault<UBASettings_EditorFeatures>(), {
			GET_MEMBER_NAME_CHECKED(UBASettings_EditorFeatures, bEnableInvisibleKnotNodes),
	});

	TMap<UObject*, TArray<FName>> MiscProps;
	MiscProps.Add(GetMutableDefault<UBASettings_EditorFeatures>(), {
			GET_MEMBER_NAME_CHECKED(UBASettings_EditorFeatures, bPlayLiveCompileSound),
	});

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SRichTextBlock)
			.AutoWrapText(true)
			.WrappingPolicy(ETextWrappingPolicy::DefaultWrapping)
			.Text(INVTEXT("<LargeText>格式化</>"))
			.DecoratorStyleSet(&BA_STYLE_CLASS::Get())
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 12.0f)
		[
			MakePropertiesList(FormattingProps)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SRichTextBlock).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::DefaultWrapping).Text(INVTEXT("<LargeText>外观</>")).DecoratorStyleSet(&BA_STYLE_CLASS::Get())
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 12.0f)
		[
			MakePropertiesList(AppearanceProps)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SRichTextBlock).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::DefaultWrapping).Text(INVTEXT("<LargeText>杂项</>")).DecoratorStyleSet(&BA_STYLE_CLASS::Get())
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 12.0f)
		[
			MakePropertiesList(MiscProps)
		];
}

TSharedRef<SWidget> SBAWelcomeScreen::MakePropertiesList(const TMap<UObject*, TArray<FName>>& Properties)
{
	TSharedRef<SVerticalBox> PropBox = SNew(SVerticalBox);
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	for (auto& Elem : Properties)
	{
		const auto& PropertyNames = Elem.Value;
		// create the widgets for setting properties we want to edit
		{
			FSinglePropertyParams Params;
			Params.NotifyHook = &SettingsPropertyHook;
			Params.NamePlacement = EPropertyNamePlacement::Type::Inside;

			for (auto& PropertyName : PropertyNames)
			{
				PropBox->AddSlot().Padding(8.0f, 4.0f).AttachWidget(EditModule.CreateSingleProperty(Elem.Key, PropertyName, Params).ToSharedRef());
			}
		}
	}

	return PropBox;
}
