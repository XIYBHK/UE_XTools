// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistWidgets/SBASizeProgress.h"

#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistStyle.h"
#include "RenderingThread.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Notifications/SProgressBar.h"

void SBASizeProgress::Construct(const FArguments& InArgs, TSharedPtr<FBAGraphHandler> InOwnerGraphHandler)
{
	OwnerGraphHandler = InOwnerGraphHandler;

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(&GraphSnapshotBrush)
		]
		+ SOverlay::Slot()
		.VAlign(VAlign_Center).HAlign(HAlign_Center)
		[
			SAssignNew(ProgressCenterPanel, SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image(FBAStyle::GetPluginBrush("BlueprintAssist.PlainBorder"))
				.ColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f))
			]
			+ SOverlay::Slot()
			.Padding(1.0f)
			[
				SNew(SBorder)
				.BorderImage(FBAStyle::GetBrush("BlueprintAssist.PanelBorder"))
				.Padding(8.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					[
						SNew(STextBlock)
						.Text_Raw(this, &SBASizeProgress::GetCacheProgressText)
						.TextStyle(BA_STYLE_CLASS::Get(), TEXT("DetailsView.CategoryTextStyle"))
					]
					+ SVerticalBox::Slot()
					[
						SNew(SSpacer).Size(FVector2D(0, 16.0f))
					]
					+ SVerticalBox::Slot()
					[
						SNew(SBox).WidthOverride(256.0f)
						[
							SNew(SProgressBar)
							.BorderPadding(FVector2D::ZeroVector)
							.FillColorAndOpacity(FSlateColor(FLinearColor(0.0f, 1.0f, 1.0f)))
							.Percent(this, &SBASizeProgress::GetCachingPercent)
						]
					]
				]
			]
		]
	];

	SetVisibility(EVisibility::Collapsed);
}

void SBASizeProgress::RenderGraphToBrush()
{
	TSharedPtr<SGraphEditor> GraphEditor = OwnerGraphHandler->GetGraphEditor();
	DrawWidgetToRenderTarget(GraphEditor);
}

bool SBASizeProgress::IsSnapshotValid() const
{
	UObject* ResourceObject = GraphSnapshotBrush.GetResourceObject();
	if (ResourceObject != nullptr && (!IsValid(ResourceObject) || ResourceObject->IsUnreachable() || ResourceObject->HasAnyFlags(RF_BeginDestroyed)))
	{
		return false;
	}

	return true;
}

void SBASizeProgress::ShowOverlay()
{
	if (!UBASettings::Get().bShowOverlayWhenCachingNodes)
	{
		return;
	}

	if (bIsVisible)
	{
		return;
	}

	RenderGraphToBrush();
	SetVisibility(EVisibility::HitTestInvisible);

	if (OwnerGraphHandler->GetNumberOfPendingNodesToCache() > UBASettings::Get().RequiredNodesToShowOverlayProgressBar)
	{
		ProgressCenterPanel->SetVisibility(EVisibility::Visible);
	}
	else
	{
		ProgressCenterPanel->SetVisibility(EVisibility::Hidden);
	}

	bIsVisible = true;
}

void SBASizeProgress::HideOverlay()
{
	if (bIsVisible || GetVisibility() != EVisibility::Collapsed)
	{
		bIsVisible = false;
		SetVisibility(EVisibility::Collapsed);

		// clear the graph snapshot brush
		GraphSnapshotBrush = FSlateBrush();
	}
}

void SBASizeProgress::DrawWidgetToRenderTarget(TSharedPtr<SWidget> Widget)
{
	if (!Widget.IsValid())
	{
		return;
	}

	const FIntPoint RenderSize = Widget->GetTickSpaceGeometry().GetLocalSize().IntPoint();
	if (RenderSize.SizeSquared() == 0)
	{
		return;
	}

	FWidgetRenderer* WidgetRenderer = new FWidgetRenderer(false, true);
	if (!WidgetRenderer)
	{
		return;
	}

	UTextureRenderTarget2D* RenderTarget = WidgetRenderer->DrawWidget(Widget.ToSharedRef(), RenderSize);
	if (!RenderTarget)
	{
		return;
	}

	FlushRenderingCommands();

	BeginCleanup(WidgetRenderer);

	GraphSnapshotBrush.SetResourceObject(RenderTarget);
	GraphSnapshotBrush.SetImageSize(FVector2D(RenderTarget->SizeX, RenderTarget->SizeY));
}

FText SBASizeProgress::GetCacheProgressText() const
{
	return FText::FromString(FString::Printf(TEXT("Caching Node Sizes (%d)"), OwnerGraphHandler->GetNumberOfPendingNodesToCache()));
}

TOptional<float> SBASizeProgress::GetCachingPercent() const
{
	return OwnerGraphHandler->GetPendingNodeSizeProgress();
}
