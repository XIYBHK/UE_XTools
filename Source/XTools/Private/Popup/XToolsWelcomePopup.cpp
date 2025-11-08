/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "Popup/XToolsWelcomePopup.h"

// Core
#include "CoreMinimal.h"
#include "Popup/XToolsUpdateConfig.h"
#include "XToolsDefines.h"

// HAL
#include "HAL/PlatformProcess.h"

// Misc
#include "Misc/Paths.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ConfigCacheIni.h"

// Slate Widgets
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSpacer.h"
#include "Framework/Application/SlateApplication.h"

// Styling
#include "Styling/CoreStyle.h"
#if ENGINE_MAJOR_VERSION >= 5
#include "Styling/AppStyle.h"
#else
#include "EditorStyleSet.h"
#endif

// Plugin Management
#include "Interfaces/IPluginManager.h"

void FXToolsWelcomePopup::OnBrowserLinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata)
{
	const FString* URL = Metadata.Find(TEXT("href"));

	if (URL)
	{
		FPlatformProcess::LaunchURL(**URL, nullptr, nullptr);
	}
}

void FXToolsWelcomePopup::Register()
{
	// 获取插件路径
	IPluginManager& PluginManager = IPluginManager::Get();
	TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(TEXT("UE_XTools"));
	
	if (!Plugin.IsValid())
	{
		UE_LOG(LogXTools, Warning, TEXT("XTools plugin not found, cannot register welcome popup"));
		return;
	}

	FString UpdateConfigPath = Plugin->GetBaseDir();
	UpdateConfigPath /= "UpdateConfig.ini";
	const FString UpdateConfigFile = FConfigCacheIni::NormalizeConfigIniPath(UpdateConfigPath);
	
	// 从 XToolsDefines.h 获取版本号
	const FString CurrentPluginVersion = FString::Printf(TEXT("%d.%d.%d"), 
		XTOOLS_VERSION_MAJOR, XTOOLS_VERSION_MINOR, XTOOLS_VERSION_PATCH);

	UXToolsUpdateConfig* UpdateConfig = GetMutableDefault<UXToolsUpdateConfig>();

	// 加载或创建配置文件
	if (FPaths::FileExists(UpdateConfigFile))
	{
		UpdateConfig->LoadConfig(nullptr, *UpdateConfigFile);
	}
	else
	{
		UpdateConfig->SaveConfig(CPF_Config, *UpdateConfigFile);
	}

	// 检查版本是否变化（首次打开或更新）
	if (UpdateConfig->PluginVersionShown != CurrentPluginVersion)
	{
		// 更新版本号
		UpdateConfig->PluginVersionShown = CurrentPluginVersion;
		UpdateConfig->SaveConfig(CPF_Config, *UpdateConfigFile);

		// 在引擎初始化完成后显示弹窗
		FCoreDelegates::OnPostEngineInit.AddLambda([]()
		{
			Open();
		});
	}
}

void FXToolsWelcomePopup::Open()
{
	if (!FSlateApplication::Get().CanDisplayWindows())
	{
		return;
	}

	TSharedRef<SBorder> WindowContent = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(8.0f, 8.0f));

	TSharedPtr<SWindow> Window = SNew(SWindow)
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::FixedSize)
		.ClientSize(FVector2D(600, 450))
		.Title(FText::FromString(TEXT("XTools - 欢迎使用")))
		.IsTopmostWindow(true)
	[
		WindowContent
	];

	const FSlateFontInfo HeadingFont = FCoreStyle::GetDefaultFontStyle("Regular", 24);
	const FSlateFontInfo ContentFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);

	// 构建版本号字符串
	const FString VersionString = FString::Printf(TEXT("XTools v%d.%d.%d"), 
		XTOOLS_VERSION_MAJOR, XTOOLS_VERSION_MINOR, XTOOLS_VERSION_PATCH);

	TSharedRef<SVerticalBox> InnerContent = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		  .AutoHeight()
		  .Padding(10)
		[
			SNew(STextBlock)
			.Font(HeadingFont)
			.Text(FText::FromString(VersionString))
		]
		+ SVerticalBox::Slot()
		  .FillHeight(1.0)
		  .Padding(10)
		[
			SNew(SBorder)
			.Padding(10)
#if ENGINE_MAJOR_VERSION >= 5
			.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
#else
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
#endif
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(SRichTextBlock)
					.Text(FText::FromString(R"(
<LargeText>欢迎使用 XTools 插件！</>

XTools 是一个为 Unreal Engine 5.3-5.6 设计的模块化插件系统，提供丰富的蓝图节点和 C++ 功能库。

<RichTextBlock.Bold>主要功能模块：</>

* <RichTextBlock.Bold>排序算法库</> - 支持整数、浮点、字符串、向量、Actor 和通用结构体排序
* <RichTextBlock.Bold>随机洗牌</> - PRD（伪随机分布）算法和数组洗牌功能
* <RichTextBlock.Bold>异步流程控制</> - 延迟、时间轴、循环、协程等异步操作
* <RichTextBlock.Bold>几何工具</> - 静态网格体内部点阵生成、贝塞尔曲线等
* <RichTextBlock.Bold>编队系统</> - 完整的编队管理功能
* <RichTextBlock.Bold>对象池</> - 高性能的 Actor 对象池系统
* <RichTextBlock.Bold>组件时间轴</> - 灵活的组件级时间轴控制
* <RichTextBlock.Bold>资产命名工具</> - 自动资产命名规范化

<RichTextBlock.Bold>版本 1.0.0</>

<RichTextBlock.Bold>新功能</>

* 完整的模块化架构设计
* 支持 UE 5.3-5.6 跨版本兼容
* 所有功能通过蓝图节点暴露，中文优先

<RichTextBlock.Bold>技术特性</>

* 零警告编译
* 完整的错误处理和日志系统
* 符合 UE 官方最佳实践

更多信息请访问：<a id="browser" href="https://github.com">GitHub 仓库</a>
)"))
#if ENGINE_MAJOR_VERSION >= 5
					.TextStyle(FAppStyle::Get(), "NormalText")
					.DecoratorStyleSet(&FAppStyle::Get())
#else
					.TextStyle(FEditorStyle::Get(), "NormalText")
					.DecoratorStyleSet(&FEditorStyle::Get())
#endif
					.AutoWrapText(true)
					+ SRichTextBlock::HyperlinkDecorator(TEXT("browser"), FSlateHyperlinkRun::FOnClick::CreateStatic(&OnBrowserLinkClicked))
				]
			]
		]
		+ SVerticalBox::Slot()
		  .AutoHeight()
		  .Padding(10)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("查看文档")))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([]()
				{
					const FString URL = "https://github.com";
					FPlatformProcess::LaunchURL(*URL, nullptr, nullptr);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SSpacer)
				.Size(FVector2D(20, 10))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("反馈问题")))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([]()
				{
					const FString URL = "https://github.com/issues";
					FPlatformProcess::LaunchURL(*URL, nullptr, nullptr);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SSpacer)
				.Size(FVector2D(20, 10))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("关闭窗口")))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([Window]()
				{
					Window->RequestDestroyWindow();
					return FReply::Handled();
				})
			]
		];

	WindowContent->SetContent(InnerContent);
	Window = FSlateApplication::Get().AddWindow(Window.ToSharedRef());
}

