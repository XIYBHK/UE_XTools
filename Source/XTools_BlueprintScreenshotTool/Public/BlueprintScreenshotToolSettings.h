// Copyright 2024 Gradess Games. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "BlueprintScreenshotToolTypes.h"
#include "UObject/Object.h"
#include "BlueprintScreenshotToolSettings.generated.h"

UCLASS(BlueprintType, Config = BluerpintScreenshotTool)
class XTOOLS_BLUEPRINTSCREENSHOTTOOL_API UBlueprintScreenshotToolSettings : public UObject
{
	GENERATED_BODY()

public:
	// If enabled the screenshot will be saved with the custom name
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|General",
		meta = (InlineEditConditionToggle, DisplayName = "覆盖截图命名", Tooltip = "启用后将使用自定义名称保存截图"))
	bool bOverrideScreenshotNaming = false;

	// Will be used as a base name for the screenshot, instead of format <AssetName>_<GraphName>
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|General",
		meta = (EditCondition = "bOverrideScreenshotNaming", DisplayName = "截图基础名称", Tooltip = "截图的基础名称，而不是默认的 <资产名称>_<图表名称> 格式"))
	FString ScreenshotBaseName = FString(TEXT("GraphScreenshot"));

	// Screenshot file format
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|General",
		meta = (DisplayName = "图片格式", Tooltip = "截图文件格式"))
	EBSTImageFormat Extension = EBSTImageFormat::PNG;

	// Quality of jpg image. 10-100
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|General",
		meta = (EditCondition = "Extension == EBSTImageFormat::JPG", ClampMin = "10", ClampMax = "100", UIMin = "10", UIMax = "100",
		DisplayName = "JPG 质量", Tooltip = "JPG 图片的质量，范围 10-100"))
	int32 Quality = 100;

	// Directory where the screenshots will be saved
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|General",
		meta = (RelativePath, DisplayName = "保存目录", Tooltip = "截图保存的目录"))
	FDirectoryPath SaveDirectory = { FPaths::ScreenShotDir() };

	// Padding around selected graph nodes in pixels when taking screenshot
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|General",
		meta = (DisplayName = "截图边距", Tooltip = "截图时选中图表节点周围的边距（像素）"))
	int32 ScreenshotPadding = 128;

	// Minimum screenshot size in pixels
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|General",
		meta = (DisplayName = "最小截图尺寸", Tooltip = "最小截图尺寸（像素）"))
	int32 MinScreenshotSize = 128;

	// Maximum screenshot size in pixels
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|General",
		meta = (DisplayName = "最大截图尺寸", Tooltip = "最大截图尺寸（像素）"))
	int32 MaxScreenshotSize = 15360;

	// Default zoom amount that is used when taking screenshot of selected nodes. ATTENTION: It will scale the size of the screenshot as well. This operation can be slow.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|General",
		meta = (ClampMin = "0.1", ClampMax = "2", UIMin = "0.1", UIMax = "2",
		DisplayName = "缩放倍数", Tooltip = "截图选中节点时使用的默认缩放倍数。注意：这会同时缩放截图的尺寸，此操作可能较慢。"))
	float ZoomAmount = 1.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|Hotkeys",
		meta = (ConfigRestartRequired = true, DisplayName = "截图快捷键", Tooltip = "截取蓝图图表的快捷键"))
	FInputChord TakeScreenshotHotkey = FInputChord(EModifierKey::Control, EKeys::F7);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|Hotkeys",
		meta = (ConfigRestartRequired = true, DisplayName = "打开目录快捷键", Tooltip = "打开截图保存目录的快捷键"))
	FInputChord OpenDirectoryHotkey = FInputChord(EModifierKey::Control, EKeys::F8);

	// If true, the notification with hyperlink to the screenshot will be shown after taking the screenshot
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|Notification",
		meta = (DisplayName = "显示通知", Tooltip = "启用后，截图完成时会显示带有超链接的通知"))
	bool bShowNotification = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|DeveloperMode|Notification",
		meta = (EditCondition="bDeveloperMode", DisplayName = "通知消息格式", Tooltip = "通知消息的格式字符串"))
	FText NotificationMessageFormat = FText::FromString("{Count}|plural(one=Screenshot,other=Screenshots) taken: ");

	// How long the notification will be shown in seconds
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|Notification",
		meta = (DisplayName = "通知显示时长", Tooltip = "通知显示的时长（秒）"))
	float ExpireDuration = 5.f;

	// If true, the notification will use success/fail icons instead of the default info icon
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|DeveloperMode|Notification",
		meta = (EditCondition="bDeveloperMode", DisplayName = "使用成功/失败图标", Tooltip = "启用后，通知将使用成功/失败图标而不是默认的信息图标"))
	bool bUseSuccessFailIcons = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|DeveloperMode",
		meta = (DisplayName = "开发者模式", Tooltip = "启用开发者模式以显示额外的高级选项"))
	bool bDeveloperMode = false;

	// The text is used for searching Blueprint Diff toolbar buttons to inject "Take Screenshot" button
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|DeveloperMode",
		meta = (EditCondition = "bDeveloperMode", DisplayName = "差异工具栏文本", Tooltip = "用于搜索蓝图差异工具栏按钮以注入【截图】按钮的文本"))
	TArray<FText> DiffToolbarTexts = {FText::FromString(TEXT("Lock/Unlock")), FText::FromString(TEXT("Vertical/Horizontal"))};

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|DeveloperMode",
		meta = (EditCondition = "bDeveloperMode", DisplayName = "差异窗口按钮标签", Tooltip = "差异窗口截图按钮的标签"))
	FText DiffWindowButtonLabel = FText::FromString(TEXT("Take Screenshot"));

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "BlueprintScreenshotTool|DeveloperMode",
		meta = (EditCondition = "bDeveloperMode", DisplayName = "差异窗口按钮提示", Tooltip = "差异窗口截图按钮的工具提示"))
	FText DiffWindowButtonToolTip = FText::FromString(TEXT("Take screenshot of the shown diff graphs"));
};
