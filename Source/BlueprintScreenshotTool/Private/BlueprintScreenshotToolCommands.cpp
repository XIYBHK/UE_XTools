// Copyright 2024 Gradess Games. All Rights Reserved.


#include "BlueprintScreenshotToolCommands.h"
#include "BlueprintScreenshotToolSettings.h"
#include "BlueprintScreenshotToolStyle.h"

#define LOCTEXT_NAMESPACE "BlueprintScreenshotTool"

FBlueprintScreenshotToolCommands::FBlueprintScreenshotToolCommands()
	: TCommands<FBlueprintScreenshotToolCommands>(
		TEXT("BlueprintScreenshotTool"),
		NSLOCTEXT("Contexts", "BlueprintScreenshotTool", "BlueprintScreenshotTool Commands"),
		NAME_None,
		FBlueprintScreenshotToolStyle::Get().GetStyleSetName()
	)
{
}

void FBlueprintScreenshotToolCommands::RegisterCommands()
{
	UI_COMMAND(TakeScreenshot, "截取截图", "截取当前激活的蓝图图表的截图", EUserInterfaceActionType::Button, GetDefault<UBlueprintScreenshotToolSettings>()->TakeScreenshotHotkey);
	UI_COMMAND(OpenDirectory, "打开目录", "打开保存截图的目录", EUserInterfaceActionType::None, GetDefault<UBlueprintScreenshotToolSettings>()->OpenDirectoryHotkey);
}

#undef LOCTEXT_NAMESPACE
