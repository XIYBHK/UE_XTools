// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Widgets/SWindow.h"

/**
 * Menu to select options before sending crash report
 */
class XTOOLS_BLUEPRINTASSIST_API SBACrashReportDialog
	: public SWindow
{
	SLATE_BEGIN_ARGS(SBACrashReportDialog)
		{
		}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
