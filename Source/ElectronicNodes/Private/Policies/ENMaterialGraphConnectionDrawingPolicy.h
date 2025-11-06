/* Copyright (C) 2024 Hugo ATTAL - All Rights Reserved
* This plugin is downloadable from the Unreal Engine Marketplace
*/

#pragma once

#include "CoreMinimal.h"
// 所有版本（包括 UE 5.6）都包含 .cpp 文件（UE 引擎的特殊做法）
#include "MaterialGraphConnectionDrawingPolicy.cpp"
#include "ENConnectionDrawingPolicy.h"

class FENMaterialGraphConnectionDrawingPolicy : public FMaterialGraphConnectionDrawingPolicy
{
public:
	FENMaterialGraphConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
		: FMaterialGraphConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj)
	{
		this->ConnectionDrawingPolicy = new FENConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj);
	}

	virtual void DrawConnection(int32 LayerId, const FVector2D& Start, const FVector2D& End, const FConnectionParams& Params) override
	{
		this->ConnectionDrawingPolicy->SetMousePosition(LocalMousePosition);
		this->ConnectionDrawingPolicy->DrawConnection(LayerId, Start, End, Params);
		SplineOverlapResult = FGraphSplineOverlapResult(this->ConnectionDrawingPolicy->SplineOverlapResult);
	}
	
	~FENMaterialGraphConnectionDrawingPolicy()
	{
		delete ConnectionDrawingPolicy;
	}

private:
	FENConnectionDrawingPolicy* ConnectionDrawingPolicy;
};
