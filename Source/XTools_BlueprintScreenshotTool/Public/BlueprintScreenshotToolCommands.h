// Copyright 2024 Gradess Games. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class XTOOLS_BLUEPRINTSCREENSHOTTOOL_API FBlueprintScreenshotToolCommands : public TCommands<FBlueprintScreenshotToolCommands>
{
public:
	TSharedPtr<FUICommandInfo> TakeScreenshot;
	TSharedPtr<FUICommandInfo> OpenDirectory;

public:
	FBlueprintScreenshotToolCommands();

	virtual void RegisterCommands() override;
};
