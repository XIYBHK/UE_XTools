// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SImage;
struct FSlateDynamicImageBrush;
class FBAGraphHandler;
class SOverlay;

class XTOOLS_BLUEPRINTASSIST_API SBASizeProgress final : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SBASizeProgress) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FBAGraphHandler> InOwnerGraphHandler);
	virtual ~SBASizeProgress() override;

public:
	bool IsSnapshotValid() const;

	void ShowOverlay();

	void HideOverlay();

	bool bIsVisible = false;

protected:
	void SnapshotWidget(TSharedPtr<SWidget> Widget);

	FText GetCacheProgressText() const;

	TOptional<float> GetCachingPercent() const;

	TSharedPtr<FBAGraphHandler> OwnerGraphHandler;

	TSharedPtr<SOverlay> ProgressCenterPanel;

	TSharedPtr<FSlateDynamicImageBrush> SnapshotBrush;

	TSharedPtr<SImage> SnapshotImage;
};
