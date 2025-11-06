// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FBAGraphHandler;
class SOverlay;

class BLUEPRINTASSIST_API SBASizeProgress final : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SBASizeProgress) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FBAGraphHandler> InOwnerGraphHandler);

public:
	void RenderGraphToBrush();

	bool IsSnapshotValid() const;

	void ShowOverlay();

	void HideOverlay();

	bool bIsVisible = false;

protected:
	void DrawWidgetToRenderTarget(TSharedPtr<SWidget> Widget);

	FText GetCacheProgressText() const;

	TOptional<float> GetCachingPercent() const;

	TSharedPtr<FBAGraphHandler> OwnerGraphHandler;

	TSharedPtr<SOverlay> ProgressCenterPanel;

	FSlateBrush GraphSnapshotBrush;

	const FSlateBrush* CachedBorderBrush = nullptr;

	bool bIsCachingOverlayVisible = false;
};
