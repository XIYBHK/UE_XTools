// Copyright 2024 Gradess Games. All Rights Reserved.


#include "BlueprintScreenshotToolHandler.h"
#include "Runtime/Launch/Resources/Version.h"

DEFINE_LOG_CATEGORY(LogBlueprintScreenshotTool);

// UE 5.7+ Slate API 兼容性
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
#define FBST_SLATE_VECTOR FVector2f
#else
#define FBST_SLATE_VECTOR FVector2D
#endif

#include "BlueprintEditor.h"
#include "BlueprintScreenshotToolSettings.h"
#include "BlueprintScreenshotToolTypes.h"
#include "BlueprintScreenshotToolWindowManager.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "ImageWriteQueue.h"
#include "ImageWriteTask.h"
#include "SBlueprintDiff.h"
#include "RenderingThread.h"
#include "Engine/TextureRenderTarget2D.h"

#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/FileHelper.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Notifications/SNotificationList.h"

struct FWidgetSnapshotTextureData;

bool UBlueprintScreenshotToolHandler::bTakingScreenshot = false;
static TArray<TSharedPtr<SGraphEditor>> CachedGraphEditorsForWarmup;
static TUniquePtr<FWidgetRenderer> CachedWarmupRenderer;

namespace
{
struct FGraphCaptureState
{
	FBSTVector2D CachedViewLocation = FBSTVector2D::ZeroVector;
	FBSTVector2D NewViewLocation = FBSTVector2D::ZeroVector;
	FBSTVector2D WindowSize = FBSTVector2D::ZeroVector;
	float CachedZoomAmount = 1.f;
	float NewZoomAmount = 1.f;
	FGraphPanelSelectionSet SelectedNodes;
};

bool PrepareGraphEditorForCapture(TSharedPtr<SGraphEditor> InGraphEditor, FGraphCaptureState& OutState)
{
	if (!InGraphEditor.IsValid())
	{
		return false;
	}

	const UBlueprintScreenshotToolSettings* Settings = GetDefault<UBlueprintScreenshotToolSettings>();
	OutState.SelectedNodes = InGraphEditor->GetSelectedNodes();
	InGraphEditor->GetViewLocation(OutState.CachedViewLocation, OutState.CachedZoomAmount);

	float WindowSizeScale = 1.f;
	if (OutState.SelectedNodes.Num() > 0)
	{
		FSlateRect BoundsForSelectedNodes;
		InGraphEditor->GetBoundsForSelectedNodes(BoundsForSelectedNodes, Settings->ScreenshotPadding);

		OutState.NewViewLocation = BoundsForSelectedNodes.GetTopLeft();
		OutState.NewZoomAmount = Settings->ZoomAmount;
		OutState.WindowSize = BoundsForSelectedNodes.GetSize();
		WindowSizeScale = Settings->ZoomAmount;
	}
	else
	{
		OutState.NewViewLocation = OutState.CachedViewLocation;
		OutState.NewZoomAmount = OutState.CachedZoomAmount;

		const FBSTVector2D WindowPosition = InGraphEditor->GetTickSpaceGeometry().GetAbsolutePosition();
		const float DPIScale = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(WindowPosition.X, WindowPosition.Y);
		const FBSTVector2D SizeOfWidget = InGraphEditor->GetCachedGeometry().GetLocalSize();
		OutState.WindowSize = SizeOfWidget * DPIScale;
	}

	OutState.WindowSize = OutState.WindowSize.ClampAxes(Settings->MinScreenshotSize, Settings->MaxScreenshotSize);
	OutState.WindowSize *= WindowSizeScale;

	InGraphEditor->SetViewLocation(OutState.NewViewLocation, OutState.NewZoomAmount);
	InGraphEditor->ClearSelectionSet();
	InGraphEditor->Invalidate(EInvalidateWidgetReason::PaintAndVolatility);
	return true;
}

void RestoreGraphEditorAfterCapture(TSharedPtr<SGraphEditor> InGraphEditor, const FGraphCaptureState& InState)
{
	if (!InGraphEditor.IsValid())
	{
		return;
	}

	for (const UObject* NodeObject : InState.SelectedNodes)
	{
		if (const UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(NodeObject))
		{
			InGraphEditor->SetNodeSelection(const_cast<UEdGraphNode*>(SelectedNode), true);
		}
	}

	InGraphEditor->SetViewLocation(InState.CachedViewLocation, InState.CachedZoomAmount);
}
}

