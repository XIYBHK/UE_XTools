#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

class FSlateStyleSet;

class FBAStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static const ISlateStyle& GetSlateStyle();
	static const FName& GetStyleSetName();

	static const ISlateStyle& GetBlueprintAssistStyle();
	static const FName& GetBlueprintAssistStyleSetName();

	static const FSlateBrush* GetBrush(FName PropertyName, const ANSICHAR* Specifier = nullptr)
	{
		return SlateStyleSet->GetBrush(PropertyName, Specifier);
	}

	static const FSlateBrush* GetPluginBrush(FName PropertyName, const ANSICHAR* Specifier = nullptr)
	{
		return BlueprintAssistStyleSet->GetBrush(PropertyName, Specifier);
	}

protected:
	static void InitSlateStyleSet();
	static void InitBlueprintAssistStyleSet();

private:
	static TSharedPtr<FSlateStyleSet> SlateStyleSet;

	static TSharedPtr<FSlateStyleSet> BlueprintAssistStyleSet;
};
