// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class FWidgetStyle;
class FSlateWindowElementList;
class FSlateRect;
struct FGeometry;
class FPaintArgs;
class FBAGraphHandler;
struct FKeyEvent;

class FBAGraphOperation : public TSharedFromThis<FBAGraphOperation>
{
public:
	explicit FBAGraphOperation(TSharedPtr<FBAGraphHandler> InGraphHandler)
		: GraphHandler(InGraphHandler) {}

	virtual ~FBAGraphOperation() {}

	TSharedPtr<FBAGraphHandler> GraphHandler;

	virtual bool OnKeyDown(const FKey& Key) { return false; }
	virtual bool OnKeyUp(const FKey& Key) { return false; }

	virtual void Tick(float DeltaTime) {}

	virtual void OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) {}
};
