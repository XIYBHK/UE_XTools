// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistNodeSizeChangeData.h"

#include "BlueprintAssistUtils.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CreateDelegate.h"
#include "UObject/TextProperty.h"

void FBAPinChangeData::UpdatePin(UEdGraphPin* Pin)
{
	bPinHidden = Pin->bHidden;
	bPinLinked = FBAUtils::IsPinLinked(Pin);
	PinValue = Pin->DefaultValue;
	PinTextValue = Pin->DefaultTextValue;
	PinLabel = GetPinLabel(Pin);
	PinObject = GetPinDefaultObjectName(Pin);
}

bool FBAPinChangeData::HasPinChanged(UEdGraphPin* Pin)
{
	if (bPinHidden != Pin->bHidden)
	{
		return true;
	}
	
	if (bPinLinked != FBAUtils::IsPinLinked(Pin))
	{
		// these pins do not change size
		if (Pin->PinType.PinSubCategory != UEdGraphSchema_K2::PC_Exec)
		{
			return true;
		}
	}

	if (PinValue != Pin->DefaultValue)
	{
		return true;
	}

	if (!PinTextValue.EqualTo(Pin->DefaultTextValue, ETextComparisonLevel::Default))
	{
		return true;
	}

	if (!PinLabel.EqualTo(GetPinLabel(Pin), ETextComparisonLevel::Default))
	{
		return true;
	}

	const FString PinDefaultObjectName = GetPinDefaultObjectName(Pin);
	if (PinObject != PinDefaultObjectName)
	{
		return true;
	}

	return false;
}

FString FBAPinChangeData::GetPinDefaultObjectName(UEdGraphPin* Pin) const
{
	return Pin->DefaultObject ? Pin->DefaultObject->GetName() : FString();
}

FText FBAPinChangeData::GetPinLabel(UEdGraphPin* Pin) const
{
	if (Pin)
	{
		if (UEdGraphNode* GraphNode = Pin->GetOwningNodeUnchecked())
		{
			return GraphNode->GetPinDisplayName(Pin);
		}
	}

	return FText::GetEmpty();
}

FBANodeSizeChangeData::FBANodeSizeChangeData(UEdGraphNode* Node)
{
	UpdateNode(Node);
}

void FBANodeSizeChangeData::UpdateNode(UEdGraphNode* Node)
{
	PinChangeData.Reset();
	for (UEdGraphPin* Pin : Node->GetAllPins())
	{
		PinChangeData.FindOrAdd(Pin->PinId).UpdatePin(Pin);
	}

	AdvancedPinDisplay = Node->AdvancedPinDisplay == ENodeAdvancedPins::Shown;
	NodeTitle = FBAUtils::GetNodeTitle(Node).ToString();
	bCommentBubblePinned = Node->bCommentBubblePinned;
	bCommentBubbleVisible = Node->bCommentBubbleVisible;
	CommentBubbleValue = Node->NodeComment;
	NodeEnabledState = Node->GetDesiredEnabledState();

	if (UK2Node_CreateDelegate* Delegate = Cast<UK2Node_CreateDelegate>(Node))
	{
		DelegateFunctionName = Delegate->GetFunctionName();
	}

	PropertyAccessTextPath = GetPropertyAccessTextPath(Node);
}

bool FBANodeSizeChangeData::HasNodeChanged(UEdGraphNode* Node)
{
	TArray<FGuid> PinGuids;
    PinChangeData.GetKeys(PinGuids);

	for (UEdGraphPin* Pin : Node->GetAllPins())
	{
		if (FBAPinChangeData* FoundPinData = PinChangeData.Find(Pin->PinId))
		{
			if (FoundPinData->HasPinChanged(Pin))
			{
				return true;
			}

			PinGuids.Remove(Pin->PinId);
		}
		else // added a new pin
		{
			return true;
		}
	}

	// If there are remaining pins, then they must have been removed
	if (PinGuids.Num())
	{
		return true;
	}

	if (AdvancedPinDisplay != (Node->AdvancedPinDisplay == ENodeAdvancedPins::Shown))
	{
		return true;
	}

	if (NodeTitle != FBAUtils::GetNodeTitle(Node).ToString())
	{
		return true;
	}

	if (bCommentBubblePinned != Node->bCommentBubblePinned)
	{
		return true;
	}

	if (bCommentBubbleVisible != Node->bCommentBubbleVisible)
	{
		return true;
	}

	if (CommentBubbleValue != Node->NodeComment)
	{
		return true;
	}

	if (NodeEnabledState != Node->GetDesiredEnabledState())
	{
		return true;
	}

	if (UK2Node_CreateDelegate* Delegate = Cast<UK2Node_CreateDelegate>(Node))
	{
		if (DelegateFunctionName != Delegate->GetFunctionName())
		{
			return true;
		}
	}

	if (PropertyAccessTextPath != GetPropertyAccessTextPath(Node))
	{
		return true;
	}

	return false;
}

FString FBANodeSizeChangeData::GetPropertyAccessTextPath(UEdGraphNode* Node)
{
	// have to read the property directly because K2Node_PropertyAccess is not exposed
	if (const FTextProperty* TextPathProperty = CastField<FTextProperty>(Node->GetClass()->FindPropertyByName("TextPath")))
	{
		if (const FText* TextResult = TextPathProperty->ContainerPtrToValuePtr<FText>(Node))
		{
			return TextResult->ToString();
		}
	}

	return FString();
}
