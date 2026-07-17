// Copyright 2024 Gradess Games. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxExtender.h"

class FToolBarBuilder;

class XTOOLS_BLUEPRINTSCREENSHOTTOOL_API FBlueprintScreenshotToolCommandManager
{
private:
	TSharedPtr<FUICommandList> CommandList;
	TSharedPtr<FExtender> ToolbarExtension;
	TWeakPtr<FUICommandList> MainFrameCommands;

public:
	void RegisterCommands();
	void UnregisterCommands();

	void OnTakeScreenshot();

private:
	void MapCommands();
	void RegisterToolbarExtension();
	void UnregisterToolbarExtension();
	static void AddToolbarExtension(FToolBarBuilder& ToolBarBuilder);
};