void UBlueprintScreenshotToolHandler::PrepareForScreenshot()
{
	auto GraphEditors = UBlueprintScreenshotToolWindowManager::FindActiveGraphEditors();
	if (GraphEditors.Num() <= 0)
	{
		return;
	}

	CachedGraphEditorsForWarmup.Reset();
	CachedGraphEditorsForWarmup = GraphEditors.Array();
	CachedWarmupRenderer = MakeUnique<FWidgetRenderer>(true, true);
	CachedWarmupRenderer->SetIsPrepassNeeded(true);

	const auto bHasSelectedNodes = HasAnySelectedNodes(GraphEditors);
	for (const auto& GraphEditor : GraphEditors)
	{
		if (bHasSelectedNodes && GraphEditor->GetSelectedNodes().Num() <= 0)
		{
			continue;
		}

		WarmupGraphEditor(GraphEditor);
	}
	
	bTakingScreenshot = true;
}

void UBlueprintScreenshotToolHandler::ExecuteAsyncScreenshot()
{
	bTakingScreenshot = false;
	
	TSet<TSharedPtr<SGraphEditor>> GraphEditors;
	if (CachedGraphEditorsForWarmup.Num() > 0)
	{
		GraphEditors = TSet<TSharedPtr<SGraphEditor>>(CachedGraphEditorsForWarmup);
		CachedGraphEditorsForWarmup.Reset();
	}
	else
	{
		GraphEditors = UBlueprintScreenshotToolWindowManager::FindActiveGraphEditors();
	}

	if (GraphEditors.Num() <= 0)
	{
		CachedWarmupRenderer.Reset();
		return;
	}

	TArray<FString> Paths;
	int32 FailedCount = 0;

	const auto bHasSelectedNodes = HasAnySelectedNodes(GraphEditors);
	for (const auto GraphEditor : GraphEditors)
	{
		if (bHasSelectedNodes && GraphEditor->GetSelectedNodes().Num() <= 0)
		{
			continue;
		}

		const auto ScreenshotData = CaptureGraphEditor(GraphEditor);
		const auto Path = SaveScreenshot(ScreenshotData);
		
		if (!Path.IsEmpty())
		{
			Paths.Add(Path);
		}
		else
		{
			FailedCount++;
		}
	}

	CachedWarmupRenderer.Reset();

	if (Paths.Num() > 0)
	{
		ShowNotification(Paths);
	}
	
	if (FailedCount > 0 && Paths.Num() == 0)
	{
		ShowSaveFailedNotification(FString::Printf(TEXT("%d"), FailedCount));
	}

	UpdateScreenshotState(false);
}

void UBlueprintScreenshotToolHandler::OnPostTick(float DeltaTime)
{
	if (bTakingScreenshot)
	{
		ExecuteAsyncScreenshot();
	}
}

void UBlueprintScreenshotToolHandler::UpdateScreenshotState(bool bIsProcessing)
{
}



TArray<FString> UBlueprintScreenshotToolHandler::TakeScreenshotWithNotification()
{
	TakeScreenshotWithPaths();
	return {};
}

void UBlueprintScreenshotToolHandler::TakeScreenshot()
{
	if (GetDefault<UBlueprintScreenshotToolSettings>()->bShowNotification)
	{
		TakeScreenshotWithNotification();
	}
	else
	{
		TakeScreenshotWithPaths();
	}
}

