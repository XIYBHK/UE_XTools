// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistTypes.h"
#include "ContentBrowserItem.h"

struct FContentBrowserItemPath;

class XTOOLS_BLUEPRINTASSIST_API FBAInputProcessorState
{
public:
	bool OnKeyOrMouseDown(const FKey& Key);
	bool OnKeyOrMouseUp(const FKey& Key);

	bool TryCopyPastePinValue();

	bool TryFocusInDetailPanel();

	bool CutSelectedAssets();

	bool ProcessContentBrowserInput();

	bool BulkMoveItems(const TArray<FContentBrowserItem>& Items, FName DestPath, FText* OutError);

	bool bConsumeMouseUp = false;

	TArray<FContentBrowserItem> CutItems;

	// TOptional<FEdGraphPinType> CopiedPinType;
	// bool TryCopyPastePinType();
	// bool SpecialCopyPasteNode(); 
};
