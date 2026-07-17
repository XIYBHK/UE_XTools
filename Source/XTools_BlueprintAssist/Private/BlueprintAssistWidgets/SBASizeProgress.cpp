// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistWidgets/SBASizeProgress.h"

#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistStyle.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Notifications/SProgressBar.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SBASizeProgress::Construct(const FArguments& InArgs, TSharedPtr<FBAGraphHandler> InOwnerGraphHandler)
{
	OwnerGraphHandler = InOwnerGraphHandler;

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SAssignNew(SnapshotImage, SImage)
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
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

SBASizeProgress::~SBASizeProgress()
{
	if (SnapshotBrush.IsValid())
	{
		SnapshotBrush->ReleaseResource();
	}
}

bool SBASizeProgress::IsSnapshotValid() const
{
	return SnapshotBrush.IsValid();
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

	UE_LOG(LogBlueprintAssist, Verbose, TEXT("[%hs] Show size progress overlay"), __FUNCTION__);

	TSharedPtr<SGraphEditor> GraphEditor = OwnerGraphHandler->GetGraphEditor();
	SnapshotWidget(GraphEditor);

	SetVisibility(EVisibility::HitTestInvisible);

	if (OwnerGraphHandler->GetNumberOfPendingNodesToCache() > UBASettings::Get().RequiredNodesToShowOverlayProgressBar)
	{
		ProgressCenterPanel->SetVisibility(EVisibility::HitTestInvisible);
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
		UE_LOG(LogBlueprintAssist, Verbose, TEXT("[%hs] Hide size progress overlay"), __FUNCTION__);
		bIsVisible = false;
		SetVisibility(EVisibility::Collapsed);

		SnapshotImage->SetImage(nullptr);

		// clear the graph snapshot brush
		if (SnapshotBrush.IsValid())
		{
			SnapshotBrush->ReleaseResource();
			SnapshotBrush.Reset();
		}
	}
}

void SBASizeProgress::SnapshotWidget(TSharedPtr<SWidget> Widget)
{
	if (!Widget.IsValid())
	{
		return;
	}

	TArray<FColor> ColorData;
	FIntVector Size;
	FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(), ColorData, Size);

	// see FWidgetSnapshotData::CreateBrushes
	TArray<uint8> TextureDataAsBGRABytes;
	TextureDataAsBGRABytes.Reserve(ColorData.Num() * 4);

	// for some reason the screenshot is offset by 1 pixel, so skip the first row
	constexpr int RowOffset = 1;
	for (int32 i = RowOffset * Size.X; i < ColorData.Num(); ++i)
	{
		const FColor& PixelColor = ColorData[i];
		TextureDataAsBGRABytes.Add(PixelColor.B);
		TextureDataAsBGRABytes.Add(PixelColor.G);
		TextureDataAsBGRABytes.Add(PixelColor.R);
		TextureDataAsBGRABytes.Add(PixelColor.A);
	}

	// Account for the initial offset by adding rows at the bottom
	for (int32 i = 0; i < RowOffset * Size.X; ++i)
	{
		TextureDataAsBGRABytes.Add(0);
		TextureDataAsBGRABytes.Add(0);
		TextureDataAsBGRABytes.Add(0);
		TextureDataAsBGRABytes.Add(255);
	}

	static FName Name_BASizeProgressBrush("SBASizeProgress_Brush");
	const FBAVector2 BrushSize(Size.X, Size.Y);
	SnapshotBrush = FSlateDynamicImageBrush::CreateWithImageData(
		Name_BASizeProgressBrush,
		BrushSize,
		TextureDataAsBGRABytes);

	if (SnapshotBrush.IsValid())
	{
		// for some reason the screenshot is offset by 1 pixel?
		SnapshotImage->SetImage(SnapshotBrush.Get());
	}
}

FText SBASizeProgress::GetCacheProgressText() const
{
	return FText::FromString(FString::Printf(TEXT("Caching Node Sizes (%d)"), OwnerGraphHandler->GetNumberOfPendingNodesToCache()));
}

TOptional<float> SBASizeProgress::GetCachingPercent() const
{
	return OwnerGraphHandler->GetPendingNodeSizeProgress();
}
