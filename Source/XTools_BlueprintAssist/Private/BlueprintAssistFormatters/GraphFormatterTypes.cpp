// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistFormatters/GraphFormatterTypes.h"

#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistSettings.h"
#include "BlueprintAssistUtils.h"
#include "BlueprintAssistFormatters/EdGraphFormatter.h"
#include "EdGraphNode_Comment.h"
#include "BlueprintAssistMisc/BAMiscUtils.h"
#include "BlueprintAssistWidgets/BlueprintAssistGraphOverlay.h"
#include "EdGraph/EdGraphPin.h"
#include "Editor/BlueprintGraph/Classes/K2Node_Knot.h"
#include "Runtime/SlateCore/Public/Layout/SlateRect.h"

FNodeInfo::FNodeInfo(
	UEdGraphNode* InNode,
	UEdGraphPin* InPin,
	TSharedPtr<FNodeInfo> InParent,
	UEdGraphPin* InParentPin,
	const EEdGraphPinDirection InDirection)
	: Node(InNode)
	, Pin(InPin)
	, Direction(InDirection)
	, Children(TArray<TSharedPtr<FNodeInfo>>())
{
	Link = FPinLink(InParentPin, InPin);
}

void FNodeInfo::SetParent(TSharedPtr<FNodeInfo> NewParent, UEdGraphPin* MyPin)
{
	Pin = MyPin;

	if (Parent.IsValid())
	{
		Parent->Children.Remove(SharedThis(this));
	}

	if (NewParent.IsValid() && NewParent != Parent)
	{
		NewParent->Children.Add(SharedThis(this));
	}

	Parent = NewParent;
}

int32 FNodeInfo::GetChildX(
	UEdGraphNode* Child,
	TSharedPtr<FBAGraphHandler> GraphHandler,
	int32 PaddingX,
	const EEdGraphPinDirection ChildDirection) const
{
	const FSlateRect MyBounds = FBAUtils::GetCachedNodeBounds(GraphHandler, Node);

	const float ChildWidth = FBAUtils::GetCachedNodeBounds(GraphHandler, Child).GetSize().X;

	const float NewNodePos
		= ChildDirection == EGPD_Input
		? MyBounds.Left - ChildWidth - PaddingX - 1
		: MyBounds.Right + PaddingX + 1;

	return FMath::RoundToInt(NewNodePos);
}

bool FNodeInfo::DetectCycle(TSharedPtr<FNodeInfo> OtherInfo)
{
	TArray<TSharedPtr<FNodeInfo>> PendingInfos;
	PendingInfos.Add(OtherInfo);

	while (PendingInfos.Num() > 0)
	{
		TSharedPtr<FNodeInfo> NextInfo = PendingInfos.Pop();
		for (TSharedPtr<FNodeInfo> Child : NextInfo->Children)
		{
			if (Child == SharedThis(this))
			{
				return true;
			}

			PendingInfos.Add(Child);
		}
	}

	return false;
}

TArray<UEdGraphNode*> FNodeInfo::GetAllChildNodes()
{
	TArray<UEdGraphNode*> OutChildren;

	// const auto& FilterByDirection = [Direction](TSharedPtr<FFormatXInfo> Info)
	// {
	// 	return Info->Link.GetDirection() == Direction || Direction == EGPD_MAX;
	// };
	TArray<TSharedPtr<FNodeInfo>> PendingInfo = Children.Array();

	while (PendingInfo.Num() > 0)
	{
		TSharedPtr<FNodeInfo> CurrentInfo = PendingInfo.Pop();
		if (OutChildren.Contains(CurrentInfo->GetNode()))
		{
			break;
		}

		OutChildren.Push(CurrentInfo->GetNode());

		for (TSharedPtr<FNodeInfo> Info : CurrentInfo->Children)
		{
			PendingInfo.Push(Info);
		}
	}

	return OutChildren;
}

void FNodeInfo::MoveChildren(
	TSharedPtr<FNodeInfo> Info,
	TSharedPtr<FBAGraphHandler> GraphHandler,
	int32 PaddingX,
	TSet<UEdGraphNode*>& TempVisited) const
{
	for (TSharedPtr<FNodeInfo> Child : Info->Children)
	{
		if (TempVisited.Contains(Child->Node))
		{
			continue;
		}

		TempVisited.Add(Child->Node);

		Child->Node->NodePosX = Info->GetChildX(Child->Node, GraphHandler, PaddingX, Child->Direction);
		MoveChildren(Child, GraphHandler, PaddingX, TempVisited);
	}
}

FString FNodeInfo::ToString() const
{
	UEdGraphNode* ParentNode = Parent.IsValid() ? Parent->Node : nullptr;

	return FString::Printf(
		TEXT("NodeInfo <%s> | Par <%s>"),
		*Node->GetNodeTitle(ENodeTitleType::ListView).ToString(),
		ParentNode ? *FBAUtils::GetNodeName(ParentNode) : *FString("null")
	);
}

