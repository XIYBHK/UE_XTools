// Copyright 2024 Gradess Games. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"

// UE 5.7+ Slate API 兼容性
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
using FBSTVector2D = FVector2f;
#else
using FBSTVector2D = FVector2D;
#endif
#include "UObject/Object.h"
#include "Logging/LogMacros.h"
#include "BlueprintScreenshotToolTypes.h"
#include "BlueprintScreenshotToolHandler.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBlueprintScreenshotTool, Log, All);

UCLASS(BlueprintType)
class XTOOLS_BLUEPRINTSCREENSHOTTOOL_API UBlueprintScreenshotToolHandler : public UObject
{
	GENERATED_BODY()

public:
	// Takes a screenshot of the current graph editor
	UFUNCTION(BlueprintCallable, Category = "Blueprint Screenshot Tool")
	static TArray<FString> TakeScreenshotWithPaths();

	// Takes a screenshot of the current graph editor and shows a notification
	UFUNCTION(BlueprintCallable, Category = "Blueprint Screenshot Tool")
	static TArray<FString> TakeScreenshotWithNotification();

	// Takes a screenshot of the current graph editor
	UFUNCTION(BlueprintCallable, Category = "Blueprint Screenshot Tool")
	static void TakeScreenshot();

	// Opens the screenshot save directory
	UFUNCTION(BlueprintCallable, Category = "Blueprint Screenshot Tool")
	static void OpenDirectory();

	static void OnPostTick(float DeltaTime);

private:
	static bool bTakingScreenshot;

	static FString GetExtension(EBSTImageFormat InFormat);
	static FBSTScreenshotData CaptureGraphEditor(TSharedPtr<SGraphEditor> InGraphEditor);
	static FString SaveScreenshot(const TArray<FColor>& InColorData, const FIntVector& InSize);
	static FString SaveScreenshot(const FBSTScreenshotData& InData);

	static void PrepareForScreenshot();
	static void ExecuteAsyncScreenshot();
	static FBSTScreenshotData CaptureWithTempWindow(TSharedPtr<SGraphEditor> InGraphEditor, const FBSTVector2D& WindowSize);
	static void UpdateScreenshotState(bool bIsProcessing);
	static FBSTVector2D CalculateOptimalWindowSize(FBSTScreenshotData& ScreenshotData, const FString& Path);

protected:
	static void RestoreNodeSelection(TSharedPtr<SGraphEditor> InGraphEditor, const TSet<UObject*>& InSelectedNodes);
	static bool HasAnySelectedNodes(const TSet<TSharedPtr<SGraphEditor>>& InGraphEditors);
	static void ShowNotification(const TArray<FString>& InPaths);
	static void ShowDirectoryErrorNotification(const FString& InPath);
	static void ShowSaveFailedNotification(const FString& InFailedCount);
	static UTextureRenderTarget2D* DrawGraphEditor(TSharedPtr<SGraphEditor> InGraphEditor, const FBSTVector2D& InWindowSize);
	static UTextureRenderTarget2D* DrawGraphEditorInternal(TSharedPtr<SGraphEditor> InGraphEditor, const FBSTVector2D& InWindowSize, bool bIsWarmup);
	static UTextureRenderTarget2D* DrawGraphEditorWithRenderer(TSharedPtr<SGraphEditor> InGraphEditor, const FBSTVector2D& InWindowSize, class FWidgetRenderer* InRenderer, bool bIsWarmup);

	static FString GenerateScreenshotName(TSharedPtr<SGraphEditor> InGraphEditor);
};