FString UBlueprintScreenshotToolHandler::SaveScreenshot(const TArray<FColor>& InColorData, const FIntVector& InSize)
{
	return SaveScreenshot({ InColorData, InSize });
}

FString UBlueprintScreenshotToolHandler::SaveScreenshot(const FBSTScreenshotData& InData)
{
	if (!InData.IsValid())
	{
		return FString();
	}
	
	const auto* Settings = GetDefault<UBlueprintScreenshotToolSettings>();
	const auto ScreenshotDir = Settings->SaveDirectory.Path;
	const auto& BaseName = Settings->bOverrideScreenshotNaming || InData.CustomName.IsEmpty() ? Settings->ScreenshotBaseName : InData.CustomName;
	const auto FileExtension = GetExtension(Settings->Extension);
	const auto Path = FPaths::Combine(ScreenshotDir, BaseName);

	FString Filename;
	FFileHelper::GenerateNextBitmapFilename(Path, FileExtension, Filename);

	const auto ImageView = FImageView(InData.ColorData.GetData(), InData.Size.X, InData.Size.Y);
	const auto Quality = Settings->Extension == EBSTImageFormat::JPG ? Settings->Quality : 0;
	const auto bSuccess = FImageUtils::SaveImageByExtension(*Filename, ImageView, Quality);

	return bSuccess ? Filename : FString();
	
}

FBSTScreenshotData UBlueprintScreenshotToolHandler::CaptureGraphEditor(TSharedPtr<SGraphEditor> InGraphEditor)
{
	if (!InGraphEditor.IsValid())
	{
		return FBSTScreenshotData();
	}

	FGraphCaptureState CaptureState;
	if (!PrepareGraphEditorForCapture(InGraphEditor, CaptureState))
	{
		return FBSTScreenshotData();
	}

	const FVector2D WindowSizeVector2D(CaptureState.WindowSize.X, CaptureState.WindowSize.Y);
	const auto RenderTarget = TStrongObjectPtr<UTextureRenderTarget2D>(DrawGraphEditor(InGraphEditor, WindowSizeVector2D, CachedWarmupRenderer.Get()));
	if (!RenderTarget.IsValid())
	{
		UE_LOG(LogBlueprintScreenshotTool, Error, TEXT("Failed to create render target for screenshot"));
		RestoreGraphEditorAfterCapture(InGraphEditor, CaptureState);
		return FBSTScreenshotData();
	}

	FBSTScreenshotData ScreenshotData;
	ScreenshotData.Size = FIntVector(CaptureState.WindowSize.X, CaptureState.WindowSize.Y, 0);
	RenderTarget->GameThread_GetRenderTargetResource()->ReadPixels(ScreenshotData.ColorData);

	RestoreGraphEditorAfterCapture(InGraphEditor, CaptureState);

	const UBlueprintScreenshotToolSettings* Settings = GetDefault<UBlueprintScreenshotToolSettings>();
	if (Settings->bOverrideScreenshotNaming)
	{
		return ScreenshotData;
	}
	
	ScreenshotData.CustomName = GenerateScreenshotName(InGraphEditor);
	return ScreenshotData;
}

void UBlueprintScreenshotToolHandler::OpenDirectory()
{
	const auto Path = FPaths::ConvertRelativePathToFull(GetDefault<UBlueprintScreenshotToolSettings>()->SaveDirectory.Path);
	if (FPaths::DirectoryExists(Path))
	{
		FPlatformProcess::ExploreFolder(*Path);
	}
	else
	{
		ShowDirectoryErrorNotification(Path);
	}
}

void UBlueprintScreenshotToolHandler::RestoreNodeSelection(TSharedPtr<SGraphEditor> InGraphEditor, const TSet<UObject*>& InSelectedNodes)
{
	for (const auto NodeObject : InSelectedNodes)
	{
		if (auto* SelectedNode = Cast<UEdGraphNode>(NodeObject))
		{
			InGraphEditor->SetNodeSelection(SelectedNode, true);
		}
	}
}

