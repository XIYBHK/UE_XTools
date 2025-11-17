#include "BlueprintAssistTypes.h"

#include "BlueprintAssistSettings.h"
#include "BlueprintAssistSettings_EditorFeatures.h"
#include "BlueprintAssistUtils.h"
#include "BlueprintAssistMisc/BAMiscUtils.h"
#include "EdGraph/EdGraph.h"

void FBAGraphPinHandle::SetPin(UEdGraphPin* Pin)
{
	if (!FBAUtils::IsValidPin(Pin))
	{
		Invalidate();
	}
	else if (UEdGraphNode* Node = Pin->GetOwningNodeUnchecked())
	{
		Graph = Node->GetGraph();
		NodeGuid = Node->NodeGuid;
		PinId = Pin->PinId;
		PinType = Pin->PinType;
		PinName = Pin->PinName;
	}
}

UEdGraphPin* FBAGraphPinHandle::GetPin(bool bFallbackOnPinName)
{
	if (!IsValid())
	{
		return nullptr;
	}

	for (auto Node : Graph->Nodes)
	{
		if (Node->NodeGuid == NodeGuid)
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->PinId == PinId)
				{
					return Pin;
				}
			}

			if (bFallbackOnPinName)
			{
				// guid failed, find using PinType & PinName
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if ((Pin->PinType == PinType) && (Pin->PinName == PinName))
					{
						// side effect: also update the latest PinId
						PinId = Pin->PinId;

						return Pin;
					}
				}
			}

			return nullptr;
		}
	}

	return nullptr;
}

FString FBAGraphPinHandle::ToString() const
{
	return FString::Printf(TEXT("N[%s] P[%s]Name[%s]"), *NodeGuid.ToString(), *PinId.ToString(), *PinName.ToString()); 
}

void FBANodePinHandle::SetPin(UEdGraphPin* Pin)
{
	if (Pin)
	{
		PinId = Pin->PinId;
		Node = Pin->GetOwningNode();
		PinType = Pin->PinType;
		PinName = Pin->PinName;
	}
	else
	{
		PinId.Invalidate();
		Node = nullptr;
		PinType.ResetToDefaults();
		PinName = NAME_None;
	}
}

UEdGraphPin* FBANodePinHandle::GetPin()
{
	// find the pin from the node using the stored guid
	if (!PinId.IsValid() || !Node.IsValid())
	{
		return nullptr;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin->PinId == PinId)
		{
			return Pin;
		}
	}

	// guid failed, find using PinType & PinName
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if ((Pin->PinType == PinType) && (Pin->PinName == PinName))
		{
			// side effect: also update the latest PinId
			PinId = Pin->PinId;

			return Pin;
		}
	}

	return nullptr;
}

void FBANodeMovementTransaction::Begin(const TSet<UEdGraphNode*>& Nodes, const FText& InSessionName, const EBADragMethod& InDragMethod)
{
	if (Nodes.Num() == 0)
	{
		return;
	}

	const bool bHasTransaction = TransactionPtr.IsValid() && TransactionPtr->IsOutstanding();
	if (!bHasTransaction)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(InSessionName);
		DragMethod = InDragMethod;
	}

	for (UEdGraphNode* Node : Nodes)
	{
		Node->Modify(false);
	}
}

void FBANodeMovementTransaction::End(const EBADragMethod& InDragMethod)
{
	if (!IsValid() || DragMethod != InDragMethod)
	{
		return;
	}

	TransactionPtr.Reset();
}

void FBANodeArray::SetArray(const TArray<UEdGraphNode*>& Nodes)
{
	NodeArrayWeak = FBAMiscUtils::AsWeakObjectPtrArray(Nodes);
}

TArray<UEdGraphNode*> FBANodeArray::GetNodes() const
{
	TArray<UEdGraphNode*> OutNodeArray;
	for (TWeakObjectPtr<UEdGraphNode> Node : NodeArrayWeak)
	{
		if (Node.IsValid() && !FBAUtils::IsNodeDeleted(Node.Get()))
		{
			OutNodeArray.Add(Node.Get());
		}
	}

	return OutNodeArray;
}

void FBANodeArray::Empty()
{
	CachedNodes.Empty();
	NodeArrayWeak.Empty();
}

void FBANodeArray::CacheNodes()
{
	CachedNodes = GetNodes();
}

void FBASettingsPropertyHook::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	FNotifyHook::NotifyPreChange(PropertyAboutToChange);
	if (UClass* OwnerClass = PropertyAboutToChange->GetOwner<UClass>())
	{
		if (UObject* CDO = OwnerClass->GetDefaultObject())
		{
			FEditPropertyChain PropertyChain;
			PropertyChain.AddHead(PropertyAboutToChange);
			CDO->PreEditChange(PropertyChain);
		}
	}
}

void FBASettingsPropertyHook::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	if (UClass* OwnerClass = PropertyThatChanged->GetOwner<UClass>())
	{
		if (UObject* CDO = OwnerClass->GetDefaultObject())
		{
			CDO->PostEditChange();
			CDO->SaveConfig();
		}
	}
}
