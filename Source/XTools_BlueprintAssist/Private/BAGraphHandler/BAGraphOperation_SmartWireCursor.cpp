// Copyright fpwong. All Rights Reserved.


#include "BAGraphHandler/BAGraphOperation_SmartWireCursor.h"

#include "BlueprintAssistCommands.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistUtils.h"
#include "InputCoreTypes.h"
#include "NodeFactory.h"
#include "SGraphPanel.h"
#include "BlueprintAssistMisc/BAGraphSchema.h"
#include "EdGraph/EdGraph.h"
#include "Framework/Application/SlateApplication.h"

FBAGraphOperation_SmartWireCursor::FBAGraphOperation_SmartWireCursor(TSharedPtr<FBAGraphHandler> InGraphHandler)
	: FBAGraphOperation(InGraphHandler)
{
	BuildConnectionPoints();
}

bool FBAGraphOperation_SmartWireCursor::OnKeyUp(const FKey& Key)
{
	if (!GraphHandler->HasValidGraphReferences())
	{
		return false;
	}

	TSharedRef<const FInputChord> Chord = FBACommands::Get().ConnectUnlinkedPins->GetFirstValidChord();
	if (Key == Chord->Key)
	{
		if (ClosestPoint)
		{
			// const FBAScopedGraphAction Transaction(GraphHandler, "Smart Wire");
			// FBAUtils::TryCreateConnectionUnsafe(ClosestPoint->GetFromPinUnsafe(), ClosestPoint->GetToPinUnsafe(), EBABreakMethod::Default, true);
			TSharedPtr<FBAScopedGraphAction> Action = MakeShared<FBAScopedGraphAction>(GraphHandler, "Smart Wire Node");

			FBANodePinHandle HandleA = ClosestPoint->FromNodePinHandle;
			FBANodePinHandle HandleB = ClosestPoint->ToNodePinHandle;
			Action->GetSchema()->TryCreateConnection(HandleA, HandleB, EBABreakMethod::Default);

			UEdGraph* Graph = GraphHandler->GetFocusedEdGraph();
			if (UBASettings::GetFormatterSettings(Graph).GetAutoFormatting() != EBAAutoFormatting::Never)
			{
				FEdGraphFormatterParameters FormatterParams;
				if (UBASettings::GetFormatterSettings(Graph).GetAutoFormatting() == EBAAutoFormatting::FormatSingleConnected)
				{
					FormatterParams.LimitedNodes.Add(HandleA.GetNode());
					FormatterParams.LimitedNodes.Add(HandleB.GetNode());
				}

				GraphHandler->RequestFormatNode(HandleA.GetNode(), Action, FormatterParams);
			}
		}

		GraphHandler->EndOperation();
	}

	return false;
}

FPinLink* FBAGraphOperation_SmartWireCursor::GetClosestConnectionPoint(TSharedPtr<SGraphPanel> GraphPanel, float CullLimit)
{
	if (!GraphPanel || !GraphPanel->GetGraphObj())
	{
		return nullptr;
	}

	const FVector2D CursorPos = FBAUtils::ScreenSpaceToPanelCoord(GraphPanel, FSlateApplication::Get().GetCursorPos());

	FVector2D ClosestPos;
	float ClosestDist = CullLimit * CullLimit;
	FPinLink* Closest = nullptr;

	for (FPinLink& Link : Points)
	{
		if (!Link.HasBothPins())
		{
			continue;
		}

		FVector2D SourcePos = FBAUtils::GetPinPos(GraphPanel, Link.GetFromPinUnsafe());
		FVector2D TargetPos = FBAUtils::GetPinPos(GraphPanel, Link.GetToPinUnsafe());

		const FVector2D PinPos = SourcePos + (TargetPos - SourcePos) * 0.5f;

		const float Dist = FVector2D::DistSquared(PinPos, CursorPos);

		if (Closest && (PinPos - ClosestPos).IsNearlyZero())
		{
			// tiebreaker, pick the link which is more aligned with the vector to cursor
			FVector2D NewDir = (TargetPos - SourcePos).GetSafeNormal();
			FVector2D NewToCursor = (CursorPos - SourcePos).GetSafeNormal();

			FVector2D CurrSourcePos = FBAUtils::GetPinPos(GraphPanel, Closest->GetFromPinUnsafe());
			FVector2D CurrTargetPos = FBAUtils::GetPinPos(GraphPanel, Closest->GetToPinUnsafe());
			FVector2D CurrDir = (CurrTargetPos - CurrSourcePos).GetSafeNormal();
			FVector2D CurrToCursor = (CursorPos - CurrSourcePos).GetSafeNormal();

			float NewAlignment = FVector2D::DotProduct(NewDir, NewToCursor);
			float CurrAlignment = FVector2D::DotProduct(CurrDir, CurrToCursor);

			if (NewAlignment > CurrAlignment)
			{
				Closest = &Link;
			}
		}
		else if (Dist < ClosestDist)
		{
			if (!FBAUtils::IsNodeVisible(GraphPanel, Link.GetFromNode()))
			{
				continue;
			}

			if (!FBAUtils::IsNodeVisible(GraphPanel, Link.GetToNode()))
			{
				continue;
			}

			ClosestPos = PinPos;
			ClosestDist = Dist;
			Closest = &Link;
		}
	}

	return Closest;
}

