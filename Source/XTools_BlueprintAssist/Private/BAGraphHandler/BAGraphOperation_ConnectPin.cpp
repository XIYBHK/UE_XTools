// Copyright fpwong. All Rights Reserved.


#include "BAGraphHandler/BAGraphOperation_ConnectPin.h"

#include "BlueprintAssistCommands.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistUtils.h"
#include "InputCoreTypes.h"
#include "NodeFactory.h"
#include "SGraphPanel.h"
#include "BlueprintAssistMisc/BAGraphSchema.h"
#include "EdGraph/EdGraph.h"
#include "Framework/Application/SlateApplication.h"

FBAGraphOperation_ConnectPin::FBAGraphOperation_ConnectPin(TSharedPtr<FBAGraphHandler> InGraphHandler)
	: FBAGraphOperation(InGraphHandler)
{
	SourceHandle.SetPin(GraphHandler->GetSelectedPin());
}

bool FBAGraphOperation_ConnectPin::OnKeyUp(const FKey& Key)
{
	TSharedRef<const FInputChord> Chord = FBACommands::Get().LinkToHoveredPin->GetFirstValidChord();

	if (Key == Chord->Key)
	{
		UEdGraphPin* Source = SourceHandle.GetPin();
		UEdGraphPin* Target = TargetHandle.GetPin();

		if (Source && Target)
		{
			if (FBAUtils::CanConnectPins(Source, Target, true, true))
			{
				const FBAScopedGraphAction Transaction(GraphHandler, "Link To Hovered Pin");
				FBAUtils::TryCreateConnectionUnsafe(Source, Target, EBABreakMethod::Default, true);
			}
		}

		GraphHandler->EndOperation();
	}

	return false;
}

UEdGraphPin* FBAGraphOperation_ConnectPin::GetClosestPinToCursor(TSharedPtr<SGraphPanel> GraphPanel, float CullLimit)
{
	if (!GraphPanel || !GraphPanel->GetGraphObj())
	{
		return nullptr;
	}

	UEdGraphPin* SourcePin = SourceHandle.GetPin();
	if (!SourcePin)
	{
		return nullptr;
	}

	UEdGraph* Graph = GraphPanel->GetGraphObj();
	if (!Graph)
	{
		return nullptr;
	}

	const FVector2D CursorPos = FBAUtils::ScreenSpaceToPanelCoord(GraphPanel, FSlateApplication::Get().GetCursorPos());

	float ClosestDist = CullLimit * CullLimit;
	UEdGraphPin* ClosestPin = nullptr;

	const TArray<UEdGraphNode*>& Nodes = Graph->Nodes;

	for (UEdGraphNode* Node : Nodes)
	{
		for (UEdGraphPin* TargetPin : Node->Pins)
		{
			if (TSharedPtr<SGraphPin> GraphPin = FBAUtils::GetGraphPinFast(GraphPanel, TargetPin))
			{
				const FVector2D PinPos = FBAUtils::GetPinPos(GraphPin);
				const float Dist = FVector2D::DistSquared(PinPos, CursorPos);

				if (Dist < ClosestDist)
				{
					if (!FBAUtils::IsPinVisible(GraphPin))
					{
						continue;
					}

					if (!FBAUtils::CanConnectPins(TargetPin, SourcePin, true, true))
					{
						continue;
					}

					ClosestDist = Dist;
					ClosestPin = TargetPin;
				}
			}
		}
	}

	return ClosestPin;
}

void FBAGraphOperation_ConnectPin::Tick(float DeltaTime)
{
	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();
	TargetHandle = GetClosestPinToCursor(GraphPanel, 100);
}

void FBAGraphOperation_ConnectPin::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled)
{
	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();

	UEdGraphPin* SourcePin = SourceHandle.GetPin();
	if (!SourcePin)
	{
		return;
	}

	TSharedPtr<SGraphPin> SourceGraphPin = FBAUtils::GetGraphPinFast(GraphPanel, SourcePin);
	if (!SourceGraphPin.IsValid())
	{
		return;
	}

	UEdGraphPin* TargetPin = TargetHandle.GetPin();
	if (!TargetPin)
	{
		return;
	}

	TSharedPtr<SGraphPin> TargetGraphPin = FBAUtils::GetGraphPinFast(GraphPanel, TargetPin);

	if (!TargetGraphPin)
	{
		return;
	}

	auto GraphObj = GraphPanel->GetGraphObj();
	if (!GraphObj)
	{
		return;
	}

	const UEdGraphSchema* Schema = GraphObj->GetSchema();
	if (!Schema)
	{
		return;
	}

	const int32 WireLayerId = LayerId++;
	const int32 MaxLayerId = LayerId;

	// const float ZoomFactor = AllottedGeometry.Scale * GraphPanel->GetZoomAmount();
	constexpr float ZoomFactor = 1.0f; // looks better when the bubble is bigger when zoomed out?

	FConnectionDrawingPolicy* ConnectionDrawingPolicy =
		FNodeFactory::CreateConnectionPolicy(Schema, WireLayerId, MaxLayerId, ZoomFactor, MyCullingRect, OutDrawElements, GraphObj);

	if (ConnectionDrawingPolicy)
	{
		// graph space
		FVector2D SourceGraphPos = FBAUtils::GetPinPos(SourceGraphPin);
		FVector2D TargetGraphPos = FBAUtils::GetPinPos(TargetGraphPin);

		// panel space
		const FVector2D SourcePanelPos = FBAUtils::GraphCoordToPanelCoord(GraphPanel, SourceGraphPos);
		const FVector2D TargetPanelPos = FBAUtils::GraphCoordToPanelCoord(GraphPanel, TargetGraphPos);

		// draw space
		const FSlateRenderTransform& RenderTransform = AllottedGeometry.GetAccumulatedRenderTransform();
		FVector2D SourceDrawSpace = RenderTransform.TransformPoint(SourcePanelPos);
		FVector2D TargetDrawSpace = RenderTransform.TransformPoint(TargetPanelPos);

		// drawing the spline with drawing policy requires selecting an input / output pin
		UEdGraphPin* OutputPin = SourcePin->Direction == EGPD_Output ? SourcePin : TargetPin;
		UEdGraphPin* InputPin = SourcePin->Direction == EGPD_Output ? TargetPin : SourcePin;

		// drawing policy expects the output pin as the start point
		FVector2D AdjustedStartPoint = SourcePin->Direction == EGPD_Output ? SourceDrawSpace : TargetDrawSpace;
		FVector2D AdjustedEndPoint = SourcePin->Direction == EGPD_Output ? TargetDrawSpace : SourceDrawSpace;

		FConnectionParams Params;
		ConnectionDrawingPolicy->DetermineWiringStyle(OutputPin, InputPin, Params);
		// Params.WireColor = Schema->GetPinTypeColor(SourcePin->PinType);
		Params.WireColor = FLinearColor::Green;
		Params.WireThickness = 5.0f;
		Params.bDrawBubbles = true;
		ConnectionDrawingPolicy->DrawSplineWithArrow(AdjustedStartPoint, AdjustedEndPoint, Params);
	}
}
