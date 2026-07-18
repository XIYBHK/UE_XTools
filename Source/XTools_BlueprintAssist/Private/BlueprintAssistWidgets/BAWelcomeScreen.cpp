// Copyright fpwong. All Rights Reserved.


#include "BlueprintAssistWidgets/BAWelcomeScreen.h"

#include "BASettings_Meta.h"
#include "BlueprintAssistCommands.h"
#include "BlueprintAssistSettings.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistSettings_EditorFeatures.h"
#include "BlueprintAssistStyle.h"
#include "BlueprintAssistTypes.h"
#include "DesktopPlatformModule.h"
#include "EditorDirectories.h"
#include "ISettingsEditorModule.h"
#include "ISinglePropertyView.h"
#include "BlueprintAssistMisc/BAMiscUtils.h"
#include "BlueprintAssistMisc/FBAScopedPropertySetter.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
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
		"<LargeText>欢迎使用 Blueprint Assist 插件！</>"
		"\n要了解插件功能，请先查看 <a id=\"browser\" href=\"https://blueprintassist.github.io/features/command-list\" style=\"Hyperlink\">Wiki 示例</> 和 "
		"<a id=\"browser\" href=\"https://blueprintassist.github.io/features/editor-features/#auto-enable-instance-editable\" style=\"Hyperlink\">新版编辑器功能概览</>。"
		"\n打开蓝图或受支持的图表后，工具栏中会显示快捷入口，可快速访问常用设置和菜单。"
	);

	const FText FeaturesText = FText::FormatOrdered(INVTEXT(
			"<LargeText>插件主要功能</>"
			"\n\t- 使用 <NormalText.Important>方向键</> 在节点引脚之间导航"
			"\n\t- 选中节点后按 {0} 布局节点"
			"\n\t- 使用 {1} 打开节点创建菜单"
			"\n\t- 使用 {2} 打开编辑器标签页和设置菜单"
			"\n\t- 使用 {3} 查看插件和编辑器快捷键"
			"\n\t- 使用 {4} 按距离连接选中节点上未连接的引脚")
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
			SNew(SRichTextBlock).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::DefaultWrapping).Text(INVTEXT("<LargeText>其他</>")).DecoratorStyleSet(&BA_STYLE_CLASS::Get())
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 12.0f)
		[
			MakePropertiesList(MiscProps)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SRichTextBlock).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::DefaultWrapping).Text(INVTEXT("<LargeText>设置文件</>")).DecoratorStyleSet(&BA_STYLE_CLASS::Get())
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 12.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f, 0.0f, 0.0f)
			[
				MakeProperty(GetMutableDefault<UBASettings_Meta>(), GET_MEMBER_NAME_CHECKED(UBASettings_Meta, CustomSettingsIniPath))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f).HAlign(HAlign_Left)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(INVTEXT("新建"))
					.ToolTipText(INVTEXT("使用当前插件设置创建新的 ini 文件"))
					.OnClicked_Static(&SBAWelcomeScreen::OnCreateNewCustomSettingsClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(INVTEXT("保存设置"))
					.ToolTipText(INVTEXT("使用当前设置覆盖 ini 文件"))
					.IsEnabled_Static(&SBAWelcomeScreen::IsCustomSettingsPathValid)
					.OnClicked_Static(&SBAWelcomeScreen::OnSaveCustomSettingsClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(INVTEXT("重新加载"))
					.ToolTipText(INVTEXT("将 ini 文件中的设置应用到当前项目"))
					.IsEnabled_Static(&SBAWelcomeScreen::IsCustomSettingsPathValid)
					.OnClicked_Static(&SBAWelcomeScreen::OnReloadCustomSettingsClicked)
				]
			]
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

TSharedRef<SWidget> SBAWelcomeScreen::MakeProperty(UObject* Obj, FName PropName)
{
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FSinglePropertyParams Params;
	Params.NotifyHook = &SettingsPropertyHook;
	Params.NamePlacement = EPropertyNamePlacement::Type::Inside;

	return EditModule.CreateSingleProperty(Obj, PropName, Params).ToSharedRef();
}

FReply SBAWelcomeScreen::OnCreateNewCustomSettingsClicked()
{
	const FString Title = "Save Custom Settings INI File";
	const FString DefaultPath = FPaths::ProjectDir();
	const FString DefaultFile = "CustomBlueprintAssistSettings.ini";
	const FString FileTypes = "INI files (*.ini)|*.ini";

	TArray<FString> OutFileNames;
	const bool bSelectedPath = FDesktopPlatformModule::Get()->SaveFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		Title,
		FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_SAVE),
		DefaultFile,
		FileTypes,
		EFileDialogFlags::None,
		OutFileNames
	);

	if (bSelectedPath && OutFileNames.Num() > 0)
	{
		FString RelativePath = OutFileNames[0];
		const bool bCreatedFile = FFileHelper::SaveStringToFile(TEXT(""), *RelativePath);
		if (bCreatedFile)
		{
			FString FullPath = FPaths::ConvertRelativePathToFull(RelativePath);

			UE_LOG(LogBlueprintAssist, Log, TEXT("Made custom settings file at %s"), *FullPath);
			FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_SAVE, FPaths::GetPath(FullPath)); // Save path as default for next time.

			{
				FBAScopedPropertySetter(&UBASettings_Meta::GetMutable(), GET_MEMBER_NAME_CHECKED(UBASettings_Meta, CustomSettingsIniPath));
				UBASettings_Meta::GetMutable().CustomSettingsIniPath.FilePath = FullPath;
			}

			OnSaveCustomSettingsClicked();
		}
	}

	return FReply::Handled();
}

FReply SBAWelcomeScreen::OnSaveCustomSettingsClicked()
{
	const FString& Path = FConfigCacheIni::NormalizeConfigIniPath(UBASettings_Meta::Get().CustomSettingsIniPath.FilePath);
	if (!Path.IsEmpty())
	{
		UE_LOG(LogBlueprintAssist, Log, TEXT("Saved settings to file: %s"), *Path);
		UBASettings::GetMutable().TryUpdateDefaultConfigFile(*Path);
		UBASettings_EditorFeatures::GetMutable().TryUpdateDefaultConfigFile(*Path);
		UBASettings_Advanced::GetMutable().TryUpdateDefaultConfigFile(*Path);
	}

	return FReply::Handled();
}

FReply SBAWelcomeScreen::OnReloadCustomSettingsClicked()
{
	const FString& Path = FConfigCacheIni::NormalizeConfigIniPath(UBASettings_Meta::Get().CustomSettingsIniPath.FilePath);
	if (FPaths::FileExists(Path))
	{
		UE_LOG(LogBlueprintAssist, Log, TEXT("Reloaded custom settings from file: %s"), *Path);
		UBASettingsBase::ReloadSettings(UBASettings::StaticClass());
		UBASettingsBase::ReloadSettings(UBASettings_EditorFeatures::StaticClass());
		UBASettingsBase::ReloadSettings(UBASettings_Advanced::StaticClass());

		GConfig->LoadFile(Path);
		UBASettings::GetMutable().ReloadConfig(nullptr, *Path);
		UBASettings_EditorFeatures::GetMutable().ReloadConfig(nullptr, *Path);
		UBASettings_Advanced::GetMutable().ReloadConfig(nullptr, *Path);
	}

	return FReply::Handled();
}

bool SBAWelcomeScreen::IsCustomSettingsPathValid()
{
	const FString& Path = UBASettings_Meta::Get().CustomSettingsIniPath.FilePath;
	return !Path.IsEmpty() && FPaths::FileExists(Path);
}
