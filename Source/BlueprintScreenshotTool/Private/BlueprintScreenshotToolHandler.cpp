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

		// UE 最佳实践：在多显示器环境下使用实际窗口位置获取准确的 DPI 缩放
		// 使用 (0,0) 坐标可能在多显示器配置下获得错误的 DPI 值
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

	const auto RenderTarget = TStrongObjectPtr<UTextureRenderTarget2D>(DrawGraphEditor(InGraphEditor, WindowSize));

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

	FFormatOrderedArguments Arguments;
	Arguments.Add(InPaths.Num());

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
	constexpr auto bUseGamma = true;
	// 绘制两次以确保所有延迟加载的资源都已正确渲染
	// 第一次绘制可能会触发资源加载，第二次绘制确保所有内容都已就绪
	constexpr auto DrawTimes = 2;
	constexpr auto Filter = TF_Default;

	// 使用 TUniquePtr 自动管理 FWidgetRenderer 生命周期，避免内存泄漏
	// UE 最佳实践：对于需要手动管理的对象，使用智能指针而非裸指针
	TUniquePtr<FWidgetRenderer> WidgetRenderer = MakeUnique<FWidgetRenderer>(bUseGamma, true);
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

	// TUniquePtr 会在作用域结束时自动调用析构函数
	// 无需手动调用 BeginCleanup

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
