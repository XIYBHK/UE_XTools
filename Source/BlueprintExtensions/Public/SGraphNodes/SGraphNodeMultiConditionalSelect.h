/*
 * Copyright (c) 2025 XIYBHK
 * Licensed under UE_XTools License
 * 
 * Based on AdvancedControlFlow by Colory Games (MIT License)
 * https://github.com/colory-games/UEPlugin-AdvancedControlFlow
 */

#pragma once

#include "SGraphNodeCasePairedPinsNode.h"

class UK2Node_MultiConditionalSelect;

class SGraphNodeMultiConditionalSelect : public SGraphNodeCasePairedPinsNode
{
	SLATE_BEGIN_ARGS(SGraphNodeMultiConditionalSelect)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UK2Node_MultiConditionalSelect* InNode);

	virtual void CreatePinWidgets() override;
};
