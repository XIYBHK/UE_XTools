// Copyright 2024 Gradess Games. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Framework/MultiBox/SToolBarButtonBlock.h"

class XTOOLS_BLUEPRINTSCREENSHOTTOOL_API FBlueprintScreenshotToolDiffWindowButton : public FToolBarButtonBlock
{
public:
	explicit FBlueprintScreenshotToolDiffWindowButton(const FButtonArgs& ButtonArgs)
		: FToolBarButtonBlock(ButtonArgs)
	{
	}
	
	FBlueprintScreenshotToolDiffWindowButton();
};
