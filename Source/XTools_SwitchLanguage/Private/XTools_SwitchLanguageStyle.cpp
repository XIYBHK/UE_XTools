/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "XTools_SwitchLanguageStyle.h"
#include "XTools_SwitchLanguage.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FXTools_SwitchLanguageStyle::StyleInstance = nullptr;

void FXTools_SwitchLanguageStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FXTools_SwitchLanguageStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FXTools_SwitchLanguageStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("XTools_SwitchLanguageStyle"));
	return StyleSetName;
}

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef<FSlateStyleSet> FXTools_SwitchLanguageStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet("XTools_SwitchLanguageStyle"));
	
	// 设置资源根目录
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("XTools");
	if (Plugin.IsValid())
	{
		Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
	}

	// 注册工具栏图标
	Style->Set("XTools_SwitchLanguage.PluginAction", new IMAGE_BRUSH_SVG(TEXT("SwitchLanguageIcon"), Icon20x20));

	return Style;
}

void FXTools_SwitchLanguageStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FXTools_SwitchLanguageStyle::Get()
{
	return *StyleInstance;
}
