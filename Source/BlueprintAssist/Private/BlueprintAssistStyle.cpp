#include "BlueprintAssistStyle.h"

#include "BlueprintAssistGlobals.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"

#if ENGINE_MAJOR_VERSION >= 5
#include "Styling/StyleColors.h"
#endif

TSharedPtr<FSlateStyleSet> FBAStyle::SlateStyleSet = nullptr;
TSharedPtr<FSlateStyleSet> FBAStyle::BlueprintAssistStyleSet = nullptr;

#define BA_IMAGE_BRUSH(StyleSet, RelativePath, ... ) FSlateImageBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BA_BOX_BRUSH(StyleSet, RelativePath, ... ) FSlateBoxBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BA_BORDER_BRUSH(StyleSet, RelativePath, ... ) FSlateBorderBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BA_DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)
#define BA_ICON_FONT(StyleSet, ...) FSlateFontInfo(StyleSet->RootToContentDir("Fonts/FontAwesome", TEXT(".ttf")), __VA_ARGS__)

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon40x40(40.0f, 40.0f);

void FBAStyle::Initialize()
{
	InitSlateStyleSet();
	InitBlueprintAssistStyleSet();
}

void FBAStyle::InitSlateStyleSet()
{
	// Only register once
	if (SlateStyleSet.IsValid())
	{
		return;
	}

	SlateStyleSet = MakeShareable(new FSlateStyleSet("BlueprintAssistSlateStyle"));

	SlateStyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	SlateStyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

#if ENGINE_MAJOR_VERSION >= 5
	SlateStyleSet->Set("BlueprintAssist.WhiteBorder", new FSlateRoundedBoxBrush(FStyleColors::White, 4.0f));
	SlateStyleSet->Set("BlueprintAssist.PanelBorder", new FSlateRoundedBoxBrush(FStyleColors::Panel, 4.0f));
#else
	SlateStyleSet->Set("BlueprintAssist.WhiteBorder", new BA_BOX_BRUSH(SlateStyleSet, "Common/RoundedSelection_16x", FMargin(4.0f/16.0f)));
	SlateStyleSet->Set("BlueprintAssist.PanelBorder", new BA_BOX_BRUSH(SlateStyleSet, "Common/DarkGroupBorder", FMargin(4.0f/16.0f)));
#endif

	FSlateStyleRegistry::RegisterSlateStyle(*SlateStyleSet.Get());
}

void FBAStyle::InitBlueprintAssistStyleSet()
{
	// Only register once
	if (BlueprintAssistStyleSet.IsValid())
	{
		return;
	}

	BlueprintAssistStyleSet = MakeShareable(new FSlateStyleSet("BlueprintAssistStyle"));

	BlueprintAssistStyleSet->SetContentRoot(IPluginManager::Get().FindPlugin("BlueprintAssist")->GetBaseDir() / TEXT("Resources"));

	BlueprintAssistStyleSet->Set("BlueprintAssist.Lock", new BA_IMAGE_BRUSH(BlueprintAssistStyleSet, "Lock", FVector2D(64.0f/64.0f)));

	BlueprintAssistStyleSet->Set("BlueprintAssist.PlainBorder", new BA_BORDER_BRUSH(BlueprintAssistStyleSet, "BAPlainBorder", 1.0f));

	FSlateStyleRegistry::RegisterSlateStyle(*BlueprintAssistStyleSet.Get());
}

#undef BA_IMAGE_BRUSH
#undef BA_BOX_BRUSH
#undef BA_BORDER_BRUSH
#undef BA_DEFAULT_FONT
#undef BA_ICON_FONT

void FBAStyle::Shutdown()
{
	if (SlateStyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*SlateStyleSet.Get());
		ensure(SlateStyleSet.IsUnique());
		SlateStyleSet.Reset();
	}

	if (BlueprintAssistStyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*BlueprintAssistStyleSet.Get());
		ensure(BlueprintAssistStyleSet.IsUnique());
		BlueprintAssistStyleSet.Reset();
	}
}

const ISlateStyle& FBAStyle::GetSlateStyle()
{
	return *(SlateStyleSet.Get());
}

const FName& FBAStyle::GetStyleSetName()
{
	return SlateStyleSet->GetStyleSetName();
}

const ISlateStyle& FBAStyle::GetBlueprintAssistStyle()
{
	return *(BlueprintAssistStyleSet.Get());
}

const FName& FBAStyle::GetBlueprintAssistStyleSetName()
{
	return BlueprintAssistStyleSet->GetStyleSetName();
}
