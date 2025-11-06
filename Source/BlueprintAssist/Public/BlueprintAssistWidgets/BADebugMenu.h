// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistTypes.h"
#include "Components/HorizontalBox.h"

class BLUEPRINTASSIST_API SBADebugMenuRow final : public SHorizontalBox
{
	SLATE_BEGIN_ARGS(SBADebugMenuRow)
		: _Label()
		, _Value()
	{
	}

	SLATE_ATTRIBUTE(FText, Label)
	SLATE_ATTRIBUTE(FText, Value)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};

class BLUEPRINTASSIST_API SBADebugMenu final : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SBADebugMenu) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	FText PinUnderCursor;
	FText NodeUnderCursor;
	FText NodeUnderCursorSize;
	FText GraphUnderCursor;
	FText HoveredWidget;
	FText FocusedWidget;
	FText CurrentTab;
	FText FocusedAssetEditor;
	FText CurrentAsset;
	FText KeyboardFocusWidget;
	FText UserFocusWidget;

	static void RegisterNomadTab();
};
