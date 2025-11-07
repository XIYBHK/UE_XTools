/* Copyright (C) 2024 Hugo ATTAL - All Rights Reserved
* This plugin is downloadable from the Unreal Engine Marketplace
*/

#pragma once

#include "CoreMinimal.h"

// UE 5.6 兼容性：MaterialGraphConnectionDrawingPolicy.cpp 在 UE 5.6 中可能不存在或路径改变
#if defined(ENGINE_MAJOR_VERSION) && defined(ENGINE_MINOR_VERSION) && ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 6
// UE 5.6: 暂时禁用 Material Graph 连接绘制增强（等待引擎 API 稳定）
#warning "ElectronicNodes: Material Graph connection drawing is disabled in UE 5.6 (MaterialGraphConnectionDrawingPolicy.cpp not found)"
#else
// UE 5.5-: 包含 .cpp 文件（UE 引擎的特殊做法）
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
#endif  // UE 5.5-
