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

	static TSharedRef<SDockTab> CreateTab(const FSpawnTabArgs& Args);

	TSharedRef<SWidget> CreateIntroductionWidget();
	TSharedRef<SWidget> CreateSettingsWidget();

	TSharedPtr<SWidgetSwitcher> WidgetSwitcher;
};
