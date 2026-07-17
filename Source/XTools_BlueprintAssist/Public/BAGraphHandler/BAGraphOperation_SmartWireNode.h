// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BAGraphOperation.h"
#include "BlueprintAssistFormatters/GraphFormatterTypes.h"

class FBAGraphOperation_SmartWireNode : public FBAGraphOperation
{
public:
	explicit FBAGraphOperation_SmartWireNode(TSharedPtr<FBAGraphHandler> InGraphHandler);

	TWeakObjectPtr<UEdGraphNode> SelectedNode;
	TArray<FPinLink> AllConnections;
	TArray<FPinLink> ConversionConnections;
	FPinLink* BestConnection = nullptr;

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

	FPinLink* FindBestConnection(TSharedPtr<SGraphPanel> GraphPanel, float CullLimit);

	void BuildConnectionPoints();

	void DrawLink(
		FPinLink& Point,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId);
};