// Copyright 2024 Gradess Games. All Rights Reserved.


#include "BlueprintScreenshotToolHandler.h"
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

TArray<FString> UBlueprintScreenshotToolHandler::TakeScreenshotWithPaths()
{
	auto GraphEditors = UBlueprintScreenshotToolWindowManager::FindActiveGraphEditors();
	if (GraphEditors.Num() <= 0)
	{
		return {};
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
			// UE 最佳实践：提供明确的失败反馈给用户
			FailedCount++;

			// 获取图表名称用于错误提示
			FString GraphName = TEXT("Unknown");
			if (GraphEditor.IsValid() && GraphEditor->GetCurrentGraph())
			{
				GraphName = GraphEditor->GetCurrentGraph()->GetName();
			}
		}
	}

	// 如果所有截图都失败，显示错误通知
	if (FailedCount > 0 && Paths.Num() == 0)
	{
		ShowSaveFailedNotification(FString::Printf(TEXT("%d"), FailedCount));
	}

	return Paths;
}

TArray<FString> UBlueprintScreenshotToolHandler::TakeScreenshotWithNotification()
{
	const auto Paths = TakeScreenshotWithPaths();

	if (Paths.Num() > 0)
	{
		ShowNotification(Paths);
	}

	return Paths;
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
	if (!InGraphEditor)
	{
		return FBSTScreenshotData();
	}

	FVector2D CachedViewLocation;
	FVector2D NewViewLocation;
	FVector2D WindowSize;
	
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

		const FVector2D WindowPosition = InGraphEditor->GetTickSpaceGeometry().GetAbsolutePosition();
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
	TStrongObjectPtr<UTextureRenderTarget2D> WarmupTarget(DrawGraphEditor(InGraphEditor, WindowSize));
	FPlatformProcess::Sleep(0.05f);
	
	const auto RenderTarget = TStrongObjectPtr<UTextureRenderTarget2D>(DrawGraphEditor(InGraphEditor, WindowSize));
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
	const auto Message = FText::Format(Settings->NotificationMessageFormat, Arguments);

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
	if (bIsWarmup)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Screenshot Debug] DrawGraphEditorWithRenderer: [预热模式]"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Screenshot Debug] DrawGraphEditorWithRenderer: [正式模式]"));
	}
	UE_LOG(LogTemp, Warning, TEXT("[Screenshot Debug] DrawGraphEditorWithRenderer: 开始, 尺寸=%s, 复用Renderer=%d"), *InWindowSize.ToString(), InRenderer != nullptr);
	
	constexpr auto bUseGamma = true;
	constexpr auto DrawTimes = 2;
	constexpr auto Filter = TF_Default;

	// 使用传入的 Renderer (首次截图时共享同一个实例)
	FWidgetRenderer* WidgetRenderer = InRenderer;
	UE_LOG(LogTemp, Warning, TEXT("[Screenshot Debug] DrawGraphEditorWithRenderer: 使用传入的 FWidgetRenderer"));
	
	UTextureRenderTarget2D* RenderTarget = FWidgetRenderer::CreateTargetFor(
		InWindowSize,
		Filter,
		bUseGamma
	);

	if (!ensureMsgf(IsValid(RenderTarget), TEXT("RenderTarget is not valid")))
	{
		return nullptr;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Screenshot Debug] DrawGraphEditorWithRenderer: RenderTarget 创建成功, 格式=%d"), RenderTarget->GetFormat());
	
	if (bUseGamma)
	{
		RenderTarget->bForceLinearGamma = true;
		RenderTarget->UpdateResourceImmediate(true);
		UE_LOG(LogTemp, Warning, TEXT("[Screenshot Debug] DrawGraphEditorWithRenderer: 设置 Gamma 校正"));
	}

	// 预绘制:第一次绘制触发所有资源(包括图标)的加载
	{
		UE_LOG(LogTemp, Warning, TEXT("[Screenshot Debug] DrawGraphEditorWithRenderer: === 开始预绘制 ==="));
		constexpr auto RenderingScale = 1.f;
		constexpr auto DeltaTime = 0.f;
		WidgetRenderer->DrawWidget(
			RenderTarget,
			InGraphEditor.ToSharedRef(),
			RenderingScale,
			InWindowSize,
			DeltaTime
		);
		UE_LOG(LogTemp, Warning, TEXT("[Screenshot Debug] DrawGraphEditorWithRenderer: 预绘制完成, 刷新渲染命令"));
		FlushRenderingCommands();
		
		RenderTarget->UpdateResourceImmediate(false);
		FlushRenderingCommands();
		
		// 预热模式:额外等待
		if (bIsWarmup)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Screenshot Debug] DrawGraphEditorWithRenderer: [预热模式] 预绘制后等待 (200ms)"));
			FPlatformProcess::Sleep(0.2f);  // 200ms 测试
		}
		
		UE_LOG(LogTemp, Warning, TEXT("[Screenshot Debug] DrawGraphEditorWithRenderer: === 预绘制流程完成 ==="));
	}

	// 正式绘制
	UE_LOG(LogTemp, Warning, TEXT("[Screenshot Debug] DrawGraphEditorWithRenderer: === 开始正式绘制 (共%d次) ==="), DrawTimes);
	for (int32 Count = 0; Count < DrawTimes; Count++)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Screenshot Debug] DrawGraphEditorWithRenderer: 正式绘制第 %d/%d 次"), Count + 1, DrawTimes);
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
		UE_LOG(LogTemp, Warning, TEXT("[Screenshot Debug] DrawGraphEditorWithRenderer: 第 %d 次绘制完成"), Count + 1);
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[Screenshot Debug] DrawGraphEditorWithRenderer: === 所有绘制完成,返回 RenderTarget ==="));
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
