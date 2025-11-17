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

	TSet<FGuid> PinLinkedTo;

	FBAPinChangeData() = default;

	void UpdatePin(UEdGraphPin* Pin);

	bool HasPinChanged(UEdGraphPin* Pin) const;

	FString GetPinDefaultObjectName(UEdGraphPin* Pin) const;

	FText GetPinLabel(UEdGraphPin* Pin) const;

	static TSet<FGuid> GetPinLinkedTo(UEdGraphPin* Pin);
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
 *		- Node width change
 *		- Node enabled state
 *		- Delegate signature pin at the bottom
 */
class FBANodeSizeChangeData
{
protected:
	TMap<FGuid, FBAPinChangeData> PinChangeData;
	bool bCommentBubblePinned;
	bool bCommentBubbleVisible;
	FString CommentBubbleValue;
	int32 NodeWidth = 0;
	FString NodeTitle;
	bool AdvancedPinDisplay;
	ENodeEnabledState NodeEnabledState;
	FName DelegateFunctionName;
	FString PropertyAccessTextPath;

public:
	virtual ~FBANodeSizeChangeData() = default;
	FBANodeSizeChangeData(UEdGraphNode* Node);
	FBANodeSizeChangeData() = default;

	virtual void UpdateNode(UEdGraphNode* Node);

	virtual bool HasNodeChanged(UEdGraphNode* Node) const;

	FString GetPropertyAccessTextPath(UEdGraphNode* Node) const;

	static TSet<FGuid> GetParentComments(UEdGraphNode* Node);
};

class FBAFormattingChangeData : public FBANodeSizeChangeData
{
public:
	explicit FBAFormattingChangeData(UEdGraphNode* Node);
	FBAFormattingChangeData() = default;

	int32 NodePosX = 0;
	int32 NodePosY = 0;
	TSet<FGuid> ParentComments;

	virtual void UpdateNode(UEdGraphNode* Node) override;
	virtual bool HasNodeChanged(UEdGraphNode* Node) const override;

	// if the links or parent comments have changed
	bool NeedsFullFormatting(UEdGraphNode* Node) const;

	static TSet<FGuid> GetParentComments(UEdGraphNode* Node);
};
