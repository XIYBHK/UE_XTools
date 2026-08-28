/* Copyright (C) 2024 Hugo ATTAL - All Rights Reserved
* This plugin is downloadable from the Unreal Engine Marketplace
*/

#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"
#include "ConnectionDrawingPolicy.h"
#include "Runtime/Launch/Resources/Version.h"
#include "../Public/ElectronicNodesSettings.h"

#include "BlueprintConnectionDrawingPolicy.h"

class FENPathDrawer;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
using FENGraphVector2D = FVector2f;
#else
using FENGraphVector2D = FVector2D;
#endif

struct ENRibbonConnection
{
	float Main;
	float Sub;
	bool Horizontal;
	float Start;
	float End;
	int32 Depth = 0;

	ENRibbonConnection(float Main, float Sub, bool Horizontal, float Start, float End, int32 Depth = 0)
	{
		this->Main = Main;
		this->Sub = Sub;
		this->Horizontal = Horizontal;
		this->Start = Start;
		this->End = End;
		this->Depth = Depth;
	}
};

struct ENCrossingConnection
{
	FVector2D Start;
	FVector2D End;

	ENCrossingConnection(const FVector2D& InStart, const FVector2D& InEnd)
		: Start(InStart), End(InEnd)
	{
	}
};

struct FENConnectionDrawingPolicyFactory : public FGraphPanelPinConnectionFactory
{
	virtual ~FENConnectionDrawingPolicyFactory()
	{
	}

	virtual class FConnectionDrawingPolicy* CreateConnectionPolicy(const class UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const class FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const override;
};

class FENConnectionDrawingPolicy : public FKismetConnectionDrawingPolicy
{
public:
	FENConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj, bool IsTree = false)
		: FKismetConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj), IsTree(IsTree)
	{
	}

	virtual void DrawConnection(int32 LayerId, const FENGraphVector2D& Start, const FENGraphVector2D& End, const FConnectionParams& Params) override;

	void ENComputeClosestPoint(const FVector2D& Start, const FVector2D& End);
	void ENComputeClosestPointDefault(const FVector2D& Start, const FVector2D& StartTangent, const FVector2D& End, const FVector2D& EndTangent);
	void ENDrawBubbles(const FVector2D& Start, const FVector2D& StartTangent, const FVector2D& End, const FVector2D& EndTangent);
	void ENDrawArrow(const FVector2D& Start, const FVector2D& End);

	void DrawDebugPoint(const FVector2D& Position, FLinearColor Color);

	TArray<ENCrossingConnection> CrossingConnections;

private:
	void DrawConnectionInternal(int32 LayerId, const FVector2D& Start, const FVector2D& End, const FConnectionParams& Params);

	const UElectronicNodesSettings& ElectronicNodesSettings = *GetDefault<UElectronicNodesSettings>();
	bool ReversePins;
	float MinXOffset;
	float ClosestDistanceSquared;
	FVector2D ClosestPoint;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
	bool bSliceLineIntersection = false;
#endif
	TArray<ENRibbonConnection> RibbonConnections;
	TMap<FVector2D, int> PinsOffset;

	bool IsTree = false;

	void ENCorrectZoomDisplacement(FVector2D& Start, FVector2D& End) const;
	void ENProcessRibbon(FENPathDrawer* PathDrawer, FVector2D& Start, FVector2D& StartDirection, FVector2D& End, FVector2D& EndDirection, const FConnectionParams& Params);
	bool ENIsRightPriority(const FConnectionParams& Params);
	int32 ENGetZoomLevel();
	int8 ENGetPinMembersCount(const UEdGraphPin* Pin);
	void ENDrawMainWire(FENPathDrawer* PathDrawer, EWireStyle WireStyle, FVector2D& Start, FVector2D& StartDirection, FVector2D& End, FVector2D& EndDirection, const FConnectionParams& Params);

	TSharedPtr<SGraphPanel> GetGraphPanel();
	void BuildRelatedNodes(UEdGraphNode* Node, TArray<UEdGraphNode*>& RelatedNodes, bool InputCheck, bool OutputCheck);

	int32 _LayerId;
	const FConnectionParams* _Params;
};
