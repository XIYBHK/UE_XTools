// Copyright fpwong. All Rights Reserved.


#include "BAGraphHandler/BAGraphOperation_SmartWireNode.h"

#include "BlueprintAssistCommands.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistSettings_EditorFeatures.h"
#include "BlueprintAssistUtils.h"
#include "InputCoreTypes.h"
#include "NodeFactory.h"
#include "SGraphPanel.h"
#include "BlueprintAssistMisc/BAGraphSchema.h"
#include "BlueprintAssistMisc/IBAInputState.h"
#include "EdGraph/EdGraph.h"
#include "Framework/Application/SlateApplication.h"

FBAGraphOperation_SmartWireNode::FBAGraphOperation_SmartWireNode(TSharedPtr<FBAGraphHandler> InGraphHandler)
	: FBAGraphOperation(InGraphHandler)
{
	SelectedNode = GraphHandler->GetSelectedNode();
	BuildConnectionPoints();
}

bool FBAGraphOperation_SmartWireNode::OnKeyUp(const FKey& Key)
{
	if (!GraphHandler->HasValidGraphReferences())
	{
		return false;
	}

	if (!SelectedNode.IsValid())
	{
		return false;
	}

	TSharedRef<const FInputChord> Chord = FBACommands::Get().ConnectUnlinkedPins->GetFirstValidChord();
	if (Key == Chord->Key)
	{
		if (BestConnection)
		{
			TSharedPtr<FBAScopedGraphAction> Action = MakeShared<FBAScopedGraphAction>(GraphHandler, "Smart Wire Node");

			FBANodePinHandle HandleA = BestConnection->FromNodePinHandle;
			FBANodePinHandle HandleB = BestConnection->ToNodePinHandle;

			bool bAllowConversion = IBAInputState::Get().IsKeyDown(UBASettings_EditorFeatures::Get().AllowConversionConnectionsModifier);
			Action->GetSchema()->TryCreateConnection(HandleA, HandleB, EBABreakMethod::Default, bAllowConversion);

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
			else
			{
				Action.Reset();
			}
		}

		GraphHandler->EndOperation();
	}

	return false;
}


float CalcScore(const FVector2D& ToTarget, float DistLimit)
{
	const float DistSq = ToTarget.SizeSquared();
	if (DistSq < KINDA_SMALL_NUMBER)
	{
		return 1.0;
	}

	// float XSq = ToTarget.X * ToTarget.X;
	// float AlignmentScore = XSq / DistSq;

	float ScaleSq = DistLimit * DistLimit;
	float DistanceScore = 1.0 / (1.0 + (DistSq / ScaleSq));

	// constexpr float AlignmentWeight = 0.05;
	// constexpr float DistWeight = 1.0 - AlignmentWeight;
	// return (AlignmentScore * AlignmentWeight) + (DistanceScore * DistWeight);
	return DistanceScore;
}

FPinLink* FBAGraphOperation_SmartWireNode::FindBestConnection(TSharedPtr<SGraphPanel> GraphPanel, float CullLimit)
{
	if (!GraphPanel || !GraphPanel->GetGraphObj())
	{
		return nullptr;
	}

	float BestScore = 0;
	FPinLink* Best = nullptr;

	const auto ScoreItems = [&](TArray<FPinLink>& Items)
	{
		for (FPinLink& Link : Items)
		{
			if (!Link.HasBothPins())
			{
				continue;
			}

			FVector2D OutPos = FBAUtils::GetPinPos(GraphPanel, Link.GetFromPinUnsafe());
			FVector2D InPos = FBAUtils::GetPinPos(GraphPanel, Link.GetToPinUnsafe());

			if (OutPos.X > InPos.X)
			{
				continue;
			}

			// dist cull
			float Dist = FVector2D::DistSquared(InPos, OutPos);
			if (Dist > CullLimit * CullLimit)
			{
				continue;
			}

			FVector2D VectorToPin = InPos - OutPos;

			// skip connections that are nearly vertical
			FVector2D Dir = VectorToPin.GetSafeNormal();
			if (Dir.X < 0.1 && Dist > 200 * 200)
			{
				continue;
			}

			float Score = CalcScore(VectorToPin, CullLimit);

			if (Score > BestScore)
			{
				BestScore = Score;
				Best = &Link;
			}
		}
	};

	ScoreItems(AllConnections);

	const bool bAllowConversion = IBAInputState::Get().IsKeyDown(UBASettings_EditorFeatures::Get().AllowConversionConnectionsModifier);
	if (bAllowConversion)
	{
		ScoreItems(ConversionConnections);
	}

	return Best;
}

void FBAGraphOperation_SmartWireNode::BuildConnectionPoints()
{
	if (!GraphHandler->HasValidGraphReferences())
	{
		return;
	}

	AllConnections.Empty();

	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();
	UEdGraph* Graph = GraphPanel->GetGraphObj();
	const UBAGraphSchema& BASchema = UBAGraphSchema::Get(Graph);

	bool bIsNiagaraGraph = FBAUtils::IsNiagaraGraph(Graph);
	const auto IsValidPinType = [bIsNiagaraGraph](UEdGraphPin* Pin)
	{
		// skip niagara misc pins
		static const FName NiagaraMiscPinType("Misc");
		if (bIsNiagaraGraph && Pin->PinType.PinCategory == NiagaraMiscPinType)
		{
			return false;
		}

		return true;
	};

	TArray<UEdGraphPin*> InputPins;
	TArray<UEdGraphPin*> OutputPins;

	TMap<UEdGraphPin*, FVector2D> Positions;

	const TArray<UEdGraphNode*>& Nodes = Graph->Nodes;
	for (UEdGraphNode* Node : Nodes)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!FBAUtils::IsPinVisible(GraphPanel, Pin))
			{
				continue;
			}

			if (!IsValidPinType(Pin))
			{
				continue;
			}

			TArray<UEdGraphPin*>& Array = Pin->Direction == EGPD_Input ? InputPins : OutputPins;
			Array.Add(Pin);

			Positions.Add(Pin, FBAUtils::GetPinPos(GraphHandler, Pin));
		}
	}

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

		if (!IsValidPinType(Pin))
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

			if (!FBAUtils::CanConnectPins(Pin, OtherPin, false, true))
			{
				continue;
			}

			UEdGraphPin* Out = Pin->Direction == EGPD_Output ? Pin : OtherPin;
			UEdGraphPin* In = Pin->Direction == EGPD_Output ? OtherPin : Pin;

			if (BASchema.GetConnectionResponse(Pin, OtherPin).Response == CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE)
			{
				ConversionConnections.Emplace(Out, In);
			}
			else
			{
				AllConnections.Emplace(Out, In);
			}
		}
	}
}

void FBAGraphOperation_SmartWireNode::DrawLink(FPinLink& Link, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId)
{
	if (!BestConnection)
	{
		return;
	}

	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();

	bool bIsClosest = (&Link) == BestConnection;

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

void FBAGraphOperation_SmartWireNode::Tick(float DeltaTime)
{
	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();
	BestConnection = FindBestConnection(GraphPanel, 1250);
}

void FBAGraphOperation_SmartWireNode::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled)
{
	if (BestConnection)
	{
		DrawLink(*BestConnection, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId);
	}
}
