// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "EdGraphUtilities.h"

class XTOOLS_BLUEPRINTASSIST_API FBlueprintAssistGraphPanelNodeFactory : public FGraphPanelNodeFactory
{
public:
	virtual TSharedPtr<SGraphNode> CreateNode(class UEdGraphNode* Node) const override;
};
