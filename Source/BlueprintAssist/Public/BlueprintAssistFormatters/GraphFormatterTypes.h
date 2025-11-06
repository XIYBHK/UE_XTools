// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BlueprintAssistSettings.h"
#include "BlueprintAssistTypes.h"
#include "SGraphPin.h"
#include "EdGraph/EdGraphNode.h"

struct FFormatterInterface;
struct FBACommentContainsGraph;
class UEdGraphNode_Comment;
class FBAGraphHandler;
class UEdGraphPin;
class UEdGraphNode;
class FEdGraphFormatter;
class FSlateRect;
class UK2Node_Knot;

struct FEdGraphFormatterParameters
{
	TSharedPtr<EBAParameterFormattingStyle> OverrideFormattingStyle;

	FBANodeArray NodesToFormat;
	FBANodeArray IgnoredNodes;
	TWeakObjectPtr<UEdGraphNode> NodeToKeepStill;

	EBAAutoFormatting FormattingMethod;
	TSharedPtr<FBACommentContainsGraph> MasterContainsGraph;

	FEdGraphFormatterParameters()
	{
		Reset();
	}

	void Init();

	void Reset()
	{
		FormattingMethod = EBAAutoFormatting::FormatAllConnected;
		OverrideFormattingStyle = nullptr;
		NodesToFormat.Empty();
		IgnoredNodes.Empty();
		MasterContainsGraph.Reset();
		NodeToKeepStill.Reset();
	}
};

struct BLUEPRINTASSIST_API FPinLink
{
	UEdGraphPin* From;
	UEdGraphPin* To;

	UEdGraphNode* FallbackNode;

	FBAGraphPinHandle FromHandle;
	FBAGraphPinHandle ToHandle;

	FPinLink()
		: From(nullptr)
		, To(nullptr)
		, FallbackNode(nullptr)
		, FromHandle(nullptr)
		, ToHandle(nullptr)
	{
	}

	FPinLink(UEdGraphPin* InFrom, UEdGraphPin* InTo, UEdGraphNode* InFallbackNode = nullptr)
		: From(InFrom)
		, To(InTo)
		, FallbackNode(InFallbackNode)
		, FromHandle(InFrom)
		, ToHandle(InTo)
	{
	}

	bool operator==(const FPinLink& Other) const
	{
		return FromHandle == Other.FromHandle && ToHandle == Other.ToHandle;
	}

	bool operator!=(const FPinLink& Other) const
	{
		return !(*this == Other);
	}

	friend uint32 GetTypeHash(const FPinLink& Link)
	{
		return HashCombine(GetTypeHash(Link.FromHandle), GetTypeHash(Link.ToHandle));
	}

	UEdGraphPin* GetFromPin();
	UEdGraphPin* GetToPin();

	UEdGraphNode* GetFromNode();
	UEdGraphNode* GetToNode();

	UEdGraphNode* GetNode() const;

	UEdGraphPin* GetFromPinUnsafe() const { return From; }
	UEdGraphPin* GetToPinUnsafe() const { return To; }
	UEdGraphNode* GetFromNodeUnsafe() const { return (From == nullptr) ? nullptr : From->GetOwningNodeUnchecked(); }
	UEdGraphNode* GetToNodeUnsafe() const { return (To == nullptr) ? nullptr : To->GetOwningNodeUnchecked(); }

	bool HasBothPins();

	EEdGraphPinDirection GetDirection() const;

	FString ToString();
	FString ToStringConst() const;

	FPinLink MakeOppositeLink() const { return FPinLink(To, From); }

	bool IsLinked(bool bDirectional = true);
};