bool UBlueprintScreenshotToolHandler::HasAnySelectedNodes(const TSet<TSharedPtr<SGraphEditor>>& InGraphEditors)
{
	for (const auto& GraphEditor : InGraphEditors)
	{
		if (GraphEditor->GetSelectedNodes().Num() > 0)
		{
			return true;
		}
	}

	return false;
}

void UBlueprintScreenshotToolHandler::ShowNotification(const TArray<FString>& InPaths)
{
	if (InPaths.Num() == 0)
	{
		UE_LOG(LogBlueprintScreenshotTool, Warning, TEXT("ShowNotification called with empty paths, skip notification."));
		return;
	}

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Count"), InPaths.Num());

	const auto* Settings = GetDefault<UBlueprintScreenshotToolSettings>();
	
	// 使用两阶段截图机制后，不再需要首次截图警告
	FText Message = FText::Format(Settings->NotificationMessageFormat, Arguments);

	FNotificationInfo NotificationInfo(Message);
	NotificationInfo.ExpireDuration = Settings->ExpireDuration;
	NotificationInfo.bFireAndForget = true;
	NotificationInfo.bUseSuccessFailIcons = Settings->bUseSuccessFailIcons;

	FString HyperLinkText;
	for (auto Id = 0; Id < InPaths.Num(); ++Id)
	{
		const auto& Path = InPaths[Id];
		const auto FullPath = FPaths::ConvertRelativePathToFull(Path);
		const auto bLast = Id == InPaths.Num() - 1;
		HyperLinkText += FString::Printf(TEXT("%s%s"), *FullPath, bLast ? TEXT("") : TEXT("\n"));
	}

	NotificationInfo.HyperlinkText = FText::FromString(HyperLinkText);

	FString HyperLinkPath = FPaths::ConvertRelativePathToFull(InPaths[0]);
	NotificationInfo.Hyperlink = FSimpleDelegate::CreateLambda([HyperLinkPath] { FPlatformProcess::ExploreFolder(*HyperLinkPath); });

	const auto Notification = FSlateNotificationManager::Get().AddNotification(NotificationInfo);
	Notification->SetCompletionState(SNotificationItem::CS_Success);
}

void UBlueprintScreenshotToolHandler::ShowDirectoryErrorNotification(const FString& InPath)
{
	const auto* Settings = GetDefault<UBlueprintScreenshotToolSettings>();

	// UE 最佳实践：使用 LOCTEXT 支持本地化，而非硬编码字符串
	FNotificationInfo NotificationInfo(
		FText::Format(
			NSLOCTEXT("BlueprintScreenshotTool", "DirectoryNotExist", "目录不存在：\n{0}"),
			FText::FromString(InPath)
		)
	);

	NotificationInfo.ExpireDuration = Settings->ExpireDuration;
	NotificationInfo.bFireAndForget = true;
	NotificationInfo.bUseSuccessFailIcons = Settings->bUseSuccessFailIcons;

	const auto Notification = FSlateNotificationManager::Get().AddNotification(NotificationInfo);
	Notification->SetCompletionState(SNotificationItem::CS_Fail);
}

void UBlueprintScreenshotToolHandler::ShowSaveFailedNotification(const FString& InFailedCount)
{
	const auto* Settings = GetDefault<UBlueprintScreenshotToolSettings>();

	// UE 最佳实践：为用户提供清晰的错误信息
	FNotificationInfo NotificationInfo(
		FText::Format(
			NSLOCTEXT("BlueprintScreenshotTool", "SaveFailed", "截图保存失败！\n失败数量：{0}"),
			FText::FromString(InFailedCount)
		)
	);

	NotificationInfo.ExpireDuration = Settings->ExpireDuration;
	NotificationInfo.bFireAndForget = true;
	NotificationInfo.bUseSuccessFailIcons = Settings->bUseSuccessFailIcons;

	const auto Notification = FSlateNotificationManager::Get().AddNotification(NotificationInfo);
	Notification->SetCompletionState(SNotificationItem::CS_Fail);
}