void FBAGraphOperation_SmartWireCursor::BuildConnectionPoints()
{
	if (!GraphHandler->HasValidGraphReferences())
	{
		return;
	}

	Points.Empty();

	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();
	UEdGraph* Graph = GraphPanel->GetGraphObj();

	TArray<UEdGraphPin*> InputPins;
	TArray<UEdGraphPin*> OutputPins;

	TMap<UEdGraphPin*, FVector2D> Positions;

	const TArray<UEdGraphNode*>& Nodes = Graph->Nodes;
	for (UEdGraphNode* Node : Nodes)
	{
		if (!FBAUtils::IsNodeVisible(GraphPanel, Node))
		{
			continue;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!FBAUtils::IsPinVisible(GraphPanel, Pin))
			{
				continue;
			}

			if (Pin->HasAnyConnections())
			{
				continue;
			}

			TArray<UEdGraphPin*>& Array = Pin->Direction == EGPD_Input ? InputPins : OutputPins;
			Array.Add(Pin);

			Positions.Add(Pin, FBAUtils::GetPinPos(GraphHandler, Pin));
		}
	}

	UEdGraphNode* SelectedNode = GraphHandler->GetSelectedNode();
	if (SelectedNode)
	{
		for (UEdGraphPin* Pin : SelectedNode->Pins)
		{
			if (!FBAUtils::IsPinVisible(GraphPanel, Pin))
			{
				continue;
			}

			if (Pin->HasAnyConnections())
			{
				continue;
			}

			TArray<UEdGraphPin*>& OtherPins = Pin->Direction == EGPD_Input ? OutputPins : InputPins;
			for (UEdGraphPin* OtherPin : OtherPins)
			{
				if (OtherPin->GetOwningNode() == SelectedNode)
				{
					continue;
				}

				if (!FBAUtils::CanConnectPins(Pin, OtherPin, true, true))
				{
					continue;
				}

				auto Out = Pin->Direction == EGPD_Output ? Pin : OtherPin;
				auto In = Pin->Direction == EGPD_Output ? OtherPin : Pin;

				// skip looping pins
				FVector2D& OutPos = Positions.FindChecked(Out);
				FVector2D& InPos = Positions.FindChecked(In);
				if (OutPos.X > InPos.X)
				{
					continue;
				}

				Points.Emplace(Out, In);
			}
		}
	}
	else
	{
		for (UEdGraphPin* Out : OutputPins)
		{
			FVector2D& OutPos = Positions.FindChecked(Out);
			for (UEdGraphPin* In : InputPins)
			{
				FVector2D& InPos = Positions.FindChecked(In);

				// skip big looping pins
				if (OutPos.X > InPos.X)// + 500)
				{
					continue;
				}

				if (!FBAUtils::CanConnectPins(In, Out, true, true))
				{
					continue;
				}

				Points.Emplace(Out, In);
			}
		}
	}
}

void FBAGraphOperation_SmartWireCursor::DrawLink(FPinLink& Link, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId)
{
	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();

	bool bIsClosest = (&Link) == ClosestPoint;

	UEdGraphPin* SourcePin = Link.GetFromPinUnsafe();
	if (!SourcePin)
	{
		return;
	}

	TSharedPtr<SGraphPin> SourceGraphPin = FBAUtils::GetGraphPinFast(GraphPanel, SourcePin);
	if (!SourceGraphPin.IsValid())
	{
		return;
	}

	UEdGraphPin* TargetPin = Link.GetToPinUnsafe();
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
		Params.WireColor = bIsClosest ? FLinearColor::Green : FLinearColor(1, 1, 1, 0.66f);
		Params.WireThickness = bIsClosest ? 5.0f : 1.0f;
		Params.bDrawBubbles = bIsClosest;
		ConnectionDrawingPolicy->DrawSplineWithArrow(AdjustedStartPoint, AdjustedEndPoint, Params);

		// also draw the control points
		FVector2D ControlPoint = FBAUtils::GraphCoordToPanelCoord(GraphPanel, TargetGraphPos + (TargetGraphPos - SourceGraphPos) * 0.5f);
		TArray<FVector2D> LinePoints = { ControlPoint, ControlPoint };

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			FLinearColor::Yellow,
			true,
			10.0f);
	}
}

void FBAGraphOperation_SmartWireCursor::Tick(float DeltaTime)
{
	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();
	ClosestPoint = GetClosestConnectionPoint(GraphPanel, 500);
}

void FBAGraphOperation_SmartWireCursor::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled)
{
	for (FPinLink& Point : Points)
	{
		DrawLink(Point, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId);
	}
}
