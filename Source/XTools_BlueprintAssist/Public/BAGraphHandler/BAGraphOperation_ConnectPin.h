// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BAGraphOperation.h"
#include "BlueprintAssistTypes.h"

class FBAGraphOperation_ConnectPin : public FBAGraphOperation
{
public:
	explicit FBAGraphOperation_ConnectPin(TSharedPtr<FBAGraphHandler> InGraphHandler);

	FBANodePinHandle SourceHandle;
	FBANodePinHandle TargetHandle;

	virtual bool OnKeyUp(const FKey& Key) override;

	virtual void Tick(float DeltaTime) override;

	virtual void OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) override;

	UEdGraphPin* GetClosestPinToCursor(TSharedPtr<SGraphPanel> GraphPanel, float CullLimit);
};