struct BLUEPRINTASSIST_API FNodeInfo
	: public TSharedFromThis<FNodeInfo>
{
	UEdGraphNode* Node = nullptr;
	UEdGraphPin* Pin = nullptr;
	TSharedPtr<FNodeInfo> Parent = nullptr;
	EEdGraphPinDirection Direction = EGPD_MAX;
	TSet<TSharedPtr<FNodeInfo>> Children;
	FPinLink Link;

	FNodeInfo(
		UEdGraphNode* InNode,
		UEdGraphPin* InPin,
		TSharedPtr<FNodeInfo> InParent,
		UEdGraphPin* InParentPin,
		EEdGraphPinDirection InDirection);

	FNodeInfo() { }

	void SetParent(TSharedPtr<FNodeInfo> NewParent, UEdGraphPin* MyPin);

	int32 GetChildX(
		UEdGraphNode* Child,
		TSharedPtr<FBAGraphHandler> GraphHandler,
		int32 PaddingX,
		EEdGraphPinDirection ChildDirection) const;

	bool DetectCycle(TSharedPtr<FNodeInfo> OtherInfo);

	TArray<UEdGraphNode*> GetAllChildNodes();

	void MoveChildren(
		TSharedPtr<FNodeInfo> Info,
		TSharedPtr<FBAGraphHandler> GraphHandler,
		int32 PaddingX,
		TSet<UEdGraphNode*>& TempVisited) const;

	UEdGraphNode* GetNode() const { return Node; }

	TSharedPtr<FNodeInfo> GetParent() const { return Parent; }
	UEdGraphNode* GetParentNode() const { return !Parent.IsValid() ? nullptr : Parent->Node; }

	FString ToString() const;

	TArray<UEdGraphNode*> GetChildNodes();
};

struct BLUEPRINTASSIST_API FFormatXInfo
	: public TSharedFromThis<FFormatXInfo>
{
	FPinLink Link;
	TSharedPtr<FFormatXInfo> Parent;
	UEdGraphNode* Node;
	bool bSameRowAsParent = false;
	bool bRootNode = false;
	TArray<TSharedPtr<FFormatXInfo>> Children;

	FFormatXInfo(UEdGraphNode* InNode) { Node = InNode; }
	FFormatXInfo(const FPinLink& InLink, TSharedPtr<FFormatXInfo> InParent);

	UEdGraphNode* GetNode() const;

	TArray<UEdGraphNode*> GetChildren(EEdGraphPinDirection Direction = EGPD_MAX, bool bInitialDirectionOnly = true) const;

	TArray<UEdGraphNode*> GetChildrenWithFilter(TFunctionRef<bool(TSharedPtr<FFormatXInfo>)> Filter, EEdGraphPinDirection Direction = EGPD_MAX, bool bInitialDirectionOnly = true) const;

	TArray<UEdGraphNode*> GetImmediateChildren() const;

	TArray<TSharedPtr<FFormatXInfo>>& GetChildInfos() { return Children; }
	TArray<TSharedPtr<FFormatXInfo>> GetAllChildren(EEdGraphPinDirection Direction = EGPD_MAX);
	TArray<TSharedPtr<FFormatXInfo>> GetAllChildrenWithFilter(TFunctionRef<bool(TSharedPtr<FFormatXInfo>)> Filter, EEdGraphPinDirection Direction = EGPD_MAX);

	TArray<FPinLink> GetChildrenAsLinks(EEdGraphPinDirection Direction = EGPD_MAX) const;

	void SetParent(TSharedPtr<FFormatXInfo> NewParent);
	void SetParentNew(TSharedPtr<FFormatXInfo> NewParent, FPinLink NewLink);

	TSharedPtr<FFormatXInfo> GetRootParent();

	FString ToString() const;
};

struct BLUEPRINTASSIST_API FNodeRelativeLocation
{
	UEdGraphNode* RootNode;
	float RelativeOffset;

	TArray<UEdGraphNode*> Children;
};

struct BLUEPRINTASSIST_API FNodeRelativeMapping
{
	FNodeRelativeMapping() = default;

	TMap<UEdGraphNode*, FNodeRelativeLocation> NodeRelativeXMap;
	TMap<UEdGraphNode*, FNodeRelativeLocation> NodeRelativeYMap;

	void UpdateRelativeY(UEdGraphNode* Node, UEdGraphNode* Root);

	void Reset()
	{
		NodeRelativeXMap.Empty();
		NodeRelativeYMap.Empty();
	}

	void DebugRelativeMapping() const;
};

// Check if the graph formatter has broken connections
struct BLUEPRINTASSIST_API FFormatterConnectionValidator
{
	TMap<UEdGraphNode*, TSet<FPinLink>> Connections;

	void Reset() { Connections.Reset(); }
	void CreateSnapshot(const TArray<UEdGraphNode*>& Nodes) { Connections = BuildConnections(Nodes); }
	TMap<UEdGraphNode*, TSet<FPinLink>> BuildConnections(const TArray<UEdGraphNode*>& Nodes);
	bool CheckChanged(const TArray<UEdGraphNode*>& Nodes);
};
