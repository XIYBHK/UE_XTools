// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class ENodeEnabledState : uint8;
class UEdGraphPin;
class UEdGraphNode;

struct FBAPinChangeData
{
	bool bPinLinked;
	bool bPinHidden;
	FString PinValue;
	FText PinTextValue;
	FText PinLabel;
	FString PinObject;

	FBAPinChangeData() = default;

	void UpdatePin(UEdGraphPin* Pin);

	bool HasPinChanged(UEdGraphPin* Pin);

	FString GetPinDefaultObjectName(UEdGraphPin* Pin) const;

	FText GetPinLabel(UEdGraphPin* Pin) const;
};


/**
 * @brief Node size can change by:
 *		- Pin being linked
 *		- Pin value changing
 *		- Pin being added or removed
 *		- Expanding the node (see print string)
 *		- Node title changing
 *		- Comment bubble pinned
 *		- Comment bubble visible
 *		- Comment bubble text
 *		- Node enabled state
 *		- Delegate signature pin at the bottom
 */
class FBANodeSizeChangeData
{
	TMap<FGuid, FBAPinChangeData> PinChangeData;
	bool bCommentBubblePinned;
	bool bCommentBubbleVisible;
	FString CommentBubbleValue;
	FString NodeTitle;
	bool AdvancedPinDisplay;
	ENodeEnabledState NodeEnabledState;
	FName DelegateFunctionName;
	FString PropertyAccessTextPath;

public:
	FBANodeSizeChangeData(UEdGraphNode* Node);

	void UpdateNode(UEdGraphNode* Node);

	bool HasNodeChanged(UEdGraphNode* Node);

	FString GetPropertyAccessTextPath(UEdGraphNode* Node);
};