void UBlueprintScreenshotToolHandler::WarmupGraphEditor(TSharedPtr<SGraphEditor> InGraphEditor)
{
	if (!InGraphEditor.IsValid() || !CachedWarmupRenderer.IsValid())
	{
		return;
	}

	FGraphCaptureState CaptureState;
	if (!PrepareGraphEditorForCapture(InGraphEditor, CaptureState))
	{
		return;
	}

	const FVector2D WindowSizeVector2D(CaptureState.WindowSize.X, CaptureState.WindowSize.Y);
	TStrongObjectPtr<UTextureRenderTarget2D> WarmupTarget(DrawGraphEditor(InGraphEditor, WindowSizeVector2D, CachedWarmupRenderer.Get()));
	RestoreGraphEditorAfterCapture(InGraphEditor, CaptureState);
}

UTextureRenderTarget2D* UBlueprintScreenshotToolHandler::DrawGraphEditor(TSharedPtr<SGraphEditor> InGraphEditor, const FVector2D& InWindowSize, FWidgetRenderer* InRenderer)
{
	constexpr auto bUseGamma = true;
	constexpr auto Filter = TF_Default;

	TUniquePtr<FWidgetRenderer> LocalWidgetRenderer;
	FWidgetRenderer* WidgetRenderer = InRenderer;
	if (WidgetRenderer == nullptr)
	{
		LocalWidgetRenderer = MakeUnique<FWidgetRenderer>(bUseGamma, true);
		LocalWidgetRenderer->SetIsPrepassNeeded(true);
		WidgetRenderer = LocalWidgetRenderer.Get();
	}
	
	UTextureRenderTarget2D* RenderTarget = FWidgetRenderer::CreateTargetFor(
		InWindowSize,
		Filter,
		bUseGamma
	);

	if (!ensureMsgf(IsValid(RenderTarget), TEXT("RenderTarget is not valid")))
	{
		return nullptr;
	}

	if (bUseGamma)
	{
		RenderTarget->bForceLinearGamma = true;
		RenderTarget->UpdateResourceImmediate(true);
	}

	constexpr auto RenderingScale = 1.f;
	constexpr auto DeltaTime = 0.f;
	WidgetRenderer->DrawWidget(
		RenderTarget,
		InGraphEditor.ToSharedRef(),
		RenderingScale,
		InWindowSize,
		DeltaTime
	);
	FlushRenderingCommands();

	return RenderTarget;
}

FString UBlueprintScreenshotToolHandler::GetExtension(EBSTImageFormat InFormat)
{
	switch (InFormat)
	{
	case EBSTImageFormat::PNG:
		return TEXT("png");
	case EBSTImageFormat::JPG:
		return TEXT("jpg");
	default:
		ensureMsgf(false, TEXT("Unknown image format"));
		return TEXT("png");
	}
}

FString UBlueprintScreenshotToolHandler::GenerateScreenshotName(TSharedPtr<SGraphEditor> InGraphEditor)
{
	if (!ensure(InGraphEditor.IsValid()))
	{
		return {};
	}
	
	const auto* GraphObject = InGraphEditor->GetCurrentGraph();

	if (!IsValid(GraphObject))
	{
		return {};
	}
	
	const auto* GraphOwner = GraphObject->GetOuter();
	if (!IsValid(GraphOwner))
	{
		return {};
	}

	const auto OwnerName = GraphOwner->GetName();
	const auto GraphName = GraphObject->GetName();
	const auto ScreenshotName = FString::Printf(TEXT("%s_%s_"), *OwnerName, *GraphName);

	return ScreenshotName;
}

TArray<FString> UBlueprintScreenshotToolHandler::TakeScreenshotWithPaths()
{
	PrepareForScreenshot();
	return {};
}
