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

void UBlueprintScreenshotToolHandler::PrepareForScreenshot()
{
	auto GraphEditors = UBlueprintScreenshotToolWindowManager::FindActiveGraphEditors();
	if (GraphEditors.Num() <= 0)
	{
		return;
	}

	CachedGraphEditorsForWarmup.Reset();
	CachedGraphEditorsForWarmup = GraphEditors.Array();

	const auto bHasSelectedNodes = HasAnySelectedNodes(GraphEditors);
	for (const auto& GraphEditor : GraphEditors)
	{
		if (bHasSelectedNodes && GraphEditor->GetSelectedNodes().Num() <= 0)
		{
			continue;
		}

		CaptureGraphEditor(GraphEditor);
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

FBSTVector2D UBlueprintScreenshotToolHandler::CalculateOptimalWindowSize(FBSTScreenshotData& ScreenshotData, const FString& Path)
{
	FBSTVector2D OptimalSize = FBSTVector2D(1920, 1080);
	
	if (ScreenshotData.Size.X > 0 && ScreenshotData.Size.Y > 0)
	{
		OptimalSize = FBSTVector2D(ScreenshotData.Size.X, ScreenshotData.Size.Y);
	}
	
	return OptimalSize;
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

FBSTScreenshotData UBlueprintScreenshotToolHandler::CaptureWithTempWindow(TSharedPtr<SGraphEditor> InGraphEditor, const FBSTVector2D& InWindowSize)
{
	if (!InGraphEditor)
	{
		return FBSTScreenshotData();
	}

	FBSTVector2D CachedViewLocation;
	float CachedZoomAmount = 1.f;
	InGraphEditor->GetViewLocation(CachedViewLocation, CachedZoomAmount);
	
	const FGraphPanelSelectionSet SelectedNodes = InGraphEditor->GetSelectedNodes();
	const auto* Settings = GetDefault<UBlueprintScreenshotToolSettings>();

	FBSTVector2D WindowSize = InWindowSize;
	FBSTVector2D NewViewLocation = CachedViewLocation;
	float NewZoomAmount = CachedZoomAmount;

	if (SelectedNodes.Num() > 0)
	{
		FSlateRect BoundsForSelectedNodes;
		InGraphEditor->GetBoundsForSelectedNodes(BoundsForSelectedNodes, Settings->ScreenshotPadding);
		
		NewViewLocation = BoundsForSelectedNodes.GetTopLeft();
		NewZoomAmount = Settings->ZoomAmount;
		WindowSize = BoundsForSelectedNodes.GetSize() * Settings->ZoomAmount;
	}

	WindowSize = WindowSize.ClampAxes(Settings->MinScreenshotSize, Settings->MaxScreenshotSize);
	InGraphEditor->SetViewLocation(NewViewLocation, NewZoomAmount);
	
	InGraphEditor->ClearSelectionSet();
	
	TSharedRef<SWindow> NewWindowRef = SNew(SWindow)
		.CreateTitleBar(false)
		.ClientSize(WindowSize)
		.ScreenPosition(FBSTVector2D(0.0f, 0.0f))
		.AdjustInitialSizeAndPositionForDPIScale(false)
		.SaneWindowPlacement(false)
		.SupportsTransparency(EWindowTransparency::PerWindow)
		.InitialOpacity(0.0f);

	NewWindowRef->SetContent(InGraphEditor.ToSharedRef());
	FSlateApplication::Get().AddWindow(NewWindowRef, false);

	InGraphEditor->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	NewWindowRef->ShowWindow();
	InGraphEditor->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	FlushRenderingCommands();

	FBSTScreenshotData ScreenshotData;
	ScreenshotData.Size = FIntVector(WindowSize.X, WindowSize.Y, 0);
	
	TArray<FColor> ColorData;
	FSlateApplication::Get().TakeScreenshot(NewWindowRef, ColorData, ScreenshotData.Size);
	ScreenshotData.ColorData = MoveTemp(ColorData);

	InGraphEditor->SetViewLocation(CachedViewLocation, CachedZoomAmount);
	RestoreNodeSelection(InGraphEditor, SelectedNodes);
	
	NewWindowRef->HideWindow();
	NewWindowRef->RequestDestroyWindow();

	if (!Settings->bOverrideScreenshotNaming)
	{
		ScreenshotData.CustomName = GenerateScreenshotName(InGraphEditor);
	}

	return ScreenshotData;
}

FBSTScreenshotData UBlueprintScreenshotToolHandler::CaptureGraphEditor(TSharedPtr<SGraphEditor> InGraphEditor)
{
	if (!InGraphEditor)
	{
		return FBSTScreenshotData();
	}

	FBSTVector2D CachedViewLocation;
	FBSTVector2D NewViewLocation;
	FBSTVector2D WindowSize;
	
	float CachedZoomAmount = 1.f;
	float NewZoomAmount = 1.f;
	float WindowSizeScale = 1.f;
	
	const auto* Settings = GetDefault<UBlueprintScreenshotToolSettings>();
	const auto SelectedNodes = InGraphEditor->GetSelectedNodes();

	InGraphEditor->GetViewLocation(CachedViewLocation, CachedZoomAmount);
	
	
	if (SelectedNodes.Num() > 0)
	{
		FSlateRect BoundsForSelectedNodes;
		InGraphEditor->GetBoundsForSelectedNodes(BoundsForSelectedNodes, Settings->ScreenshotPadding);
		
		NewViewLocation = BoundsForSelectedNodes.GetTopLeft();
		NewZoomAmount = Settings->ZoomAmount;

		WindowSizeScale = Settings->ZoomAmount;

		WindowSize = BoundsForSelectedNodes.GetSize();
	}
	else
	{
		NewViewLocation = CachedViewLocation;
		NewZoomAmount = CachedZoomAmount;

		const FBSTVector2D WindowPosition = InGraphEditor->GetTickSpaceGeometry().GetAbsolutePosition();
		const auto DPIScale = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(WindowPosition.X, WindowPosition.Y);

		const auto& CachedGeometry = InGraphEditor->GetCachedGeometry();
		const auto SizeOfWidget = CachedGeometry.GetLocalSize();
		WindowSize = SizeOfWidget * DPIScale;

		InGraphEditor->SetViewLocation(NewViewLocation, CachedZoomAmount);
	}

	InGraphEditor->SetViewLocation(NewViewLocation, NewZoomAmount);
	
	WindowSize = WindowSize.ClampAxes(Settings->MinScreenshotSize, Settings->MaxScreenshotSize);
	WindowSize *= WindowSizeScale;

	InGraphEditor->ClearSelectionSet();

	// 触发资源加载
	InGraphEditor->Invalidate(EInvalidateWidgetReason::Paint);
	InGraphEditor->SlatePrepass(1.0f);
	FlushRenderingCommands();
	InGraphEditor->SlatePrepass(1.0f);
	FlushRenderingCommands();

	// 双重绘制:第一次触发资源加载,第二次使用已加载资源
	// 注意:由于 Slate 资源加载机制,打开蓝图后的第一次截图可能仍有部分图标显示异常
	// 建议:如果第一次截图有问题,请再截一次
	// 转换 FBSTVector2D 到 FVector2D
	const FVector2D WindowSizeVector2D(WindowSize.X, WindowSize.Y);
	TStrongObjectPtr<UTextureRenderTarget2D> WarmupTarget(DrawGraphEditor(InGraphEditor, WindowSizeVector2D));
	FPlatformProcess::Sleep(0.05f);

	const auto RenderTarget = TStrongObjectPtr<UTextureRenderTarget2D>(DrawGraphEditor(InGraphEditor, WindowSizeVector2D));
	if (!RenderTarget.IsValid())
	{
		UE_LOG(LogBlueprintScreenshotTool, Error, TEXT("Failed to create render target for screenshot"));
		RestoreNodeSelection(InGraphEditor, SelectedNodes);
		InGraphEditor->SetViewLocation(CachedViewLocation, CachedZoomAmount);
		return FBSTScreenshotData();
	}
	FlushRenderingCommands();

	FBSTScreenshotData ScreenshotData;
	ScreenshotData.Size = FIntVector(WindowSize.X, WindowSize.Y, 0);
	RenderTarget->GameThread_GetRenderTargetResource()->ReadPixels(ScreenshotData.ColorData);

	RestoreNodeSelection(InGraphEditor, SelectedNodes);
	InGraphEditor->SetViewLocation(CachedViewLocation, CachedZoomAmount);

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
	checkf(InPaths.Num() > 0, TEXT("InPaths must not be empty"));

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

UTextureRenderTarget2D* UBlueprintScreenshotToolHandler::DrawGraphEditor(TSharedPtr<SGraphEditor> InGraphEditor, const FVector2D& InWindowSize)
{
	return DrawGraphEditorInternal(InGraphEditor, InWindowSize, false);
}

UTextureRenderTarget2D* UBlueprintScreenshotToolHandler::DrawGraphEditorWithRenderer(TSharedPtr<SGraphEditor> InGraphEditor, const FVector2D& InWindowSize, FWidgetRenderer* InRenderer, bool bIsWarmup)
{
	UE_LOG(LogBlueprintScreenshotTool, VeryVerbose, TEXT("Start rendering: Size=%s, Warmup=%d"), *InWindowSize.ToString(), bIsWarmup);

	constexpr auto bUseGamma = true;
	constexpr auto DrawTimes = 2;
	constexpr auto Filter = TF_Default;

	// 使用传入的 Renderer (首次截图时共享同一个实例)
	FWidgetRenderer* WidgetRenderer = InRenderer;

	UTextureRenderTarget2D* RenderTarget = FWidgetRenderer::CreateTargetFor(
		InWindowSize,
		Filter,
		bUseGamma
	);

	if (!ensureMsgf(IsValid(RenderTarget), TEXT("RenderTarget is not valid")))
	{
		UE_LOG(LogBlueprintScreenshotTool, Error, TEXT("Failed to create render target"));
		return nullptr;
	}

	if (bUseGamma)
	{
		RenderTarget->bForceLinearGamma = true;
		RenderTarget->UpdateResourceImmediate(true);
	}

	// 预绘制:第一次绘制触发所有资源(包括图标)的加载
	{
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
		
		RenderTarget->UpdateResourceImmediate(false);
		FlushRenderingCommands();
		
		// 预热模式:额外等待
		if (bIsWarmup)
		{
			UE_LOG(LogBlueprintScreenshotTool, VeryVerbose, TEXT("Warmup phase: waiting for resources to load"));
			FPlatformProcess::Sleep(0.2f);  // 200ms for resource loading
		}
	}

	// 正式绘制
	for (int32 Count = 0; Count < DrawTimes; Count++)
	{
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
	}
	
	UE_LOG(LogBlueprintScreenshotTool, VeryVerbose, TEXT("Rendering complete"));
	return RenderTarget;
}

UTextureRenderTarget2D* UBlueprintScreenshotToolHandler::DrawGraphEditorInternal(TSharedPtr<SGraphEditor> InGraphEditor, const FVector2D& InWindowSize, bool bIsWarmup)
{
	constexpr auto bUseGamma = true;
	constexpr auto DrawTimes = 2;
	constexpr auto Filter = TF_Default;

	TUniquePtr<FWidgetRenderer> WidgetRenderer = MakeUnique<FWidgetRenderer>(bUseGamma, true);
	WidgetRenderer->SetIsPrepassNeeded(true);
	
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

	// 预绘制
	{
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
		RenderTarget->UpdateResourceImmediate(false);
		FlushRenderingCommands();
	}

	// 正式绘制
	for (int32 Count = 0; Count < DrawTimes; Count++)
	{
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
	}

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
