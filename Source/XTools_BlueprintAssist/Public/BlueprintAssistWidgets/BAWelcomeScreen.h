// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistTypes.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/SCompoundWidget.h"


class FUICommandInfo;
class SCheckBox;
class SDockTab;

class XTOOLS_BLUEPRINTASSIST_API SBAWelcomeScreen : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SBAWelcomeScreen)
		{
		}
	SLATE_END_ARGS()

	static FName GetTabId() { return FName("BlueprintAssistWelcomeScreen"); }

	void Construct(const FArguments& InArgs);

	static TSharedRef<SDockTab> CreateWelcomeScreenTab(const FSpawnTabArgs& Args);

	TSharedRef<SWidget> MakeCommandWidget(TSharedPtr<FUICommandInfo> Command);

	FText GetCommandText(TSharedPtr<FUICommandInfo> Command);

	TSharedRef<SWidget> MakeIntroPage();

	TSharedRef<SWidget> MakeCustomizePage();

	TSharedRef<SWidget> MakePropertiesList(const TMap<UObject*, TArray<FName>>& Properties);

	FBASettingsPropertyHook SettingsPropertyHook;

	TArray<TSharedPtr<SCheckBox>> MenuEntries;
	TSharedPtr<SWidgetSwitcher> WidgetSwitcher;
};
