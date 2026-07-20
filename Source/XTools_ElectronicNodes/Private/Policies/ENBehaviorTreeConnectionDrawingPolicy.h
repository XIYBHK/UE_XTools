/* Copyright (C) 2024 Hugo ATTAL - All Rights Reserved
* This plugin is downloadable from the Unreal Engine Marketplace
*/

#pragma once

#include "CoreMinimal.h"
#include "ENConnectionDrawingPolicy.h"
#include "BehaviorTreeConnectionDrawingPolicy.h"

class FENBehaviorTreeConnectionDrawingPolicy : public FBehaviorTreeConnectionDrawingPolicy
{
public:
	FENBehaviorTreeConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
		: FBehaviorTreeConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj)
	{
		this->ConnectionDrawingPolicy = new FENConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj, true);
	}

	virtual void DrawConnection(int32 LayerId, const FENGraphVector2D& Start, const FENGraphVector2D& End, const FConnectionParams& Params) override
	{
		this->ConnectionDrawingPolicy->SetMousePosition(LocalMousePosition);
		this->ConnectionDrawingPolicy->DrawConnection(LayerId, Start, End, Params);
		SplineOverlapResult = FGraphSplineOverlapResult(this->ConnectionDrawingPolicy->SplineOverlapResult);
	}

	virtual void DrawSplineWithArrow(const FGeometry& StartGeom, const FGeometry& EndGeom, const FConnectionParams& Params) override
	{
		const FENGraphVector2D StartGeomDrawSize = StartGeom.GetDrawSize();
		const FENGraphVector2D StartCenter = FENGraphVector2D(
			StartGeom.AbsolutePosition.X + StartGeomDrawSize.X / 2,
			StartGeom.AbsolutePosition.Y + StartGeomDrawSize.Y);

		const FENGraphVector2D EndGeomDrawSize = EndGeom.GetDrawSize();
		const FENGraphVector2D EndCenter = FENGraphVector2D(
			EndGeom.AbsolutePosition.X + EndGeomDrawSize.X / 2,
			EndGeom.AbsolutePosition.Y);

		DrawSplineWithArrow(StartCenter, EndCenter, Params);
	}

	virtual void DrawPreviewConnector(const FGeometry& PinGeometry, const FENGraphVector2D& StartPoint, const FENGraphVector2D& EndPoint, UEdGraphPin* Pin) override
	{
		FConnectionParams Params;
		DetermineWiringStyle(Pin, nullptr, /*inout*/ Params);

		if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
		{
			const FENGraphVector2D GeomDrawSize = PinGeometry.GetDrawSize();
			const FENGraphVector2D Center = FENGraphVector2D(
				PinGeometry.AbsolutePosition.X + GeomDrawSize.X / 2,
				PinGeometry.AbsolutePosition.Y + GeomDrawSize.Y);

			DrawSplineWithArrow(Center, EndPoint, Params);
		}
		else
		{
			const FENGraphVector2D GeomDrawSize = PinGeometry.GetDrawSize();
			const FENGraphVector2D Center = FENGraphVector2D(
				PinGeometry.AbsolutePosition.X + GeomDrawSize.X / 2,
				PinGeometry.AbsolutePosition.Y);

			DrawSplineWithArrow(StartPoint, Center, Params);
		}
	}

	virtual void DrawSplineWithArrow(const FENGraphVector2D& StartAnchorPoint, const FENGraphVector2D& EndAnchorPoint, const FConnectionParams& Params) override
	{
		// bUserFlag1 indicates that we need to reverse the direction of connection (used by debugger)
		const FENGraphVector2D& P0 = Params.bUserFlag1 ? EndAnchorPoint : StartAnchorPoint;
		const FENGraphVector2D& P1 = Params.bUserFlag1 ? StartAnchorPoint : EndAnchorPoint;

		Internal_DrawLineWithStraightArrow(P0, P1, Params);
	}

	void Internal_DrawLineWithStraightArrow(const FENGraphVector2D& StartAnchorPoint, const FENGraphVector2D& EndAnchorPoint, const FConnectionParams& Params)
	{
		DrawConnection(WireLayerID, StartAnchorPoint, EndAnchorPoint, Params);

		const FENGraphVector2D ArrowDrawPos = EndAnchorPoint - ArrowRadius;

		FSlateDrawElement::MakeRotatedBox(
			DrawElementsList,
			ArrowLayerID,
			FPaintGeometry(ArrowDrawPos, ArrowImage->ImageSize * ZoomFactor, ZoomFactor),
			ArrowImage,
			ESlateDrawEffect::None,
			HALF_PI,
			TOptional<FENGraphVector2D>(),
			FSlateDrawElement::RelativeToElement,
			Params.WireColor
		);
	}

	~FENBehaviorTreeConnectionDrawingPolicy()
	{
		delete ConnectionDrawingPolicy;
	}

private:
	FENConnectionDrawingPolicy* ConnectionDrawingPolicy;
};