TArray<UEdGraphNode*> FNodeInfo::GetChildNodes()
{
	TArray<UEdGraphNode*> ChildNodes;
	for (TSharedPtr<FNodeInfo> Info : Children)
	{
		ChildNodes.Emplace(Info->Node);
	}

	return ChildNodes;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

void FEdGraphFormatterParameters::Init()
{
	NodesToFormat.CacheNodes();
	IgnoredNodes.CacheNodes();
}

UEdGraphPin* FPinLink::GetFromPin()
{
	return FromHandle.GetPin();
}

UEdGraphPin* FPinLink::GetToPin()
{
	return ToHandle.GetPin();
}

UEdGraphNode* FPinLink::GetFromNode()
{
	if (UEdGraphPin* Pin = GetFromPin())
	{
		return Pin->GetOwningNodeUnchecked();
	}

	return nullptr;
}

UEdGraphNode* FPinLink::GetToNode()
{
	if (UEdGraphPin* Pin = GetToPin())
	{
		return Pin->GetOwningNodeUnchecked();
	}

	return nullptr;
}

UEdGraphNode* FPinLink::GetNode() const
{
	return To == nullptr ? FallbackNode : To->GetOwningNodeUnchecked();
}

bool FPinLink::HasBothPins()
{
	return GetFromPin() != nullptr && GetToPin() != nullptr;
}

EEdGraphPinDirection FPinLink::GetDirection() const
{
	return From != nullptr ? From->Direction.GetValue() : EGPD_Output;
}

FString FPinLink::ToString()
{
	return FString::Printf(
		TEXT("%s: \"%s\" > \"%s\""),
		*(GetDirection() == EGPD_Input ? FString("In") : FString("Out")),
		*FBAUtils::GetPinName(GetFromPin(), true),
		*FBAUtils::GetPinName(GetToPin(), true)
	);
}

FString FPinLink::ToStringConst() const
{
	UEdGraphNode* ParentNode = From == nullptr ? nullptr : From->GetOwningNodeUnchecked();
	return FString::Printf(
		TEXT("%s: <%s> to <%s> | Pin <%s> to <%s> "),
		*(GetDirection() == EGPD_Input ? FString("In") : FString("Out")),
		*FBAUtils::GetNodeName(ParentNode),
		*FBAUtils::GetNodeName(GetNode()),
		*FBAUtils::GetPinName(From),
		*FBAUtils::GetPinName(To)
	);
}

bool FPinLink::IsLinked(bool bDirectional)
{
	UEdGraphPin* FromPin = GetFromPin();
	UEdGraphPin* ToPin = GetToPin();

	if (!FromPin || !ToPin)
	{
		return false;
	}

	if (FromPin->LinkedTo.Contains(ToPin))
	{
		return true;
	}

	if (!bDirectional)
	{
		return ToPin->LinkedTo.Contains(FromPin);
	}

	return false;
}

FFormatXInfo::FFormatXInfo(const FPinLink& InLink, TSharedPtr<FFormatXInfo> InParent)
	: Link(InLink)
	, Parent(InParent) {}

UEdGraphNode* FFormatXInfo::GetNode() const
{
	return Link.GetNode();
}

void FFormatXInfo::SetParent(TSharedPtr<FFormatXInfo> NewParent)
{
	if (Parent.IsValid())
	{
		Parent->Children.Remove(AsShared());
	}

	if (NewParent.IsValid())
	{
		//const FString OldParent
		//	= Parent != nullptr
		//	? *FBlueprintAssistUtils::GetNodeName(Parent->GetNode())
		//	: FString("None");
		//UE_LOG(
		//	LogBlueprintAssist,
		//	Warning,
		//	TEXT("\tSet Parent for %s to %s (Old : %s)"),
		//	*FBlueprintAssistUtils::GetNodeName(GetNode()),
		//	*FBlueprintAssistUtils::GetNodeName(NewParent->GetNode()),
		//	*OldParent);

		NewParent->Children.Add(AsShared());
	}

	Parent = NewParent;
}

void FFormatXInfo::SetParentNew(TSharedPtr<FFormatXInfo> NewParent, FPinLink NewLink)
{
	SetParent(NewParent);
	Link = NewLink;
}

TArray<UEdGraphNode*> FFormatXInfo::GetChildren(EEdGraphPinDirection Direction, bool bInitialDirectionOnly) const
{
	TArray<UEdGraphNode*> OutChildren;

	const auto& FilterByDirection = [Direction](TSharedPtr<FFormatXInfo> Info)
	{
		return Info->Link.GetDirection() == Direction || Direction == EGPD_MAX;
	};
	TArray<TSharedPtr<FFormatXInfo>> PendingInfo = Children.FilterByPredicate(FilterByDirection);

	while (PendingInfo.Num() > 0)
	{
		TSharedPtr<FFormatXInfo> CurrentInfo = PendingInfo.Pop();
		if (OutChildren.Contains(CurrentInfo->GetNode()))
		{
			continue;
		}

		OutChildren.Push(CurrentInfo->GetNode());

		for (TSharedPtr<FFormatXInfo> Info : bInitialDirectionOnly ? CurrentInfo->Children : CurrentInfo->Children.FilterByPredicate(FilterByDirection))
		{
			PendingInfo.Push(Info);
		}
	}

	return OutChildren;
}

TArray<UEdGraphNode*> FFormatXInfo::GetChildrenWithFilter(TFunctionRef<bool(TSharedPtr<FFormatXInfo>)> Filter, EEdGraphPinDirection Direction, bool bInitialDirectionOnly) const
{
	TArray<UEdGraphNode*> OutChildren;

	const auto& FilterByDirection = [Direction](TSharedPtr<FFormatXInfo> Info)
	{
		return Info->Link.GetDirection() == Direction || Direction == EGPD_MAX;
	};
	TArray<TSharedPtr<FFormatXInfo>> PendingInfo = Children.FilterByPredicate(FilterByDirection);

	while (PendingInfo.Num() > 0)
	{
		TSharedPtr<FFormatXInfo> CurrentInfo = PendingInfo.Pop();
		if (OutChildren.Contains(CurrentInfo->GetNode()))
		{
			continue;
		}

		if (Filter(CurrentInfo))
		{
			continue;
		}

		OutChildren.Push(CurrentInfo->GetNode());

		for (TSharedPtr<FFormatXInfo> Info : bInitialDirectionOnly ? CurrentInfo->Children : CurrentInfo->Children.FilterByPredicate(FilterByDirection))
		{
			PendingInfo.Push(Info);
		}
	}

	return OutChildren;
}

TArray<UEdGraphNode*> FFormatXInfo::GetImmediateChildren() const
{
	TArray<UEdGraphNode*> OutChildren;
	for (TSharedPtr<FFormatXInfo> Child : Children)
	{
		OutChildren.Add(Child->GetNode());
	}
	return OutChildren;
}

TArray<TSharedPtr<FFormatXInfo>> FFormatXInfo::GetAllChildren(EEdGraphPinDirection Direction)
{
	TSet<TSharedPtr<FFormatXInfo>> OutChildren;

	const auto& FilterByDirection = [Direction](TSharedPtr<FFormatXInfo> Info)
	{
		return Info->Link.GetDirection() == Direction || Direction == EGPD_MAX;
	};

	TArray<TSharedPtr<FFormatXInfo>> PendingInfo = Children.FilterByPredicate(FilterByDirection);

	while (PendingInfo.Num() > 0)
	{
		TSharedPtr<FFormatXInfo> CurrentInfo = PendingInfo.Pop();
		if (!FilterByDirection(CurrentInfo))
		{
			continue;
		}

		if (OutChildren.Contains(CurrentInfo))
		{
			UE_LOG(LogBlueprintAssist, Error, TEXT("FFormatXInfo::GetAllChildren - Detected child loop"));
			continue;
		}

		OutChildren.Add(CurrentInfo);
		PendingInfo.Append(CurrentInfo->GetChildInfos());
	}

	return OutChildren.Array();
}

TArray<TSharedPtr<FFormatXInfo>> FFormatXInfo::GetAllChildrenWithFilter(TFunctionRef<bool(TSharedPtr<FFormatXInfo>)> Filter, EEdGraphPinDirection Direction)
{
	TSet<TSharedPtr<FFormatXInfo>> OutChildren;

	const auto& FilterByDirection = [Direction](TSharedPtr<FFormatXInfo> Info)
	{
		return Info->Link.GetDirection() == Direction || Direction == EGPD_MAX;
	};

	TArray<TSharedPtr<FFormatXInfo>> PendingInfo = Children.FilterByPredicate(FilterByDirection);

	while (PendingInfo.Num() > 0)
	{
		TSharedPtr<FFormatXInfo> CurrentInfo = PendingInfo.Pop();
		if (!FilterByDirection(CurrentInfo))
		{
			continue;
		}

		if (!Filter(CurrentInfo))
		{
			continue;
		}

		if (OutChildren.Contains(CurrentInfo))
		{
			UE_LOG(LogTemp, Error, TEXT("DETECTED CHILDREN LOOP"));
			continue;
		}

		OutChildren.Add(CurrentInfo);
		PendingInfo.Append(CurrentInfo->GetChildInfos());
	}

	return OutChildren.Array();
}

TArray<FPinLink> FFormatXInfo::GetChildrenAsLinks(EEdGraphPinDirection Direction) const
{
	TArray<FPinLink> OutLinks;
	for (TSharedPtr<FFormatXInfo> Child : Children)
	{
		if (Child->Link.GetDirection() == Direction)
		{
			OutLinks.Add(Child->Link);
		}
	}
	return OutLinks;
}

TSharedPtr<FFormatXInfo> FFormatXInfo::GetRootParent()
{
	TSet<TSharedPtr<FFormatXInfo>> Visited;
	TSharedPtr<FFormatXInfo> Next = SharedThis(this);
	while (Next->Parent.IsValid())
	{
		if (Visited.Contains(Next))
		{
			return nullptr;
		}
		Visited.Add(Next);

		Next = Next->Parent;
	}

	return Next;
}

FString FFormatXInfo::ToString() const
{
	return Link.ToStringConst();
}

void FNodeRelativeMapping::UpdateRelativeY(UEdGraphNode* Node, UEdGraphNode* Root)
{
	FNodeRelativeLocation& NodeRelative = NodeRelativeYMap.FindOrAdd(Node);
	FNodeRelativeLocation& RootRelative = NodeRelativeYMap.FindOrAdd(Root);

	// update the old relative info
	if (NodeRelative.RootNode)
	{
		if (auto OldRelative = NodeRelativeYMap.Find(NodeRelative.RootNode))
		{
			OldRelative->Children.Remove(Node);
		}
	}

	NodeRelative.RootNode = Root;
	NodeRelative.RelativeOffset = Root->NodePosY - Node->NodePosY;

	RootRelative.Children.Add(Node);
}

void FNodeRelativeMapping::DebugRelativeMapping() const
{
	for (TTuple<UEdGraphNode*, FNodeRelativeLocation> RelativeYMap : NodeRelativeYMap)
	{
		UE_LOG(LogTemp, Warning, TEXT("PARENT %s"), *FBAUtils::GetNodeName(RelativeYMap.Key));

		for (UEdGraphNode* Child : RelativeYMap.Value.Children)
		{
			UE_LOG(LogTemp, Warning, TEXT("\tCHILD %s"), *FBAUtils::GetNodeName(Child));

			// draw line between parent and child
			if (auto GraphHandler = FBAUtils::GetCurrentGraphHandler())
			{
				if (auto Overlay = GraphHandler->GetGraphOverlay())
				{
					Overlay->DrawLine(
						FBAUtils::GetNodePosition(RelativeYMap.Key),
						FBAUtils::GetNodePosition(Child),
						FLinearColor::MakeRandomColor());
				}
			}
		}
	}
}

TMap<UEdGraphNode*, TSet<FPinLink>> FFormatterConnectionValidator::BuildConnections(const TArray<UEdGraphNode*>& Nodes)
{
	TMap<UEdGraphNode*, TSet<FPinLink>> ConnectionsMap;
	for (UEdGraphNode* Node : Nodes)
	{
		ConnectionsMap.Add(Node, TSet<FPinLink>(FBAUtils::GetPinLinksIgnoringKnots(Node)));
	}

	return ConnectionsMap;
}

bool FFormatterConnectionValidator::CheckChanged(const TArray<UEdGraphNode*>& Nodes)
{
	TMap<UEdGraphNode*, TSet<FPinLink>> NewConnections = BuildConnections(Nodes);

	TSet<UEdGraphNode*> NewNodes;
	NewConnections.GetKeys(NewNodes);

	TSet<UEdGraphNode*> OldNodes;
	Connections.GetKeys(OldNodes);

	bool bChanged = false;

	TSet<UEdGraphNode*> DifferentNodes = NewNodes.Difference(OldNodes);
	if (DifferentNodes.Num())
	{
		for (UEdGraphNode* Node : DifferentNodes)
		{
			UE_LOG(LogBlueprintAssist, Error, TEXT("Formatting has caused node to be removed or added: %s"), *FBAUtils::GetNodeName(Node));
		}

		bChanged = true;
	}

	// check connections
	for (auto& Node : OldNodes)
	{
		TSet<FPinLink>& OldLinks = Connections[Node];
		TSet<FPinLink>& NewLinks = NewConnections[Node];

		TSet<FPinLink> DifferentLinks = OldLinks.Difference(NewLinks);
		if (DifferentLinks.Num())
		{
			for (FPinLink& Difference : OldLinks.Difference(NewLinks))
			{
				UE_LOG(LogBlueprintAssist, Error, TEXT("Formatting has caused node connections to change %s"), *Difference.ToString());
			}

			bChanged = true;
		}
	}

	if (bChanged)
	{
		FBAMiscUtils::MessageLogError(INVTEXT("Formatting has caused node connections to change, more details in output log. Please report this issue to github!"));
	}

	return bChanged;
}
